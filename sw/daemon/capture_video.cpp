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
#include <mutex>

#include <opencv2/opencv.hpp>
#include <opencv2/videoio.hpp>  // Video write

#include <vlc/vlc.h>

#include "rcc_ov5642_ctrl.h"
#include "rcc_img_proc.h"

class frameStruct
{
public:
    frameStruct() : mStreamRunning(false), mFrames(0), mRdFrame(0),
                    mWrFrame(0), mDts(0), mPts(0) {}

    ~frameStruct() {};

    bool                 mStreamRunning;
    std::mutex           mMutex;
    std::vector<cv::Mat> mFrames;
    std::size_t          mRdFrame;
    std::size_t          mWrFrame;
    int64_t              mDts, mPts;
};

libvlc_instance_t     *vlc_libvlc = NULL;
libvlc_media_t        *vlc_media  = NULL;
libvlc_media_player_t *vlc_mp     = NULL;
std::vector<const char *> vlc_argv;

frameStruct mFrameStruct;
//static char const *vlc_argv[] =
//{
//    "--no-audio",
////    "--no-xlib",
//    "--ignore-config",
//    "--input-repeat", "100",
////    "--sout=#duplicate{dst=display,dst=rtp{sdp=rtsp://127.0.0.1:8080/}}",
//    "--sout=#transcode{vcodec=h264,scale=Auto}:duplicate{dst=rtp{sdp=rtsp://127.0.0.1:8080/}}",
//    "--rtsp-tcp"
//};



//#define USE_OV5642
static bool strIsNumber(const std::string& s)
{
    std::string::const_iterator it = s.begin();
    while (it != s.end() && std::isdigit(*it)) ++it;
    return !s.empty() && it == s.end();
}

/**
    \brief Callback method triggered by VLC to get image data from
    a custom memory source. This is used to tell VLC where the
    data is and to allocate buffers as needed.

    To set this callback, use the "--imem-get=<memory_address>"
    option, with memory_address the address of this function in memory.

    When using IMEM, be sure to indicate the format for your data
    using "--imem-cat=2" where 2 is video. Other options for categories are
    0 = Unknown,
    1 = Audio,
    2 = Video,
    3 = Subtitle,
    4 = Data

    When creating your media instance, use libvlc_media_new_location and
    set the location to "imem:/" and then play.
    \param[in] data Pointer to user-defined data, this is your data that
    you set by passing the "--imem-data=<memory_address>" option when
    initializing VLC instance.
    \param[in] cookie A user defined string. This works the same way as
    data, but for string. You set it by adding the "--imem-cookie=<your_string>"
    option when you initialize VLC. Use this when multiple VLC instances are
    running.
    \param[out] dts The decode timestamp, value is in microseconds. This value
    is the time when the frame was decoded/generated. For example, 30 fps
    video would be every 33 ms, so values would be 0, 33333, 66666, 99999, etc.
    \param[out] pts The presentation timestamp, value is in microseconds. This
    value tells the receiver when to present the frame. For example, 30 fps
    video would be every 33 ms, so values would be 0, 33333, 66666, 99999, etc.
    \param[out] flags Unused,ignore.
    \param[out] bufferSize Use this to set the size of the buffer in bytes.
    \param[out] buffer Change to point to your encoded frame/audio/video data.
        The codec format of the frame is user defined and set using the
        "--imem-codec=<four_letter>," where 4 letter is the code for your
        codec of your source data.
*/
int vlcGetCallback(void *data,
                   const char *cookie,
                   int64_t *dts,
                   int64_t *pts,
                   unsigned *flags,
                   size_t * bufferSize,
                   void ** buffer)
{
//    frameStruct *imem = (frameStruct *)data;
//
//    return 0;
//    if(imem == NULL)
//    {
//        return 1;
//    }
//
//    /* We are not yet ready */
//    if(imem->mStreamRunning == false)
//    {
//        return 0;
//    }
//
//    // add also locking?
////TODO    if(imem->mRdFrame >= imem->mFrames.size())
//    {
//        imem->mRdFrame = 0;
//    }
//
////    int64 uS = 33333; // 30 fps
//    int64 uS = 66667; // 15 fps
//
//    cv::Mat img = imem->mFrames[imem->mRdFrame++];
//    *bufferSize = img.rows*img.cols*img.channels();
//    *buffer = img.data;
//    *dts = *pts = imem->mDts = imem->mPts = imem->mPts + uS;

    return 0;
}

/**
    \brief Callback method triggered by VLC to release memory allocated
    during the GET callback.

    To set this callback, use the "--imem-release=<memory_address>"
    option, with memory_address the address of this function in memory.

    \param[in] data Pointer to user-defined data, this is your data that
    you set by passing the "--imem-data=<memory_address>" option when
    initializing VLC instance.
    \param[in] cookie A user defined string. This works the same way as
    data, but for string. You set it by adding the "--imem-cookie=<your_string>"
    option when you initialize VLC. Use this when multiple VLC instances are
    running.
    \param[int] bufferSize The size of the buffer in bytes.
    \param[out] buffer Pointer to data you allocated or set during the GET
    callback to handle  or delete as needed.
*/
int vlcReleaseCallback (void *data,
                        const char *cookie,
                        size_t bufferSize,
                        void * buffer)
{
    // Since I did not allocate any new memory, I don't need
    // to delete it here. However, if you did in your get method, you
    // should delete/free it here.
    std::cout << "vlcReleaseCallback()" << std::endl;
    return 0;
}

void vlcFillArgList(int port, std::string &a_input)
{
    char tmpStr[256];
    vlc_argv.resize(0);

    vlc_argv.push_back("--no-audio");
    vlc_argv.push_back("--no-xlib");
    vlc_argv.push_back("--verbose=2");
    vlc_argv.push_back("--ignore-config");
    vlc_argv.push_back("--input-repeat=100");
//    sprintf(&tmpStr[0], "--sout=#transcode{vcodec=mjpg,scale=Auto}:duplicate{dst=rtp{sdp=rtsp://:%d/}}", port);
//    vlc_argv.push_back(tmpStr);
//    vlc_argv.push_back("--sout=#transcode{vcodec=mjpg,scale=Auto}:duplicate{dst=rtp{sdp=rtsp://:8080/}}");
    vlc_argv.push_back("--rtsp-tcp");

    // The imem interface
    char imemDataArg[256];
    std::cout << "Frame struct address = 0x" << std::hex << (uint64_t)&mFrameStruct << std::endl;
    sprintf(imemDataArg, "--imem-data=%p", &mFrameStruct);
    vlc_argv.push_back(imemDataArg);

    std::cout << "GET address = 0x" << std::hex << (uint64_t)&vlcGetCallback << std::endl;
    char imemGetArg[256];
    sprintf(imemGetArg, "--imem-get=%p", vlcGetCallback);
    vlc_argv.push_back(imemGetArg);

    std::cout << "RELEASE address = 0x" << std::hex << (uint64_t)&vlcReleaseCallback << std::dec << std::endl;
    char imemRelArg[256];
    sprintf(imemRelArg, "--imem-release=%p", vlcReleaseCallback);
    vlc_argv.push_back(imemRelArg);

    vlc_argv.push_back("--imem-cookie=\"IMEM\"");
    vlc_argv.push_back("--imem-codec=raw");
    vlc_argv.push_back("--imem-cat=2");

    vlc_argv.push_back("--imem-width=640");
    vlc_argv.push_back("--imem-height=480");
    vlc_argv.push_back("--imem-channels=3");

    const char **arg_str = vlc_argv.data();
    for(size_t i = 0; i < vlc_argv.size(); i++)
        std::cout << "Arg " << i << " is: " << arg_str[i] << std::endl;
    return;
}

int startServiceServer(int port, std::string &a_input)
{
    int fd = 0;

    vlcFillArgList(port, a_input);
    int vlc_argc = vlc_argv.size();
    std::cout << "IMEM number of arguments: " << vlc_argc << std::endl;

    vlc_libvlc = libvlc_new(vlc_argc, vlc_argv.data());
    if(!vlc_libvlc)
    {
        std::cerr << "LibVLC initialized failed!" << std::endl;
        return -1;
    }

    vlc_media = libvlc_media_new_location(vlc_libvlc, "imem://");
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

//    libvlc_media_release(vlc_media);
//    vlc_media = NULL;

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

    int fps;
    int width;
    int height;
    int fourcc;

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

    mFrameStruct.mFrames.resize(3);

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
        if(!imgProc->readFrame(mFrameStruct.mFrames[0]))
        {
            std::cerr << "Problem getting the frame" << std::endl;

            if(--retries == 0)
            {
                std::cerr << "Too many retries, quitting" << std::endl;
                goto end;
            }
        }

        if(mFrameStruct.mFrames[0].empty())
            break;

        if(!startServer)
        {
            outputVideo << mFrameStruct.mFrames[0];
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
