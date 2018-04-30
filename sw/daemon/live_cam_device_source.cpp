#include <iostream>

#include <GroupsockHelper.hh>
#include <opencv2/opencv.hpp>
#include <opencv2/videoio.hpp>  // Video write

#include "live_cam_device_source.h"



EventTriggerId LiveCamDeviceSource::eventTriggerId = 0;
LiveCamDeviceSource* LiveCamDeviceSource::camDevice = NULL;

LiveCamDeviceSource *LiveCamDeviceSource::createNew(UsageEnvironment &env)
{
    camDevice = new LiveCamDeviceSource(env);
    return camDevice;
}

LiveCamDeviceSource::LiveCamDeviceSource(UsageEnvironment &env)
    : FramedSource(env)
{
    mEncodingVar.resize(0);
    mEncodingVar.push_back(CV_IMWRITE_JPEG_QUALITY);
    mEncodingVar.push_back(70);
    mEncodedBuffer.resize(0);

    if(eventTriggerId == 0)
    {
        eventTriggerId =
            envir().taskScheduler().createEventTrigger(deliverFrame0);
    }
    envir() << "LiveCamDeviceSource::LiveCamDeviceSource()\n";
}

LiveCamDeviceSource::~LiveCamDeviceSource(void)
{
    envir().taskScheduler().deleteEventTrigger(eventTriggerId);
    eventTriggerId = 0;

    envir() << "LiveCamDeviceSource::~LiveCamDeviceSource()\n";
}

void LiveCamDeviceSource::doGetNextFrame(void)
{
    envir() << "doGetNextFrame()\n";
}

void LiveCamDeviceSource::deliverFrame0(void *clientData)
{
    LiveCamDeviceSource *devSource = (LiveCamDeviceSource *)clientData;

    if(devSource == NULL)
    {
        std::cerr << "deliverFrame0(): Client data not available!\n" << std::endl;
        return;
    }

    devSource->deliverFrame();
}

void LiveCamDeviceSource::deliverFrame(void)
{
    // This function is called when new frame data is available from the device.
    // We deliver this data by copying it to the 'downstream' object, using the following parameters (class members):
    // 'in' parameters (these should *not* be modified by this function):
    //     fTo: The frame data is copied to this address.
    //         (Note that the variable "fTo" is *not* modified.  Instead,
    //          the frame data is copied to the address pointed to by "fTo".)
    //     fMaxSize: This is the maximum number of bytes that can be copied
    //         (If the actual frame is larger than this, then it should
    //          be truncated, and "fNumTruncatedBytes" set accordingly.)
    // 'out' parameters (these are modified by this function):
    //     fFrameSize: Should be set to the delivered frame size (<= fMaxSize).
    //     fNumTruncatedBytes: Should be set iff the delivered frame would have been
    //         bigger than "fMaxSize", in which case it's set to the number of bytes
    //         that have been omitted.
    //     fPresentationTime: Should be set to the frame's presentation time
    //         (seconds, microseconds).  This time must be aligned with 'wall-clock time' - i.e., the time that you would get
    //         by calling "gettimeofday()".
    //     fDurationInMicroseconds: Should be set to the frame's duration, if known.
    //         If, however, the device is a 'live source' (e.g., encoded from a camera or microphone), then we probably don't need
    //         to set this variable, because - in this case - data will never arrive 'early'.
    // Note the code below.

    if(!isCurrentlyAwaitingData())
        return;

    envir() << "deliverFrame()\n";

    gettimeofday(&fPresentationTime, NULL);
//    memcpy(fTo, mEncodedBuffer.data(), mEncodedBuffer.size());
    fFrameSize = mEncodedBuffer.size();

    FramedSource::afterGetting(this);
}

void LiveCamDeviceSource::signalNewFrame(void)
{
    envir().taskScheduler().triggerEvent(eventTriggerId, camDevice);
}

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

bool LiveCamDeviceSource::encodeAndStream(cv::Mat &frame)
{
    /* of course protect buffers ;-) */
    cv::imencode(".jpg", frame, mEncodedBuffer, mEncodingVar);

    std::cerr << "encodeAndStream jpeg size = " << mEncodedBuffer.size() << std::endl;
    int fo;
    fo=::open("test.jpg", O_WRONLY | O_CREAT);
    write(fo, mEncodedBuffer.data(), mEncodedBuffer.size());
    ::close(fo);

//    signalNewFrame();
    return true;
}
