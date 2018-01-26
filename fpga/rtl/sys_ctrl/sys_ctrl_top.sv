`timescale 1 ps / 1 ps

module sys_ctrl_top #(
  int unsigned DW = 32, // Width of AXI data    bus
  int unsigned AW = 5, // Width of AXI address bus
  int unsigned VER = 'h100,
  int          PWM_CNT_WIDTH = 24) // PWM counters width
   (
    // AXI-Lite slave
    axi4_lite_if.s axi_bus,

    // PWM signals for drive/steer
    output pwm0_pad_o, // output PWM0 to pad
    output pwm1_pad_o, // output PWM1 to pad
    input  pwm0_pad_i, // input PWM0 from pad/RF controller
    input  pwm1_pad_i  // input PWM1 from pad/RF controller
    );

   logic    pwm_mux_sel;
   logic    pwm0_sys, pwm1_sys;

   logic    pwm_enable;
   logic [PWM_CNT_WIDTH-1:0] pwm_period;
   logic [PWM_CNT_WIDTH-1:0] pwm_active_0;
   logic [PWM_CNT_WIDTH-1:0] pwm_active_1;

   assign pwm0_pad_o = pwm_mux_sel ? pwm0_pad_i : pwm0_sys;
   assign pwm1_pad_o = pwm_mux_sel ? pwm1_pad_i : pwm1_sys;

   sys_ctrl_axi #(.DW(DW), .AW(AW), .VER(VER), .PWM_CNT_WIDTH(PWM_CNT_WIDTH)) sys_ctrl_axi_i
     (
      .axi_bus(axi_bus),
      .pwm_mux_sel_o(pwm_mux_sel),

      .pwm_enable_o(pwm_enable),
      .pwm_period_o(pwm_period),
      .pwm_active_0_o(pwm_active_0),
      .pwm_active_1_o(pwm_active_1));

   pwm_gen #(.CNT_WIDTH(PWM_CNT_WIDTH)) pwm_gen_i
     (
      .axi_clk(axi_bus.ACLK),
      .axi_rstn(axi_bus.ARESETn),

      .pwm_0_o(pwm0_sys),
      .pwm_1_o(pwm1_sys),

      .pwm_enable_i(pwm_enable),
      .pwm_period_i(pwm_period),
      .pwm_active_0_i(pwm_active_0),
      .pwm_active_1_i(pwm_active_1));

endmodule : sys_ctrl_top


