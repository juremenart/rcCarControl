#ifdef USE_LIVE555

#include <iostream>

#include <GroupsockHelper.hh>

#include <opencv2/videoio.hpp>  // Video write

#include "live_cam_device_source.h"



//unsigned LiveCamDeviceSource::refCount = 0;

//LiveCamDeviceSource *LiveCamDeviceSource::createNew(UsageEnvironment &env)
//{
//    return new LiveCamDeviceSource(env);
//}

LiveCamDeviceSource::LiveCamDeviceSource(UsageEnvironment &env)
    : FramedSource(env), eventTriggerId(0)
{
    mEncodingVar.resize(0);
    mEncodingVar.push_back(CV_IMWRITE_JPEG_QUALITY);
    mEncodingVar.push_back(cQFactor);

    mEncodedBuffer.resize(0);

    mJpegFrameParser = new JpegFrameParser();

    if(eventTriggerId == 0)
    {
        eventTriggerId =
            envir().taskScheduler().createEventTrigger(deliverFrame0);
//        std::cout << "Got new trigger ID: " << std::hex << eventTriggerId << std::endl;
    }
}

LiveCamDeviceSource::~LiveCamDeviceSource(void)
{
    envir().taskScheduler().deleteEventTrigger(eventTriggerId);
    eventTriggerId = 0;

    delete mJpegFrameParser;
}

void LiveCamDeviceSource::doGetNextFrame(void)
{
//    envir() << "doGetNextFrame()\n";
}

void LiveCamDeviceSource::deliverFrame0(void *clientData)
{
    LiveCamDeviceSource *devSource = (LiveCamDeviceSource *)clientData;

    if(devSource == NULL)
    {
        std::cerr << "deliverFrame0(): Client data not available!\n" << std::endl;
        return;
    }

//    std::cout << "Delivering frame for device 0x" << std::hex << (uint64_t)devSource << std::endl;
    devSource->deliverFrame();
}

static std::chrono::steady_clock::time_point lastTp;

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
    {
//        std::cout << "!isCurrentlAwaitingData()" << std::endl;
        return;
    }
//    std::chrono::steady_clock::time_point tp = std::chrono::steady_clock::now();
//
//    std::cerr << "deliverFrame() delay= " <<
//        std::chrono::duration_cast<std::chrono::microseconds>(tp-lastTp).count()
//              << " [us]" << std::endl;
//
//    lastTp = tp;


    unsigned int jpeg_length;
    unsigned char const *scan_data;

    {
        std::lock_guard<std::mutex> guard(mBufferProt);

        // TODO: Estimate if we want to lock here or for parsing or rather copy
        // the buffer to estimated one

//        std::cout << "Reading buffer from 0x" << std::hex << (uint64_t)mEncodedBuffer.data() << std::endl;
        if(mJpegFrameParser->parse(mEncodedBuffer.data(), mEncodedBuffer.size()) < 0)
        {
            std::cerr << "JPEG Frame parser failed!" << std::endl;
        }
        scan_data = mJpegFrameParser->scandata(jpeg_length);

        fFrameSize = jpeg_length;

        // Deliver the data here:
        if (jpeg_length > fMaxSize) {
            fFrameSize = fMaxSize;
            fNumTruncatedBytes = jpeg_length - fMaxSize;
        }

        memmove(fTo, scan_data, fFrameSize);
    }

    mLastQFactor = mJpegFrameParser->qFactor();
    mLastWidth   = mJpegFrameParser->width();
    mLastHeight  = mJpegFrameParser->height();
    mType        = mJpegFrameParser->type();

    gettimeofday(&fPresentationTime, NULL);

    FramedSource::afterGetting(this);
}

void LiveCamDeviceSource::signalNewFrame(LiveCamDeviceSource *camDevice)
{
//    std::cout << "Signaling new from for device 0x" << std::hex <<
//        (uint64_t)camDevice << " with event ID: " << eventTriggerId << std::endl;
    envir().taskScheduler().triggerEvent(eventTriggerId, camDevice);
}

bool LiveCamDeviceSource::encodeAndStream(LiveCamDeviceSource *camDevice,
                                          cv::Mat &frame)
{
    {
        std::lock_guard<std::mutex> guard(mBufferProt);
        /* of course protect buffers ;-) */
        cv::imencode(".jpg", frame, mEncodedBuffer, mEncodingVar);
//        std::cout << "Encoding for device 0x" << std::hex << (uint64_t)camDevice
//                  << " in buffer 0x" << (uint64_t)mEncodedBuffer.data() << std::endl;
    }

    signalNewFrame(camDevice);
    return true;
}

u_int8_t const* LiveCamDeviceSource::quantizationTable(u_int8_t& precision,
                                                       u_int16_t& length)
{
    precision = mJpegFrameParser->precision();
    return mJpegFrameParser->quantizationTables(length);
}


#endif // USE_LIVE555
