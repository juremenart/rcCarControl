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

   // TODO: Put TP to interface type
   logic        tp_gen_en;
   logic [1:0]  tp_type;
   logic [10:0] tp_width, tp_height;

   rx_cfg_if    rx_cfg();

   axi4_stream_if axi_tp_gen_video(.ACLK(axi_video_o.ACLK),
                                   .ARESETn(axi_video_o.ARESETn));
   axi4_stream_if axi_bt656_video(.ACLK(axi_video_o.ACLK),
                                  .ARESETn(axi_video_o.ARESETn));

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

   // TODO: Be careful about reset for bt656_to_axi_stream!
   bt656_to_axi_stream bt656_to_axi_stream_i
     (
      .bt656_stream_i(bt656_video_i),

      .axi_stream_o(axi_bt656_video),

      .axi_clk_i(axi_bus.ACLK),
      .axi_rstn_i(axi_bus.ARESETn),
      .rx_cfg(rx_cfg)
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


