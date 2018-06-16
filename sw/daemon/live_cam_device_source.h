#ifndef LIVE_CAM_DEVICE_SOURCE_H
#define LIVE_CAM_DEVICE_SOURCE_H

#ifdef USE_LIVE555

// Requires www.live555.com
#include <FramedSource.hh>

#include <mutex>
#include <opencv2/opencv.hpp>

#include "JpegFrameParser.hh"

class LiveCamDeviceSource : public FramedSource
{
public:
//    static LiveCamDeviceSource *createNew(UsageEnvironment &env);
    LiveCamDeviceSource(UsageEnvironment &env);
    virtual ~LiveCamDeviceSource();

    bool encodeAndStream(LiveCamDeviceSource *camDevice, cv::Mat &frame);

private:

    virtual void doGetNextFrame();

    static void deliverFrame0(void* clientData);
    void deliverFrame();

    void signalNewFrame(LiveCamDeviceSource *camDevice);

    // needed if using JPEG compression
    const u_int8_t cQFactor = 70;
    JpegFrameParser *mJpegFrameParser;

    virtual u_int8_t type() { return mType; };
    virtual u_int8_t qFactor() { return mLastQFactor; };
    virtual u_int8_t width() { return mLastWidth; };
    virtual u_int8_t height() { return mLastHeight; };
    virtual u_int8_t const *quantizationTable(u_int8_t &precision,
                                              u_int16_t &length);
    virtual Boolean isJPEGVideoSource() const { return True; };
    u_int8_t mType, mLastQFactor, mLastWidth, mLastHeight;
    u_int8_t mJPEGHeader[JPEG_HEADER_SIZE];

protected:

    EventTriggerId eventTriggerId;

    std::vector<int> mEncodingVar;
    std::vector<uchar> mEncodedBuffer;

    std::mutex mBufferProt;
};

#endif // USE_LIVE555

#endif // LIVE_CAM_DEVICE_SOURCE_H
