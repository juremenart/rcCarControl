#include "rcc_video_streamer.h"

rccVideoStreamer::rccVideoStreamer(void)
    : mServerStarted(0), mServerThread(NULL),
      mLiveEnv(NULL), mLiveScheduler(NULL),
      mLiveSink(NULL), mRtspServer(NULL),
      mLiveRtcp(NULL)
{
    mRccStreams.resize(0);
}

rccVideoStreamer::~rccVideoStreamer(void)
{
    stopServer();
}

bool rccVideoStreamer::startServer(int port)
{
    // start server thread now
    mServerThread = new std::thread(&rccVideoStreamer::serverThread, this,
                                    port);

    // Wait for thread to semaphore you success or fail
    std::unique_lock<std::mutex> lk(mServerCvProt);
    mServerInitSuccess.wait(lk, [this]{return mServerStarted;});

    // Should actually not happen - in case of error exception will be thrown
    if(!mServerStarted)
    {
        std::cerr << "Failed to setup video stream server" << std::endl;
        stopServer();
        return false;
    }

    return true;
}

// Stop server also functions as general cleanup method
bool rccVideoStreamer::stopServer(void)
{
    // clean-up everything carefully (this is also cleanup method)
    if(mLiveSink)
    {
        mLiveSink->stopPlaying();
    }

    for(auto it = mRccStreams.begin(); it != mRccStreams.end(); ++it)
    {
        if(it->devSource)
            Medium::close(it->devSource);
    }

    if(mServerThread)
    {
        mServerStarted = 0; // to signal scheduler we want to finish
        mServerThread->join();
        delete mServerThread;
        mServerThread = NULL;
    }

    if(mRtspServer)
    {
        // This is protected destructor - how to mark it NULL?
        //delete mRtspServer;
        mRtspServer = NULL;
    }

    if(mLiveRtcp)
    {
        // This is protected destructor - how to mark it NULL?
        //delete mLiveRtcp;
        mLiveRtcp = NULL;
    }
    if(mLiveSink)
    {
        // This is protected destructor - how to mark it NULL?
        //delete mLiveSink;
        mLiveSink = NULL;
    }
    if(mLiveEnv)
    {
        // This is protected destructor - how to mark it NULL?
        //delete mLiveEnv;
        mLiveEnv = NULL;
    }

    if(mLiveScheduler)
    {
        delete mLiveScheduler;
        mLiveScheduler = NULL;
    }
    return true;
}

bool rccVideoStreamer::isServerStarted(void)
{
    // check all relevant stuff...overkill?
    return (mServerStarted == 1) && mServerThread &&
        mLiveEnv && mLiveScheduler && mRtspServer &&
        mLiveSink;
}

rccVideoStreamer::rcc_stream_id_t rccVideoStreamer::addStream(const char *streamName,
                                                              int fps)
{
    // Add session
    if(!isServerStarted())
    {
        return -1;
    }

    rcc_streams_info_t new_stream;

    new_stream.name = std::string(streamName);
    new_stream.fps = fps;
//    new_stream.devSource = LiveCamDeviceSource::createNew(*mLiveEnv);
    new_stream.devSource = new LiveCamDeviceSource(*mLiveEnv);
    if(new_stream.devSource == NULL)
    {
        std::string t(std::string("Failed to create device source: ")
                      +std::string(mLiveEnv->getResultMsg()));
        std::cerr << t << std::endl;
        throw t;
    }

    const char *descString = "Streamed by RCC video streamer";
    ServerMediaSession *sms = ServerMediaSession::createNew(*mLiveEnv,
                                                            streamName,
                                                            streamName,
                                                            descString,
                                                            True);
    PassiveServerMediaSubsession *subs =
        PassiveServerMediaSubsession::createNew(*mLiveSink, mLiveRtcp);
    sms->addSubsession(subs);

    mRtspServer->addServerMediaSession(sms);

    new_stream.url = std::string(mRtspServer->rtspURL(sms));

    // otherwise start Live scheduler
    mLiveSink->startPlaying(*new_stream.devSource, NULL, mLiveSink);

    *mLiveEnv << "Starting server thread for URL: " <<
        new_stream.url.c_str() << "\n";

    mRccStreams.push_back(new_stream);

    return mRccStreams.size()-1;
}

bool rccVideoStreamer::encodeAndStream(rccVideoStreamer::rcc_stream_id_t stream_id,
                                       cv::Mat &frame)
{
    if(isServerStarted() && ((size_t)stream_id < mRccStreams.size()))
    {
        LiveCamDeviceSource *camDevice = mRccStreams[stream_id].devSource;
        return camDevice->encodeAndStream(camDevice, frame);
    }

    return false;
}

void rccVideoStreamer::serverThread(int port)
{
    const unsigned char ttl = 255;
    Port rtpPort(18888);
    Port rtcpPort(18888+1);

    // if already running keep it that way
    if(isServerStarted())
    {
        std::string t( std::string("Server already running") );
        std::cerr << t << std::endl;
        throw t;
    }

    mLiveScheduler = BasicTaskScheduler::createNew();
    mLiveEnv = BasicUsageEnvironment::createNew(*mLiveScheduler);

    // create groupsocks for RTP & RTCP
    struct in_addr dstAddr;
    dstAddr.s_addr = chooseRandomIPv4SSMAddress(*mLiveEnv);
    // This is multicast address

    Groupsock rtpGroupsock(*mLiveEnv, dstAddr, rtpPort, ttl);
    rtpGroupsock.multicastSendOnly();
    Groupsock rtcpGroupsock(*mLiveEnv, dstAddr, rtcpPort, ttl);
    rtcpGroupsock.multicastSendOnly();

    // Create RTP sink for JPEG (currently only one supported)
    mLiveSink = JPEGVideoRTPSink::createNew(*mLiveEnv, &rtpGroupsock);

    // Create and start RTCP instance for this RTP sink:
    const unsigned estSessionBandwidth = 500;
    const unsigned maxCNAMElen = 100;
    unsigned char CNAME[maxCNAMElen+1];
    gethostname((char *)CNAME, maxCNAMElen);
    CNAME[maxCNAMElen]='\0';

    mLiveRtcp = RTCPInstance::createNew(*mLiveEnv, &rtcpGroupsock,
                                        estSessionBandwidth, CNAME,
                                        mLiveSink, NULL, True);

    // Create RTSP server
    mRtspServer = RTSPServer::createNew(*mLiveEnv, port);
    if(mRtspServer == NULL)
    {
        std::string t( std::string("Failed to create RTSP server: ")+
                       std::string(mLiveEnv->getResultMsg()) );
        std::cerr << t << std::endl;
        throw t;
    }

    {
        std::unique_lock<std::mutex> lk(mServerCvProt);
        mServerStarted = 1;
    }
    mServerInitSuccess.notify_all();

    mLiveEnv->taskScheduler().doEventLoop();
}

