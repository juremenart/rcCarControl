`timescale 1 ps / 1 ps

module video_ctrl_top #(
  int unsigned DW = 32, // Width of AXI data    bus
  int unsigned AW = 8,  // Width of AXI address bus
  int unsigned VER = 'hdead0100)
   (
    // AXI-Lite slave
    axi4_lite_if.s axi_bus,

    // AXI Streamer for axi_vdma
    axi4_stream_if.s axi_video_o,

    // BT 656 input video stream
    bt656_stream_if.d bt656_video_i
    );

   logic        tp_gen_en;
   logic [1:0]  tp_type;
   logic [10:0] tp_width, tp_height;

   logic        rx_enable, pure_bt656;
   logic [31:0] rx_size_status;
   logic [31:0] rx_frame_cnts;
   logic        rx_rst_size_err;
   logic        cam_rstn;
   logic        cam_pwdn;

   axi4_stream_if axi_tp_gen_video(.ACLK(FCLK_CLK1), .ARESETn(FCLK_RESET1_N));
   axi4_stream_if axi_bt656_video(.ACLK(FCLK_CLK1), .ARESETn(FCLK_RESET1_N));

   assign bt656_video_i.RSTN = cam_rstn;
   assign bt656_video_i.PWDN = cam_pwdn;

   // TODO: tp_* signals are not sync'd to stream clock!
   video_ctrl_axi #(.DW(DW), .AW(AW), .VER(VER)) video_ctrl_axi_i
     (
      .axi_bus(axi_bus),

      .tp_en_gen_o(tp_gen_en),
      .tp_type_o(tp_type),
      .tp_width_o(tp_width),
      .tp_height_o(tp_height),

      // Receiver / bt656_to_axi_stream configuration
      .rx_enable_o(rx_enable),
      .pure_bt656_o(pure_bt656),
      .rx_size_status_i(rx_size_status),
      .rx_rst_size_err_o(rx_rst_size_err),
      .rx_frame_cnts_i(rx_frame_cnts),

      .cam_rstn_o(cam_rstn),
      .cam_pwdn_o(cam_pwdn)
      );

   // TODO: Be careful about reset for bt656_to_axi_stream!
   bt656_to_axi_stream bt656_to_axi_stream_i
     (
      .bt656_stream_i(bt656_video_i),
      .axi_stream_o(axi_bt656_video),

      .axi_clk_i(axi_bus.ACLK),
      .axi_rstn_i(axi_bus.ARESETn),
      .rx_enable_i(rx_enable),
      .pure_bt656_i(pure_bt656),
      .ignore_href_i(1'b0),
      .ignore_hsync_i(1'b1),
      .ignore_vsync_i(1'b0),
      .size_status_o(rx_size_status),
      .rst_size_err_i(rx_rst_size_err),
      .frame_cnts_o(rx_frame_cnts)
      );

   axi_stream_tp_gen axi_stream_tp_gen_i
     (
      .axi_stream_o(axi_tp_gen_video),

      .tp_enable_i(tp_gen_en),
      .tp_type_i(tp_type),
      .tp_width_i(tp_width),
      .tp_height_i(tp_height));

   axi_stream_mux axi_stream_mux_i
     (
      .axi_video_o(axi_video_o),

      .axi_video_in1_i(axi_bt656_video),
      .axi_video_in2_i(axi_tp_gen_video),

      .mux_sel(tp_gen_en));

endmodule : video_ctrl_top


