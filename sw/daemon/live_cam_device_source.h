#ifndef LIVE_CAM_DEVICE_SOURCE_HH
#define LIVE_CAM_DEVICE_SOURCE_HH

// Requires www.live555.com
#include <FramedSource.hh>

class LiveCamDeviceSource : public FramedSource
{
public:
    static LiveCamDeviceSource *createNew(UsageEnvironment &env);

    virtual void doGetNextFrame();
protected:
    LiveCamDeviceSource(UsageEnvironment &env);
    virtual ~LiveCamDeviceSource();

    static LiveCamDeviceSource *camDevice;
};

#endif // LIVE_CAM_DEVICE_SOURCE_HH
