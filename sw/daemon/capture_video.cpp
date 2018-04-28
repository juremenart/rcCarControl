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

#include <opencv2/opencv.hpp>
#include <opencv2/videoio.hpp>  // Video write

#include "rcc_ov5642_ctrl.h"
#include "rcc_img_proc.h"

#define USE_LIVE555

#include "live_cam_device_source.h"

#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"

#ifdef USE_LIVE555
typedef struct sessionState_s {
    FramedSource *source;
    RTPSink* sink;
    RTCPInstance* rtcpInstance;
    Groupsock* rtpGroupsock;
    Groupsock* rtcpGroupsock;
    RTSPServer* rtspServer;

    sessionState_s()
        : source(NULL), sink(NULL), rtcpInstance(NULL), rtpGroupsock(NULL),
          rtcpGroupsock(NULL), rtspServer(NULL) { };
} sessionState_t;

UsageEnvironment *liveEnv        = NULL;
TaskScheduler    *liveScheduler  = NULL;
sessionState_t    liveSession;


#endif // USE_LIVE555

//#define USE_OV5642
bool strIsNumber(const std::string& s)
{
    std::string::const_iterator it = s.begin();
    while (it != s.end() && std::isdigit(*it)) ++it;
    return !s.empty() && it == s.end();
}

void closeServiceServer(int fd)
{
    if(fd > 0)
    {
        close(fd);
    }

#ifdef USE_LIVE555
    if(liveScheduler)
    {
//        delete [] liveScheduler;
        liveScheduler = NULL;
    }

#endif // USE_LIVE555
}

void startServiceServer(int port, int fps)
{
#ifdef USE_LIVE555
    liveScheduler = BasicTaskScheduler::createNew();
    liveEnv = BasicUsageEnvironment::createNew(*liveScheduler);

    // Create the RTSP server:
    liveSession.rtspServer = RTSPServer::createNew(*liveEnv, port);
    if (liveSession.rtspServer == NULL) {
        *liveEnv << "Failed to create RTSP server: " << liveEnv->getResultMsg() << "\n";
        return;
    }

    std::cout << "RTSP Server done!" << std::endl;

    liveSession.source = LiveCamDeviceSource::createNew(*liveEnv);
    if(liveSession.source == NULL)
    {
        *liveEnv << "Unable to open camera: "
                 << liveEnv->getResultMsg() << "\n";
        return;
    }


    /* Add session */
    {
        char const *descString = "Session streamed by capture_video test server";
        char const *streamName = "test";
        ServerMediaSession *sms = ServerMediaSession::createNew(*liveEnv,
                                                                streamName,
                                                                streamName,
                                                                descString);

        // JPEGMediaSubsession class replacement?
        liveSession.rtspServer->addServerMediaSession(sms);
        *liveEnv << "Session available at URL: " <<
            liveSession.rtspServer->rtspURL(sms) << "\n";
    }

    liveEnv->taskScheduler().doEventLoop();
#endif // USE_LIVE555
    return;
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

    int fps;
    int width;
    int height;
    int fourcc;

    cv::Mat frame;

    std::thread *serverThread;

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

    /* Initalize frame structure */
    fps = (int)imgProc->getVideoDev()->get(cv::CAP_PROP_FPS);
    width = (int)imgProc->getVideoDev()->get(cv::CAP_PROP_FRAME_WIDTH);
    height = (int)imgProc->getVideoDev()->get(cv::CAP_PROP_FRAME_HEIGHT);
    fourcc = (int)imgProc->getVideoDev()->get(cv::CAP_PROP_FOURCC);

    /* Enable three buffers */
    {
        std::string fourcc_str = cv::format("%c%c%c%c", fourcc & 255, (fourcc >> 8) & 255, (fourcc >> 16) & 255, (fourcc >> 24) & 255);
        cv::Mat fMat;
        bool retVal = imgProc->getVideoDev()->retrieve(fMat);

        if(retVal == true)
        {
            std::cout << "retrieve() suceeded" << std::endl;
        }
        else
        {
            std::cout << "retrieve() NOT suceeded" << std::endl;
        }
        std::cout << "Video resolution=" << width << "x" << height
                  << " fps=" << fps << " fourcc=" << fourcc_str
                  << std::endl;
    }

    // Prepare output video stream
    {
        if(startServer)
        {
            serverThread = new std::thread(startServiceServer, serverPort, fps);
//            serverFd = startServiceServer(serverPort, inputFile, fps);
//            if(serverFd < 0)
//            {
//                std::cerr << "Failed to open server on port " << serverPort
//                          << std::endl;
//            }
            std::cout << "Open server on port " << serverPort << std::endl;
        }
        else
        {
            //int out_fourcc = CV_FOURCC('F','M','P','4'); // mpeg
            int out_fourcc = CV_FOURCC('X','2','6','4'); // x264
//        int fps = imgProc->getVideoDev()->get(cv::CAP_PROP_FPS);
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
            usleep(500000);
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
