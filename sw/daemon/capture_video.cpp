#include <cstdio>
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

#include <opencv2/opencv.hpp>
#include <opencv2/videoio.hpp>  // Video write

#include <vlc/vlc.h>

#include "rcc_ov5642_ctrl.h"
#include "rcc_img_proc.h"

static libvlc_instance_t     *vlc_libvlc = NULL;
static libvlc_media_t        *vlc_media  = NULL;
static libvlc_media_player_t *vlc_mp     = NULL;

static char const *vlc_argv[] =
{
    "--no-audio",
//    "--no-xlib",
    "--ignore-config",
    "--sout=#transcode{vcodec=h264,scale=Auto}:duplicate{dst=display,dst=rtp{sdp=rtsp://127.0.0.1:8080/}}",
    "--rtsp-tcp"
};
static int vlc_argc = sizeof(vlc_argv) / sizeof(*vlc_argv);


//#define USE_OV5642
static bool strIsNumber(const std::string& s)
{
    std::string::const_iterator it = s.begin();
    while (it != s.end() && std::isdigit(*it)) ++it;
    return !s.empty() && it == s.end();
}

int startServiceServer(int port, std::string &a_input)
{
    int fd = 0;

    vlc_libvlc = libvlc_new(vlc_argc, vlc_argv);
    if(!vlc_libvlc)
    {
        std::cerr << "LibVLC initialized failed!" << std::endl;
        return -1;
    }

    vlc_media = libvlc_media_new_path(vlc_libvlc, a_input.c_str());
    if(!vlc_media)
    {
        std::cerr << "LibVLC media new path failed!" << std::endl;
        return -1;
    }

    vlc_mp = libvlc_media_player_new_from_media(vlc_media);
    if(!vlc_mp)
    {
        std::cerr << "LibVLC creating new media player failed!" << std::endl;
        return -1;
    }

    libvlc_media_player_play(vlc_mp);

    usleep(60000000);

    libvlc_media_player_stop(vlc_mp);
    return fd;
}

void closeServiceServer(int fd)
{
    if(fd > 0)
    {
        close(fd);
    }
}

int main(int argc, char *argv[])
{
    // OV5642
    rccOv5642Ctrl *ov5642Ctrl = NULL;
    rccOv5642Ctrl::ov5642_mode_t mode = rccOv5642Ctrl::ov5642_vga_yuv;

    rccImgProc *imgProc;
    std::string inputFile("/dev/video0");
    std::string outputFile("/tmp/bla.avi");
    cv::VideoWriter outputVideo;
    int retVal = -1;

    int retries = 100;
    bool startServer = false;
    int serverPort = -1, serverFd = -1;

#ifdef USE_OV5642
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

    // Prepare output video stream
    {
        if(startServer)
        {
            serverFd = startServiceServer(serverPort, inputFile);
            if(serverFd < 0)
            {
                std::cerr << "Failed to open server on port " << serverPort
                          << std::endl;
            }
            std::cout << "Open server on port " << serverFd << std::endl;
        }
        else
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

        if(startServer)
        {

        }
        else
        {
            outputVideo << frame;
        }
    }

    retVal = 0;

end:
    closeServiceServer(serverFd);
    if(ov5642Ctrl)
    {
        delete ov5642Ctrl;
    }
    delete imgProc;
    return retVal;
}
