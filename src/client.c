#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include "zmq_comm.h"

// context and sockets
static void *   CONTEXT;
static void *   SUBSCRIBER;
static void *   PARENT_PUB;
static void *   RIGHT_PUB;
static void *   LEFT_PUB;

// SOCKETS ENDPOINT
// for subscriber socket
static char PARENT_SUB_ENDPOINT[MAX_LEN]; // recieve by parent
static char LEFT_SUB_ENDPOINT[MAX_LEN];
static char RIGHT_SUB_ENDPOINT[MAX_LEN];

static char PARENT_PUB_ENDPOINT[MAX_LEN]; // recieve by parent
static char LEFT_PUB_ENDPOINT[MAX_LEN];
static char RIGHT_PUB_ENDPOINT[MAX_LEN];

static int PARENT_ID;
static int CLIENT_PID;
static int CLIENT_ID;

bool HAS_LEFT;
bool HAS_RIGHT;



void init_client() {
    create_endpoint(LEFT_PUB_ENDPOINT, CLIENT_ID, client_left_pub);
    create_endpoint(RIGHT_PUB_ENDPOINT, CLIENT_ID, client_right_pub);

    CONTEXT = create_zmq_context();

    SUBSCRIBER = create_zmq_socket(CONTEXT, ZMQ_SUB);
    connect_zmq_socket(SUBSCRIBER, PARENT_SUB_ENDPOINT);

    PARENT_PUB = create_zmq_socket(CONTEXT, ZMQ_PUB);
    bind_zmq_socket(PARENT_PUB, PARENT_PUB_ENDPOINT);

    RIGHT_PUB = create_zmq_socket(CONTEXT, ZMQ_PUB);
    bind_zmq_socket(RIGHT_PUB, RIGHT_PUB_ENDPOINT);

    LEFT_PUB = create_zmq_socket(CONTEXT, ZMQ_PUB);
    bind_zmq_socket(LEFT_PUB, LEFT_PUB_ENDPOINT);
}

void deinit_client() {
        // printf("[%d] Deinit client %d.\n", CLIENT_PID, CLIENT_ID); fflush(stdout);
        disconnect_zmq_socket(SUBSCRIBER, PARENT_SUB_ENDPOINT);
        if (HAS_LEFT) {
                disconnect_zmq_socket(SUBSCRIBER, LEFT_SUB_ENDPOINT);
        }
        if (HAS_RIGHT) {
                disconnect_zmq_socket(SUBSCRIBER, RIGHT_SUB_ENDPOINT);
        }
        close_zmq_socket(SUBSCRIBER);
        unbind_zmq_socket(PARENT_PUB, PARENT_PUB_ENDPOINT);
        unbind_zmq_socket(LEFT_PUB, LEFT_PUB_ENDPOINT);
        unbind_zmq_socket(RIGHT_PUB, RIGHT_PUB_ENDPOINT);
        close_zmq_socket(PARENT_PUB);
        close_zmq_socket(LEFT_PUB);
        close_zmq_socket(RIGHT_PUB);
}

void hdl_signal(int signal) {
        // printf("[%d] Terminating client %d...\n", CLIENT_PID, CLIENT_ID); fflush(stdout);
        deinit_client();
        exit(EXIT_SUCCESS);
}

void print_info() {
        printf("[%d] I'm client %d\n"
                        "\t parent_id  = \"%d\"\n"
                        "\t parent_sub = \"%s\"\n"
                        "\t   left_sub = \"%s\"\n"
                        "\t  right_sub = \"%s\"\n"
                        "\t parent_pub = \"%s\"\n"
                        "\t   left_pub = \"%s\"\n"
                        "\t  right_pub = \"%s\"\n",
        CLIENT_PID, CLIENT_ID, PARENT_ID,
        PARENT_SUB_ENDPOINT, LEFT_SUB_ENDPOINT, RIGHT_SUB_ENDPOINT,
        PARENT_PUB_ENDPOINT, LEFT_PUB_ENDPOINT, RIGHT_PUB_ENDPOINT);
        fflush(stdout);
}

void create_child_process(const int child_id) {
        // printf("[%d] Client %d: creating child...\n", CLIENT_PID, CLIENT_ID); fflush(stdout);
        char parent_sub_endpoint[MAX_LEN]; // parent --> child
        bool is_left = child_id < CLIENT_ID;
        if (is_left) {
                strcpy(parent_sub_endpoint, LEFT_PUB_ENDPOINT);
        } else {
                strcpy(parent_sub_endpoint, RIGHT_PUB_ENDPOINT);
        }
        char parent_pub_endpoint[MAX_LEN]; // parent <-- child
        create_endpoint(parent_pub_endpoint, child_id, client_parent_pub);

        int fork_pid = fork();
        if (fork_pid < 0) {
                fprintf(stderr, "[%d] Client error: Unable to fork a child\n", CLIENT_PID);
                exit(10);
        } else if (fork_pid == 0) {
                char child_id_str[MAX_LEN];
                char parent_id_str[MAX_LEN];
                sprintf(child_id_str, "%d", child_id);
                sprintf(parent_id_str, "%d", CLIENT_ID);
                // argv = [id, parent_id, parent_sub_end, parent_pub_end]
                execl(CLIENT_EXE, CLIENT_EXE, child_id_str, parent_id_str, parent_sub_endpoint, parent_pub_endpoint, NULL);
        } else {
                if (is_left) {
                        strcpy(LEFT_SUB_ENDPOINT, parent_pub_endpoint);
                        HAS_LEFT = true;
                } else {
                        strcpy(RIGHT_SUB_ENDPOINT, parent_pub_endpoint);
                        HAS_RIGHT = true;
                }
                connect_zmq_socket(SUBSCRIBER, parent_pub_endpoint);
        }
}

void change_subscription(const int from_id) {
        if (from_id < CLIENT_ID) {
                HAS_LEFT = false;
                disconnect_zmq_socket(SUBSCRIBER, LEFT_SUB_ENDPOINT);
                memset(LEFT_SUB_ENDPOINT, 0, MAX_LEN);
        } else {
                HAS_RIGHT = false;
                disconnect_zmq_socket(SUBSCRIBER, RIGHT_SUB_ENDPOINT);
                memset(RIGHT_SUB_ENDPOINT, 0, MAX_LEN);
        }
}


void client_loop() {
        while(3) {
                message msg;
                get_msg(SUBSCRIBER, &msg);

                // back to server msg
                if (msg.to_id == -1) {
                        //printf("[%d] Client %d: resending msg to top through \"%s\"\n", CLIENT_PID, CLIENT_ID, PARENT_PUB_ENDPOINT);
                        fflush(stdout);
                        send_msg(PARENT_PUB, &msg);
                        continue;
                }

                // redirect msg, except no id one
                if (msg.cmd != create && msg.cmd != kill_all) {
                        if (msg.to_id != CLIENT_ID) {
                                if (msg.to_id < CLIENT_ID) {
                                        send_msg(LEFT_PUB, &msg);
                                } else {
                                        send_msg(RIGHT_PUB, &msg);
                                }
                                continue;
                        }
                }

                // parsing msg
                if (msg.cmd == create) {
                        if (msg.to_id == CLIENT_ID) {
                                msg.to_id = -1;
                                msg.value = CLIENT_PID;
                        }
                        else if (msg.to_id < CLIENT_ID && HAS_LEFT) {
                                send_msg(LEFT_PUB, &msg);
                                // printf("[%d] Client %d: send to left\n", CLIENT_PID, CLIENT_ID); fflush(stdout);
                        }
                        else if (msg.to_id > CLIENT_ID && HAS_RIGHT) {
                                send_msg(RIGHT_PUB, &msg);
                                // printf("[%d] Client %d: send to right\n", CLIENT_PID, CLIENT_ID); fflush(stdout);
                        }
                        else {
                                create_child_process(msg.to_id);
                        }
                }
                else if (msg.cmd == delete) {
                        if (PARENT_ID != -1) {
                                msg.to_id = PARENT_ID;
                                msg.value = CLIENT_ID;
                                msg.cmd = change_sub;
                                // printf("[%d] Client %d: send unsub to %d\n", CLIENT_PID, CLIENT_ID, PARENT_ID); fflush(stdout);
                                send_msg(PARENT_PUB, &msg);
                        }
                        msg.cmd = kill_all;
                        send_msg(LEFT_PUB, &msg);
                        send_msg(RIGHT_PUB, &msg);
                        raise(SIGTERM);
                }
                else if (msg.cmd == kill_all) {
                        send_msg(LEFT_PUB, &msg);
                        send_msg(RIGHT_PUB, &msg);
                        raise(SIGTERM);
                }
                else if (msg.cmd == ping) {
                        // printf("[%d] Client %d: recived ping msg\n", CLIENT_PID, CLIENT_ID); fflush(stdout);
                        msg.to_id = -1;
                        msg.value = CLIENT_PID;
                        send_msg(PARENT_PUB, &msg);
                        // printf("\t[%d] Client %d: send ping msg to \"%s\"\n", CLIENT_PID, CLIENT_ID, PARENT_PUB_ENDPOINT); fflush(stdout);
                }
                else if (msg.cmd == change_sub) {
                        change_subscription(msg.value);
                }
                else if (msg.cmd == substring) {
                        int control = 0;
                        char str[MAX_LEN];
                        for (int i = 0; i < strlen(msg.str); i++) {
                                int save_pos = i;
                                for (int j = 0; j < strlen(msg.sub); j++) {
                                        if (msg.str[i] != msg.sub[j]) {
                                                break;
                                        }
                                        if (j == strlen(msg.sub) - 1) {
                                                str[control++] = save_pos + '0';
                                                str[control++] = ';';
                                                break;
                                        }
                                        i++;
                                }
                        }
                        if (control) {
                                str[control - 1] = '\0';
                        }
                        else {
                                str[control++] = '-';
                                str[control++] = '1';
                                str[control] = '\0';
                        }
                        strcpy(msg.str, str);
                        msg.to_id = -1;
                        send_msg(PARENT_PUB, &msg);
                }
                else if (msg.cmd == test) {

                        print_info();
                }
                else {

                        printf("[%d] Client error: unknown command\n", CLIENT_PID);
                        print_message(&msg);
                        continue;
                }
        }
}

// argv = [id, parent_id, parent_sub_end, parent_pub_end]
int main(int argc, char const *argv[])  {
        if (signal(SIGTERM, hdl_signal) == SIG_ERR) {
                fprintf(stderr, "[%d] ", getpid());
                perror("ERROR signal ");
                return CLIENT_SIG_ERR;
        }
        CLIENT_PID = getpid();
        CLIENT_ID = atoi(argv[1]);
        PARENT_ID = atoi(argv[2]);
        strcpy(PARENT_SUB_ENDPOINT, argv[3]);
        strcpy(PARENT_PUB_ENDPOINT, argv[4]);
        init_client();
        // printf("[%d] Starting client %d...\n", CLIENT_PID, CLIENT_ID);
        // print_info();
        client_loop();
        deinit_client();
        return 0;
}