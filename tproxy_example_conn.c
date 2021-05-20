//Written by Kristian Evensen <kristian.evensen@gmail.com>

#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <errno.h>
#include <string.h>
#include <sys/epoll.h>
#include <stdlib.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "tproxy_example_conn.h"

//Createas a socket and initiates the connection to the host specified by 
//remote_addr.
//Returns 0 if something fails, >0 on success (socket fd).
static int connect_remote(struct sockaddr_storage *remote_addr){
    int remote_fd = 0, yes = 1;

    //Use NONBLOCK to avoid slow connects affecting the performance of other
    //connections
    if((remote_fd = socket(remote_addr->ss_family, SOCK_STREAM | 
                    SOCK_NONBLOCK, 0)) < 0){
        perror("socket (connect_remote): ");
        return 0;
    }

    if(setsockopt(remote_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0){
        perror("setsockopt (SO_REUSEADDR, connect_remote): ");
        close(remote_fd);
        return 0;
    }
   
    if(connect(remote_fd, (struct sockaddr*) remote_addr, 
            remote_addr->ss_family == AF_INET ? sizeof(struct sockaddr_in) :
            sizeof(struct sockaddr_in6)) < 0){
        if(errno != EINPROGRESS){
            perror("connect (connect_remote): ");
            close(remote_fd);
            return 0;
        }
    }

    return remote_fd;
}

//Store the original destination address in remote_addr
//Return 0 on success, <0 on failure
static int get_org_dstaddr(int sockfd, struct sockaddr_storage *orig_dst){
    char orig_dst_str[INET6_ADDRSTRLEN];
    socklen_t addrlen = sizeof(*orig_dst);

    memset(orig_dst, 0, addrlen);

    //For UDP transparent proxying:
    //Set IP_RECVORIGDSTADDR socket option for getting the original 
    //destination of a datagram

    //Socket is bound to original destination
    if(getsockname(sockfd, (struct sockaddr*) orig_dst, &addrlen) 
            < 0){
        perror("getsockname: ");
        return -1;
    } else {
        if(orig_dst->ss_family == AF_INET){
            inet_ntop(AF_INET, 
                    &(((struct sockaddr_in*) orig_dst)->sin_addr),
                    orig_dst_str, INET_ADDRSTRLEN);
            fprintf(stderr, "Original destination %s\n", orig_dst_str);
        } else if(orig_dst->ss_family == AF_INET6){
            inet_ntop(AF_INET6, 
                    &(((struct sockaddr_in6*) orig_dst)->sin6_addr),
                    orig_dst_str, INET6_ADDRSTRLEN);
            fprintf(stderr, "Original destination %s\n", orig_dst_str);
        }

        return 0;
    }
}

//Acquires information, initiates a connect and initialises a new connection
//object. Return NULL if anything fails, pointer to object otherwise
tproxy_conn_t* add_tcp_connection(int efd, struct tailhead *conn_list, 
        int local_fd){
    struct sockaddr_storage orig_dst;
    tproxy_conn_t *conn;
    int remote_fd;
    struct epoll_event ev;
 
    if(get_org_dstaddr(local_fd, &orig_dst)){
        fprintf(stderr, "Could not get local address\n");
        close(local_fd);
        local_fd = 0;
        return NULL;
    }

    if((remote_fd = connect_remote(&orig_dst)) == 0){
        fprintf(stderr, "Failed to connect\n");
        close(remote_fd);
        close(local_fd);
        return NULL;
    }

    //Create connection object and fill in information
    if((conn = (tproxy_conn_t*) malloc(sizeof(tproxy_conn_t))) == NULL){
        fprintf(stderr, "Could not allocate memory for connection\n");
        close(remote_fd);
        close(local_fd);
        return NULL;
    }

    memset(conn, 0, sizeof(tproxy_conn_t));
    conn->state = CONN_AVAILABLE;
    conn->remote_fd = remote_fd;
    conn->local_fd = local_fd;
    TAILQ_INSERT_HEAD(conn_list, conn, conn_ptrs);

    if(pipe(conn->splice_pipe) != 0){
        fprintf(stderr, "Could not create the required pipe\n");
        free_conn(conn);
        return NULL;
    }

    //remote_fd is connecting. Non-blocking connects are signaled as done by 
    //socket being marked as ready for writing
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN | EPOLLOUT;
    ev.data.ptr = (void*) conn;

    if(epoll_ctl(efd, EPOLL_CTL_ADD, remote_fd, &ev) == -1){
        perror("epoll_ctl (remote_fd)");
        free_conn(conn);
        return NULL;
    }

    //Local socket can be closed while waiting for connection attempt. I need
    //to detect this when waiting for connect() to complete. However, I dont
    //want to get EPOLLIN-events, as I dont want to receive any data before
    //remote connection is established
    ev.events = EPOLLRDHUP;

    if(epoll_ctl(efd, EPOLL_CTL_ADD, local_fd, &ev) == -1){
        perror("epoll_ctl (local_fd)");
        free_conn(conn);
        return NULL;
    } else
        return conn;
} 

//Free resources occupied by this connection
void free_conn(tproxy_conn_t *conn){
    close(conn->remote_fd);
    close(conn->local_fd);

    if(conn->splice_pipe[0] != 0){
        close(conn->splice_pipe[0]);
        close(conn->splice_pipe[1]);
    }

    free(conn);
}

//Checks if a connection attempt was successful or not
//Returns 0 if successfull, -1 if not
int8_t check_connection_attempt(tproxy_conn_t *conn, int efd){
    struct epoll_event ev;
    int conn_success = 0;
    int fd_flags = 0;
    socklen_t optlen = sizeof(conn_success);

    //If the connection was sucessfull or not is contained in SO_ERROR
    if(getsockopt(conn->remote_fd, SOL_SOCKET, SO_ERROR, &conn_success, 
                &optlen) == -1){
        perror("getsockopt (SO_ERROR)");
        return -1;
    }

    if(conn_success == 0){
        fprintf(stderr, "Socket %d connected\n", conn->remote_fd);
       
        //Set socket as blocking now, for ease of processing
        //TODO: Non-blocking
        if((fd_flags = fcntl(conn->remote_fd, F_GETFL)) == -1){
            perror("fcntl (F_GETFL)");
            return -1;
        }

        if(fcntl(conn->remote_fd, F_SETFL, fd_flags & ~O_NONBLOCK) == -1){
            perror("fcntl (F_SETFL)");
            return -1;
        }

        //Update both file descriptors. I am interested in EPOLLIN (if there is
        //any data) and EPOLLRDHUP (remote peer closed socket). As this is just
        //an example, EPOLLOUT is ignored and it is OK for send() to block
        memset(&ev, 0, sizeof(ev));
        ev.events = EPOLLIN | EPOLLRDHUP;
        ev.data.ptr = (void*) conn;

        if(epoll_ctl(efd, EPOLL_CTL_MOD, conn->remote_fd, &ev) == -1 ||
                epoll_ctl(efd, EPOLL_CTL_MOD, conn->local_fd, &ev) == -1){
            perror("epoll_ctl (check_connection_attempt)");
            return -1;
        } else {
            return 0;
        }
    }
        
    return -1;
}
