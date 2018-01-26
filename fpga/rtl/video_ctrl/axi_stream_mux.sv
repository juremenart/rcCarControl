`timescale 1 ps / 1 ps

module axi_stream_mux
  (
   axi4_stream_if.s axi_video_o,

   axi4_stream_if.d axi_video_in1_i,
   axi4_stream_if.d axi_video_in2_i,

   input logic mux_sel);

   assign axi_video_o.TDATA  = (mux_sel) ? axi_video_in2_i.TDATA  : axi_video_in1_i.TDATA;
   assign axi_video_o.TKEEP  = (mux_sel) ? axi_video_in2_i.TKEEP  : axi_video_in1_i.TKEEP;
   assign axi_video_o.TLAST  = (mux_sel) ? axi_video_in2_i.TLAST  : axi_video_in1_i.TLAST;
   assign axi_video_o.TUSER  = (mux_sel) ? axi_video_in2_i.TUSER  : axi_video_in1_i.TUSER;
   assign axi_video_o.TVALID = (mux_sel) ? axi_video_in2_i.TVALID : axi_video_in1_i.TVALID;

   assign axi_video_in1_i.TREADY = (!mux_sel) ? axi_video_o.TREADY : 1'b1;
   assign axi_video_in2_i.TREADY = ( mux_sel) ? axi_video_o.TREADY : 1'b1;

endmodule : axi_stream_mux

