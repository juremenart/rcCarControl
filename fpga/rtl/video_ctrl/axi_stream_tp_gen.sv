`timescale 1 ps / 1 ps

module axi_stream_tp_gen
  (
   axi4_stream_if.s axi_stream_o,

   input logic        tp_enable_i,
   input logic [10:0] tp_width_i,
   input logic [10:0] tp_height_i,
   input logic [6:0]  tp_num_frames_i,
   input logic [23:0] tp_blanking_i
   );

   logic              clk, rstn;

   assign clk  = axi_stream_o.ACLK;
   assign rstn = axi_stream_o.ARESETn;

   // Registers of configuration (updated only when started)
   logic [2:0]        enable_r;
   logic [10:0]       width_r, height_r;
   logic [6:0]        num_frames_r;
   logic [23:0]       blanking_r;

   logic              gen_start;

   // Register input configuration & start/stop request detection
   assign gen_start = !enable_r[2] && enable_r[1];

   always_ff @(posedge clk)
     if (!rstn)
       begin
          // '0 operation is available in SystemVerilog yeee :)
          enable_r      <= '0;
          width_r       <= '0;
          height_r      <= '0;
          num_frames_r  <= '0;
          blanking_r    <= '0;
       end
     else
       begin
          enable_r    <= {enable_r[1:0], tp_enable_i};

          // Register new configuration only when new generation of data is
          // requested
          if(gen_start)
            begin
               width_r         <= tp_width_i;
               height_r        <= tp_height_i;
               num_frames_r    <= tp_num_frames_i;
               blanking_r      <= tp_blanking_i;

            end
       end // else: !if(rstn)

   // counters
   logic [7:0]  frame_cnt;
   logic [23:0] pixel_cnt; // pixels in line
   logic [10:0] line_cnt;
   logic        inc_frame_cnt, rst_frame_cnt;
   logic        inc_pixel_cnt, rst_pixel_cnt;
   logic        inc_line_cnt, rst_line_cnt;
   logic        data_valid_out;

   always_ff @(posedge clk)
     if(!rstn)
       begin
          frame_cnt   <= '0;
          pixel_cnt   <= '0;
          line_cnt    <= '0;
       end
     else
       begin
          if(rst_frame_cnt)
            frame_cnt <= '0;
          else if(inc_frame_cnt)
            frame_cnt   <= frame_cnt + 1;

          if(rst_pixel_cnt)
            pixel_cnt   <= '0;
          else if(inc_pixel_cnt)
            pixel_cnt <= pixel_cnt + 1;

          if(rst_line_cnt)
            line_cnt <= '0;
          else if(inc_line_cnt)
            line_cnt <= line_cnt + 1;
       end // else: !if(!rstn)

      // state machine signals
   enum logic [2:0]
        {
         FSM_IDLE     = 3'b000,
         FSM_BLANKING = 3'b001,
         FSM_VDATA    = 3'b011
         } state, nstate;

   always_ff @(posedge clk)
     if(!rstn)
       begin
          state <= FSM_IDLE;
       end
     else
       begin
          state <= nstate;
       end

   assign axi_stream_o.TDATA = (data_valid_out == 1'b1) ? { line_cnt[3:0], pixel_cnt[3:0] } : 'x;

   always_comb
     begin
        nstate = state;

        axi_stream_o.TVALID = 1'b0;
        axi_stream_o.TUSER  =   '0;
        axi_stream_o.TLAST  = 1'b0;
        axi_stream_o.TKEEP  =   '1;

        inc_frame_cnt  = 1'b0;
        rst_pixel_cnt  = 1'b0;
        inc_pixel_cnt  = 1'b0;
        rst_line_cnt   = 1'b0;
        inc_line_cnt   = 1'b0;
        data_valid_out = 1'b0;
        rst_frame_cnt  = 1'b0;

        case(state)
          FSM_IDLE:
            begin
               rst_frame_cnt = 1'b1;
               if(gen_start)
                 begin
                    nstate = FSM_VDATA;
                 end
            end
          FSM_BLANKING:
            begin
               // wait one line here before continue with another
               // frame (this should be programmable)
               inc_pixel_cnt = 1'b1;

               if((enable_r[0] == 1'b0) ||
                  ((frame_cnt >= num_frames_r) && (num_frames_r > 0)))
                 begin
                    nstate = FSM_IDLE;
                 end

               if(pixel_cnt >= blanking_r)
                 begin
                    rst_pixel_cnt = 1'b1;
                    nstate = FSM_VDATA;
                 end
            end
          FSM_VDATA:
            begin
               axi_stream_o.TVALID = 1'b1;
               data_valid_out = 1'b1;
               if(axi_stream_o.TREADY)
                 begin
                    inc_pixel_cnt = 1'b1;

                    if((pixel_cnt == 0) && (line_cnt == 0))
                      axi_stream_o.TUSER[0] = 1'b1;

                    if(pixel_cnt == (width_r-1))
                      begin
                         axi_stream_o.TLAST = 1'b1;
                         inc_line_cnt = 1'b1;
                         rst_pixel_cnt = 1'b1;
                      end

                    if((pixel_cnt == (width_r-1)) && (line_cnt == (height_r-1)))
                      begin
                         inc_frame_cnt = 1'b1;
                         rst_line_cnt  = 1'b1;
                         rst_pixel_cnt = 1'b1;
                         nstate = FSM_BLANKING;
                      end
                 end
            end
        endcase; // case (state)
     end // always_comb

endmodule: axi_stream_tp_gen
