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

#include "rcc_ov2640_ctrl.h"

// TODO: Add logger() here instead of std::cerr

const uint8_t cOv2640SlaveAddr   = 0x30;

rccOv2640Ctrl::rccOv2640Ctrl(uint8_t devNum)
    : rccI2cCtrl(devNum, cOv2640SlaveAddr)
{
}

rccOv2640Ctrl::~rccOv2640Ctrl(void)
{
    close();
}
