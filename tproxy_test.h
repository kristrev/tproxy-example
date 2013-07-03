#ifndef TPROXY_EXAMPLE_H
#define TPROXY_EXAMPLE_H

#include <stdint.h>
#include <sys/queue.h>

#define TPROXY_PORT "9876"
#define BACKLOG 10
#define MAX_EPOLL_EVENTS BACKLOG
#define IP_TRANSPARENT 19 //So that application compiles on OpenWRT
#define SPLICE_LEN 4096 //Try one page
#define PIPE_CAPCITY 65536

//Three different states of a connection
enum{
    CONN_AVAILABLE=0,
    CONN_CLOSED,
};
typedef uint8_t conn_state_t;

struct tproxy_conn{
    int local_fd; //Connection to host on local network
    int remote_fd; //Connection to remote host
    int splice_pipe[2]; //Have pipes per connection for now. Multiplexing 
                        //different connections onto pipes is tricky, for
                        //example when flushing pipe after one connection has
                        //failed.
    conn_state_t state;

    //Create the struct which contains ptrs to next/prev element
    TAILQ_ENTRY(tproxy_conn) conn_ptrs;
};
typedef struct tproxy_conn tproxy_conn_t;

//Define the struct tailhead (code in sys/queue.h is quite intuitive)
//Use tail queue for efficient delete
TAILQ_HEAD(tailhead, tproxy_conn);
#endif
