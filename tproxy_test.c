//Written by Kristian Evensen <kristian.evensen@gmail.com>
//Use for whatever you want :)

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/queue.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <errno.h>

#include "tproxy_test.h"
#include "tproxy_test_conn.h"

void close_tcp_conn(tproxy_conn_t *conn, struct tailhead *conn_list, 
        struct tailhead *close_list){
    conn->state = CONN_CLOSED;
    TAILQ_REMOVE(conn_list, conn, conn_ptrs);
    TAILQ_INSERT_TAIL(close_list, conn, conn_ptrs);
}

int handle_epollin(tproxy_conn_t *conn){
    int numbytes;
    int fd_in, fd_out;

    //Easy way to determin which socket is ready for reading
    //TODO: Optimize. This one allows me quick lookup for conn, but
    //I need to make a system call to determin which socket
    if(ioctl(conn->local_fd, FIONREAD, &numbytes) != -1 
            && numbytes > 0){
        fd_in = conn->local_fd;
        fd_out = conn->remote_fd;
    } else {
        fd_in = conn->remote_fd;
        fd_out = conn->local_fd;
    }

    //Optimize with SPLICE_F_MORE later
    numbytes = splice(fd_in, NULL, conn->splice_pipe[1], NULL, 
            SPLICE_LEN, SPLICE_F_MOVE);

    if(numbytes > 0)
        numbytes = splice(conn->splice_pipe[0], NULL, fd_out, NULL,
                numbytes, SPLICE_F_MOVE);

    return numbytes;
}

void remove_closed_connections(struct tailhead *close_list){
    tproxy_conn_t *conn = NULL;

    while(close_list->tqh_first != NULL){
        conn = (tproxy_conn_t*) close_list->tqh_first;
        TAILQ_REMOVE(close_list, close_list->tqh_first, conn_ptrs);
        fprintf(stderr, "Socket %d and %d closed, connection removed\n",
            conn->local_fd, conn->remote_fd);
        free_conn(conn);
    } 
}

int event_loop(int listen_fd){
    int numbytes = 0, retval = 0, num_events = 0;
    int tmp_fd = 0; //Used to temporarily hold the accepted file descriptor
    tproxy_conn_t *conn = NULL;
    int efd, i;
    struct epoll_event ev, events[MAX_EPOLL_EVENTS];
    struct tailhead conn_list, close_list;
    uint8_t check_close = 0;

    //Initialize queue (remember that TAILQ_HEAD just defines the struct)
    TAILQ_INIT(&conn_list);
    TAILQ_INIT(&close_list);
    
    if((efd = epoll_create(1)) == -1){
        perror("epoll_create");
        return -1;
    }

    //Start monitoring listen socket
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;
    //There is only one listen socket, and I want to use ptr in order to have 
    //easy access to the connections. So if ptr is NULL that means an event on
    //listen socket.
    ev.data.ptr = NULL;
    if(epoll_ctl(efd, EPOLL_CTL_ADD, listen_fd, &ev) == -1){
        perror("epoll_ctl (listen socket)");
        return -1;
    }
    
    while(1){
        if((num_events = epoll_wait(efd, events, MAX_EPOLL_EVENTS, -1)) == -1){
            perror("epoll_wait");
            retval = -1;
            break;
        }

        for(i=0; i<num_events; i++){
            if(events[i].data.ptr == NULL){
                //Accept new connection
                tmp_fd = accept(listen_fd, NULL, NULL);            
                if((conn = add_tcp_connection(efd, &conn_list, tmp_fd)) 
                        == NULL){
                    fprintf(stderr, "Failed to add connection\n");
                    close(tmp_fd);
                } 
            } else {
                conn = (tproxy_conn_t*) events[i].data.ptr;

                //Only applies to remote_fd, connection attempt has
                //succeeded/failed
                if(events[i].events & EPOLLOUT){
                    if(check_connection_attempt(conn, efd) == -1){
                        fprintf(stderr, "Connection attempt failed for %d\n", 
                                conn->remote_fd);
                        check_close = 1;
                        close_tcp_conn(conn, &conn_list, &close_list);
                    }
                    continue;
                } else if(conn->state != CONN_CLOSED && 
                        (events[i].events & EPOLLRDHUP || 
                         events[i].events & EPOLLHUP ||
                        events[i].events & EPOLLERR)){
                    check_close = 1;
                    close_tcp_conn(conn, &conn_list, &close_list);
                    continue;
                }

                //Since I use an event cache, earlier events might cause for
                //example this connection to be closed. No need to process fd if
                //that is the case
                if(conn->state == CONN_CLOSED){
                    continue;
                }

                numbytes = handle_epollin(conn);
              
                //Splice fails if for example remote socket is
                //closed during splicing. Returns 0 if a peer closes the socket.
                //Another way of detecting the latter is to use EPOLLRDHUP
                if(numbytes <= 0){
                    close_tcp_conn(conn, &conn_list, &close_list);
                    check_close = 1;
                }
            }
        }

        //Remove connections
        if(check_close)
            remove_closed_connections(&close_list);

        check_close = 0;
    }

    //Add cleanup
    return retval;
}

int8_t block_sigpipe(){
    sigset_t sigset;
    memset(&sigset, 0, sizeof(sigset));

    //Get the old sigset, add SIGPIPE and update sigset
    if(sigprocmask(SIG_BLOCK, NULL, &sigset) == -1){
        perror("sigprocmask (get)");
        return -1;
    }

    if(sigaddset(&sigset, SIGPIPE) == -1){
        perror("sigaddset");
        return -1;
    }

    if(sigprocmask(SIG_BLOCK, &sigset, NULL) == -1){
        perror("sigprocmask (set)");
        return -1;
    }

    return 0;
}

int main(int argc, char *argv[]){
    int listen_fd = 0;
    int yes = 1, retval = 0;
    struct addrinfo hints, *res;
    
    memset(&hints, 0, sizeof(hints));

    //IPv4/IPv6 transition works out of the box, so no need to specify family
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    //Create the listen sock and bind it to the desired port
    if(getaddrinfo(NULL, TPROXY_PORT, &hints, &res) != 0){
        perror("getaddrinfo: ");
        exit(EXIT_FAILURE);
    }

    if((listen_fd = socket(res->ai_family, res->ai_socktype, 
                    res->ai_protocol)) == -1){
        perror("socket: ");
        exit(EXIT_FAILURE);
    }

    if(setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) 
            == -1){
        perror("setsockopt (SO_REUSEADDR): ");
        close(listen_fd);
        exit(EXIT_FAILURE);
    }

    //Mark that this socket can be used for transparent proxying
    //This allows the socket to accept connections for non-local IPs
    if(setsockopt(listen_fd, SOL_IP, IP_TRANSPARENT, &yes, sizeof(yes)) 
            == -1){
        perror("setsockopt (IP_TRANSPARENT): ");
        close(listen_fd);
        exit(EXIT_FAILURE);
    }

    if(bind(listen_fd, res->ai_addr, res->ai_addrlen) == -1){
        perror("bind: ");
        close(listen_fd);
        exit(EXIT_FAILURE);
    }

    if(listen(listen_fd, BACKLOG) == -1){
        perror("listen: ");
        close(listen_fd);
        exit(EXIT_FAILURE);
    }

    //splice() causes the process to receive the SIGPIPE-signal if one part (for
    //example a socket) is closed during splice(). I would rather have splice()
    //fail and return -1, so blocking SIGPIPE.
    if(block_sigpipe() == -1){
        fprintf(stderr, "Could not block SIGPIPE signal\n");
        close(listen_fd);
        exit(EXIT_FAILURE);
    }

    fprintf(stderr, "Will listen to port %s\n", TPROXY_PORT);

    retval = event_loop(listen_fd);
    close(listen_fd);

    fprintf(stderr, "Will exit\n");
        
    if(retval < 0)
        exit(EXIT_FAILURE);
    else
        exit(EXIT_SUCCESS);
}
