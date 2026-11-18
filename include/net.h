#ifndef NET_H
#define NET_H

#include <stddef.h>

#include "message.h"
#include "error_code.h"

// ipv4 only, port as in argv
ErrorCode net_listen(const char* address, const char* port, int* fd);
// peer gets "ip:port" of the client
ErrorCode net_accept(int listen_fd, int* fd, char* peer, size_t peer_size);

int net_has_client(int listen_fd);
ErrorCode net_connect(const char* address, const char* port, int* fd);

// ERR_INTERRUPTED on signal, ERR_NETWORK on error or closed connection
ErrorCode net_send(int fd, const Message* message);
ErrorCode net_receive(int fd, Message* message);

#endif //NET_H
