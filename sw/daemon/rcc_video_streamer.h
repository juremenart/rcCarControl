#ifndef __RCC_VIDEO_STREAMER_H
#define __RCC_VIDEO_STREAMER_H

#include <thread>
#include <condition_variable>

// Live555 includes
#include <liveMedia.hh>
#include <BasicUsageEnvironment.hh>
#include <GroupsockHelper.hh>

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
        std::string          url;
        int                  fps;
        LiveCamDeviceSource *devSource;
    } rcc_streams_info_t;

public:
    typedef int rcc_stream_id_t;

    rccVideoStreamer(void);
    ~rccVideoStreamer(void);

    bool            startServer(int port);
    bool            stopServer(void);
    bool            isServerStarted(void);
    rcc_stream_id_t addStream(const char *streamName, int fps);

    bool encodeAndStream(rccVideoStreamer::rcc_stream_id_t stream_id,
                         cv::Mat &frame);
private:
    void serverThread(int port);

    // Server stuff
    char                    mServerStarted;
    std::condition_variable mServerInitSuccess; // used to signal when server is up
    std::mutex              mServerCvProt; // protects mServerInitSuccess
    std::thread            *mServerThread;

    // Live streaming stuff
    UsageEnvironment    *mLiveEnv;
    TaskScheduler       *mLiveScheduler;
    RTPSink             *mLiveSink;
    RTSPServer          *mRtspServer;
    RTCPInstance        *mLiveRtcp;

    std::vector<rcc_streams_info_t> mRccStreams;
};
#endif // __RCC_VIDEO_STREAMER_H
