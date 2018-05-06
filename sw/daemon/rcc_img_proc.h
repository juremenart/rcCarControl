#ifndef __RCC_IMG_PROC_H
#define __RCC_IMG_PROC_H

#include <string>
#include <ctime>

#include "opencv2/opencv.hpp"

// Define if want to use V4L2 driver handling directly w/o
// slow OpenCV VideoCapture overhead - recommended for RCC ;-)
#define V4L2_DIRECT_CTRL

class rccImgProc {
public:
    rccImgProc(void);
    ~rccImgProc(void);

    bool open(const char *a_devName = NULL);
    bool isOpened(void) { return m_isOpened; };
    bool close(void);

    bool readFrame(cv::Mat &frame);
    void reset(void);

    int getFps(void)    { return m_fps; };
    int getWidth(void)  { return m_width; };
    int getHeight(void) { return m_height; };
    int getFourcc(void) { return m_fourcc; };

private:
#ifdef V4L2_DIRECT_CTRL
    int xioctl(int fd, int request, void *arg);
    bool openV4L2Device(std::string a_devName);
    bool initV4L2Device(void);
    void closeV4L2Device(void);
    bool readV4L2Frame(cv::Mat &a_frame, struct timeval &a_timestamp);
#endif

private:
    std::string      m_devName;
    bool             m_isOpened;
    int              m_fps, m_width, m_height, m_fourcc;

    cv::VideoCapture m_videoCap;

#ifdef V4L2_DIRECT_CTRL
    bool                   m_v4l2Open; // if True then m_videoCap is not valid
    int                    m_devFd;
    std::vector<uint8_t *> m_buffers; // frame buffers
#endif
};

#endif // __RCC_IMG_PROC_H
