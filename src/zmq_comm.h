#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <zmq.h>

#define MAX_LEN 64

#define ERR_ZMQ_CTX            100
#define ERR_ZMQ_SOCKET         101
#define ERR_ZMQ_BIND           102
#define ERR_ZMQ_CLOSE          102
#define ERR_ZMQ_CTX_DESTROY    103
#define ERR_ZMQ_CONNECT        104
#define ERR_ZMQ_DISCONNECT     105
#define ERR_ZMQ_SEND           106

#define SERVER_SOCKET_PUB           "ipc://tmp/server_pub"
#define CLIENT_PARENT_PUB_PATTERN   "ipc://tmp/client_parent_pub_"
#define CLIENT_LEFT_PUB_PATTERN     "ipc://tmp/client_left_pub_"
#define CLIENT_RIGHT_PUB_PATTERN    "ipc://tmp/client_right_pub_"

#define CLIENT_EXE "./client"

#define CLIENT_SIG_ERR       3
#define SERVER_SIG_ERR       4

typedef enum {
    create, // to_id = id to create
    delete,
    kill_all, // no to_id

    ping,
    change_sub,
    test,
    substring
} cmd_type;

typedef struct {
    cmd_type  cmd;
    int to_id;
    int value;
    char str[MAX_LEN];
    char sub[MAX_LEN];
} message;

typedef enum {
    server_pub,
    client_left_pub,
    client_right_pub,
    client_parent_pub
} endpoint_type;

void  create_msg(message* msg, const cmd_type cmd, const int to_id, const int value);
void  print_message(message* msg);
void  create_zmq_msg(zmq_msg_t* zmq_msg, const message* msg);
void  send_msg(void* socket, const message* msg);
int   get_msg(void* socket, message* msg);

void  create_endpoint(char* endpoint, int id, endpoint_type type);

void* create_zmq_context();
void  connect_zmq_socket(void* socket, char* endpoint);
void  disconnect_zmq_socket(void* socket, char* endpoint);
void  bind_zmq_socket(void* socket, char* endpoint);
void  unbind_zmq_socket(void* socket, char* endpoint);
void  reconnect_zmq_socket(void* socket, char* from, char* to);
void* create_zmq_socket(void* context, const int type);
void  close_zmq_socket(void* socket);
void  destroy_zmq_context(void* context);