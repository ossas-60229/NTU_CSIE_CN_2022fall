#include "GBN.hpp"
#include "opencv2/opencv.hpp"
using namespace cv;
SEGMENT buffer_pkt[BUFF_SIZE];
static int frame_number = 0, frame_collect = 0, now_width, now_height;
const char *player_exec = "./openCV";
const char *fifo_name = "video.fifo";
int flush_time = 0;
pid_t flush_pid = -1;
FILE *fp;
int init_ed = 0;
pid_t pid = -1;
void sighandler(int signo) {
    if (signo == SIGPIPE) {
        wait(NULL);
        ERR_EXIT("player is stopped\n");
    }
    return;
}
// handle the SIGPIPE signal
void init_player(int width, int height);
// initialize player
void flush_vid(int index);
// flush frame data to player process
void initSEG(SEGMENT &a);
// initialize some segment
void setIP(char *dst, const char *src);
void corruptData(char *data, int len);
unsigned long get_checksum(char *str);
// get the checksum
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
    int width = 0, height = 0, index = 0;  // index of buffer
    while (1) {
        if (recvfrom(recvsocket, &now_seg, sizeof(SEGMENT), 0,
                     (struct sockaddr *)&agent, &addr_len) > 0) {
            if (now_seg.header.seqNumber == ack_sure + 1) {  // order check
                unsigned long checksum = get_checksum(now_seg.data);
                if (checksum == now_seg.header.checksum) {  // checksum check
                    if (index < BUFF_SIZE) {  // buffer flow or not
                        fprintf(stderr, "recv\tdata\t#%d\n",
                                now_seg.header.seqNumber);
                        memcpy(&buffer_pkt[index++], &now_seg, sizeof(SEGMENT));
                        ack_sure = now_seg.header.seqNumber;
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
            if (now_seg.header.seqNumber == ack_sure &&
                now_seg.header.fin) {  // finback
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
    if (flush_pid > 0) waitpid(flush_pid, NULL, 0);
    if (pid > 0) waitpid(pid, NULL, 0);
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
    char data[SEG_SIZE];
    memcpy(data, str, SEG_SIZE);
    unsigned long checksum = crc32(0L, (const Bytef *)data, SEG_SIZE);
    return checksum;
}
void init_player(int width, int height) {
    if (init_ed > 0) return;
    init_ed = 1;
    signal(SIGPIPE, sighandler);
    mkfifo(fifo_name, 0777);
    now_width = width;
    now_height = height;
    pid = fork();
    if (pid == 0) {
        char w[50], h[50];
        sprintf(w, "%d", width);
        sprintf(h, "%d", height);
        execlp(player_exec, player_exec, fifo_name, w, h, NULL);
    } else {
        sleep(1);
        fp = fopen(fifo_name, "w");
    }
    return;
}
void flush_vid(int index) {
    fprintf(stderr, "flush\n");
    if (buffer_pkt[0].header.seqNumber == 0) {
        int width, height;
        sscanf(buffer_pkt[0].data, "%d %d", &width, &height);
        init_player(width, height);
    }
    pid_t tmp_pid = fork();
    if (tmp_pid == 0) {
        if (flush_pid > 0) waitpid(flush_pid, NULL, 0);
        for (int i = 0; i < index; i++) {
            if (buffer_pkt[i].header.seqNumber != 0) {
                fwrite(buffer_pkt[i].data, sizeof(char),
                       buffer_pkt[i].header.length, fp);
            }
        }
        fflush(fp);
        exit(0);
    }
    flush_pid = tmp_pid;

    return;
}
