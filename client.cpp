#include <cstdio>
#include <cstdio>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <strings.h>
#include <string.h>
#include <arpa/inet.h>
#define SERVER_PORT 9876
#define MESSAGE_LEN 1024

using namespace std;

int main(int argc, char* argv[]){

        int socket_fd;
        int on = 1;
        int ret = -1;
        struct sockaddr_in serveraddr;
        char send_buf[MESSAGE_LEN] = "Hello ! This is Client";
        char recv_buf[MESSAGE_LEN];

        /* 1. 创建文件描述符 */
        socket_fd = socket(AF_INET, SOCK_STREAM, 0);
        if(socket_fd == -1){
                std::cout << "Failed to create socket !" << std::endl;
                exit(-1);
        }
        // 设置 socket 选项
        ret = setsockopt(socket_fd,
                        SOL_SOCKET,
                        SO_REUSEADDR,
                        (void *)&on,
                        sizeof(on));
        if(ret == -1){
                std::cout << "Failed to set socket !" << std::endl;
                exit(-1);
        }
        /* 2. 连接服务器 */
		// 配置服务端信息
        serveraddr.sin_family = AF_INET;
        serveraddr.sin_port = htons(SERVER_PORT);
        serveraddr.sin_addr.s_addr = inet_addr("10.250.16.149");
        bzero(&(serveraddr.sin_zero), 8);       // 将预留位赋值为0
		// 连接
        ret = connect(socket_fd,
                    (struct sockaddr *)&serveraddr,
                    sizeof(struct sockaddr));
        if(ret == -1){
                std::cout << "Failed to connect !" << std::endl;
                exit(-1);
        }

        /* 3. 发送数据 */
        while(1){
				// 从控制台输入要发送的数据
                memset((void *)send_buf , 0, MESSAGE_LEN);
                cin.getline(send_buf, 90);
                // 发送数据
            	ret = send(socket_fd, (void *)send_buf, strlen(send_buf), 0);
                if(ret <= 0){
                        std::cout << "Failed to send data !" << std::endl;
                        break;
                }
                std::cout << "Client Send --------> \n" << send_buf << std::endl;

                /* 4. 接收数据 */
                ret = recv(socket_fd, (void *)recv_buf, MESSAGE_LEN, 0);
                if(ret <= 0){
                        break;
                }
            	// 要把接收到的数据的数组中此条长度的后面切掉，否则会出现拼接上一次数据的现象
                recv_buf[ret] = '\0';
                std::cout << "Recv From Server------------> \n" << recv_buf << std::endl;
                bzero((void *)recv_buf, MESSAGE_LEN);
        }
        close(socket_fd);
        return 0;
}

