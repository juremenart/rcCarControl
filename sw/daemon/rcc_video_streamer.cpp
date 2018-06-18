#include <unistd.h>

#include "rcci_type.h"
#include "rcc_video_streamer.h"

rccVideoStreamer::rccVideoStreamer(void)
#ifdef USE_LIVE555
    : mServerStarted(0), mServerThread(NULL),
      mLiveEnv(NULL), mLiveScheduler(NULL), mRtspServer(NULL)
#endif // USE_LIVE555
{
    mRccStreams.resize(0);
}

rccVideoStreamer::~rccVideoStreamer(void)
{
    stopServer();
}

bool rccVideoStreamer::startServer(int port)
{
#ifdef USE_LIVE555
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
#else // USE_LIVE555
    (void)port;
#endif // USE_LIVE555
    return true;
}

// Stop server also functions as general cleanup method
bool rccVideoStreamer::stopServer(void)
{
    // clean-up everything carefully (this is also cleanup method)
//    if(mLiveSink)
//    {
//        mLiveSink->stopPlaying();
//    }

    for(auto it = mRccStreams.begin(); it != mRccStreams.end(); ++it)
    {
#ifdef USE_LIVE555
        if(it->devSource)
            Medium::close(it->devSource);
        closeMulticastSocket(it->sock);
#endif // USE_LIVE555
    }
    mRccStreams.resize(0);

#ifdef USE_LIVE555
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
#endif // USE_LIVE555

    return true;
}

bool rccVideoStreamer::isServerStarted(void)
{
#ifdef USE_LIVE555
    // check all relevant stuff...overkill?
    return (mServerStarted == 1) && mServerThread
        && mLiveEnv && mLiveScheduler && mRtspServer;
#endif // USE_LIVE555
#ifdef USE_UDP_MULTICAST
    return (mRccStreams.size() > 0) ? true : false;
#endif
}


rccVideoStreamer::rcc_stream_id_t rccVideoStreamer::addStream(const char *streamName,
                                                              int fps, int port)
{
    rcc_streams_info_t new_stream;

    new_stream.name = std::string(streamName);
    new_stream.fps = fps;

#ifdef USE_LIVE555
    const unsigned char ttl = 255;
    Port rtpPort(18888);
    Port rtcpPort(18888+1);

    // Add session
    if(!isServerStarted())
    {
        return -1;
    }

//    new_stream.devSource = LiveCamDeviceSource::createNew(*mLiveEnv);
    new_stream.devSource = new LiveCamDeviceSource(*mLiveEnv);
    if(new_stream.devSource == NULL)
    {
        std::string t(std::string("Failed to create device source: ")
                      +std::string(mLiveEnv->getResultMsg()));
        std::cerr << t << std::endl;
        throw t;
    }

    // create groupsocks for RTP & RTCP
    struct in_addr dstAddr;
    dstAddr.s_addr = chooseRandomIPv4SSMAddress(*mLiveEnv);
    // This is multicast address

    Groupsock rtpGroupsock(*mLiveEnv, dstAddr, rtpPort, ttl);
    rtpGroupsock.multicastSendOnly();
    Groupsock rtcpGroupsock(*mLiveEnv, dstAddr, rtcpPort, ttl);
    rtcpGroupsock.multicastSendOnly();

    // Create RTP sink for JPEG (currently only one supported)
    RTPSink *mLiveSink = JPEGVideoRTPSink::createNew(*mLiveEnv, &rtpGroupsock);

    // Create and start RTCP instance for this RTP sink:
    const unsigned estSessionBandwidth = 500;
    const unsigned maxCNAMElen = 100;
    unsigned char CNAME[maxCNAMElen+1];
    gethostname((char *)CNAME, maxCNAMElen);
    CNAME[maxCNAMElen]='\0';

    RTCPInstance *mLiveRtcp = RTCPInstance::createNew(*mLiveEnv, &rtcpGroupsock,
                                                      estSessionBandwidth, CNAME,
                                                      mLiveSink, NULL, True);

    // Create RTSP server
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


#endif // USE_LIVE555

#ifdef USE_UDP_MULTICAST
    new_stream.port = port;
    new_stream.sock = openMulticastSocket();
    if(new_stream.sock < 0)
    {
        return -1;
    }

    memset(&new_stream.addr, 0, sizeof(struct sockaddr_in));
    new_stream.addr.sin_family      = PF_INET;
    new_stream.addr.sin_addr.s_addr = htonl(INADDR_ANY);
//    new_stream.addr.sin_addr.s_addr = inet_addr("226.0.0.1");
    new_stream.addr.sin_port        = htons(0);

    if(bind(new_stream.sock, (struct sockaddr *)&new_stream.addr,
            sizeof(struct sockaddr_in)) < 0)
    {
        std::cerr << "bind() failed: " << strerror(errno) << std::endl;
        return -1;
    }

    struct in_addr iaddr;
    iaddr.s_addr = INADDR_ANY; // use DEFAULT interface

    // Set the outgoing interface to DEFAULT
    setsockopt(new_stream.sock, IPPROTO_IP, IP_MULTICAST_IF, &iaddr,
               sizeof(struct in_addr));

    // Set multicast packet TTL to 3; default TTL is 1
    unsigned char ttl = 3;
    setsockopt(new_stream.sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl,
               sizeof(unsigned char));

    new_stream.addr.sin_family      = PF_INET;
    new_stream.addr.sin_addr.s_addr = inet_addr("226.0.0.1");
    new_stream.addr.sin_port        = htons(port);

    std::cout << "New multicast stream '" << new_stream.name
              << "' opened at port: " << port << std::endl;

#endif // USE_UDP_MULTICAST

    mRccStreams.push_back(new_stream);

    return mRccStreams.size()-1;
}

bool rccVideoStreamer::encodeAndStream(rccVideoStreamer::rcc_stream_id_t stream_id,
                                       cv::Mat &frame)
{
    if(isServerStarted() && ((size_t)stream_id < mRccStreams.size()))
    {
#ifdef USE_LIVE555
        LiveCamDeviceSource *camDevice = mRccStreams[stream_id].devSource;
        return camDevice->encodeAndStream(camDevice, frame);
#endif // USE_LIVE555
#ifdef USE_UDP_MULTICAST
        return (sendMulticastData(stream_id, frame) > 0) ? true : false;
#endif // USE_UDP_MULTICAST
    }

    return false;
}

#ifdef USE_LIVE555
void rccVideoStreamer::serverThread(int port)
{
    // if already running keep it that way
    if(isServerStarted())
    {
        std::string t( std::string("Server already running") );
        std::cerr << t << std::endl;
        throw t;
    }

    mLiveScheduler = BasicTaskScheduler::createNew();
    mLiveEnv = BasicUsageEnvironment::createNew(*mLiveScheduler);

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
#endif // USE_LIVE555


#ifdef USE_UDP_MULTICAST
int rccVideoStreamer::openMulticastSocket(void)
{
    int fd;

    if((fd=socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        std::string t(std::string("Failed to open new socket: ")+
                      std::string(strerror(errno)));
        std::cerr << t << std::endl;
        return -1;
    }

    return fd;
}

int rccVideoStreamer::closeMulticastSocket(int sock)
{
    ::shutdown(sock, 2);
    return ::close(sock);
}

int rccVideoStreamer::sendMulticastData(rccVideoStreamer::rcc_stream_id_t stream_id,
                                        cv::Mat &frame)
{
    rcci_msg_vframe_t videoFrame;

    std::vector<uchar> encodedBuffer;
    static std::vector<int> encodingVar;
    const u_int8_t cQFactor = 70;
    int retVal;

    // TODO: Change this horrible instantiation to class member :)
    static uint8_t frameCnt = 0;

    if((size_t)stream_id >= mRccStreams.size())
    {
        return -1;
    }

    if(encodingVar.size() > 0)
    {
        encodingVar.resize(0);
        encodingVar.push_back(CV_IMWRITE_JPEG_QUALITY);
        encodingVar.push_back(cQFactor);
    }

    cv::imencode(".jpg", frame, encodedBuffer, encodingVar);

    videoFrame.header.magic = rcci_msg_init_magic;

    if(encodedBuffer.size() > rcci_msg_vframe_max_frame)
    {
        std::cerr << "Buffer not large enough!" << std::endl;
        return -1;
    }

    uint32_t msgCounter = 0;
    const int maxMsgLength = rcci_msg_vframe_max_packet_size; // max one for UDP
    const int maxFrameLength = maxMsgLength - rcci_msg_vframe_header_size;
    uint8_t *frameBuffer = encodedBuffer.data();
    uint8_t  cnt_msg = 0;
    //memcpy(&videoFrame.frame[0], encodedBuffer.data(), encodedBuffer.size());
    videoFrame.header.size = encodedBuffer.size();
    videoFrame.cnt_frame = frameCnt++;
    videoFrame.all_msgs = ((videoFrame.header.size+maxMsgLength) / maxMsgLength);

    while(msgCounter < videoFrame.header.size)
    {
        int sendBytes =
            (videoFrame.header.size - msgCounter) + rcci_msg_vframe_header_size;
        if(sendBytes > maxMsgLength)
        {
            sendBytes = maxMsgLength;
        }

        videoFrame.size_frame = sendBytes - rcci_msg_vframe_header_size;
        videoFrame.idx_frame = msgCounter;
        videoFrame.cur_msg = cnt_msg++;

//        std::cout << "Copying from " << msgCounter << " bytes " <<
//            sendBytes-rcci_msg_vframe_header_size << std::endl;
//        std::cout << "Sending out " << (int)sendBytes << " bytes" <<
//            " cur_msg=" << (int)videoFrame.cur_msg << std::endl;

        memcpy(&videoFrame.frame[0], (void *)&frameBuffer[msgCounter],
               videoFrame.size_frame);

        retVal = ::sendto(mRccStreams[stream_id].sock,
//                          (void *)&videoFrame, videoFrame.header.size, 0,
                          (void *)&videoFrame, sendBytes, 0,
                          (struct sockaddr *)&mRccStreams[stream_id].addr,
                          sizeof(mRccStreams[stream_id].addr));

        msgCounter += sendBytes - rcci_msg_vframe_header_size;

        if(retVal < 0)
        {
            std::cerr << "sendto() failed: " << strerror(errno) << std::endl;
            msgCounter = retVal;
            break;
        }
        if(retVal != sendBytes)
        {
            std::cerr << "sendto() wrong return: " << sendBytes << " != " <<
                retVal << std::endl;
        }
    }

    std::cout << "Sending new frame stream_id=" << stream_id << " numStreams="
              << mRccStreams.size()  << " frame size=" << videoFrame.header.size
              << " ret val=" << msgCounter << std::endl;

    return (int)msgCounter;
}

#endif // USE_UDP_MULTICAST
