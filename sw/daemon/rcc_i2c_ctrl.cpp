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

#include "rcc_i2c_ctrl.h"


// TODO: Add logger() here instead of std::cerr

// TODO: Add why burst read is not working? Nice example to test is below.
// I checked also with scope and noticed that indeed the values returned from
// the sensor are all '1' so it's not SW problem.
//
//snickerdoodle@snickerdoodle:~/rcCarControl/sw$ sudo ./bin/access_ov5642 -r 0x3009 3
//I2C device opened and slave address set to : 0x3c
//Received data:
//   Reg=0x3009   Value=1
//   Reg=0x300a   Value=1
//   Reg=0x300b   Value=1
//snickerdoodle@snickerdoodle:~/rcCarControl/sw$ sudo ./bin/access_ov5642 -r 0x300a 3
//I2C device opened and slave address set to : 0x3c
//Received data:
//   Reg=0x300a   Value=56
//   Reg=0x300b   Value=42
//   Reg=0x300c   Value=1

rccI2cCtrl::rccI2cCtrl(uint8_t devNum, uint8_t slaveAddr)
    : mDevFd(-1), mDevNum(devNum), mSlaveAddr(slaveAddr)
{
}

rccI2cCtrl::~rccI2cCtrl(void)
{
    close();
}

int rccI2cCtrl::open()
{
    std::ostringstream strStream;
    if(mDevFd > 0)
    {
        close();
    }

    std::string devFilename(std::string("/dev/i2c-")+
                            std::string(std::to_string(mDevNum)));

    mDevFd = ::open(devFilename.c_str(), O_RDWR);
    if(mDevFd < 0)
    {
        std::cerr << "Failed to open " << devFilename << ": " <<
            strerror(errno) << std::endl;
        return -1;
    }

    if(ioctl(mDevFd, I2C_SLAVE, mSlaveAddr) < 0)
    {
        std::cerr << "Failed to set I2C slave address 0x" << std::hex <<
            mSlaveAddr << ": " << strerror(errno) << std::endl;
        close();
        return -1;
    }

    std::cout << "I2C device opened and slave address set to : 0x"
              << std::hex << (int)mSlaveAddr << std::endl;

    return mDevFd;
}

int rccI2cCtrl::close()
{
    if(mDevFd > 0)
    {
        ::close(mDevFd);
        mDevFd = -1;
    }

    return 0;
}

ssize_t rccI2cCtrl::write(const std::vector<uint8_t> data)
{
    ssize_t bytes;

    if(mDevFd <= 0)
    {
        std::cerr << "rccI2cCtrl::write() I2C module not initialized"
                  << std::endl;
        return -1;
    }

    bytes = ::write(mDevFd, data.data(), data.size());
    if(bytes < 0)
    {
        std::cerr << "rccI2cCtrl::write() write failed: "
                  << strerror(errno) << std::endl;
        return -1;
    }

    return bytes;
}

ssize_t rccI2cCtrl::write(const uint8_t regAddr,
                          const std::vector<uint8_t> data)
{
    if(mDevFd <= 0)
    {
        std::cerr << "rccI2cCtrl::write() I2C module not initialized"
                  << std::endl;
        return -1;
    }

    std::vector<uint8_t> wData;
    wData.push_back(regAddr);

    for(int i = 0; i < (int)data.size(); i++)
    {
        wData.push_back(data[i]);
    }
    ssize_t bytes = write(wData);

    return bytes;
}

ssize_t rccI2cCtrl::read(const uint8_t regAddr,
                         std::vector<uint8_t> &data)
{
    if(mDevFd <= 0)
    {
        std::cerr << "rccI2cCtrl::read() I2C module not initialized"
                  << std::endl;
        return -1;
    }

    // stupid but it seems that burst read has some problems on OV5642
    int bytes = 0;

    for(int i = 0; i < (int)data.size(); i++)
    {
        std::vector<uint8_t> byteAddr;
        byteAddr.push_back(regAddr+i);
        ssize_t wBytes = write(byteAddr);
        if(wBytes < 0)
        {
            return -1;
        }
        bytes += ::read(mDevFd, &data[i], 1);
        if(bytes <= 0)
        {
            std::cerr << "rccI2cCtrl::read() read failed: "
                      << strerror(errno) << std::endl;
            return -1;
        }
    }

    return bytes;
}

ssize_t rccI2cCtrl::write(const uint16_t regAddr,
                          const std::vector<uint8_t> &data)
{
    if(mDevFd <= 0)
    {
        std::cerr << "rccI2cCtrl::write() I2C module not initialized"
                  << std::endl;
        return -1;
    }

    std::vector<uint8_t> wData;
    wData.push_back((regAddr >> 8) & 0xFF);
    wData.push_back((regAddr >> 0) & 0xFF);

    for(int i = 0; i < (int)data.size(); i++)
    {
        wData.push_back(data[i]);
    }
    ssize_t bytes = write(wData);

    return bytes;
}

ssize_t rccI2cCtrl::read(const uint16_t regAddr,
                         std::vector<uint8_t> &data)
{
    if(mDevFd <= 0)
    {
        std::cerr << "rccI2cCtrl::read() I2C module not initialized"
                  << std::endl;
        return -1;
    }

    // stupid but it seems that burst read has some problems on OV5642
    int bytes = 0;

    for(int i = 0; i < (int)data.size(); i++)
    {
        std::vector<uint8_t> byteAddr;

        byteAddr.push_back(((regAddr+i) >> 8) & 0xFF);
        byteAddr.push_back(((regAddr+i) >> 0) & 0xFF);

        ssize_t wBytes = write(byteAddr);
        if(wBytes < 0)
        {
            return -1;
        }
        bytes += ::read(mDevFd, &data[i], 1);
        if(bytes <= 0)
        {
            std::cerr << "rccI2cCtrl::read() read failed: "
                      << strerror(errno) << std::endl;
            return -1;
        }
    }

    return bytes;
}


