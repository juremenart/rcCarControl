#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <chrono>
#include <iostream>

const uint8_t cNumBuffers = 3;

uint8_t *buffer[cNumBuffers];

static int xioctl(int fd, int request, void *arg)
{
        int r;

        do r = ioctl (fd, request, arg);
        while (-1 == r && EINTR == errno);

        return r;
}

int print_caps(int fd)
{
        struct v4l2_capability caps = {};
        if (-1 == xioctl(fd, VIDIOC_QUERYCAP, &caps))
        {
                perror("Querying Capabilities");
                return 1;
        }

        printf( "Driver Caps:\n"
                "  Driver: \"%s\"\n"
                "  Card: \"%s\"\n"
                "  Bus: \"%s\"\n"
                "  Version: %d.%d\n"
                "  Capabilities: %08x\n",
                caps.driver,
                caps.card,
                caps.bus_info,
                (caps.version>>16)&&0xff,
                (caps.version>>24)&&0xff,
                caps.capabilities);


        struct v4l2_fmtdesc fmtdesc;

        memset(&fmtdesc, 0, sizeof(struct v4l2_fmtdesc));

        fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        char fourcc[5] = {0};
        char c, e;
        printf("  FMT : CE Desc\n--------------------\n");
        while (0 == xioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc))
        {
                strncpy(fourcc, (char *)&fmtdesc.pixelformat, 4);

                c = fmtdesc.flags & 1? 'C' : ' ';
                e = fmtdesc.flags & 2? 'E' : ' ';
                printf("  %s: %c%c %s\n", fourcc, c, e, fmtdesc.description);
                fmtdesc.index++;
        }

        struct v4l2_format fmt;

        memset(&fmt, 0, sizeof(struct v4l2_format));

        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        fmt.fmt.pix.width = 640;
        fmt.fmt.pix.height = 480;
        //fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_BGR24;
        //fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_GREY;
        //fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
        fmt.fmt.pix.field = V4L2_FIELD_NONE;

        if (-1 == xioctl(fd, VIDIOC_S_FMT, &fmt))
        {
            perror("Setting Pixel Format");
            return 1;
        }

        strncpy(fourcc, (char *)&fmt.fmt.pix.pixelformat, 4);
        printf( "Selected Camera Mode:\n"
                "  Width: %d\n"
                "  Height: %d\n"
                "  PixFmt: %s\n"
                "  Field: %d\n",
                fmt.fmt.pix.width,
                fmt.fmt.pix.height,
                fourcc,
                fmt.fmt.pix.field);
        return 0;
}

int init_mmap(int fd)
{
    struct v4l2_requestbuffers req = v4l2_requestbuffers();

    req.count = cNumBuffers;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req))
    {
        perror("Requesting Buffer");
        return 1;
    }
    if(cNumBuffers != req.count)
    {
        fprintf(stderr, "Buffer count mismatch %d != %d\n",
                cNumBuffers, req.count);
        return 1;
    }
    for(int i = 0; i < cNumBuffers; i++)
    {
        struct v4l2_buffer buf = v4l2_buffer();
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if(-1 == xioctl(fd, VIDIOC_QUERYBUF, &buf))
        {
            perror("Querying Buffer");
            return 1;
        }

        printf("Buffer length: %d, image length: %d\n", buf.length, buf.bytesused);
        buffer[i] = (uint8_t *)mmap (NULL, buf.length,
                                     PROT_READ | PROT_WRITE, MAP_SHARED,
                                     fd, buf.m.offset);
        printf("Buffer %d addess: %p\n", i, buffer[i]);
    }


    for(int i = 0; i < cNumBuffers; i++)
    {
        struct v4l2_buffer buf = v4l2_buffer();

//    memset(&buf, 0, sizeof(struct v4l2_buffer));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if(-1 == xioctl(fd, VIDIOC_QBUF, &buf))
        {
            perror("Query Buffer");
            return 1;
        }
    }

    // Start streaming
    v4l2_buf_type buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(-1 == xioctl(fd, VIDIOC_STREAMON, &buf_type))
    {
        perror("Start Capture");
        return 1;
    }


    return 0;
}

int capture_image(int fd, int index)
{
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    struct timeval tv;

    v4l2_buffer buf = v4l2_buffer();

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    memset(&tv, 0, sizeof(struct timeval));

    std::chrono::steady_clock::time_point tp1 = std::chrono::steady_clock::now();
    tv.tv_sec = 2;
    int r = select(fd+1, &fds, NULL, NULL, &tv);
    if(-1 == r)
    {
        perror("Waiting for Frame");
        return 1;
    }

    std::chrono::steady_clock::time_point tp2 = std::chrono::steady_clock::now();
    if(-1 == xioctl(fd, VIDIOC_DQBUF, &buf))
    {
        perror("Retrieving Frame");
        return 1;
    }

    std::chrono::steady_clock::time_point tp3 = std::chrono::steady_clock::now();
    // Here copy received buffer

    if(-1 == xioctl(fd, VIDIOC_QBUF, &buf))
    {
        perror("VIDIOC_QBUF");
        return 1;
    }

    std::chrono::steady_clock::time_point tp4 = std::chrono::steady_clock::now();
    std::cout << " SelectTime=" << std::dec <<
        std::chrono::duration_cast<std::chrono::microseconds>(tp2-tp1).count()
              << " AcquireTime=" <<
        std::chrono::duration_cast<std::chrono::microseconds>(tp3-tp2).count()
              << " FullTime=" <<
        std::chrono::duration_cast<std::chrono::microseconds>(tp4-tp1).count()
              << std::endl;

//    IplImage* frame;
//    CvMat cvmat = cvMat(480, 640, CV_8UC3, (void*)buffer);
//    frame = cvDecodeImage(&cvmat, 1);
//    cvNamedWindow("window",CV_WINDOW_AUTOSIZE);
//    cvShowImage("window", frame);
//    cvWaitKey(0);
//    cvSaveImage("image.jpg", frame, 0);

    {
        int fout;
        char fout_str[64];

        sprintf((char *)&fout_str[0], "/tmp/image%02d.raw", index);
        printf("Saving image to: %s\n", fout_str);

        fout = open(fout_str, O_RDWR | O_CREAT);
        if(fout < 0)
        {
            perror("Can not open output file");
            return -1;
        }

        write(fout, buffer[index%cNumBuffers], 640*480*2);

        close(fout);
    }
    return 0;
}

int main(int argc, char *argv[])
{
        int fd;

        if(argc == 1)
        {
            fprintf(stderr, "Usage: %s <video device>\n", argv[0]);
            return -1;
        }

        fd = open(argv[1], O_RDWR | O_NONBLOCK);
        if (fd == -1)
        {
                perror("Opening video device");
                return 1;
        }
        if(print_caps(fd))
            return 1;

        if(init_mmap(fd))
            return 1;
        int i;
        for(i=0; i<20; i++)
        {
            if(capture_image(fd, i))
                return 1;
        }
        close(fd);
        return 0;
}
