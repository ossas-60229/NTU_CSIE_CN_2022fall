#include <fstream>
#include <iostream>

#include "GBN.hpp"
#include "opencv2/opencv.hpp"
using namespace std;
using namespace cv;

int main() {
    VideoCapture cap("video.mpg");

    // Get the resolution of the video
    int width = cap.get(CAP_PROP_FRAME_WIDTH);
    int height = cap.get(CAP_PROP_FRAME_HEIGHT);
    Mat tmp_frame = Mat::zeros(height, width, CV_8UC3);
    FILE *fp = fopen("fuck.tmp", "w");
    printf("width: %d, height: %d\n", width, height);
    while (1) {
        cap >> tmp_frame;
        int imgSize = tmp_frame.elemSize() * tmp_frame.total();
        if (tmp_frame.empty()) break;
        uchar buf[imgSize];
        memcpy(buf, tmp_frame.data, imgSize);
        fwrite(p, imgSize, 1, fp);
        fflush(fp);
    }
    fclose(fp);

    return 0;
}