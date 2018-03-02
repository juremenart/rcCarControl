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


rccImgProc::rccImgProc(void)
    : m_devName(std::string("/dev/video0"))
{

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

    m_videoCap.open(m_devName);
    if(!m_videoCap.isOpened())
    {
        std::cerr << "Failed to open OpenCV capture device" << std::endl;
        return false;
    }

    return true;
}

bool rccImgProc::close(void)
{
    return true;
}

bool rccImgProc::readFrame(cv::Mat &frame)
{
    if(!isOpened())
    {
        return false;
    }

    m_videoCap >> frame;

    return true;
}
