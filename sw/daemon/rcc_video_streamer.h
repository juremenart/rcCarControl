#ifndef __RCC_VIDEO_STREAMER_H
#define __RCC_VIDEO_STREAMER_H

#include <thread>
#include <condition_variable>

#define USE_UDP_MULTICAST

#ifdef USE_LIVE555
// Live555 includes
#include <liveMedia.hh>
#include <BasicUsageEnvironment.hh>
#include <GroupsockHelper.hh>
#endif // USE_LIVE555

#ifdef USE_UDP_MULTICAST
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif // USE_UDP_MULTICAST

// OpenCV
#include <opencv2/opencv.hpp>

// local includes
#include "live_cam_device_source.h"

class rccVideoStreamer {
private:
    // internal structure holding info on available streams
    // Outside world uses 'rcc_stream_id_t' when addressing specific stream and
    // this is index to this table (removal not possible right now)
    typedef struct rcc_streams_info_s {
        std::string          name;
        int                  fps;
#ifdef USE_LIVE555
        std::string          url;
        LiveCamDeviceSource *devSource;
#endif // USE_LIVE555
#ifdef USE_UDP_MULTICAST
        int                  sock;
        int                  port;
        struct sockaddr_in   addr;
#endif // USE_UDP_MULTICAST
    } rcc_streams_info_t;

public:
    typedef int rcc_stream_id_t;

    rccVideoStreamer(void);
    ~rccVideoStreamer(void);

    bool            startServer(int port);
    bool            stopServer(void);
    bool            isServerStarted(void);
    rcc_stream_id_t addStream(const char *streamName, int fps, int port = 0);

    bool encodeAndStream(rccVideoStreamer::rcc_stream_id_t stream_id,
                         cv::Mat &frame);
private:
#ifdef USE_LIVE555
    void serverThread(int port);
#endif // USE_LIVE555

#ifdef USE_UDP_MULTICAST
    int openMulticastSocket(void);
    int closeMulticastSocket(int fd);
    int sendMulticastData(rccVideoStreamer::rcc_stream_id_t stream_id,
                          cv::Mat &frame);
#endif // USE_UDP_MULTICAST
    // Server stuff

#ifdef USE_LIVE555
    // Server stuff
    char                    mServerStarted;
    std::condition_variable mServerInitSuccess; // used to signal when server is up
    std::mutex              mServerCvProt; // protects mServerInitSuccess
    std::thread            *mServerThread;

    // Live streaming stuff
    UsageEnvironment    *mLiveEnv;
    TaskScheduler       *mLiveScheduler;
    RTSPServer          *mRtspServer;
#endif // USE_LIVE555

    std::vector<rcc_streams_info_t> mRccStreams;
};
#endif // __RCC_VIDEO_STREAMER_H
