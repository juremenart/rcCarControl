#ifndef LIVE_CAM_DEVICE_SOURCE_HH
#define LIVE_CAM_DEVICE_SOURCE_HH

// Requires www.live555.com
#include <FramedSource.hh>

class LiveCamDeviceSource : public FramedSource
{
public:
    static LiveCamDeviceSource *createNew(UsageEnvironment &env);

    virtual void doGetNextFrame();

    void signalNewFrame(void);
    bool encodeAndStream(cv::Mat &frame);

private:
    static void deliverFrame0(void* clientData);
    void deliverFrame();

protected:
    LiveCamDeviceSource(UsageEnvironment &env);
    virtual ~LiveCamDeviceSource();

    static EventTriggerId eventTriggerId;
    static LiveCamDeviceSource *camDevice;

    std::vector<int> mEncodingVar;
    std::vector<uchar> mEncodedBuffer;
};

#endif // LIVE_CAM_DEVICE_SOURCE_HH
