`timescale 1 ps / 1 ps

module video_ctrl_top #(
  int unsigned DW = 32, // Width of AXI data    bus
  int unsigned AW = 8, // Width of AXI address bus
  int unsigned VER = 'hdead0100)
   (
    // AXI-Lite slave
    axi4_lite_if.s axi_bus,

    // AXI Streamer for axi_vdma
    axi4_stream_if.s axi_video_o,

    // BT 656 input video stream
    bt656_stream_if.d bt656_video_i
    );

   // TODO: Put TP to interface type
   logic        tp_gen_en;
   logic [1:0]  tp_type;
   logic [10:0] tp_width, tp_height;

   rx_cfg_if    rx_cfg();

   // TODO: tp_* signals are not sync'd to stream clock!
   video_ctrl_axi #(.DW(DW), .AW(AW), .VER(VER)) video_ctrl_axi_i
     (
      .axi_bus(axi_bus),


      .tp_en_gen_o(tp_gen_en),
      .tp_type_o(tp_type),
      .tp_width_o(tp_width),
      .tp_height_o(tp_height),

      // Receiver / bt656_to_axi_stream configuration
      .rx_cfg(rx_cfg)
      );

   bt656_to_axi_stream #(.DW(axi_video_o.DW)) bt656_to_axi_stream_i
     (
      .bt656_stream_i(bt656_video_i),

      .axi_stream_o(axi_video_o),

      .axi_clk_i(axi_bus.ACLK),
      .axi_rstn_i(axi_bus.ARESETn),
      .rx_cfg(rx_cfg)
      );

endmodule : video_ctrl_top


