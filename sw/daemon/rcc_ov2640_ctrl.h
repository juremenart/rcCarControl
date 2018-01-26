#ifndef __RCC_OV2640_CTRL_H
#define __RCC_OV2640_CTRL_H

#include <vector>
#include <string>

#include "rcc_i2c_ctrl.h"

class rccOv2640Ctrl : public rccI2cCtrl {
public:
    rccOv2640Ctrl(uint8_t devNum = 0);
    ~rccOv2640Ctrl(void);

private:
};

#endif // __RCC_PWM_CTRL_H
