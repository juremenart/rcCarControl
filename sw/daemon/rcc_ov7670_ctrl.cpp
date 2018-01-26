#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>

#include <iostream>
#include <sstream>

#include "rcc_ov7670_ctrl.h"

// TODO: Add logger() here instead of std::cerr

const uint8_t cOv7670SlaveAddr   = 0x21;

rccOv7670Ctrl::rccOv7670Ctrl(uint8_t devNum)
    : rccI2cCtrl(devNum, cOv7670SlaveAddr)
{
}

rccOv7670Ctrl::~rccOv7670Ctrl(void)
{
    close();
}
