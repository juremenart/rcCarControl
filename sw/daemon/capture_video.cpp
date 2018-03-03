#include <cstdio>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>

#include <iostream>

#include <opencv2/opencv.hpp>
#include <opencv2/videoio.hpp>  // Video write

#include "rcc_ov5642_ctrl.h"
#include "rcc_img_proc.h"

int main(int argc, char *argv[])
{
    // OV5642
    rccOv5642Ctrl *ov5642Ctrl = new rccOv5642Ctrl(0);
    rccOv5642Ctrl::ov5642_mode_t mode = rccOv5642Ctrl::ov5642_vga_yuv;
    rccImgProc *imgProc;
    std::string inputFile("/dev/video0");
    std::string outputFile("/tmp/bla.avi");
    cv::VideoWriter outputVideo;
    int retVal = -1;

    int retries = 100;

    if(!ov5642Ctrl->init(mode))
    {
        return -1;
    }

    if(argc > 1)
    {
        outputFile = std::string(argv[1]);
    }
    if(argc > 2)
    {
        inputFile = std::string(argv[2]);
    }

    imgProc = new rccImgProc();
    if(imgProc->open(inputFile.c_str()))
    {
        std::cout << "Opened input file " << inputFile << std::endl;
    }
    else
    {
        std::cerr << "Can not open input file " << inputFile << std::endl;
        goto end;
    }

    // Prepare output video stream
    {
        //int out_fourcc = CV_FOURCC('F','M','P','4'); // mpeg
        int out_fourcc = CV_FOURCC('X','2','6','4'); // x264
//        int fps = imgProc->getVideoDev()->get(cv::CAP_PROP_FPS);
        int fps = 15;
        int width = (int)imgProc->getVideoDev()->get(cv::CAP_PROP_FRAME_WIDTH);
        int height = (int)imgProc->getVideoDev()->get(cv::CAP_PROP_FRAME_HEIGHT);

        std::cout << "Opening video file " << outputFile << " fps=" << fps
                  << " size=" << width <<  " x " << height << std::endl;

        outputVideo.open(outputFile, out_fourcc, fps, cv::Size(width, height));
        if(!outputVideo.isOpened())
        {
            std::cerr << "Can not open output stream!" << std::endl;
            goto end;
        }
    }

    while(true)
    {
        cv::Mat frame;
        if(!imgProc->readFrame(frame))
        {
            std::cerr << "Problem getting the frame" << std::endl;

            if(--retries == 0)
            {
                std::cerr << "Too many retries, quitting" << std::endl;
                goto end;
            }
        }

        if(frame.empty())
            break;

        outputVideo << frame;
    }

    retVal = 0;

end:
    delete ov5642Ctrl;
    delete imgProc;
    return retVal;
}
