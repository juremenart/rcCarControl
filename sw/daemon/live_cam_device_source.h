#ifndef LIVE_CAM_DEVICE_SOURCE_HH
#define LIVE_CAM_DEVICE_SOURCE_HH

// Requires www.live555.com
#include <FramedSource.hh>

class LiveCamDeviceSource : public FramedSource
{
public:
    static LiveCamDeviceSource *createNew(UsageEnvironment &env);

    virtual void doGetNextFrame();
    virtual unsigned maxFramesize() const
    {
        envir() << "maxFrameSize()\n";
        return 640*480*2;
    }

    virtual Boolean isFramedSource() const
    {
        envir() << "isFramedSource()\n";
        return true;
    };
protected:
    LiveCamDeviceSource(UsageEnvironment &env);
    virtual ~LiveCamDeviceSource();

    static LiveCamDeviceSource *camDevice;
};

#endif // LIVE_CAM_DEVICE_SOURCE_HH
