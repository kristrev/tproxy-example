/*
	使用 epoll 的服务端程序
*/
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <strings.h>
#include <sys/epoll.h>
#include <fcntl.h>

#define SERVER_PROT  9876
#define MESSAGE_LEN  1024
#define MAX_EVENTS 20
#define TIMEOUT 500

int main(int argc, char* argv[]){
    int socket_fd, accept_fd;
    int on = 1;
    int ret = -1;
    struct sockaddr_in localaddr, remoteaddr;
    socklen_t addrlen;
    char recv_buf[1024];
    int epoll_fd;
    struct epoll_event ev, events[MAX_EVENTS];
    int event_number;
    int flags;

    // 1. 创建套接字文件描述符,并设置 socket 选项
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(socket_fd == -1){
        std::cout << "Failed to create socket !" << std::endl;
        exit(-1);
    }
    ret = setsockopt(socket_fd,
                     SOL_SOCKET,
                     SO_REUSEADDR,
                     &on,
                     sizeof(on));
    if(ret == -1){
        std::cout << "Failed to set socket !" << std::endl;
    }

    /* 要把文件描述符设置为非阻塞的 */
    flags = fcntl(accept_fd, F_GETFL, 0);
    fcntl(accept_fd, F_SETFL, flags | O_NONBLOCK);

    // 2. 绑定端口和地址
    localaddr.sin_family = AF_INET;
    localaddr.sin_port = htons(SERVER_PROT);
    localaddr.sin_addr.s_addr = INADDR_ANY;
    bzero(&(localaddr.sin_zero), 8);                // 填充 0 到数组中

    ret = bind(socket_fd,
               (struct sockaddr *)&localaddr,
               sizeof(struct sockaddr_in));
    if(ret == -1){
        std::cout << "Failed to bind the socket !" << std::endl;
        exit(-1);
    }

    // 3. 开始监听
    ret = listen(socket_fd, 10);    //由于 listen 一次只能接受一个，所以需要一个队列来存放并发的请求，长度为10够了
    if(ret == -1){
        std::cout << "Failed to listen... !" << std::endl;
        exit(-1);
    }
	std::cout << "will to listen port: " << SERVER_PROT << std::endl;
    /* epoll_1：创建一个 epoll 文件描述符 */
    epoll_fd = epoll_create(256);
    ev.events = EPOLLIN;
    ev.data.fd = socket_fd;
    /* epoll_2：将监听文件描述符添加到 epoll_fd 中 */
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, socket_fd, &ev);

    addrlen = sizeof(struct sockaddr_in);
    for(;;){
        /* epoll_3：等待事件，相当于 select 函数 */
        event_number = epoll_wait(epoll_fd, events, MAX_EVENTS, TIMEOUT);

        for(int i=0; i<event_number; i++){
            /* 判断是数据的，还是侦听的 socket */
            if(events[i].data.fd == socket_fd){		// 如果是侦听的，说明有新连接来了
                accept_fd = accept(socket_fd,
                                   (struct sockaddr *)&remoteaddr,
                                   &addrlen);
                /* 要把文件描述符设置为非阻塞的 */
                flags = fcntl(accept_fd, F_GETFL, 0);
                fcntl(accept_fd, F_SETFL, flags | O_NONBLOCK);
                /* 设置边缘触发 */
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = accept_fd;
                /* 将新的文件描述符添加到 epoll_fd 中 */
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, accept_fd, &ev);
            }else if(events[i].events & EPOLLIN){          // 如果是一个读入事件
                do{
                    ret = recv(events[i].data.fd,
                               (void *)recv_buf,
                               MESSAGE_LEN,
                               0);
                }while(ret < 0 && errno == EINTR);  // 如果由于中断出错,说明数据未读完,就继续读
                /* 如果对端关闭连接 */
                if(ret == 0){
                    close(events[i].data.fd);
                }
                /* 如果缓冲区满了 */
                if(ret == MESSAGE_LEN){
                    std::cout << "may be have more data" << std::endl;
                }
                /* 如果数据已经读完了，出错了 */
                if(ret < 0 ){
                    switch(errno){
                        case EAGAIN:
                            continue;
                        default:
                            continue;
                    }
                }
                /* 如果有数据 */
                if(ret > 0){
                    /* 打印数据 */
                    recv_buf[ret] = '\0';
                    std::cout << "Server Recv:-----> " << recv_buf << std::endl;
                    /* 数据回显 */
                    ret = send(events[i].data.fd,
                               (void *)recv_buf,
                               MESSAGE_LEN,
                               0);
                }
            }
        } 
    }
    close(socket_fd);
    return 0;
}                                                                                                   

