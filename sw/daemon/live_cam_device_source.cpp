#include <GroupsockHelper.hh>

#include "live_cam_device_source.h"


LiveCamDeviceSource* LiveCamDeviceSource::camDevice = NULL;

LiveCamDeviceSource *LiveCamDeviceSource::createNew(UsageEnvironment &env)
{
    camDevice = new LiveCamDeviceSource(env);
    return camDevice;
}

LiveCamDeviceSource::LiveCamDeviceSource(UsageEnvironment &env)
    : FramedSource(env)
{

}

LiveCamDeviceSource::~LiveCamDeviceSource(void)
{

}

void LiveCamDeviceSource::doGetNextFrame(void)
{
    envir() << "doGetNextFrame()\n";
}
