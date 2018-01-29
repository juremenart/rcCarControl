module sync_pulse
  (
   // TODO: Check this primitive reset scheem :)
   input logic  rstn_i,

   input logic  a_clk_i,
   input logic  a_pulse_i,

   input logic  b_clk_i,
   output logic b_pulse_o
   );

   logic        a_hold;
   logic [1:0]  a_sync;
   logic [1:0]  b_sync;

   always_ff @(posedge a_clk_i)
     if(!rstn_i)
       begin
          a_hold <= 1'b0;
       end
     else
       begin
          if(a_pulse_i)
            a_hold <= 1'b1;
          else if(a_sync[1])
            a_hold <= 1'b0;
       end // else: !if(!rstn_i)


   // B -> A sync signal sync
   always_ff @(posedge a_clk_i)
     if(!rstn_i)
       begin
          a_sync <= '0;
       end
     else
       begin
          a_sync[1] <= a_sync[0];
          a_sync[0] <= b_sync[1];
       end

   // A -> B hold signal sync
   always_ff @(posedge b_clk_i)
     if(!rstn_i)
       begin
          b_sync <= '0;
       end
     else
       begin
          b_sync[0] <= a_hold;
          b_sync[1] <= b_sync[0];
       end

   always_ff @(posedge b_clk_i)
     if(!rstn_i)
       begin
          b_sync    <= '0;
          b_pulse_o <= 'b0;
       end
     else
       begin
          if(b_sync[0] && !b_sync[1])
            b_pulse_o <= 1'b1;
          else
            b_pulse_o <= 1'b0;
       end // else: !if(!rstn_i)


endmodule: sync_pulse
