#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>
#include "zmq_comm.h"

static void* CONTEXT;
static void* PUBLISHER;
static void* SUBSCRIBER;

char PUB_ENDPOINT[MAX_LEN];
char SUB_ENDPOINT[MAX_LEN];

bool FIRST_CLIENT_CONNECTED;
int FIRST_CLIENT_ID;
pid_t SERVER_PID;
int FEEDBACK_TIME;


void init_server() {
    FEEDBACK_TIME = 1000;
    CONTEXT = create_zmq_context();

    PUBLISHER = create_zmq_socket(CONTEXT, ZMQ_PUB);
    create_endpoint(PUB_ENDPOINT, 0, server_pub);
    bind_zmq_socket(PUBLISHER, SERVER_SOCKET_PUB);

    SUBSCRIBER = create_zmq_socket(CONTEXT, ZMQ_SUB);
    zmq_setsockopt(SUBSCRIBER, ZMQ_RCVTIMEO, &FEEDBACK_TIME, sizeof(FEEDBACK_TIME));
    FIRST_CLIENT_CONNECTED = false;
}

void deinit_server() {
    unbind_zmq_socket(PUBLISHER, PUB_ENDPOINT);
    close_zmq_socket(PUBLISHER);
    if (FIRST_CLIENT_CONNECTED) {
        disconnect_zmq_socket(SUBSCRIBER, SUB_ENDPOINT);
    }
    close_zmq_socket(SUBSCRIBER);
    destroy_zmq_context(CONTEXT);
}

void term_clients() {
    if (FIRST_CLIENT_CONNECTED) {
        message msg;
        msg.cmd = kill_all;
        send_msg(PUBLISHER, &msg);
    }
}

void hdl_signal(int signal) {
        printf("[%d] Terminating server...\n", SERVER_PID); fflush(stdout);
        term_clients();
        deinit_server();
        exit(EXIT_SUCCESS);
}

void send_exec_msg(const int to_id, const cmd_type type, char* str, char* sub) {
    message msg;
    strcpy(msg.str, str);
    strcpy(msg.sub, sub);
    create_msg(&msg, type, to_id, 0);
    send_msg(PUBLISHER, &msg);
}

void send_create_msg(const int to_create_id) {
    message msg;
    create_msg(&msg, create, to_create_id, 0);
    send_msg(PUBLISHER, &msg);
}

void send_ping_msg(const int to_id) {
    message msg;
    create_msg(&msg, ping, to_id, 0);
    send_msg(PUBLISHER, &msg);
}

void send_remove_msg(const int to_id) {
    message msg;
    create_msg(&msg, delete, to_id, 0);
    send_msg(PUBLISHER, &msg);
}

// returns PID on success, either 0
int ping_client(const int client_id) {
    send_ping_msg(client_id);
    message msg;
    if (!get_msg(SUBSCRIBER, &msg)) {
        return 0;
    }
    if (msg.to_id == -1 && msg.cmd == ping) {
        return msg.value;
    }
    fprintf(stderr, "[%d] SERVER ERROR: received another msg in ping_client\n", SERVER_PID); fflush(stdout);
    print_message(&msg);
    return 0;
}

int create_child_process(const int client_id) {
    if (client_id < 1) {
        fprintf(stderr, "[%d] SERVER ERROR: invalid id\n", SERVER_PID);
        return -1;
    }
    if (!FIRST_CLIENT_CONNECTED) {
        FIRST_CLIENT_ID = client_id;
        char parent_sub_endpoint[MAX_LEN]; // Подписка клиента на родителя
        char parent_pub_endpoint[MAX_LEN]; // Паблиш клиента родителю
        create_endpoint(parent_sub_endpoint, 0, server_pub);
        create_endpoint(parent_pub_endpoint, client_id, client_parent_pub);
        int fork_pid = fork();
        if (fork_pid < 0) {
            fprintf(stderr, "[%d] SERVER ERROR: Unable to fork a child\n", SERVER_PID);
            exit(10);
        } else if (fork_pid == 0) {
            char client_id_str[MAX_LEN];
            sprintf(client_id_str, "%d", client_id);
            // argv = [id, parent_id, parent_sub_end, parent_pub_end]
            execl(CLIENT_EXE, CLIENT_EXE, client_id_str, "-1", parent_sub_endpoint, parent_pub_endpoint, NULL);
        } else {
            strcpy(SUB_ENDPOINT, parent_pub_endpoint);
            connect_zmq_socket(SUBSCRIBER, SUB_ENDPOINT);
            FIRST_CLIENT_CONNECTED = true;
            printf("[%d] OK: %d\n", SERVER_PID, fork_pid);
        }
    } else {
        int client_pid = ping_client(client_id);
        if (client_pid) {
            fprintf(stderr, "[%d] Error: already exist with pid = \"%d\"\n", SERVER_PID, client_pid);
            return -1;
        } else {
            send_create_msg(client_id);
            ping_client(client_id);
            int client_pid = ping_client(client_id);
            if (!client_pid) {
                fprintf(stderr, "[%d] Error: client wasn't created\n", SERVER_PID);
                return -1;
            } else {
                printf("[%d] OK: %d\n", SERVER_PID, client_pid);
            }
        }
    }
    return 0;
}

void exec_command(const int client_id, const cmd_type cmd, char* str, char* sub) {
    if (!ping_client(client_id)) {
        fprintf(stderr, "[%d] Error: not found\n", SERVER_PID);
        return;
    }
    send_exec_msg(client_id, cmd, str, sub);
    message msg;
    if (!get_msg(SUBSCRIBER, &msg)) {
        fprintf(stderr, "[%d] Error: client haven't responed\n", SERVER_PID);
        return;
    }
    if (msg.to_id == -1 && msg.cmd == cmd) {
        if (cmd == substring) {
            printf("[%d] OK:%d:%s\n", SERVER_PID, client_id, msg.str);
        }
        return;
    }

    fprintf(stderr, "[%d] SERVER ERROR: received another msg in exec_command\n", SERVER_PID); fflush(stdout);
    print_message(&msg);
    return;
}

void remove_client(const int client_id) {
    int ping_res = ping_client(client_id);
    if (!ping_res) {
        fprintf(stderr, "[%d] Error: not found\n", SERVER_PID);
        return;
    }
    send_remove_msg(client_id);
    if (FIRST_CLIENT_ID == client_id) {
        disconnect_zmq_socket(SUBSCRIBER, SUB_ENDPOINT);
                memset(SUB_ENDPOINT, 0, MAX_LEN);
        FIRST_CLIENT_CONNECTED = false;
    }
    if (!ping_client(client_id)) {
        printf("[%d] OK\n", SERVER_PID);
    } else {
        fprintf(stderr, "[%d] Error: client wasn't removed\n", SERVER_PID);
    }
}

void skip_line() {
    char cc = getchar();
    while (cc != '\n' && cc != EOF) { cc = getchar(); }
}

/*
 FUNCTIONS :
    + create id
    + remove id
    + exec id
      str
      substr
    + pingall
*/

void input_loop() {
    char cmd[MAX_LEN];
    int ids[MAX_LEN];
    int id;
    int control = 0;
    while (3) {
        int scanf_res = scanf("%s", cmd);
        if (scanf_res == EOF) {
            printf("[%d] Shutting down server...\n", SERVER_PID);
            break;
        }
        else if (scanf_res == 0) {
            printf("[%d] Invalid command\n", SERVER_PID);
            skip_line();
            continue;
        }
        if (strcmp(cmd, "pingall")) {
                scanf_res = scanf("%d", &id);
                if (scanf_res == EOF) {
                        printf("[%d] Shutting down server...\n", SERVER_PID);
                        break;
                }
                else if (scanf_res == 0) {
                        printf("[%d] Invalid argument\n", SERVER_PID);
                        skip_line();
                        continue;
                }
        }
        if (!strcmp(cmd, "create")) {
            if (create_child_process(id) != -1) {
                ids[control++] = id;
            }
        }
        else if (!strcmp(cmd, "remove")) {
            remove_client(id);
        }
        else if (!strcmp(cmd, "exec")) {
            char str[MAX_LEN];
            char sub[MAX_LEN];
            int scanf_res = scanf("%s", str);
            int another_res = scanf("%s", sub);
            if (scanf_res == EOF || another_res == EOF) {
                printf("[%d] Shutting down server.\n", SERVER_PID);
                break;
            } else if (scanf_res == 0 || another_res == 0) {
                printf("[%d] Invalid exec command\n", SERVER_PID);
                skip_line();
                continue;
            }
            cmd_type type = substring;
            exec_command(id, type, str, sub);
        }
        else if (!strcmp(cmd, "pingall")) {
            bool flag = true;
            printf("[%d] OK: ", SERVER_PID);
            for (int i = 0; i < control; i++) {
                if (!ping_client(ids[i])) {
                        printf("%d;", ids[i]);
                        flag = false;
                }
            }
            if (flag) {
                printf("-1");
            }
            printf("\n");
        }
        else if (!strcmp(cmd, "test")) {
            if (id == 0) {
                printf("[%d] I'm server\n"
                        "\t publisher = \"%s\"\n"
                        "\t subscriber = \"%s\"\n",
                SERVER_PID, PUB_ENDPOINT, SUB_ENDPOINT);
                            fflush(stdout);
            }
            else {
                message msg;
                create_msg(&msg, test, id, 0);
                send_msg(PUBLISHER, &msg);
            }
        }
        else {
            printf("[%d] Invalid command\n", SERVER_PID);
            skip_line();
            continue;
        }
    }
}

int main (int argc, char const *argv[])  {
    if (signal(SIGINT, hdl_signal) == SIG_ERR) {
        fprintf(stderr, "[%d] SERVER", getpid());
        perror("ERROR signal ");
        return SERVER_SIG_ERR;
    }
    if (signal(SIGSEGV, hdl_signal) == SIG_ERR) {
        fprintf(stderr, "[%d] SERVER", getpid());
        perror("ERROR signal ");
        return SERVER_SIG_ERR;
    }
    SERVER_PID = getpid();
    printf("[%d] Starting server...\n", SERVER_PID);
    init_server();

    input_loop();
    term_clients();
    deinit_server();
    return 0;
}