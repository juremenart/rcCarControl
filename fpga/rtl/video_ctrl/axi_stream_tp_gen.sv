`timescale 1 ps / 1 ps

module axi_stream_tp_gen
  (
   axi4_stream_if.s axi_stream_o,

   input logic        tp_enable_i,
   input logic  [1:0] tp_type_i,
   input logic [10:0] tp_width_i,
   input logic [10:0] tp_height_i
   );

   logic              clk, rstn;

   assign clk  = axi_stream_o.ACLK;
   assign rstn = axi_stream_o.ARESETn;

   // Registers of configuration (updated only when started)
   logic [2:0]        enable_r;
   logic [1:0]        type_r;
   logic [10:0]       width_r, height_r;

   logic              gen_start, gen_end;
   logic              gen_start_d, gen_end_d;
   logic              gen_cnts_on; // counters running

   // Register input configuration & start/stop request detection
   assign gen_start = !enable_r[2] && enable_r[1];
   assign gen_end   = enable_r[2] && !enable_r[1];

   always_ff @(posedge clk)
     if (!rstn)
       begin
          // '0 operation is available in SystemVerilog yeee :)
          enable_r      <= '0;
          type_r        <= '0;
          width_r       <= '0;
          height_r      <= '0;
          gen_start_d   <= '0;
          gen_cnts_on   <= '0;
       end
     else
       begin
          enable_r    <= {enable_r[1:0], tp_enable_i};
          gen_start_d <= gen_start;
          gen_end_d   <= gen_end_d;

          // Register new configuration only when new generation of data is
          //  requested
          if(gen_start)
            begin
               type_r        <= tp_type_i;
               width_r       <= tp_width_i;
               height_r      <= tp_height_i;
               gen_cnts_on   <= 1'b1;
            end
          if(gen_end)
            gen_cnts_on      <= 1'b0;
       end // else: !if(rstn)

   // counters
   logic [7:0]  frame_cnt;
   logic [10:0] pixel_cnt; // pixels in line
   logic [10:0] line_cnt;
   logic [15:0] free_cnt;
   logic        new_frame, new_frame_d;
   logic        new_line;
   logic        pause_pixel_cnt, pause_pixel_cnt_d;

   // more elegant way to do this?
   assign new_frame = gen_start || ((line_cnt == (height_r-1)) && new_line);
   assign new_line = ((pixel_cnt == (width_r-1)) && enable_r[2]);

   // TODO: This bug still exists - below 'the complicated fix' is commented out
   // with JJJ_TREADY
   // Dangerous situation - if we get TREADY=0 just when pixel_cnt is at the end
   // is very tricky situation - TLAST would be asserted for one clock cycle when
   // TREADY is low. We need to detect when TREADY goes low and freeze all output
   // changes until it's re-asserted. pixel_cnt is taken care of so it is freezed
   // More critical is the handling of AXI4-Stream output where we need to store
   // intermediate data (see below).
   // Need more testing.
   // Potentially the TVALID (with gen_start_d & gen_end_d) is still a problem.
   // But I think not critical because gen_end_d happens when we disable the
   // generator and should not produce anything.
   assign pause_pixel_cnt = !axi_stream_o.TREADY;

   always_ff @(posedge clk)
     if(!rstn)
       begin
          frame_cnt   <= '0;
          pixel_cnt   <= '0;
          line_cnt    <= '0;
          free_cnt    <= '0;
          new_frame_d <= '0;
       end
     else
       begin
          pause_pixel_cnt_d <= pause_pixel_cnt;
          if(!gen_cnts_on)
            begin
               frame_cnt <= '0;
               pixel_cnt <= '0;
               line_cnt <= '0;
            end
          else
            begin
               if(new_frame)
                 frame_cnt   <= frame_cnt + 1;

               if(new_line)
                 pixel_cnt   <= '0;
//JJJ_TREADY               else if(!pause_pixel_cnt && !pause_pixel_cnt_d)
               else if(!pause_pixel_cnt && !pause_pixel_cnt_d)
                 pixel_cnt <= pixel_cnt + 1;

               if(new_frame)
                 line_cnt <= '0;
               else if(new_line)
                 line_cnt <= line_cnt + 1;
            end // else: !if(gen_cnts_on)
          free_cnt <= free_cnt + 1;
          new_frame_d <= new_frame;
       end // else: !if(!rstn || !tp_enable_i)

//JJJ_TREADY   // AXI4-Stream frame generation
//JJJ_TREADY   logic freeze_output;
//JJJ_TREADY   logic [31:0] f_tdata;
//JJJ_TREADY   logic        f_new_frame_d;
//JJJ_TREADY   logic        f_new_line;

   always_ff @(posedge clk)
     if(!rstn || !tp_enable_i)
       begin
          axi_stream_o.TDATA  <= '0;
          axi_stream_o.TVALID <= '0;
          axi_stream_o.TUSER  <= '0;
          axi_stream_o.TLAST  <= '0;
          // Unused signal:
          axi_stream_o.TKEEP <= '1;
//JJJ_TREADY          freeze_output <= 1'b0;
//JJJ_TREADY          f_tdata <= '0;
//JJJ_TREADY          f_new_frame_d <= '0;
//JJJ_TREADY          f_new_line <= '0;
       end
     else
       begin
//JJJ_TREADY          if(!axi_stream_o.TREADY && !freeze_output)
//JJJ_TREADY            begin
//JJJ_TREADY               // we are going into the 'output freeze' mode - let's store
//JJJ_TREADY               // what should we output next
//JJJ_TREADY               f_tdata       <=  { free_cnt[7:0], frame_cnt[7:0],
//JJJ_TREADY                                   pixel_cnt[7:0], line_cnt[7:0] };
//JJJ_TREADY               f_new_frame_d <= new_frame_d;
//JJJ_TREADY               f_new_line    <= new_line;
//JJJ_TREADY               freeze_output = 1'b1;
//JJJ_TREADY            end
//JJJ_TREADY          else if(axi_stream_o.TREADY && freeze_output)
//JJJ_TREADY            begin
//JJJ_TREADY               freeze_output = 1'b0;
//JJJ_TREADY               // depends on the type I guess - for now hard-coded
//JJJ_TREADY               // Assuming TDATA = 32 bit!
//JJJ_TREADY               axi_stream_o.TDATA <= f_tdata;
//JJJ_TREADY
//JJJ_TREADY               // or'ing gen_start_d takes care of first SoF when TP is enabled
//JJJ_TREADY               if(f_new_frame_d)
//JJJ_TREADY                 axi_stream_o.TUSER[0] <= 1'b1;
//JJJ_TREADY               else
//JJJ_TREADY                 axi_stream_o.TUSER[0] <= 1'b0;
//JJJ_TREADY
//JJJ_TREADY               if(f_new_line)
//JJJ_TREADY                 axi_stream_o.TLAST    <= 1'b1;
//JJJ_TREADY               else
//JJJ_TREADY                 axi_stream_o.TLAST    <= 1'b0;
//JJJ_TREADY            end // if (axi_stream_o.TREADY && freeze_output)
//JJJ_TREADY          else if(!axi_stream_o.TREADY && freeze_output)
//JJJ_TREADY            begin
//JJJ_TREADY               // pass through without changes
//JJJ_TREADY            end
//JJJ_TREADY          else
            begin
               // depends on the type I guess - for now hard-coded
               // Assuming TDATA = 32 bit!
               axi_stream_o.TDATA <= { free_cnt[7:0], frame_cnt[7:0],
                                       pixel_cnt[7:0], line_cnt[7:0] };

               if(gen_start_d)
                 axi_stream_o.TVALID <= 1'b1;
               else if(gen_end_d)
                 axi_stream_o.TVALID <= 1'b0;

               // or'ing gen_start_d takes care of first SoF when TP is enabled
               if(new_frame_d)
                 axi_stream_o.TUSER[0] <= 1'b1;
               else
                 axi_stream_o.TUSER[0] <= 1'b0;

               if(new_line)
                 axi_stream_o.TLAST    <= 1'b1;
               else
                 axi_stream_o.TLAST    <= 1'b0;
            end // else: !if(axi_stream_o.TREADY && freeze_output)
       end
endmodule: axi_stream_tp_gen
