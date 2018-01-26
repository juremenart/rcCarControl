`timescale 1ps / 1ps

module sys_ctrl_tb
  #(
    // time periods
    realtime               TP = 20.0ns, // 50MHz
    // RTL config
    parameter int unsigned SYS_CTRL_VER = 32'ha5a5a5a5 // data width
    );

   ////////////////////////////////////////////////////////////////////////////////
   // signal generation
   ////////////////////////////////////////////////////////////////////////////////

   logic                                    clk ;
   logic                                    rstn;

   logic                                    pwm0_pad, pwm1_pad;
   logic                                    pwm0_rf, pwm1_rf, pwm0_sys, pwm1_sys;


   // Clock
   initial        clk = 1'b0;
   always #(TP/2) clk = ~clk;

   // Reset
   initial begin
      rstn = 1'b0;
      repeat(4) @(posedge clk);
      rstn = 1'b1;
   end

   ////////////////////////////////////////////////////////////////////////////////
   // test sequence
   ////////////////////////////////////////////////////////////////////////////////

   logic [32-1:0] ver;

   initial begin
      repeat(8) @(posedge clk);
      axi_read('h00, ver);
      $display("ver = 0x%08x", ver);
      if(ver != SYS_CTRL_VER)
        $display("Incorrect version: 0x%08x != 0x%08x", ver, SYS_CTRL_VER);

      // switch PWM outputs to PWM_SYS
      axi_write('h01, 0);
      if((pwm0_pad != pwm0_sys) || (pwm1_pad != pwm1_sys))
        $display("Incorrect SYS PWM signal values");

      repeat(2) @(posedge clk);

      // switch PWM outputs to PWM_RF
      axi_write('h04, 1);
      if((pwm0_pad != pwm0_rf) || (pwm1_pad != pwm1_rf))
        $display("Incorrect RF signal values incorrect");

      axi_read('h08, ver);
      if(ver != 32'hdeadbeef)
        $display("Unexpected value from reserved register: 0x%08x", ver);

      repeat(16) @(posedge clk);
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
   assign pwm0_rf = 1'b0;
   assign pwm1_rf = 1'b1;

   assign pwm0_sys = 1'b1;
   assign pwm1_sys = 1'b0;

   axi4_lite_if axi4_lite (.ACLK (clk), .ARESETn (rstn));

   axi4_lite_master bus_master (.intf (axi4_lite));

   sys_ctrl_top #(.VER(SYS_CTRL_VER)) sys_ctrl_top_i
     (
      .axi_bus    (axi4_lite),

      .pwm0_pad_o(pwm0_pad),
      .pwm1_pad_o(pwm1_pad),
      .pwm0_pad_i(pwm0_rf),
      .pwm1_pad_i(pwm1_rf),
      .pwm0_sys_i(pwm0_sys),
      .pwm1_sys_i(pwm1_sys)
      );


   ////////////////////////////////////////////////////////////////////////////////
   // waveforms
   ////////////////////////////////////////////////////////////////////////////////

   initial begin
      $dumpfile("sys_ctrl_tb.vcd");
      $dumpvars(0, sys_ctrl_tb);
   end

endmodule: sys_ctrl_tb
