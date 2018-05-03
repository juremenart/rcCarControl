TARGETS=rcc_daemon drive_ctrl setup_ov5642 access_ov5642 access_ov7670 access_ov2640 access_video_ctrl access_vdma_ctrl access_sys_ctrl capture_video

HEADERS=rcc_logger.h rcci_server.h rcc_sys_ctrl.h rcc_i2c_ctrl.h rcc_ov5642_ctrl.h ov5642_720p_init.h ov5642_vga_yuv_init.h rcc_ov7670_ctrl.h rcc_ov2640_ctrl.h rcc_video_ctrl.h rcc_vdma_ctrl.h rcc_img_proc.h live_cam_device_source.h JpegFrameParser.hh

SOURCES=rcc_logger.cpp rcci_server.cpp rcc_sys_ctrl.cpp rcc_i2c_ctrl.cpp rcc_ov5642_ctrl.cpp rcc_ov7670_ctrl.cpp rcc_ov2640_ctrl.cpp rcc_video_ctrl.cpp rcc_vdma_ctrl.cpp rcc_img_proc.cpp live_cam_device_source.cpp JpegFrameParser.cpp



EXT_HEADERS=../interface/rcci_type.h
EXT_SOURCES=

THIS_DIR:=$(dir $(abspath $(lastword $(MAKEFILE_LIST))))

include ../Makefile.core
