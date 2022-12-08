#include <arpa/inet.h>
#include <dirent.h>
#include <net/if.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <zlib.h>

using namespace std;

#define ERR_EXIT(msg)                 \
    {                                 \
        fprintf(stderr, "%s\n", msg); \
        exit(1);                      \
    }

typedef struct {
    int length;
    int seqNumber;
    int ackNumber;
    int fin;
    int syn;
    int ack;
    unsigned long checksum;
} HEADER;

typedef struct {
    HEADER header;
    char data[1000];
} SEGMENT;
typedef struct NODE {
    SEGMENT seg;
    struct NODE *next;
    struct NODE *prev;
} SEGNODE;
typedef struct SEGLIST {
    int size;
    NODE *head;
    NODE *tail;
} LIST;
