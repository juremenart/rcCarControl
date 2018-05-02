#ifndef LIVE_CAM_DEVICE_SOURCE_H
#define LIVE_CAM_DEVICE_SOURCE_H

// Requires www.live555.com
#include <FramedSource.hh>
#include <JPEGVideoSource.hh>

#include <mutex>
#include <opencv2/opencv.hpp>

class LiveCamDeviceSource : public JPEGVideoSource //FramedSource
{
public:
    static LiveCamDeviceSource *createNew(UsageEnvironment &env);

    bool encodeAndStream(cv::Mat &frame);

private:
    const u_int8_t cQFactor = 70;

    virtual void doGetNextFrame();

    static void deliverFrame0(void* clientData);
    void deliverFrame();

    void signalNewFrame(void);


    virtual u_int8_t type() { return 0; };
    virtual u_int8_t qFactor() { return cQFactor; };
    virtual u_int8_t width() { return 640/8; };
    virtual u_int8_t height() { return 480/8; };

//    virtual u_int8_t width() { return 256/8; };
//    virtual u_int8_t height() { return 240/8; };


protected:
    LiveCamDeviceSource(UsageEnvironment &env);
    virtual ~LiveCamDeviceSource();

    static EventTriggerId eventTriggerId;
    static LiveCamDeviceSource *camDevice;

    std::vector<int> mEncodingVar;
    std::vector<uchar> mEncodedBuffer;

    std::mutex mBufferProt;
};

#endif // LIVE_CAM_DEVICE_SOURCE_H
