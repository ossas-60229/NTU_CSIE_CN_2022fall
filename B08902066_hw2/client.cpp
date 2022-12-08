#include <arpa/inet.h>
#include <dirent.h>
#include <fcntl.h>
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define EMPTY(buf) bzero((buf), sizeof(buf))

#define BUFF_SIZE 1024
#define PORT 8787
#define LS 0
#define PUT 1
#define GET 2
#define PLAY 3
#define BAN 4
#define UNBAN 5
#define BLOCKLIST 6
#define ERR_EXIT(a) \
    {               \
        perror(a);  \
        exit(1);    \
    }
char buffer[2 * BUFF_SIZE] = {};
char user_name[BUFF_SIZE] = {};
const char *cli_path = "./client_dir";
const char *ban_list = "./BLOCKLIST";
void play_vid(char *name);
void blocklist();
int fsend(char *name, int connfd);
int frecv(char *name, int connfd);
int ban(int connfd);
int unban(int connfd);
int decode(char *input, char *name);
static void init_client(int *connfd, char *name, char *buf);
static int handle_request(int connfd);

int main(int argc, char *argv[]) {
    if (argc != 3) {
        ERR_EXIT("wrong instruction");
    }
    strcpy(user_name, argv[1]);

    int sockfd, read_byte;
    struct sockaddr_in addr;
    // printf("fuck you\n");
    init_client(&sockfd, argv[1], argv[2]);
    // Receive message from server
    if ((read_byte = read(sockfd, buffer, sizeof(buffer) - 1)) < 0) {
        ERR_EXIT("receive failed\n");
    }
    printf("Received %d bytes from the server\n", read_byte);
    printf("%s\n", buffer);
    while (1) {
        if (handle_request(sockfd) < 0) {
            // printf("wrong request\n");
        }
        EMPTY(buffer);
        sleep(1);
    }
    close(sockfd);
    return 0;
}

static void init_client(int *connfd, char *name, char *buf) {
    // Get socket file descriptor
    struct sockaddr_in addr;
    if ((*connfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        ERR_EXIT("socket failed\n");
    }
    char ip_addr[10], port[10];
    char *token = strtok(buf, ":");
    strcpy(ip_addr, token);
    token = strtok(NULL, ":");
    if (token == NULL) {
        printf("command error\n");
        exit(1);
    }
    strcpy(port, token);
    printf("ip:%s, port:%s", ip_addr, port);
    // Set server address
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    // addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_addr.s_addr = inet_addr(ip_addr);
    addr.sin_port = htons(atoi(port));

    // Connect to the server
    if (connect(*connfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ERR_EXIT("connect failed\n");
    }
    if (opendir(cli_path) == NULL) mkdir(cli_path, 0755);
    chdir(cli_path);
    char temp[50];
    sprintf(temp, "./%s", name);
    if (opendir(temp) == NULL) mkdir(temp, 0755);
    chdir(temp);

    EMPTY(buffer);

    sprintf(buffer, "%s", name);
    int write_byte;
    if ((write_byte = send(*connfd, buffer, strlen(buffer), 0)) < 0) {
        ERR_EXIT("write failed\n");
    }

    return;
}
static int handle_request(int connfd) {
    // printf("new request...\n");
    printf("$");
    EMPTY(buffer);
    fgets(buffer, sizeof(buffer), stdin);
    // printf("get %s from stdin\n", buffer);
    if (buffer[0] == '\n') {
        printf("\n");
        return 0;
    }

    char name[BUFF_SIZE];
    EMPTY(name);
    int inst = decode(buffer, name);
    // printf("inst: %d, name: %s\n", inst, name);

    if (inst < 0) {
        return -1;
    }
    if (inst == BAN || inst == UNBAN || inst == BLOCKLIST) {
        if (strcmp(user_name, "admin") != 0) {
            printf("Command not found.\n");
            return -1;
        }
    }
    EMPTY(buffer);
    sprintf(buffer, "%d %s", inst, name);
    // send the instruction and file name

    int write_byte, s_bytes, r_bytes;
    // printf("send %s\n", buffer);
    if ((write_byte = send(connfd, buffer, strlen(buffer), 0)) < 0) {
        ERR_EXIT("write failed\n");
    }
    EMPTY(buffer);
    // tell permission
    if (recv(connfd, buffer, sizeof(buffer), 0) < 0) {
        printf("receiving error");
        return -1;
    }
    if (strcmp(buffer, "ban") == 0) {
        printf("Permission denied.\n");
        return 0;
    }
    if (inst == LS) {
        EMPTY(buffer);

        if (recv(connfd, buffer, sizeof(buffer), 0) < 0) {
            printf("receiving error");
            return -1;
        }
        printf("%s", buffer);
        // tell server I'm ready
        sprintf(buffer, "ready");
        if ((s_bytes = send(connfd, buffer, strlen(buffer), 0)) < 0) {
            return -1;
        }
        EMPTY(buffer);
    }
    // BLOCKLIST AND LS
    if (inst == PUT) {
        EMPTY(buffer);
        sprintf(buffer, "./%s", name);
        if (fsend(buffer, connfd) < 0) {
            printf("%s doesn't exist.\n", name);
            EMPTY(buffer);
            if (recv(connfd, buffer, sizeof(buffer), 0) < 0) {
                printf("receiving error");
                return -1;
            }
        } else {
            printf("putting %s...\n", name);
        }
        EMPTY(buffer);
    }
    // PUT
    if (inst == GET) {
        EMPTY(buffer);
        sprintf(buffer, "./%s", name);
        if (frecv(buffer, connfd) < 0) {
            printf("%s doesn't exist.\n", name);
            EMPTY(buffer);
            if (recv(connfd, buffer, sizeof(buffer), 0) < 0) {
                printf("receiving error");
                return -1;
            }
        } else {
            printf("getting %s...\n", name);
        }
        EMPTY(buffer);
        // sprintf(buffer, "ready");
        //  if ((write_byte = send(connfd, buffer, strlen(buffer), 0)) < 0) {
        //  return -1;
        // }
        // EMPTY(buffer);
    }
    // GET
    if (inst == PLAY) {
        EMPTY(buffer);
        sprintf(buffer, "./%s", name);
        int ln = strlen(name);
        if (ln >= 5) {
            if (strcmp(&name[ln - 4], ".mpg") != 0) {
                printf("This is not a .mpg file\n");
            }
            if (frecv(buffer, connfd) < 0) {
                printf("%s doesn't exist\n", name);
                EMPTY(buffer);
                if (recv(connfd, buffer, sizeof(buffer), 0) < 0) {
                    printf("receiving error");
                    return -1;
                }
            } else {
                EMPTY(buffer);
                sprintf(buffer, "./%s", name);
                printf("playing the video...\n");

                play_vid(buffer);
                remove(name);
            }
            EMPTY(buffer);
        } else {
            printf("This is not a .mpg file\n");
        }
        EMPTY(buffer);
    }
    // PLAY
    if (inst == BAN) {
        // wait for out
        EMPTY(buffer);
        ban(connfd);

        // if ((r_bytes = recv(connfd, buffer, sizeof(buffer), 0)) < 0) {
        // return -1;
        //}
    }
    // BAN
    if (inst == UNBAN) {
        // wait for out
        EMPTY(buffer);
        unban(connfd);
    }
    // BAN
    if (inst == BLOCKLIST) {
        EMPTY(buffer);
        sprintf(buffer, "%s", ban_list);
        frecv(buffer, connfd);
        blocklist();
        remove(ban_list);

        EMPTY(buffer);
    }
    // BLOCKLIST
    // printf("request finished\n");

    // sprintf(buffer, "out");
    // if ((s_bytes = send(connfd, buffer, strlen(buffer), 0)) < 0) {
    // return -1;
    //}

    return 0;
}

int decode(char *input, char *name) {
    input = strtok(input, " \n");
    char *nm = strtok(NULL, " \n");
    if (nm != NULL) strcpy(name, nm);
    // printf("the input is %s %d\n", input, strlen(input));

    if (strcmp(input, "ban") == 0) {
        if (nm == NULL) {
            printf("command not found\n");
            return -1;
        }
        EMPTY(name);
        while (nm != NULL) {
            strcat(name, nm);
            strcat(name, " ");
            nm = strtok(NULL, " \n");
        }
        strcat(name, "\0");
        // printf("%s are banned\n", name);

        return BAN;
    }
    if (strcmp(input, "unban") == 0) {
        if (nm == NULL) {
            printf("command not found\n");
            return -1;
        }
        EMPTY(name);
        while (nm != NULL) {
            strcat(name, nm);
            strcat(name, " ");
            nm = strtok(NULL, " \n");
        }
        strcat(name, "\0");
        return UNBAN;
    }

    if (strtok(NULL, " \n") != NULL) {
        printf("command not found\n");
        return -1;
    }

    if (strcmp(input, "ls") == 0) {
        if (nm != NULL) {
            printf("command not found\n");
            return -1;
        }
        return LS;
    }
    if (strcmp(input, "blocklist") == 0) {
        if (nm != NULL) {
            printf("command not found\n");
            return -1;
        }
        return BLOCKLIST;
    }
    if (strcmp(input, "put") == 0) {
        if (nm == NULL) {
            printf("command not found\n");
            return -1;
        }
        return PUT;
    }
    if (strcmp(input, "get") == 0) {
        if (nm == NULL) {
            printf("command not found\n");
            return -1;
        }
        return GET;
    }
    if (strcmp(input, "play") == 0) {
        if (nm == NULL) {
            printf("command not found\n");
            return -1;
        }
        return PLAY;
    }
    printf("Command not found.\n");

    return -1;
}
int fsend(char *name, int connfd) {
    FILE *fp = fopen(name, "r");
    struct stat tmp;
    stat(name, &tmp);
    int size = tmp.st_size;
    int64_t r_bytes = 0, s_bytes = 0, total = 0;
    if (fp == NULL) {
        size = -1;
    }
    EMPTY(buffer);
    sprintf(buffer, "%d", size);
    if ((s_bytes = send(connfd, buffer, sizeof(buffer), 0)) < 0) {
        return -1;
    }
    printf("send %s to server\n", buffer);
    EMPTY(buffer);
    if (fp == NULL) {
        return -1;
    }
    while (total < size) {
        // wait for start
        if ((r_bytes = recv(connfd, buffer, sizeof(buffer), 0)) < 0) {
            return -1;
        }
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
    // printf("send %d bytes to fd %d\n", size, connfd);
    EMPTY(buffer);
    return 0;
}
// send files
int frecv(char *name, int connfd) {
    char temp[500];
    strcpy(temp, name);
    EMPTY(buffer);
    int64_t r_bytes = 0, s_bytes = 0, total = 0, size = 0;
    if ((r_bytes = recv(connfd, buffer, sizeof(buffer), 0)) < 0) {
        return -1;
    }
    size = atoi(buffer);
    if (size < 0) {
        return -1;
    }
    //    printf("the damn size is %d\n", size);
    // ready for recv
    FILE *fp = fopen(temp, "w");
    while (total < size) {
        sprintf(buffer, "ready");
        if ((s_bytes = send(connfd, buffer, sizeof(buffer), 0)) < 0) {
            return -1;
        }
        EMPTY(buffer);
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
    // printf("receive %d bytes from fd %d\n", size, connfd);

    EMPTY(buffer);
    return 0;
}
// receive files
void play_vid(char *name) {
    int stat, pid = fork();
    if (pid == 0) {
        execlp("../../openCV", "../../openCV", name, NULL);
    } else {
        wait(&stat);
    }
    return;
}
// play the cideo
void blocklist() {
    FILE *fp = fopen(ban_list, "r");
    while (fscanf(fp, "%s", buffer) != EOF) {
        printf("%s\n", buffer);
    }
    EMPTY(buffer);
    return;
}
// blocklist
int ban(int connfd) {
    int r_bytes, s_bytes;
    // wait for start
    while (strcmp(buffer, "end") != 0) {
        // ready for recv
        EMPTY(buffer);
        if ((r_bytes = recv(connfd, buffer, sizeof(buffer), 0)) < 0) {
            return -1;
        }
        if (strcmp(buffer, "end") != 0) {
            printf("%s", buffer);
            printf("\n");
        } else {
            break;
        }
        if ((s_bytes = send(connfd, buffer, sizeof(buffer), 0)) < 0) {
            return -1;
        }
    }
    EMPTY(buffer);
    return 0;
}
int unban(int connfd) {
    int r_bytes, s_bytes;
    // wait for start
    while (strcmp(buffer, "end") != 0) {
        sprintf(buffer, "fuck");
        // ready for recv
        EMPTY(buffer);
        if ((r_bytes = recv(connfd, buffer, sizeof(buffer), 0)) < 0) {
            return -1;
        }
        if (strcmp(buffer, "end") != 0) {
            printf("%s", buffer);
            printf("\n");
        } else {
            break;
        }
        if ((s_bytes = send(connfd, buffer, sizeof(buffer), 0)) < 0) {
            return -1;
        }
    }
    EMPTY(buffer);
    return 0;
}