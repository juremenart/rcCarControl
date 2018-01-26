`timescale 1ps / 1ps

module bt656_to_axi_stream_tb
  #(
    // time periods
    realtime               AXI_TP = 10.0ns,   // 100MHz
    realtime               V_TP   = 37.037ns // 27MHz
    // RTL config
    );

   ////////////////////////////////////////////////////////////////////////////////
   // signal generation
   ////////////////////////////////////////////////////////////////////////////////

   logic                                    axi_clk;
   logic                                    axi_rstn;

   logic                                    v_in_clk; // video input clock

   logic                                    v_clk; // internal video clock
   logic                                    v_rstn, rx_rstn;

   // Clock
   initial            axi_clk = 1'b1;
   always #(AXI_TP/2) axi_clk = ~axi_clk;

   initial            v_in_clk = 1'b1;
   always #(V_TP/2)   v_in_clk = ~v_in_clk;

   initial            v_clk = 1'b1;
   always #(V_TP/2)   v_clk = ~v_clk;

   // Reset
   initial begin
      axi_rstn = 1'b0;
      repeat(4) @(posedge axi_clk);
      axi_rstn = 1'b1;
   end

   initial begin
      v_rstn = 1'b0;
      rx_rstn = 1'b0;
      repeat(10) @(posedge v_clk);
      v_rstn = 1'b1;
      repeat(20) @(posedge v_clk);
      rx_rstn = 1'b1;
   end

   ////////////////////////////////////////////////////////////////////////////////
   // test sequence
   ////////////////////////////////////////////////////////////////////////////////

   initial begin
      // check stuff on waveform (counters, frame generation, ..)
      repeat(10000) @(posedge axi_clk);
      $finish();
   end

   ////////////////////////////////////////////////////////////////////////////////
   // module instances
   ////////////////////////////////////////////////////////////////////////////////
   bt656_stream_if bt656_stream();
   axi4_stream_if  axi_stream (.ACLK(axi_clk), .ARESETn(axi_rstn));

   bt656_stream_gen bt656_stream_gen_i
     (.bt656_stream_o(bt656_stream));

   bt656_to_axi_stream bt656_to_axi_stream_i
     (
      .bt656_stream_i(bt656_stream),
      .axi_stream_o (axi_stream),
      .rstn_i(rx_rstn)
      );


   ////////////////////////////////////////////////////////////////////////////////
   // waveforms
   ////////////////////////////////////////////////////////////////////////////////
//
//   initial begin
//      $dumpfile("bt656_to_axi_stream_tb.vcd");
//      $dumpvars(0, bt656_to_axi_stream_tb);
//   end

endmodule: bt656_to_axi_stream_tb
