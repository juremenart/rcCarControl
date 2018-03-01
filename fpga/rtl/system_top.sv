//Copyright 1986-2017 Xilinx, Inc. All Rights Reserved.
//--------------------------------------------------------------------------------
//Tool Version: Vivado v.2017.3 (lin64) Build 2018833 Wed Oct  4 19:58:07 MDT 2017
//Date        : Fri Dec 22 08:53:28 2017
//Host        : menart-VirtualBox running 64-bit Ubuntu 17.10
//Command     : generate_target bd_system_wrapper.bd
//Design      : bd_system_wrapper
//Purpose     : IP block netlist
//--------------------------------------------------------------------------------
`timescale 1 ps / 1 ps

module bd_system_wrapper
  (DDR_addr,
   DDR_ba,
   DDR_cas_n,
   DDR_ck_n,
   DDR_ck_p,
   DDR_cke,
   DDR_cs_n,
   DDR_dm,
   DDR_dq,
   DDR_dqs_n,
   DDR_dqs_p,
   DDR_odt,
   DDR_ras_n,
   DDR_reset_n,
   DDR_we_n,
   FIXED_IO_ddr_vrn,
   FIXED_IO_ddr_vrp,
   FIXED_IO_mio,
   FIXED_IO_ps_clk,
   FIXED_IO_ps_porb,
   FIXED_IO_ps_srstb,
   pwm0,
   pwm1,
   pwm0_rf,
   pwm1_rf,
   cam_iic_scl_io,
   cam_iic_sda_io,
   cam_pclk_i,
   cam_href_i,
   cam_hsync_i,
   cam_vsync_i,
   cam_data_i,
   cam_rstn_o,
   cam_pwdn_o);

   inout [14:0]DDR_addr;
   inout [2:0] DDR_ba;
   inout       DDR_cas_n;
   inout       DDR_ck_n;
   inout       DDR_ck_p;
   inout       DDR_cke;
   inout       DDR_cs_n;
   inout [3:0] DDR_dm;
   inout [31:0] DDR_dq;
   inout [3:0]  DDR_dqs_n;
   inout [3:0]  DDR_dqs_p;
   inout        DDR_odt;
   inout        DDR_ras_n;
   inout        DDR_reset_n;
   inout        DDR_we_n;
   inout        FIXED_IO_ddr_vrn;
   inout        FIXED_IO_ddr_vrp;
   inout [53:0] FIXED_IO_mio;
   inout        FIXED_IO_ps_clk;
   inout        FIXED_IO_ps_porb;
   inout        FIXED_IO_ps_srstb;
   output       pwm0;
   output       pwm1;
   input        pwm0_rf;
   input        pwm1_rf;
   inout        cam_iic_scl_io;
   inout        cam_iic_sda_io;
   input        cam_pclk_i;
   input        cam_href_i;
   input        cam_hsync_i;
   input        cam_vsync_i;
   input [7:0]  cam_data_i;
   output       cam_rstn_o;
   output       cam_pwdn_o;

   wire [14:0]  DDR_addr;
   wire [2:0]   DDR_ba;
   wire         DDR_cas_n;
   wire         DDR_ck_n;
   wire         DDR_ck_p;
   wire         DDR_cke;
   wire         DDR_cs_n;
   wire [3:0]   DDR_dm;
   wire [31:0]  DDR_dq;
   wire [3:0]   DDR_dqs_n;
   wire [3:0]   DDR_dqs_p;
   wire         DDR_odt;
   wire         DDR_ras_n;
   wire         DDR_reset_n;
   wire         DDR_we_n;
   wire         FCLK_CLK0; // 50MHz from PS, used for all AXI4-Lite interconnection
   wire         FCLK_RESET0_N;
   wire         FCLK_CLK1; // 100MHz from PS, used for AXI4-Stream connections
   wire         FCLK_RESET1_N;
   wire         FIXED_IO_ddr_vrn;
   wire         FIXED_IO_ddr_vrp;
   wire [53:0]  FIXED_IO_mio;
   wire         FIXED_IO_ps_clk;
   wire         FIXED_IO_ps_porb;
   wire         FIXED_IO_ps_srstb;

   wire         cam_iic_scl_i;
   wire         cam_iic_scl_io;
   wire         cam_iic_scl_o;
   wire         cam_iic_scl_t;
   wire         cam_iic_sda_i;
   wire         cam_iic_sda_io;
   wire         cam_iic_sda_o;
   wire         cam_iic_sda_t;

   wire         cam_pclk_gclk;

   wire [5:0]   s2mm_frame_ptr_in;
   wire [5:0]   s2mm_frame_ptr_out;
   wire         vdma_frame_int;


   parameter int AXI_STREAM_DW = 8;

   // AXI4-Lite buses
   // SYS_CTRL physical address: 0x43C0_0000
   axi4_lite_if axi_bus_sys_ctrl(.ACLK(FCLK_CLK0), .ARESETn(FCLK_RESET0_N));
   // VIDEO_CTRL physical address: 0x43C1_0000
   axi4_lite_if axi_bus_video_ctrl(.ACLK(FCLK_CLK0), .ARESETn(FCLK_RESET0_N));

   // Input to axi_vdma_0
   // AXI4-Lite VDMA physical address: 0x4300_0000
   axi4_stream_if #(.DW(AXI_STREAM_DW)) axi_str_video(.ACLK(FCLK_CLK0), .ARESETn(FCLK_RESET0_N));

   // Input of BT 656 stream (YCbCr 4:2:2, but also RGB should be supported)
   bt656_stream_if bt656_input_video();

   // cam_pclk_i goes to global clock buffer
   BUFG cam_clk_bufg(.I(cam_pclk_i), .O(cam_pclk_gclk));

   assign bt656_input_video.PCLK  = cam_pclk_gclk;
   assign bt656_input_video.HREF  = cam_href_i;
   assign bt656_input_video.HSYNC = cam_hsync_i;
   assign bt656_input_video.VSYNC = cam_vsync_i;
   assign bt656_input_video.DATA  = cam_data_i;
   assign cam_rstn_o = bt656_input_video.RSTN;
   assign cam_pwdn_o = bt656_input_video.PWDN;

   sys_ctrl_top sys_ctrl_top_i
     (.axi_bus(axi_bus_sys_ctrl),
      .pwm0_pad_o(pwm0),
      .pwm1_pad_o(pwm1),
      .pwm0_pad_i(wm0_rf),
      .pwm1_pad_i(pwm1_rf),

      .axi_video_i(axi_str_video));


   // I2C for camera is connected directly from PS as iic0
   video_ctrl_top video_ctrl_top_i
     (.axi_bus(axi_bus_video_ctrl),
      .axi_video_o(axi_str_video),
      .bt656_video_i(bt656_input_video),
      .vdma_frame_ptr_i(s2mm_frame_ptr_out),
      .vdma_frame_ptr_o(s2mm_frame_ptr_in),
      .vdma_frame_int_o(vdma_frame_int));

   bd_system bd_system_i
     (.DDR_addr(DDR_addr),
      .DDR_ba(DDR_ba),
      .DDR_cas_n(DDR_cas_n),
      .DDR_ck_n(DDR_ck_n),
      .DDR_ck_p(DDR_ck_p),
      .DDR_cke(DDR_cke),
      .DDR_cs_n(DDR_cs_n),
      .DDR_dm(DDR_dm),
      .DDR_dq(DDR_dq),
      .DDR_dqs_n(DDR_dqs_n),
      .DDR_dqs_p(DDR_dqs_p),
      .DDR_odt(DDR_odt),
      .DDR_ras_n(DDR_ras_n),
      .DDR_reset_n(DDR_reset_n),
      .DDR_we_n(DDR_we_n),
      .FCLK_CLK0(FCLK_CLK0),
      .FCLK_RESET0_N(FCLK_RESET0_N),
      .FCLK_CLK1_0(FCLK_CLK1),
      .FCLK_RESET1_N_0(FCLK_RESET1_N),
      .FIXED_IO_ddr_vrn(FIXED_IO_ddr_vrn),
      .FIXED_IO_ddr_vrp(FIXED_IO_ddr_vrp),
      .FIXED_IO_mio(FIXED_IO_mio),
      .FIXED_IO_ps_clk(FIXED_IO_ps_clk),
      .FIXED_IO_ps_porb(FIXED_IO_ps_porb),
      .FIXED_IO_ps_srstb(FIXED_IO_ps_srstb),
      .M00_AXI_0_araddr(axi_bus_sys_ctrl.ARADDR),
      .M00_AXI_0_arprot(axi_bus_sys_ctrl.ARPROT),
      .M00_AXI_0_arready(axi_bus_sys_ctrl.ARREADY),
      .M00_AXI_0_arvalid(axi_bus_sys_ctrl.ARVALID),
      .M00_AXI_0_awaddr(axi_bus_sys_ctrl.AWADDR),
      .M00_AXI_0_awprot(axi_bus_sys_ctrl.AWPROT),
      .M00_AXI_0_awready(axi_bus_sys_ctrl.AWREADY),
      .M00_AXI_0_awvalid(axi_bus_sys_ctrl.AWVALID),
      .M00_AXI_0_bready(axi_bus_sys_ctrl.BREADY),
      .M00_AXI_0_bresp(axi_bus_sys_ctrl.BRESP),
      .M00_AXI_0_bvalid(axi_bus_sys_ctrl.BVALID),
      .M00_AXI_0_rdata(axi_bus_sys_ctrl.RDATA),
      .M00_AXI_0_rready(axi_bus_sys_ctrl.RREADY),
      .M00_AXI_0_rresp(axi_bus_sys_ctrl.RRESP),
      .M00_AXI_0_rvalid(axi_bus_sys_ctrl.RVALID),
      .M00_AXI_0_wdata(axi_bus_sys_ctrl.WDATA),
      .M00_AXI_0_wready(axi_bus_sys_ctrl.WREADY),
      .M00_AXI_0_wstrb(axi_bus_sys_ctrl.WSTRB),
      .M00_AXI_0_wvalid(axi_bus_sys_ctrl.WVALID),
      .M02_AXI_0_araddr(axi_bus_video_ctrl.ARADDR),
      .M02_AXI_0_arprot(axi_bus_video_ctrl.ARPROT),
      .M02_AXI_0_arready(axi_bus_video_ctrl.ARREADY),
      .M02_AXI_0_arvalid(axi_bus_video_ctrl.ARVALID),
      .M02_AXI_0_awaddr(axi_bus_video_ctrl.AWADDR),
      .M02_AXI_0_awprot(axi_bus_video_ctrl.AWPROT),
      .M02_AXI_0_awready(axi_bus_video_ctrl.AWREADY),
      .M02_AXI_0_awvalid(axi_bus_video_ctrl.AWVALID),
      .M02_AXI_0_bready(axi_bus_video_ctrl.BREADY),
      .M02_AXI_0_bresp(axi_bus_video_ctrl.BRESP),
      .M02_AXI_0_bvalid(axi_bus_video_ctrl.BVALID),
      .M02_AXI_0_rdata(axi_bus_video_ctrl.RDATA),
      .M02_AXI_0_rready(axi_bus_video_ctrl.RREADY),
      .M02_AXI_0_rresp(axi_bus_video_ctrl.RRESP),
      .M02_AXI_0_rvalid(axi_bus_video_ctrl.RVALID),
      .M02_AXI_0_wdata(axi_bus_video_ctrl.WDATA),
      .M02_AXI_0_wready(axi_bus_video_ctrl.WREADY),
      .M02_AXI_0_wstrb(axi_bus_video_ctrl.WSTRB),
      .M02_AXI_0_wvalid(axi_bus_video_ctrl.WVALID),
      .S_AXIS_S2MM_0_tdata(axi_str_video.TDATA),
      .S_AXIS_S2MM_0_tkeep(axi_str_video.TKEEP),
      .S_AXIS_S2MM_0_tlast(axi_str_video.TLAST),
      .S_AXIS_S2MM_0_tready(axi_str_video.TREADY),
      .S_AXIS_S2MM_0_tuser(axi_str_video.TUSER),
      .S_AXIS_S2MM_0_tvalid(axi_str_video.TVALID),
      .s_axis_s2mm_aclk_0(axi_str_video.ACLK),
      .s2mm_frame_ptr_in(s2mm_frame_ptr_in),
      .s2mm_frame_ptr_out(s2mm_frame_ptr_out),
      .vdma_frame_int_in(vdma_frame_int),
      .IIC_0_0_scl_i(cam_iic_scl_i),
      .IIC_0_0_scl_o(cam_iic_scl_o),
      .IIC_0_0_scl_t(cam_iic_scl_t),
      .IIC_0_0_sda_i(cam_iic_sda_i),
      .IIC_0_0_sda_o(cam_iic_sda_o),
      .IIC_0_0_sda_t(cam_iic_sda_t));

   // pulled-up externally
   IOBUF cam_iic_scl_iobuf
       (.I(cam_iic_scl_o),
        .IO(cam_iic_scl_io),
        .O(cam_iic_scl_i),
        .T(cam_iic_scl_t));
   IOBUF cam_iic_sda_iobuf
       (.I(cam_iic_sda_o),
        .IO(cam_iic_sda_io),
        .O(cam_iic_sda_i),
        .T(cam_iic_sda_t));

endmodule
