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
    if (!tmp_frame.isContinuous()) {
        tmp_frame = tmp_frame.clone();
    }
    int imgSize = tmp_frame.elemSize() * tmp_frame.total();
    printf("width: %d, height: %d imgsize: %d\n", width, height, imgSize);
    int number = 0;
    while (1) {
        cap >> tmp_frame;
        if (tmp_frame.empty()) break;
        printf("%d\n", number++);
    }
    cap.release();

    return 0;
}