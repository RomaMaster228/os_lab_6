#include "zmq_comm.h"

void create_msg(message* msg, const cmd_type cmd, const int to_id, const int value) {
    msg->cmd = cmd;
    msg->to_id = to_id;
    msg->value = value;
}

void print_message(message* msg) {
    printf("\tmsg content:\n"
    "\t  cmd = \"%d\"\n"
    "\tto_id = \"%d\"\n"
    "\tvalue = \"%d\"\n",
    msg->cmd, msg->to_id, msg->value);
    fflush(stdout);
}

void create_zmq_msg(zmq_msg_t* zmq_msg, const message* msg) {
    zmq_msg_init_size(zmq_msg, sizeof(*msg));
    memcpy(zmq_msg_data(zmq_msg), msg, sizeof(*msg));
}

void send_msg(void* socket, const message* msg) {
    zmq_msg_t zmq_msg;
    create_zmq_msg(&zmq_msg, msg);
    if (!zmq_msg_send(&zmq_msg, socket, 0)) {
        fprintf(stderr, "[%d] ", getpid());
        perror("ERROR send_msg \n");
        exit(ERR_ZMQ_SEND);
    }
    zmq_msg_close(&zmq_msg);
}

// 1 on success
int get_msg(void* socket, message* msg) {
    zmq_msg_t zmq_msg;
    zmq_msg_init(&zmq_msg);
    zmq_msg_init_size(&zmq_msg, sizeof(message));
    int rc = zmq_msg_recv(&zmq_msg, socket, 0);
    if (rc == -1 && errno == EAGAIN) {
        return 0;
    }
    memcpy(msg, zmq_msg_data(&zmq_msg), sizeof(message));
    zmq_msg_close(&zmq_msg);
    return 1;
}

void create_endpoint(char* endpoint, int id, endpoint_type type) {
    char id_str[MAX_LEN];
    sprintf(id_str, "%d", id);
    char* pattern;
    if (type == server_pub) {
        strcpy(endpoint, SERVER_SOCKET_PUB);
        return;
    } else if (type == client_left_pub) {
        pattern = CLIENT_LEFT_PUB_PATTERN;
    } else if (type == client_right_pub) {
        pattern = CLIENT_RIGHT_PUB_PATTERN;
    } else if (type == client_parent_pub) {
        pattern = CLIENT_PARENT_PUB_PATTERN;
    }
    strcpy(endpoint, pattern);
    strcat(endpoint, id_str);
}

///

void* create_zmq_context() {
    void* context = zmq_ctx_new();
    if (context == NULL) {
        fprintf(stderr, "[%d] ", getpid());
        perror("ERROR zmq_ctx_new ");
        exit(ERR_ZMQ_CTX);
    }
    return context;
}

void disconnect_zmq_socket(void* socket, char* endpoint) {
    if (zmq_disconnect(socket, endpoint) != 0) {
        fprintf(stderr, "[%d] ", getpid());
        perror("ERROR zmq_disconnect ");
        exit(ERR_ZMQ_DISCONNECT);
    }
}

void connect_zmq_socket(void* socket, char* endpoint) {
    if (zmq_connect(socket, endpoint) != 0) {
        fprintf(stderr, "[%d] ", getpid());
        perror("ERROR zmq_connect ");
        exit(ERR_ZMQ_CONNECT);
    }
}

void bind_zmq_socket(void* socket, char* endpoint) {
    if (zmq_bind(socket, endpoint) != 0) {
        fprintf(stderr, "[%d] ", getpid());
        perror("ERROR zmq_bind ");
        exit(ERR_ZMQ_BIND);
    }
}

void unbind_zmq_socket(void* socket, char* endpoint) {
    if (zmq_unbind(socket, endpoint) != 0) {
        fprintf(stderr, "[%d] ", getpid());
        perror("ERROR zmq_unbind ");
        exit(ERR_ZMQ_BIND);
    }
}

void* create_zmq_socket(void* context, const int type) {
    void* socket = zmq_socket(context, type);
    if (socket == NULL) {
        fprintf(stderr, "[%d] ", getpid());
        perror("ERROR zmq_socket ");
        exit(ERR_ZMQ_SOCKET);
    }
    if (type == ZMQ_SUB) {
        zmq_setsockopt(socket, ZMQ_SUBSCRIBE, 0, 0);
    }
    return socket;
}

void reconnect_zmq_socket(void* socket, char* from, char* to) {
    disconnect_zmq_socket(socket, from);
    connect_zmq_socket(socket, to);
}

void close_zmq_socket(void* socket) {
    if (zmq_close(socket) != 0) {
        fprintf(stderr, "[%d] ", getpid());
        perror("ERROR zmq_close ");
        exit(ERR_ZMQ_CLOSE);
    }
}

void destroy_zmq_context(void* context) {
    if (zmq_ctx_destroy(context) != 0) {
        fprintf(stderr, "[%d] ", getpid());
        perror("ERROR zmq_ctx_destroy ");
        exit(ERR_ZMQ_CLOSE);
    }
}