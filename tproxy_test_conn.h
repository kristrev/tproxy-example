//Written by Kristian Evensen <kristian.evensen@gmail.com>
//Use for whatever you want :)

#ifndef TPROXY_TEST_CONN_H
#define TPROXY_TEST_CONN_H

#include "tproxy_test.h"

tproxy_conn_t* add_tcp_connection(int efd, struct tailhead *conn_list, 
        int local_fd);
void free_conn(tproxy_conn_t *conn);
int8_t check_connection_attempt(tproxy_conn_t *conn, int efd);
#endif
