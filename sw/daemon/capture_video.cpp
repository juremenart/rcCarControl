#include <cstdio>
#include <thread>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>

#include <iostream>
#include <mutex>
#include <chrono>

#include <opencv2/opencv.hpp>
#include <opencv2/videoio.hpp>  // Video write

#include "rcc_ov5642_ctrl.h"
#include "rcc_img_proc.h"

#include "rcc_video_streamer.h"

#define TRACK_TIME
//#define USE_OV5642
bool strIsNumber(const std::string& s)
{
    std::string::const_iterator it = s.begin();
    while (it != s.end() && std::isdigit(*it)) ++it;
    return !s.empty() && it == s.end();
}

int main(int argc, char *argv[])
{
    // OV5642
    rccOv5642Ctrl *ov5642Ctrl = NULL;

    rccImgProc *imgProc;
    std::string inputFile("/dev/video0");
    std::string outputFile("/tmp/bla.avi");
    cv::VideoWriter outputVideo;
    int retVal = -1;

    int retries = 100;
    bool startServer = false;
    int serverPort = -1;

    int fps;
    int width;
    int height;
    int fourcc;

    cv::Mat frame;

    std::chrono::steady_clock::time_point tp;
    std::chrono::duration <int, std::micro> interval(1000000/15);

    rccVideoStreamer *videoStreamer = NULL;
    rccVideoStreamer::rcc_stream_id_t origStreamId, greyStreamId;

#ifdef USE_OV5642
    rccOv5642Ctrl::ov5642_mode_t mode = rccOv5642Ctrl::ov5642_vga_yuv;
    ov5642Ctrl = new rccOv5642Ctrl(0);
    if(!ov5642Ctrl->init(mode))
    {
        return -1;
    }
#endif

    if(argc > 1)
    {
        outputFile = std::string(argv[1]);

        if(strIsNumber(outputFile))
        {
            serverPort = std::stoi(outputFile);
            startServer = true;
        }
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

    /* Initalize frame structure */
    fps = (int)imgProc->getFps();
    interval = std::chrono::duration<int, std::micro>(1000000 / fps);
    width = (int)imgProc->getWidth();
    height = (int)imgProc->getHeight();
    fourcc = (int)imgProc->getFourcc();


    /* Enable three buffers */
    {
        std::string fourcc_str = cv::format("%c%c%c%c", fourcc & 255, (fourcc >> 8) & 255, (fourcc >> 16) & 255, (fourcc >> 24) & 255);

        std::cout << std::dec << "Video resolution=" << width << "x" << height
                  << " fps=" << fps << " fourcc=" << fourcc_str
                  << std::endl;
    }

    // Prepare output video stream
    {
        if(startServer)
        {
            const char *origName = "orig";
            const char *greyName = "grey";

            videoStreamer = new rccVideoStreamer();
            if(!videoStreamer->startServer(serverPort))
            {
                std::cerr << "Failed to open server on port " << serverPort
                          << std::endl;
                goto end;
            }

            origStreamId = videoStreamer->addStream(origName, fps, serverPort);
            if(origStreamId < 0)
            {
                std::cerr << "Could not add stream " << origName << std::endl;
                goto end;
            }

#if 0
            greyStreamId = videoStreamer->addStream(greyName, fps, serverPort+1);
            if(greyStreamId < 0)
            {
                std::cerr << "Could not add stream " << greyName << std::endl;
                goto end;
            }
#endif

            std::cout << "Added streams for original (" << origStreamId <<
                ") and grey (" << greyStreamId << ")" << std::endl;
        }
        else
        {
            //int out_fourcc = CV_FOURCC('F','M','P','4'); // mpeg
            int out_fourcc = CV_FOURCC('X','2','6','4'); // x264

            std::cout << "Opening video file " << outputFile << " fps=" << fps
                      << " size=" << width <<  " x " << height << std::endl;

            outputVideo.open(outputFile, out_fourcc, fps, cv::Size(width, height));
            if(!outputVideo.isOpened())
            {
                std::cerr << "Can not open output stream!" << std::endl;
                goto end;
            }
        }
    }

    tp = std::chrono::steady_clock::now();
    while(true)
    {
#ifdef TRACK_TIME
        std::chrono::steady_clock::time_point tp1 = std::chrono::steady_clock::now();
#endif

        if(!imgProc->readFrame(frame))
        {
            std::cerr << "Problem getting the frame" << std::endl;

            if(--retries == 0)
            {
                std::cerr << "Too many retries, quitting" << std::endl;
                goto end;
            }
        }

#ifdef TRACK_TIME
        std::chrono::steady_clock::time_point tp2 = std::chrono::steady_clock::now();
#endif

        if(frame.empty())
        {
//            break;
            imgProc->reset();
            continue;
        }

        if(startServer)
        {
            // Stream also greyscale just to show multiple streams
//            cv::Mat grey;
//            cv::Mat grey2;

            // TODO: need double-conversion because encodeAndStream
            // can currently work only with 3-dimensional matrices
//            cv::cvtColor(frame, grey, CV_BGR2GRAY);
//            cv::cvtColor(grey, grey2, CV_GRAY2BGR);

//            videoStreamer->encodeAndStream(greyStreamId, grey2);

            videoStreamer->encodeAndStream(origStreamId, frame);
        }
        else
        {
            outputVideo << frame;
        }
#ifdef TRACK_TIME
        std::chrono::steady_clock::time_point tp3 = std::chrono::steady_clock::now();
#endif

        tp = tp + interval;
        if(tp < std::chrono::steady_clock::now())
        {
            std::cerr << "Warning: Loop too slow" << std::endl;
        }
        std::this_thread::sleep_until(tp);
#ifdef TRACK_TIME
        std::chrono::steady_clock::time_point tp4 = std::chrono::steady_clock::now();
        std::cout << " Loop: " << std::dec <<
            std::chrono::duration_cast<std::chrono::microseconds>(tp4-tp1).count()
                  << " FrameAcq: " <<
            std::chrono::duration_cast<std::chrono::microseconds>(tp2-tp1).count()
                  << " FrameWrite: " <<
            std::chrono::duration_cast<std::chrono::microseconds>(tp3-tp2).count()
                  << " SleepTime: " <<
            std::chrono::duration_cast<std::chrono::microseconds>(tp4-tp3).count()
                  << " Interval: " <<
            std::chrono::duration_cast<std::chrono::microseconds>(interval).count()
                  << std::endl;
#endif

    }

    retVal = 0;

end:
    if(videoStreamer)
    {
        delete videoStreamer;
        videoStreamer = NULL;
    }

    if(ov5642Ctrl)
    {
        delete ov5642Ctrl;
    }
    delete imgProc;
    return retVal;
}
