#include "opencv2/opencv.hpp"

#include <fstream>
#include <iostream>
using namespace std;
using namespace cv;

int main(int argc, char *argv[]) {
    Mat server_img, client_img;
    ifstream in;
    in.open(argv[1]);
    printf("open %s\n", argv[1]);
    // Allocate container to load frames
    int width = atoi(argv[2]);
    int height = atoi(argv[3]);
    server_img = Mat::zeros(height, width, CV_8UC3);
    client_img = Mat::zeros(height, width, CV_8UC3);

    // Ensure the memory is continuous (for efficiency issue.)
    if (!server_img.isContinuous()) {
        server_img = server_img.clone();
    }
    if (!client_img.isContinuous()) {
        client_img = client_img.clone();
    }
    while (1) {
        if (in.eof()) break;
        int imgSize = server_img.total() * server_img.elemSize();

        // Allocate a buffer to load the frame (there would be 2 buffers in the
        // world of the Internet)
        uchar *buffer = new uchar[imgSize];
        in >> buffer;
        // Get the size of a frame in bytes

        // Here, we assume that the buffer is transmitted from the server to the
        // client Copy a frame from the buffer to the container of the client
        uchar *iptr = client_img.data;
        memcpy(iptr, buffer, imgSize);

        // show the frame
        imshow("Video", client_img);

        // Press ESC on keyboard to exit
        // Notice: this part is necessary due to openCV's design.
        // waitKey function means a delay to get the next frame. You can change
        // the value of delay to see what will happen
        char c = (char)waitKey(33);
        if (c == 27) break;
    }

    destroyAllWindows();
    return 0;
}
