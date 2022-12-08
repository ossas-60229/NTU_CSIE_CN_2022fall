#include <arpa/inet.h>
#include <dirent.h>
#include <fcntl.h>
#include <net/if.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#define EMPTY(buf) bzero((buf), sizeof(buf))
const char *svr_path = "./server_dir";
const char *admin_path = "./admin";
const char *ban_list = "./admin/ban_list.txt";
const char *ban_list_tmp = "./admin/.ban_list.txt";
const char *already_banned = "is already on the blocklist!";
typedef struct {
    char host[512];  // client's host
    int conn_fd;     // fd to talk with client
    char buf[512];   // data sent by/to client
    size_t buf_len;  // bytes used by buf
    // you don't need to change this.
    char name[512];

} request;
#define LS 0
#define PUT 1
#define GET 2
#define PLAY 3
#define BAN 4
#define UNBAN 5
#define BLOCKLIST 6
#define BUFF_SIZE 1024
#define ERR_EXIT(a) \
    {               \
        perror(a);  \
        exit(1);    \
    }
int maxfd = 1024;
int temp_fd;
char fuck[50];
request request_all[1025];
int checkadmin(int connfd);
void ban(char *name);
void unban(char *name);
void list(char *name);
void blocklist(int connfd);
int fsend(char *name, int connfd);
int frecv(char *name, int connfd);
int banned(char *name);  // if banned return 1 if not return 0

static void sigpipe_deal(int signum);
static void init_server(int *server_sockfd, char *buf);
static int handle_request(int newfd);
char buffer[2 * BUFF_SIZE] = {};

fd_set readfds, master;
int fd;
int main(int argc, char *argv[]) {
    if (argc != 2) {
        ERR_EXIT("wrong instruction");
    }
    int server_sockfd, client_sockfd, write_byte;
    struct sockaddr_in server_addr, client_addr;
    int client_addr_len = sizeof(client_addr);
    char message[BUFF_SIZE] = "Hello World from Server";
    // initialize the sever
    // printf("fuck you\n");

    init_server(&server_sockfd, argv[1]);
    signal(SIGPIPE, sigpipe_deal);

    FD_ZERO(&master);
    FD_SET(server_sockfd, &master);

    // Accept the client and get client file descriptor
    while (1) {
        readfds = master;
        if (select(maxfd, &readfds, NULL, NULL, NULL) < 0) {
            ERR_EXIT("select failed");
        }
        for (fd = 0; fd < maxfd; fd++) {
            if (FD_ISSET(fd, &readfds)) {
                if (fd == server_sockfd) {  // new connection
                    if ((client_sockfd = accept(
                             server_sockfd, (struct sockaddr *)&client_addr,
                             (socklen_t *)&client_addr_len)) < 0) {
                        ERR_EXIT("accept failed\n");
                    }
                    request_all[client_sockfd].conn_fd = client_sockfd;
                    request_all[client_sockfd].name[0] = '\0';
                    sprintf(buffer, "%s\n", message);
                    if ((write_byte = send(client_sockfd, buffer,
                                           strlen(buffer), 0)) < 0) {
                        ERR_EXIT("write failed\n");
                    }

                    FD_SET(client_sockfd, &master);
                } else {
                    if (handle_request(fd) < 0) {
                        FD_CLR(fd, &master);
                        close(fd);
                    }
                }
            }
        }
    }
    close(server_sockfd);
    return 0;
}

void list(char *name) {
    EMPTY(buffer);
    DIR *dir = opendir(name);
    struct dirent *now;
    int count = 0;
    while ((now = readdir(dir)) != NULL) {
        if (strcmp(now->d_name, ".") == 0) continue;
        if (strcmp(now->d_name, "..") == 0) continue;
        strcat(buffer, now->d_name);
        strcat(buffer, "\n");
        count++;
    }
    if (count == 0) strcpy(buffer, "\n");

    printf("list %s with %d\n", name, count);
    printf("the buffer now is %s\n", buffer);

    return;
}
static void init_server(int *server_sockfd, char *buf) {
    int PORT = atoi(buf);
    struct sockaddr_in server_addr, client_addr;
    // Get socket file descriptor
    if ((*server_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        ERR_EXIT("socket failed\n")
    }

    // Set server address information
    bzero(&server_addr, sizeof(server_addr));  // erase the data
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);

    // Bind the server file descriptor to the server address
    if (bind(*server_sockfd, (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0) {
        ERR_EXIT("bind failed\n");
    }

    // Listen on the server file descriptor
    if (listen(*server_sockfd, 1024) < 0) {
        ERR_EXIT("listen failed\n");
    }

    if (opendir(svr_path) == NULL) mkdir(svr_path, 0755);
    chdir(svr_path);
    if (opendir(admin_path) == NULL) mkdir(admin_path, 0755);
    printf("admin created\n");
    if (open(ban_list, O_RDWR) < 0) {
        creat(ban_list, 0755);
    }
    return;
}
static int handle_request(int connfd) {
    printf("\nnew request...\n");
    if (strlen(request_all[connfd].name) <= 0) {
        int read_byte;
        printf("buffer is : %s\n", buffer);
        EMPTY(buffer);
        if ((read_byte = read(connfd, buffer, sizeof(buffer) - 1)) < 0) {
            ERR_EXIT("receive failed\n");
        }
        // if (banned(buffer)) {
        // printf(" this guy is banned\n");
        // return -1;
        //}
        printf(
            "Accept a new connection on socket [%d]. Login as "
            "<%s>.",
            connfd, buffer);
        char temp[50];
        sprintf(temp, "./%s", buffer);
        // printf("new path is %s\n", temp);
        if (opendir(temp) == NULL) {
            printf("%s not exist so creat\n", temp);
            mkdir(temp, 0755);
        }
        strcpy(request_all[connfd].name, temp);
    } else {
        EMPTY(buffer);
        int read_byte;
        if ((read_byte = read(connfd, buffer, sizeof(buffer) - 1)) < 0) {
            return -1;
        }
        printf("read %s\n", buffer);

        int inst, write_byte, r_bytes;
        // tell the instruction
        char name[BUFF_SIZE], *token;
        int len_tmp = strlen(buffer);
        for (int i = 0; i < len_tmp; i++) {
            if (buffer[i] == ' ') {
                buffer[i] = '\0';
                strcpy(name, &buffer[i + 1]);
                inst = atoi(buffer);
                break;
            }
        }
        // sscanf(buffer, "%d %s", &inst, name);
        printf("inst = %d name = %s\n", inst, name);
        EMPTY(buffer);
        if (banned(&request_all[connfd].name[2])) {
            sprintf(buffer, "ban");
            if ((write_byte = send(connfd, buffer, strlen(buffer), 0)) < 0) {
                return -1;
            }
            EMPTY(buffer);
            return 0;
        } else {
            sprintf(buffer, "ok");
            if ((write_byte = send(connfd, buffer, strlen(buffer), 0)) < 0) {
                return -1;
            }
            EMPTY(buffer);
        }
        EMPTY(buffer);
        // tell the permission

        switch (inst) {
            case LS:
                EMPTY(buffer);
                list(request_all[connfd].name);
                if ((write_byte = send(connfd, buffer, strlen(buffer), 0)) <
                    0) {
                    return -1;
                }
                EMPTY(buffer);
                // wait for ready
                if ((r_bytes = recv(connfd, name, sizeof(name), 0)) < 0) {
                    return -1;
                }
                EMPTY(buffer);
                break;
            case PUT:
                EMPTY(buffer);
                sprintf(buffer, "%s/%s", request_all[connfd].name, name);
                if (frecv(buffer, connfd) < 0) {
                    sleep(1);
                    sprintf(buffer, "ready");
                    if ((write_byte = send(connfd, buffer, strlen(buffer), 0)) <
                        0) {
                        return -1;
                    }
                }
                EMPTY(buffer);
                break;
            case GET:
                EMPTY(buffer);
                sprintf(buffer, "%s/%s", request_all[connfd].name, name);
                if (fsend(buffer, connfd) < 0) {
                    EMPTY(buffer);
                    // if (recv(connfd, buffer, sizeof(buffer), 0) < 0) {
                    // printf("receiving error");
                    // return -1;
                    //}
                    // EMPTY(buffer);
                    sprintf(buffer, "ready");
                    sleep(1);

                    if ((write_byte = send(connfd, buffer, strlen(buffer), 0)) <
                        0) {
                        return -1;
                    }
                }
                EMPTY(buffer);
                break;
            case PLAY:
                EMPTY(buffer);
                sprintf(buffer, "%s/%s", request_all[connfd].name, name);
                // fsend(buffer, connfd);
                if (fsend(buffer, connfd) < 0) {
                    EMPTY(buffer);
                    // if (recv(connfd, buffer, sizeof(buffer), 0) < 0) {
                    // printf("receiving error");
                    // return -1;
                    //}
                    // EMPTY(buffer);
                    sprintf(buffer, "ready");
                    sleep(1);

                    if ((write_byte = send(connfd, buffer, strlen(buffer), 0)) <
                        0) {
                        return -1;
                    }
                }
                EMPTY(buffer);
                break;
            case BAN:
                EMPTY(buffer);
                token = strtok(name, " \n");
                // ban(name);
                // if ((write_byte = send(connfd, buffer, strlen(buffer),
                // 0)) < 0) { return -1;
                //}
                while (token != NULL) {
                    EMPTY(buffer);
                    ban(token);
                    token = strtok(NULL, " \n");
                    if ((write_byte = send(connfd, buffer, strlen(buffer), 0)) <
                        0) {
                        return -1;
                    }
                    // wait for ready
                    if ((r_bytes = recv(connfd, buffer, sizeof(buffer), 0)) <
                        0) {
                        return -1;
                    }
                    EMPTY(buffer);
                }
                sprintf(buffer, "end");
                if ((write_byte = send(connfd, buffer, strlen(buffer), 0)) <
                    0) {
                    return -1;
                }
                EMPTY(buffer);
                break;
            case UNBAN:
                EMPTY(buffer);
                token = strtok(name, " \n");
                while (token != NULL) {
                    EMPTY(buffer);
                    unban(token);
                    token = strtok(NULL, " \n");
                    if ((write_byte = send(connfd, buffer, strlen(buffer), 0)) <
                        0) {
                        return -1;
                    }
                    // wait for ready
                    if ((r_bytes = recv(connfd, buffer, sizeof(buffer), 0)) <
                        0) {
                        return -1;
                    }
                    EMPTY(buffer);
                }
                sprintf(buffer, "end");
                if ((write_byte = send(connfd, buffer, strlen(buffer), 0)) <
                    0) {
                    return -1;
                }
                EMPTY(buffer);
                break;
            case BLOCKLIST:
                EMPTY(buffer);
                sprintf(buffer, "%s", ban_list);
                fsend(buffer, connfd);
                break;
            default:
                break;
        }

        // wait for out
        // if ((r_bytes = recv(connfd, fuck, sizeof(fuck), 0)) < 0) {
        // return -1;
        //}
    }
    return 0;
}
int fsend(char *name, int connfd) {
    FILE *fp = fopen(name, "r");
    printf("file name is %s\n", name);
    struct stat tmp;
    stat(name, &tmp);
    int size = tmp.st_size;
    int r_bytes = 0, s_bytes = 0, total = 0;
    if (fp == NULL) size = -1;
    EMPTY(buffer);
    sprintf(buffer, "%d", size);
    if ((s_bytes = send(connfd, buffer, sizeof(buffer), 0)) < 0) {
        return -1;
    }
    if (fp == NULL) return -1;
    printf("send %s to client\n", name);

    while (total < size) {
        // wait for start
        if ((r_bytes = recv(connfd, buffer, sizeof(buffer), 0)) < 0) {
            return -1;
        }
        if (strcmp(buffer, "error") == 0) return -1;

        EMPTY(buffer);

        // start !!
        if ((r_bytes = fread(buffer, sizeof(char), sizeof(buffer), fp)) < 0) {
            return -1;
        }
        if ((s_bytes = send(connfd, buffer, r_bytes, 0)) < 0) {
            return -1;
        }
        total += s_bytes;
    }
    fclose(fp);
    printf("send %d bytes to fd %d\n", size, connfd);
    EMPTY(buffer);
    return 0;
}
// send files
int frecv(char *name, int connfd) {
    char temp[500];
    strcpy(temp, name);
    EMPTY(buffer);
    int r_bytes = 0, s_bytes = 0, total = 0, size = 0;
    if ((r_bytes = recv(connfd, buffer, sizeof(buffer), 0)) < 0) {
        return -1;
    }
    size = atoi(buffer);
    if (size < 0) {
        printf("not exist \n");
        return -1;
    }
    FILE *fp = fopen(temp, "w");

    while (total < size) {
        sprintf(buffer, "ready");
        if ((s_bytes = send(connfd, buffer, sizeof(buffer), 0)) < 0) {
            return -1;
        }
        EMPTY(buffer);
        // ready for recv
        if ((r_bytes = recv(connfd, buffer, sizeof(buffer), 0)) < 0) {
            printf("recv error\n");
            return -1;
        }
        if ((s_bytes = fwrite(buffer, sizeof(char), r_bytes, fp)) < 0) {
            printf("write error\n");
            return -1;
        }
        total += s_bytes;
    }
    fclose(fp);
    printf("receive %d bytes from fd %d\n", size, connfd);

    EMPTY(buffer);
    return 0;
}
// receive files
int checkadmin(int connfd) {
    if (strcmp(request_all[connfd].name, admin_path) == 0) return 1;
    return 0;
}
// check if one is admin user
void ban(char *name) {
    if (strcmp(name, "admin") == 0) {
        sprintf(buffer, "You cannot ban yourself!");
        return;
    }
    FILE *fp1 = fopen(ban_list, "r");
    while (fscanf(fp1, "%s", buffer) != EOF) {
        if (strcmp(buffer, name) == 0) {
            sprintf(buffer, "User %s %s", name, already_banned);
            printf("%s\n", buffer);
            return;
        }
    }
    fclose(fp1);
    FILE *fp = fopen(ban_list, "a");
    fprintf(fp, "%s\n", name);
    sprintf(buffer, "Ban %s successfully!", name);
    printf("%s\n", buffer);
    fclose(fp);
    return;
}
void unban(char *name) {
    EMPTY(buffer);
    printf("%s is unbanned\n", name);
    int count = 0;

    FILE *fp = fopen(ban_list, "r"), *fp2 = fopen(ban_list_tmp, "w");
    while (fscanf(fp, "%s", buffer) != EOF) {
        printf("%s scanned\n", buffer);
        if (strcmp(buffer, name) == 0) {
            count = 1;
            continue;
        }
        fprintf(fp2, "%s\n", buffer);
    }
    fclose(fp);
    fclose(fp2);
    fp = fopen(ban_list, "w");
    fclose(fp);
    fp = fopen(ban_list, "a");

    fp2 = fopen(ban_list_tmp, "r");
    while (fscanf(fp2, "%s", buffer) != EOF) {
        printf("%s scanned\n", buffer);
        fprintf(fp, "%s\n", buffer);
    }
    fclose(fp);
    fclose(fp2);
    EMPTY(buffer);
    if (count == 0) {
        sprintf(buffer, "User %s is not on the blocklist!", name);
    } else {
        sprintf(buffer, "Successfully removed %s from the blocklist!", name);
    }

    return;
}
void blocklist(int connfd) {}
static void sigpipe_deal(int signum) {
    FD_CLR(fd, &master);
    close(fd);
    return;
}
// deal with sigpipe
int banned(char *name) {
    FILE *fp = fopen(ban_list, "r");
    while (fscanf(fp, "%s", buffer) != EOF) {
        if (strcmp(name, buffer) == 0) return 1;
    }
    return 0;
}