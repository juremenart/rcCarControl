#ifndef __RCC_IMG_PROC_H
#define __RCC_IMG_PROC_H

#include <string>

#include "opencv2/opencv.hpp"

class rccImgProc {
private:

public:
    rccImgProc(void);
    ~rccImgProc(void);

    bool open(const char *a_devName = NULL);
    bool isOpened(void) { return (m_videoCap.isOpened()); };
    bool close(void);

    bool readFrame(cv::Mat &frame);
    void reset(void);

    // Temporary exposure of this
    cv::VideoCapture *getVideoDev(void) { return &m_videoCap; };
private:
    std::string m_devName;

    cv::VideoCapture m_videoCap;
};

#endif // __RCC_IMG_PROC_H
