#include "GBN.hpp"
#include "opencv2/opencv.hpp"
using namespace cv;
#define THRESH 16
#define DWSIZE 1
static int read_final = 0;
FILE *fp = fopen("tempfuck.txt", "w");
void setIP(char *dst, const char *src);
void corruptData(char *data, int len);
void initSEG(SEGMENT &a);
char *collectSEG(SEGMENT &seg, char *buff);
void empback(LIST &lit, SEGMENT seg);
void empfront(LIST &lit, SEGMENT &seg);
void popback(LIST &lit);
void popfront(LIST &lit);
unsigned long get_checksum(char *str);
void seg_collect(LIST &lit, Mat &tmp_frame, int &seq);
int max(int a, int b) { return (a > b) ? a : b; }
int main(int argc, char *argv[]) {
    if (argc != 4)
        ERR_EXIT(
            "usage: ./sender <sender port> <agent IP>:<agent port> <.mpg "
            "filename>");
    int sendersocket, portNum, nBytes;
    uint addr_len = sizeof(struct sockaddr_in);
    SEGMENT s_tmp;
    struct sockaddr_in sender, agent, receiver, tmp_addr;
    socklen_t sender_size, recv_size, tmp_size, agent_size;
    char sendIP[50], agentIP[50], file_name[100], tmpIP[50];
    int sendPort, agentPort, recvPort;
    // declare
    sendPort = atoi(argv[1]);
    setIP(sendIP, "local");
    // sender
    sscanf(argv[2], "%[^:]:%d", tmpIP, &agentPort);  // sender
    setIP(agentIP, tmpIP);
    // agent
    strcpy(file_name, argv[3]);
    // file name
    int tl = strlen(file_name);
    if (strcmp(&file_name[tl - 4], ".mpg") != 0) ERR_EXIT("Not a .mpg file");
    VideoCapture cap;
    cap.open(file_name);
    if (!cap.isOpened()) ERR_EXIT("Open file failed");
    // open the video file
    int width = cap.get(CAP_PROP_FRAME_WIDTH);
    int height = cap.get(CAP_PROP_FRAME_HEIGHT);
    Mat tmp_frame = Mat::zeros(height, width, CV_8UC3);
    if (!tmp_frame.isContinuous()) tmp_frame = tmp_frame.clone();
    // ensure continuous

    // get the resolution, init

    /* Create UDP socket */
    sendersocket = socket(PF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);

    /* Configure settings in sender struct */
    sender.sin_family = AF_INET;
    sender.sin_port = htons(sendPort);
    sender.sin_addr.s_addr = inet_addr(sendIP);
    memset(sender.sin_zero, '\0', sizeof(sender.sin_zero));

    /* Configure settings in agent struct */
    agent.sin_family = AF_INET;
    agent.sin_port = htons(agentPort);
    agent.sin_addr.s_addr = inet_addr(agentIP);
    memset(agent.sin_zero, '\0', sizeof(agent.sin_zero));

    /* bind socket */
    bind(sendersocket, (struct sockaddr *)&sender, sizeof(sender));

    /* Initialize size variable to be used later on */
    sender_size = sizeof(sender);
    agent_size = sizeof(agent);
    tmp_size = sizeof(tmp_addr);

    printf("sender start==\n");
    printf("sender info: ip = %s port = %d\n", sendIP, sendPort);
    printf("agent info: ip = %s port = %d\n", agentIP, agentPort);
    // start to send
    int threshold = THRESH, winsize = DWSIZE;
    LIST window_list;
    window_list.head = NULL;
    window_list.tail = NULL;
    window_list.size = 0;

    SEGMENT now_seg;
    initSEG(now_seg);
    printf("hight is %d, width is %d\n", height, width);
    sprintf(now_seg.data, "%d %d\n", width, height);
    now_seg.header.checksum = get_checksum(now_seg.data);
    empback(window_list, now_seg);
    initSEG(now_seg);
    int dosend = 0, send_o = -1, wait_now = 0, win_right = 0, win_left = 0,
        len = 0, seq = 1, ack_sure = -1;
    int64_t start_t = clock(), now_t = clock();
    char *p = NULL, *tmp_buf = (char *)malloc(sizeof(char));
    SEGNODE *node_now = NULL;
    while (1) {
        if (window_list.size < winsize * 100 && (!read_final)) {
            // make sure the space complexity is acceptable
            cap >> tmp_frame;
            seg_collect(window_list, tmp_frame, seq);
        }
        if (node_now == NULL) node_now = window_list.head;
        if (node_now->seg.header.seqNumber <= win_right) {
            dosend = 1;
        } else {
            dosend = 0;
        }
        if (dosend) {
            // send pkt
            now_seg = node_now->seg;
            sendto(sendersocket, &now_seg, sizeof(SEGMENT), 0,
                   (struct sockaddr *)&agent, addr_len);
            if (now_seg.header.fin) {
                fprintf(stderr, "send\tfin\n");
            } else if (now_seg.header.seqNumber == send_o + 1) {
                fprintf(stderr, "send\tdata\t#%d,\twinSize = %d\n",
                        now_seg.header.seqNumber, winsize);
                send_o++;
            } else {
                fprintf(stderr, "rsend\tdata\t#%d,\twinSize = %d\n",
                        now_seg.header.seqNumber, winsize);
            }
            // init
            initSEG(now_seg);
            // clock
            if (!wait_now) {
                start_t = clock();
                wait_now = 1;
            }
            node_now = node_now->next;
        }
        // recv ack
        if (ack_sure != win_right) {  // not end
            // receive some ack
            if (recvfrom(sendersocket, &now_seg, sizeof(SEGMENT), 0,
                         (struct sockaddr *)&agent, &addr_len) > 0) {
                start_t = clock();
                if (now_seg.header.fin == 1) {
                    fprintf(stderr, "recv\tfinback\n");
                    break;
                }
                if (ack_sure < now_seg.header.ackNumber)
                    ack_sure = now_seg.header.ackNumber;

                fprintf(stderr, "recv\tack\t#%d\n", now_seg.header.ackNumber);
                if (ack_sure >= win_right) {  // congestion controll
                    if (winsize >= threshold) {
                        winsize++;
                    } else {
                        winsize *= 2;
                    }
                    win_left = ack_sure + 1;
                    win_right = win_left + winsize - 1;
                    popfront(window_list);
                } else if (ack_sure >= win_left) {
                    while (win_left++ <= ack_sure) {
                        popfront(window_list);
                    }
                }
            }
        }
        // fail check timeout
        now_t = clock();
        if (now_t - start_t >= 1 * CLOCKS_PER_SEC) {
            winsize = 1;
            win_right = win_left;
            threshold = max(winsize / 2, 1);
            fprintf(stderr, "time\tout,\t\tthreshold = %d\n", threshold);
            node_now = NULL;
            start_t = clock();
        }
        initSEG(now_seg);
    }
    cap.release();
    fclose(fp);
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
char *collectSEG(SEGMENT &seg, char *buff) {
    int rest = sizeof(seg.data) - seg.header.length;
    if (rest < strlen(buff)) {
        memcpy(&seg.data[seg.header.length], buff, rest);
        seg.header.length += rest;
        return buff + rest;
    } else {
        memcpy(&seg.data[seg.header.length], buff, strlen(buff));
        seg.header.length += strlen(buff);
        return NULL;
    }
}
void empback(LIST &lit, SEGMENT seg) {
    SEGNODE *nod = (SEGNODE *)malloc(sizeof(SEGNODE));
    nod->seg = seg;
    nod->next = NULL;
    nod->prev = lit.tail;
    if (lit.tail == NULL) {
        lit.tail = nod;
        lit.head = nod;
    } else {
        lit.tail->next = nod;
        lit.tail = nod;
    }
    lit.size++;
    return;
}
void empfront(LIST &lit, SEGMENT &seg) {
    SEGNODE *nod = (SEGNODE *)malloc(sizeof(SEGNODE));
    nod->seg = seg;
    nod->prev = NULL;
    nod->next = lit.head;
    if (lit.tail == NULL) {
        lit.tail = nod;
        lit.head = nod;
    } else {
        lit.head->prev = nod;
        lit.head = nod;
    }
    lit.size++;
    return;
}
void popback(LIST &lit) {
    if (lit.tail == NULL) return;
    SEGNODE *node = lit.tail->prev;
    free(lit.tail);
    if (node != NULL) {
        node->next = NULL;
    } else {
        lit.head = NULL;
        lit.tail = NULL;
    }
    lit.tail = node;
    lit.size--;

    return;
}
void popfront(LIST &lit) {
    if (lit.head == NULL) return;
    SEGNODE *node = lit.head->next;
    free(lit.head);
    if (node != NULL) {
        node->prev = NULL;
    } else {
        lit.head = NULL;
        lit.tail = NULL;
    }
    lit.head = node;
    lit.size--;
    return;
}

unsigned long get_checksum(char *str) {
    char data[SEG_SIZE];
    memcpy(data, str, SEG_SIZE);
    unsigned long checksum = crc32(0L, (const Bytef *)data, SEG_SIZE);
    return checksum;
}

void seg_collect(LIST &lit, Mat &tmp_frame, int &seq) {
    if (tmp_frame.empty()) {
        SEGMENT *tmp_seg = (SEGMENT *)malloc(sizeof(SEGMENT));
        initSEG(*tmp_seg);
        tmp_seg->header.fin = 1;
        empback(lit, *tmp_seg);
        read_final = 1;
        return;
    }
    int imgSize = tmp_frame.total() * tmp_frame.elemSize();
    uchar *p = tmp_frame.data;
    char *fuck = new char[imgSize];
    memcpy(fuck, p, imgSize);
    int rest = imgSize;
    while (rest > 0) {
        SEGMENT *tmp_seg = (SEGMENT *)malloc(sizeof(SEGMENT));
        initSEG(*tmp_seg);
        int set = SEG_SIZE;
        if (rest < set) set = rest;
        tmp_seg->header.seqNumber = seq++;
        tmp_seg->header.fin = 0;
        tmp_seg->header.length = set;
        memcpy(tmp_seg->data, &fuck[imgSize - rest], set);
        tmp_seg->header.checksum = get_checksum(tmp_seg->data);
        fwrite(tmp_seg->data, sizeof(char), set, fp);
        fflush(fp);
        empback(lit, *tmp_seg);
        rest -= set;
        free(tmp_seg);
    }
    return;
}