#ifndef __RCC_I2C_CTRL_H
#define __RCC_I2C_CTRL_H

#include <vector>

class rccI2cCtrl {
public:
    rccI2cCtrl(uint8_t devNum, uint8_t slaveAddr);
    ~rccI2cCtrl(void);

    int open();
    int close();

    ssize_t write(const std::vector<uint8_t> data);

    ssize_t write(const uint8_t regAddr, const std::vector<uint8_t> data);
    ssize_t write(const uint16_t regAddr, const std::vector<uint8_t> &data);
    ssize_t write(const uint8_t regAddr, uint8_t data)
    {
        return write(regAddr, std::vector<uint8_t>(data));
    };

    ssize_t write(const uint16_t regAddr, uint8_t data)
    {
        return write(regAddr, std::vector<uint8_t>(data));
    };

    ssize_t read(const uint8_t regAddr, std::vector<uint8_t> &data);
    ssize_t read(const uint16_t regAddr, std::vector<uint8_t> &data);

private:
    int     mDevFd;
    uint8_t mDevNum;
    uint8_t mSlaveAddr;
};

#endif // __RCC_PWM_CTRL_H
