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
    bt656_stream_if.d bt656_video_i,

    // Direct VDMA control - read & write pointers
    input  logic [5:0] vdma_frame_ptr_i, // the pointer that VDMA is currently writting to
    output logic [5:0] vdma_frame_ptr_o, // the pointer that slave is currently reading from (set from AXI by driver)
    output logic vdma_frame_int_o // frame interrupt (VDMA changed buffer)
    );

   // TODO: Put TP to interface type
   logic        tp_gen_en;
   logic [10:0] tp_width, tp_height;
   logic [6:0]  tp_num_frames;
   logic [23:0] tp_blanking;

   rx_cfg_if    rx_cfg();

   axi4_stream_if #(.DW(DW)) tp_stream(.ACLK(axi_video_o.ACLK), .ARESETn(axi_video_o.ARESETn));

   axi4_stream_if #(.DW(DW)) video_stream(.ACLK(axi_video_o.ACLK), .ARESETn(axi_video_o.ARESETn));

   // TODO: tp_* signals are not sync'd to stream clock!
   video_ctrl_axi #(.DW(DW), .AW(AW), .VER(VER)) video_ctrl_axi_i
     (
      .axi_bus(axi_bus),

      .tp_en_gen_o(tp_gen_en),
      .tp_width_o(tp_width),
      .tp_height_o(tp_height),
      .tp_num_frames_o(tp_num_frames),
      .tp_blanking_o(tp_blanking),

      // Receiver / bt656_to_axi_stream configuration
      .rx_cfg(rx_cfg),
      .vdma_frame_ptr_i(vdma_frame_ptr_i),
      .vdma_frame_ptr_o(vdma_frame_ptr_o),
      .vdma_frame_int_o(vdma_frame_int_o)
      );

   bt656_to_axi_stream #(.DW(axi_video_o.DW)) bt656_to_axi_stream_i
     (
      .bt656_stream_i(bt656_video_i),

      .axi_stream_o(video_stream),

      .axi_clk_i(axi_bus.ACLK),
      .axi_rstn_i(axi_bus.ARESETn),
      .rx_cfg(rx_cfg)
      );

   axi_stream_tp_gen axi_stream_tp_gen_i
     (
      .axi_stream_o(tp_stream),

      .tp_enable_i(tp_gen_en),
      .tp_width_i(tp_width),
      .tp_height_i(tp_height),
      .tp_num_frames_i(tp_num_frames),
      .tp_blanking_i(tp_blanking));

   axi_stream_mux axi_stream_mux_i
     (
      .axi_video_o(axi_video_o),

      .axi_video_in1_i(video_stream),
      .axi_video_in2_i(tp_stream),

      .mux_sel(tp_gen_en));

endmodule : video_ctrl_top


