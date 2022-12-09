#include "GBN.hpp"
#include "opencv2/opencv.hpp"
using namespace std;
using namespace cv;

const char* player_exec = "./openCV";
int main() {
    VideoCapture cap("video.mpg");

    // Get the resolution of the video
    int width = cap.get(CAP_PROP_FRAME_WIDTH);
    int height = cap.get(CAP_PROP_FRAME_HEIGHT);
    Mat tmp_frame = Mat::zeros(height, width, CV_8UC3);
    if (!tmp_frame.isContinuous()) {
        tmp_frame = tmp_frame.clone();
    }
    int imgSize = tmp_frame.elemSize() * tmp_frame.total();
    printf("width: %d, height: %d imgsize: %d\n", width, height, imgSize);
    int number = 0;
    const char* file_name = "fuckyou.txt";
    remove(file_name);
    mkfifo(file_name, 0777);
    pid_t pid = fork();
    char w[50], h[50];
    sprintf(w, "%d", width);
    sprintf(h, "%d", height);

    if (pid == 0) {
        execlp(player_exec, player_exec, file_name, w, h, NULL);
    }
    FILE* fp = fopen(file_name, "w+");
    fprintf(stderr, "start loop");
    while (1) {
        cap >> tmp_frame;
        if (tmp_frame.empty()) break;
        uchar buffer[imgSize];
        uchar* p = tmp_frame.data;
        memcpy(buffer, p, imgSize);
        fwrite(buffer, sizeof(uchar), imgSize, fp);
        fflush(fp);
    }
    fclose(fp);
    cap.release();
    waitpid(pid, NULL, 0);
    return 0;
}