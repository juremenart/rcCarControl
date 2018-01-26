#ifndef __RCC_OV5642_CTRL_H
#define __RCC_OV5642_CTRL_H

#include <vector>
#include <string>

#include "rcc_i2c_ctrl.h"

class rccOv5642Ctrl : public rccI2cCtrl {
private:
    const uint16_t cChipIdAddr = 0x300A;
    const uint8_t  cChipIdMsb  = 0x56;
    const uint8_t  cChipIdLsb  = 0x42;

public:
    typedef struct ov5642_init_s {
        uint16_t regAddr;
        uint8_t  regValue;
    } ov5642_init_t;
    typedef std::vector<ov5642_init_t> ov5642_init_vect_t;

    // Indexes must match the table cOv5642ModeTable;
    typedef enum ov5642_supp_mode_e {
        ov5642_720p_video = 0,
        ov5642_vga_yuv    = 1,
        ov5642_mode_nonexisting // must be last
    } ov5642_mode_t;

    typedef struct ov5642_mode_entry_s {
        bool                valid;
        ov5642_init_vect_t *pInitTable;
        std::string         shortDesc;
    } ov5642_mode_entry_t;
    typedef std::vector<ov5642_mode_entry_t> ov5642_mode_table_t;

public:
    rccOv5642Ctrl(uint8_t devNum = 0);
    ~rccOv5642Ctrl(void);

    bool init(ov5642_mode_t mode = ov5642_720p_video);
private:
};

#endif // __RCC_PWM_CTRL_H
