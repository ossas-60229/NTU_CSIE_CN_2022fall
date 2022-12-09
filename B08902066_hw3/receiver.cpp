#include "GBN.hpp"
#include "opencv2/opencv.hpp"
using namespace cv;
#define buff_size 256
#define SEG_SIZE 1000
SEGMENT buffer_pkt[buff_size];
static int frame_number = 0, frame_collect = 0;
const char *player_exec = "./openCV";
const char *fifo_name = "video.fifo";
FILE *fp;
pid_t pid = -1;

void init_player(int width, int height) {
    fprintf(stderr, "start init\n");
    char w[50], h[50];
    sprintf(w, "%d", width);
    sprintf(h, "%d", height);
    mkfifo(fifo_name, 0777);
    fp = fopen(fifo_name, "w");
    pid = fork();
    if (pid == 0) {
        execlp(player_exec, player_exec, fifo_name, w, h, NULL);
    }
    fprintf(stderr, "start init\n");
    return;
}
void flush_vid(int index) {
    for (int i = 0; i < index; i++) {
        fwrite(buffer_pkt[i].data, sizeof(char), buffer_pkt[i].header.length,
               fp);
        fflush(fp);
    }
    return;
}

void initSEG(SEGMENT &a);
void setIP(char *dst, const char *src);
void corruptData(char *data, int len);
unsigned long get_checksum(char *str);
int min(int a, int b) { return (a < b) ? a : b; }
int main(int argc, char *argv[]) {
    if (argc != 3)
        ERR_EXIT("usage: ./receiver <receiver port> <agent IP>:<agent port>");
    int recvsocket, portNum, nBytes;
    uint addr_len = sizeof(struct sockaddr_in);
    SEGMENT s_tmp;
    struct sockaddr_in sender, agent, receiver, tmp_addr;
    socklen_t sender_size, recv_size, tmp_size, agent_size;
    char sendIP[50], agentIP[50], recvIP[50], tmpIP[50];
    int sendPort, agentPort, recvPort;
    // declare
    recvPort = atoi(argv[1]);
    setIP(recvIP, "local");
    // sender
    sscanf(argv[2], "%[^:]:%d", tmpIP, &agentPort);  // sender
    setIP(agentIP, tmpIP);
    // agent
    /* Create UDP socket */
    recvsocket = socket(PF_INET, SOCK_DGRAM, 0);

    /* Configure settings in agent struct */
    agent.sin_family = AF_INET;
    agent.sin_port = htons(agentPort);
    agent.sin_addr.s_addr = inet_addr(agentIP);
    memset(agent.sin_zero, '\0', sizeof(agent.sin_zero));

    /* Configure settings in receiver struct */
    receiver.sin_family = AF_INET;
    receiver.sin_port = htons(recvPort);
    receiver.sin_addr.s_addr = inet_addr(recvIP);
    memset(receiver.sin_zero, '\0', sizeof(receiver.sin_zero));

    /* bind socket */
    bind(recvsocket, (struct sockaddr *)&receiver, sizeof(receiver));

    /* Initialize size variable to be used later on */
    agent_size = sizeof(agent);
    recv_size = sizeof(receiver);
    tmp_size = sizeof(tmp_addr);

    printf("Start!! ^Q^\n");
    printf("receiver info: ip = %s port = %d\n", recvIP, recvPort);
    printf("agent info: ip = %s port = %d\n", agentIP, agentPort);
    // start to recv
    int ack_sure = -1;
    SEGMENT now_seg;
    // first receive resolution
    VideoCapture cap;
    int width = 0, height = 0, index = 0;  // index of buffer
    while (1) {
        if (recvfrom(recvsocket, &now_seg, sizeof(SEGMENT), 0,
                     (struct sockaddr *)&agent, &addr_len) > 0) {
            if (ack_sure < 0) {
                sscanf(now_seg.data, "%d %d", &height, &width);
                printf("height is %d, width is %d\n", height, width);
            }
            if (now_seg.header.seqNumber == ack_sure + 1) {
                unsigned long checksum = get_checksum(now_seg.data);
                if (checksum == now_seg.header.checksum) {
                    if (index < 256) {
                        fprintf(stderr, "recv\tdata\t#%d\n",
                                now_seg.header.seqNumber);
                        if (now_seg.header.seqNumber > 0) {
                            memcpy(&buffer_pkt[index++], &now_seg,
                                   sizeof(SEGMENT));
                        } else {
                            init_player(width, height);
                        }
                        ack_sure++;
                    } else {
                        fprintf(stderr, "drop\tdata\t#%d\t(buffer overflow)\n",
                                now_seg.header.seqNumber);
                        flush_vid(index);
                        index = 0;
                    }
                } else {
                    fprintf(stderr, "drop\tdata\t#%d\t(corrupted)\n",
                            now_seg.header.seqNumber);
                }
            } else {
                fprintf(stderr, "drop\tdata\t#%d\t(out of order)\n",
                        now_seg.header.seqNumber);
            }
            now_seg.header.ack = 1;
            now_seg.header.ackNumber = ack_sure;
            sendto(recvsocket, &now_seg, sizeof(SEGMENT), 0,
                   (struct sockaddr *)&agent, addr_len);
            if (now_seg.header.seqNumber == ack_sure && now_seg.header.fin) {
                fprintf(stderr, "send\tfinack\n");
                flush_vid(index);
                break;
            } else {
                fprintf(stderr, "send\tack\t#%d\n", now_seg.header.ackNumber);
            }
        }
        initSEG(now_seg);
    }
    if (pid > 0) waitpid(pid, NULL, 0);
    fclose(fp);
    wait(NULL);
    return 0;
}
void setIP(char *dst, const char *src) {
    if (strcmp(src, "0.0.0.0") == 0 || strcmp(src, "local") == 0 ||
        strcmp(src, "localhost") == 0) {
        sscanf("127.0.0.1", "%s", dst);
    } else {
        sscanf(src, "%s", dst);
    }
    return;
}

void corruptData(char *data, int len) {
    for (int i = 0; i < len; ++i) {
        data[i] = ~data[i];
    }
}

void initSEG(SEGMENT &a) {
    a.header.ack = 0;
    a.header.ackNumber = 0;
    a.header.fin = 0;
    a.header.checksum = 0;
    a.header.length = 0;
    a.header.seqNumber = 0;
    a.header.syn = 0;
    memset(a.data, 0, sizeof(a.data));
    return;
}
unsigned long get_checksum(char *str) {
    char data[1000];
    memcpy(data, str, 1000);
    unsigned long checksum = crc32(0L, (const Bytef *)data, 1000);
    return checksum;
}