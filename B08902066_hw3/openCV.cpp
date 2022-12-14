
#include "opencv2/opencv.hpp"

#include <fcntl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <iostream>

using namespace std;
using namespace cv;

int main(int argc, char *argv[]) {
    Mat server_img, client_img;
    if (argc != 4) {
        fprintf(stderr, "argument error");
        exit(0);
    }
    FILE *fp = fopen(argv[1], "r");

    // Get the resolution of the video
    int width = atoi(argv[2]);
    int height = atoi(argv[3]);
    // fprintf(stderr, "width is %d, height is %d\n", width, height);
    //  Allocate container to load frames
    server_img = Mat::zeros(height, width, CV_8UC3);
    client_img = Mat::zeros(height, width, CV_8UC3);

    // Ensure the memory is continuous (for efficiency issue.)
    if (!server_img.isContinuous()) {
        server_img = server_img.clone();
    }

    if (!client_img.isContinuous()) {
        client_img = client_img.clone();
    }
    int imgSize = server_img.total() * server_img.elemSize();
    uchar buffer[imgSize];

    while (!feof(fp)) {
        fread(buffer, sizeof(uchar), imgSize, fp);
        uchar *iptr = client_img.data;
        memcpy(iptr, buffer, imgSize);

        imshow("Video", client_img);

        char c = (char)waitKey(1000);
        if (c == 27) break;
    }
    fclose(fp);
    // destroyAllWindows();
    return 0;
}
