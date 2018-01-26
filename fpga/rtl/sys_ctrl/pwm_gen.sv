`timescale 1 ps / 1 ps

module pwm_gen
  #(
   int unsigned CNT_WIDTH = 24
    )
  (
   input logic        axi_clk,
   input logic        axi_rstn,

   output logic       pwm_0_o,
   output logic       pwm_1_o,

   input logic        pwm_enable_i,
   input logic [CNT_WIDTH-1:0] pwm_period_i,
   input logic [CNT_WIDTH-1:0] pwm_active_0_i,
   input logic [CNT_WIDTH-1:0] pwm_active_1_i);

   // Main counter
   logic [CNT_WIDTH-1:0]       main_cnt;
   logic                       pwm_0_on, pwm_1_on;
   logic [CNT_WIDTH-1:0]       r_period, r_0_active, r_1_active;

   always_ff @(posedge axi_clk)
     if(axi_rstn == 1'b0)
       begin
          main_cnt   <= '0;
          r_period   <= '0;
          r_0_active <= '0;
          r_1_active <= '0;
       end
     else
       begin
          if(pwm_enable_i == 1'b0)
            begin
               main_cnt <= '0;
               r_period   <= pwm_period_i;
               r_0_active <= pwm_active_0_i;
               r_1_active <= pwm_active_1_i;
            end
          else
            begin
               // update always only when overflow or start
               if(main_cnt == 0)
                 begin
                    r_period   <= pwm_period_i;
                    r_0_active <= pwm_active_0_i;
                    r_1_active <= pwm_active_1_i;
                 end
               if(main_cnt == r_period-1)
                 main_cnt <= '0;
               else
                 main_cnt <= main_cnt + 1;
            end
       end // else: !if(axi_rstn == 1'b0)

   assign pwm_0_on = pwm_enable_i && (main_cnt <= r_0_active);
   assign pwm_1_on = pwm_enable_i && (main_cnt <= r_1_active);

   assign pwm_0_o = pwm_0_on;
   assign pwm_1_o = pwm_1_on;

endmodule: pwm_gen
