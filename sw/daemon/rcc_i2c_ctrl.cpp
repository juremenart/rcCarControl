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

    std::vector<uint8_t> byteAddr;
    byteAddr.push_back(regAddr);


    ssize_t bytes = write(byteAddr);
    if(bytes < 0)
    {
        return -1;
    }

    uint8_t *pData = data.data();
    bytes = ::read(mDevFd, pData, data.size());
    if(bytes < 0)
    {
        std::cerr << "rccI2cCtrl::read() read failed: "
                  << strerror(errno) << std::endl;
        return -1;
    }
    else if(bytes != (ssize_t)data.size())
    {
        std::ostringstream strStream;
        std::cerr << "rccI2cCtrl::read() read bytes not expected: "
                  << data.size() << " != " << bytes << std::endl;
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

    std::vector<uint8_t> byteAddr;
    byteAddr.push_back((regAddr >> 8) & 0xFF);
    byteAddr.push_back((regAddr >> 0) & 0xFF);

    ssize_t bytes = write(byteAddr);
    if(bytes < 0)
    {
        return -1;
    }

    bytes = ::read(mDevFd, &data[0], data.size());
    if(bytes < 0)
    {
        std::cerr << "rccI2cCtrl::read() read failed: "
                  << strerror(errno) << std::endl;
        return -1;
    }
    else if(bytes != (ssize_t)data.size())
    {
        std::ostringstream strStream;
        std::cerr << "rccI2cCtrl::read() read bytes not expected: "
                  << data.size() << " != " << bytes << std::endl;
    }

    return bytes;
}


