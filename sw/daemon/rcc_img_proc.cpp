#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <iostream>
#include <sstream>

#include "rcc_img_proc.h"

#ifdef V4L2_DIRECT_CTRL
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#endif


rccImgProc::rccImgProc(void)
    : m_devName(std::string("/dev/video0")), m_isOpened(false),
      m_fps(-1), m_width(-1), m_height(-1), m_fourcc(-1)
{
#ifdef V4L2_DIRECT_CTRL
    m_devFd    = -1;
    m_v4l2Open = false;
    m_buffers.resize(0);
#endif
}

rccImgProc::~rccImgProc(void)
{

}

bool rccImgProc::open(const char *a_devName)
{
    if(a_devName)
    {
        m_devName = std::string(a_devName);
    }

#ifdef V4L2_DIRECT_CTRL
    if(openV4L2Device(m_devName) && initV4L2Device())
    {
        /* We have V4L2 device, forget about cv::VideoCapture for this run */
        m_isOpened = true;
        return true;
    }

#endif
    m_videoCap.open(m_devName);
    if(!m_videoCap.isOpened())
    {
        std::cerr << "Failed to open OpenCV capture device" << std::endl;
        return false;
    }

    // fill in important info
    m_fps    = m_videoCap.get(cv::CAP_PROP_FPS);
    m_width  = m_videoCap.get(cv::CAP_PROP_FRAME_WIDTH);
    m_height = m_videoCap.get(cv::CAP_PROP_FRAME_HEIGHT);
    m_fourcc = m_videoCap.get(cv::CAP_PROP_FOURCC);
    m_isOpened = true;

    return true;
}

bool rccImgProc::close(void)
{
#ifdef V4L2_DIRECT_CTRL
    closeV4L2Device();
#endif
    m_isOpened = false;
    return true;
}

bool rccImgProc::readFrame(cv::Mat &frame)
{
    if(!isOpened())
    {
        return false;
    }

#ifdef V4L2_DIRECT_CTRL
    if(m_v4l2Open)
    {
        return readV4L2Frame(frame);
    }
#endif

    m_videoCap >> frame;

    return true;
}

void rccImgProc::reset(void)
{
    if(!isOpened()
#ifdef V4L2_DIRECT_CTRL
       // reset not supported on directly streaming V4L2 device
        || m_v4l2Open
#endif
        )
    {
        return;
    }

    m_videoCap.set(CV_CAP_PROP_POS_FRAMES, 0);

    return;
}

#ifdef V4L2_DIRECT_CTRL
int rccImgProc::xioctl(int fd, int request, void *arg)
{
        int r;

        do r = ioctl (fd, request, arg);
        while (-1 == r && EINTR == errno);

        return r;
}

bool rccImgProc::openV4L2Device(std::string a_devName)
{
    // Check if a_devName starts with /dev then open it and check
    // VIDIOC_QUERYCAP
    const std::string devStart("/dev/");
    if(a_devName.compare(0, devStart.length(), devStart) != 0)
    {
        std::cout << "openV4L2Device() not device file: "
                  << a_devName << std::endl;
        return false;
    }

    // it's device - let's open it and check if V4L2
    if(m_devFd > 0)
    {
        closeV4L2Device();
    }

    m_devFd = ::open(a_devName.c_str(), O_RDWR | O_NONBLOCK);
    if(m_devFd < 0)
    {
        std::cerr << "openV4L2Device() Can not open " << a_devName
                  << ": " << strerror(errno) << std::endl;
        // if this fails anyway other means of opening will report error
        return false;
    }

    struct v4l2_capability caps = v4l2_capability();
    if(xioctl(m_devFd, VIDIOC_QUERYCAP, &caps) == -1)
    {
        /* Failed to query capabilities - not V4L */
        std::cerr << "openV4L2Device() VIDIOC_QUERYCAP failed: "
                  << strerror(errno) << std::endl;
        closeV4L2Device();
        return false;
    }

    // we have capabilities, what to check?

    // get format
    struct v4l2_format fmt = v4l2_format();
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(m_devFd, VIDIOC_G_FMT, &fmt) == -1)
    {
        std::cerr << "openV4L2Device() VIDIOC_G_FMT failed: "
                  << strerror(errno) << std::endl;
        closeV4L2Device();
        return false;
    }

    // also FPS
    v4l2_streamparm sp = v4l2_streamparm();
    sp.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(xioctl(m_devFd, VIDIOC_G_PARM, &sp) == -1)
    {
        std::cerr << "openV4L2Device() VIDIOC_G_PARM failed: "
                  << strerror(errno) << std::endl;
        closeV4L2Device();
        return false;
    }

    m_fps = sp.parm.capture.timeperframe.denominator /
        (double)sp.parm.capture.timeperframe.numerator;
    m_width = fmt.fmt.pix.width;
    m_height = fmt.fmt.pix.height;
    m_fourcc = fmt.fmt.pix.pixelformat;

    m_v4l2Open = true;

    return true;
}

bool rccImgProc::initV4L2Device(void)
{
    struct v4l2_requestbuffers req = v4l2_requestbuffers();

    if(!m_v4l2Open)
        return false;

    req.count = 3;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if(xioctl(m_devFd, VIDIOC_REQBUFS, &req))
    {
        std::cerr << "initV4L2Device() VIDIOC_REQBUFS failed: "
                  << strerror(errno) << std::endl;
        return false;
    }

    int numBuffers = req.count;
    std::cout << "initV4L2Device() number of buffers = "
              << numBuffers << std::endl;

    for(int i = 0; i < numBuffers; i++)
    {
        uint8_t *buffer;
        struct v4l2_buffer buf = v4l2_buffer();
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if(xioctl(m_devFd, VIDIOC_QUERYBUF, &buf) == -1)
        {
            std::cerr << "initV4L2Device() VIDIOC_QUERYBUF failed: "
                      << strerror(errno) << std::endl;
            return false;
        }

        buffer = (uint8_t *)mmap (NULL, buf.length,
                                 PROT_READ | PROT_WRITE, MAP_SHARED,
                                 m_devFd, buf.m.offset);
        m_buffers.push_back(buffer);
    }

    for(int i = 0; i < numBuffers; i++)
    {
        struct v4l2_buffer buf = v4l2_buffer();

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if(xioctl(m_devFd, VIDIOC_QBUF, &buf) == -1)
        {
            std::cerr << "initV4L2Device() VIDIOC_QBUF failed: "
                      << strerror(errno) << std::endl;
            return false;
        }
    }

    // Start streaming
    v4l2_buf_type buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(xioctl(m_devFd, VIDIOC_STREAMON, &buf_type) == -1)
    {
        std::cerr << "initV4L2Device() VIDIOC_STREAMON failed: "
                  << strerror(errno) << std::endl;

        return false;
    }


    return true;
}

void rccImgProc::closeV4L2Device(void)
{
    if(m_devFd > 0)
    {
        ::close(m_devFd);
        m_devFd = -1;
    }
    m_v4l2Open = false;
}

bool rccImgProc::readV4L2Frame(cv::Mat &a_frame)
{
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(m_devFd, &fds);
    struct timeval tv;

    v4l2_buffer buf = v4l2_buffer();

    if(!m_v4l2Open)
        return false;

    std::cout << "readV4L2Frame() waiting for select" << std::endl;
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    memset(&tv, 0, sizeof(struct timeval));

    tv.tv_sec = 2;
    int r = select(m_devFd+1, &fds, NULL, NULL, &tv);
    if(-1 == r)
    {
        std::cerr << "readV4L2Frame() select failed: "
                  << strerror(errno) << std::endl;
        return false;
    }

        std::cout << "readV4L2Frame() waiting for DQBUF" << std::endl;
    if(xioctl(m_devFd, VIDIOC_DQBUF, &buf) == -1)
    {
        std::cerr << "readV4L2Frame() VIDIOC_DQBUF failed: "
                  << strerror(errno) << std::endl;
        return false;
    }

     // Here copy received buffer
    // buf.index points to correct buffer
    if(xioctl(m_devFd, VIDIOC_QBUF, &buf) == -1)
    {
        std::cerr << "readV4L2Frame() VIDIOC_QBUF failed: "
                  << strerror(errno) << std::endl;
        return false;
    }

            std::cout << "readV4L2Frame() frame acquired" << std::endl;
    return true;
}

#endif // V4L2_DIRECT_CTRL
