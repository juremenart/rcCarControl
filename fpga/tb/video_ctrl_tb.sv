`timescale 1ps / 1ps

module video_ctrl_tb
  #(
    // time periods
    realtime TP = 20.0ns, // 50MHz
    realtime V_TP = 41.667ns // 24MHz
    // RTL config
    );

   ////////////////////////////////////////////////////////////////////////////////
   // signal generation
   ////////////////////////////////////////////////////////////////////////////////

   logic                                    clk ;
   logic                                    rstn;
   logic                                    s_clk;
   logic                                    s_rstn;
   logic                                    b_clk;
   logic                                    b_rstn;

   logic                                    pure_bt656;

   // to check either BT656 generation or OV DVP like interface
   assign pure_bt656 = 1'b0;

   // Clock
   initial        clk = 1'b1;
   always #(TP/2) clk = ~clk;

   // Stream clock (currently hard-coded to twice the AXI speed)
   initial        s_clk = 1'b1;
   always #(TP/4) s_clk = ~s_clk;

   // BT656 PCLK
   initial b_clk = 1'b1;
   always #(V_TP/2) b_clk = ~b_clk;

   // Reset
   initial begin
      rstn = 1'b0;
      repeat(10) @(posedge clk);
      rstn = 1'b1;
      repeat(20) @(posedge clk);
   end

   initial begin
      s_rstn = 1'b0;
      repeat(10) @(posedge s_clk);
      s_rstn = 1'b1;
   end

   initial begin
      b_rstn = 1'b0;
      repeat(10) @(posedge b_clk);
      b_rstn = 1'b1;
   end

   ////////////////////////////////////////////////////////////////////////////////
   // test sequence
   ////////////////////////////////////////////////////////////////////////////////

   logic [32-1:0] ver;

   initial begin
      repeat(20) @(posedge clk);
      video_stream.TREADY <= 1'b1;
      axi_read('h00, ver);
      $display("Video controller version = 0x%08x", ver);
      // set some small frame count and enable test pattern generator
      axi_write('h08, 32'h000a_0010); // 16x10 frame size
      axi_write('h1C, 32'h0000_0020); // AXI FIFO write set to line * 2

      repeat(2) @(posedge clk);
      axi_write('h0C, { {30{1'b0}}, pure_bt656, 1'b1 }); // Enable receiver
//      axi_write('h04, 32'h0000_0001); // Enable pattern generation
      repeat(50000) @(posedge clk);

      // One more case to get it at EOL
//      repeat(80) @(posedge clk);
//      video_stream.TREADY <= 1'b0;
//      repeat(10) @(posedge clk);
//      video_stream.TREADY <= 1'b1;

      // check stuff on waveform (counters, frame generation, ..)
      axi_write('h10, 32'h1000_0000); // Clear errors

      repeat(50000) @(posedge clk);
      $finish();
   end

   task axi_write (
                   input logic [32-1:0] adr,
                   input logic [32-1:0] dat
                   );
      int                               b;
      bus_master.WriteTransaction (
                                   .AWDelay (1),  .aw ('{
                                                         addr: adr,
                                                         prot: 0
                                                         }),
                                   .WDelay (0),   .w ('{
                                                        data: dat,
                                                        strb: '1
                                                        }),
                                   .BDelay (0),   .b (b)
                                   );
   endtask: axi_write

   task axi_read (
                  input logic [32-1:0]  adr,
                  output logic [32-1:0] dat
                  );
      logic [32+2-1:0]                  r;
      bus_master.ReadTransaction (
                                  .ARDelay (0),  .ar ('{
                                                        addr: adr,
                                                        prot: 0
                                                        }),
                                  .RDelay (0),   .r (r)
                                  //     .RDelay (0),   .r ('{
                                  //                          data: dat,
                                  //                          resp: rsp
                                  //                         })
                                  );
      dat = r >> 2;
   endtask: axi_read

   ////////////////////////////////////////////////////////////////////////////////
   // module instances
   ////////////////////////////////////////////////////////////////////////////////
   axi4_lite_if axi4_lite (.ACLK (clk), .ARESETn (rstn));
   axi4_stream_if video_stream (.ACLK(s_clk), .ARESETn(s_rstn));
   bt656_stream_if bt656_stream();


   axi4_lite_master bus_master (.intf (axi4_lite));

   video_ctrl_top video_ctrl_top_i
     (
      .axi_bus     (axi4_lite),
      .axi_video_o (video_stream),
      .bt656_video_i(bt656_stream)
      );

   bt656_stream_gen bt656_stream_gen_i
     (.ACLK(b_clk),
      .ARESETn(b_rstn),
      .bt656_stream_o(bt656_stream),
      .pure_bt656_i(pure_bt656));

   ////////////////////////////////////////////////////////////////////////////////
   // waveforms
   ////////////////////////////////////////////////////////////////////////////////
//
//   initial begin
//      $dumpfile("video_ctrl_tb.vcd");
//      $dumpvars(0, video_ctrl_tb);
//   end

endmodule: video_ctrl_tb
