`timescale 1 ps / 1 ps

module bt656_to_axi_stream
  #(
    int unsigned DW = 8 // data width
    )
  (
   bt656_stream_if.d bt656_stream_i,

   axi4_stream_if.s axi_stream_o,

   // AXI4-Lite configuration
   input logic axi_clk_i,
   input logic axi_rstn_i,

   input logic rx_enable_i,
   input logic pure_bt656_i, // 'pure' is probably not correct name - this means use header from data to control the stream and ignore all control signals
   input logic ignore_href_i,
   input logic ignore_hsync_i,
   input logic ignore_vsync_i,

   output logic [31:0] size_status_o,
   input logic         rst_size_err_i,
   output logic [31:0] frame_cnts_o);

   logic              clk;
   logic              rstn;

   // TODO: Should we have internal clock of the same speed as external for this
   // or would it be unnecessary complication? Be sure to connect external clock
   // to pin connected to clock network - this should do the trick
   assign clk  = bt656_stream_i.PCLK;
   assign rstn = rx_enable_i;

   // state machine signals
   enum logic [2:0]
        {
         FSM_IDLE     = 3'b000,
         FSM_BLANKING = 3'b001,
         FSM_SAV      = 3'b010,
         FSM_VDATA    = 3'b011
         } state, nstate;

   // Input data FFs (on input video clock domain)
   parameter input_data_ffs = 4;
   logic [DW-1:0] in_data [input_data_ffs-1:0];

   // header EAV/SAV/blanking/field detection logic
   logic         hdr_det, eav_det, sav_det;
   logic         eav_det_d, sav_det_d;
   logic         in_hblank, in_vblank;

   logic         in_vsync_valid;
   logic         in_vsync_field, in_vsync_field_d, in_new_frame;

   logic         rst_pixel_cnt, rst_line_cnt;
   logic         inc_line_cnt;
   logic         latch_pixel_cnt;

   assign rst_line_cnt = in_new_frame;
   assign inc_line_cnt = (!pure_bt656_i && !in_hblank && bt656_stream_i.HREF) ||
                         (pure_bt656_i && sav_det);

   always_ff @(posedge clk)
     if(!rstn)
       begin
          state <= FSM_IDLE;
       end
     else
       begin
          if(!rx_enable_i)
            state <= FSM_IDLE;
          else
            state <= nstate;
       end

   always_comb
     begin
        nstate          = state;
        rst_pixel_cnt   = 1'b1;
        latch_pixel_cnt = 1'b0;
        case (state)
          FSM_IDLE:
            begin
               // Wait for start of new frame (ignore all other headers)
               // When new frame is detected wait for SAV
               if(in_new_frame && pure_bt656_i)
                 nstate = FSM_SAV;
               else if(in_new_frame && !pure_bt656_i)
                 nstate = FSM_BLANKING;
            end
          FSM_SAV:
            begin
               if(sav_det_d && !in_vblank)
                 nstate = FSM_VDATA;
               else if(sav_det_d && in_vblank)
                 nstate = FSM_BLANKING;
            end
          FSM_VDATA:
            begin
               // Active data only here
               rst_pixel_cnt = 1'b0;
               if(pure_bt656_i)
                 begin
                    if(eav_det_d)
                      begin
                         latch_pixel_cnt <= 1'b1;
                         nstate = FSM_BLANKING;
                      end
                 end
               else
                 begin
                    if(!bt656_stream_i.HREF && !ignore_href_i)
                      begin
                         latch_pixel_cnt <= 1'b1;
                         nstate = FSM_BLANKING;
                      end
                 end
            end
          FSM_BLANKING:
            begin
               if(pure_bt656_i)
                 begin
                    if(sav_det_d && !in_vblank)
                      nstate = FSM_VDATA;
                    else if(sav_det_d && in_vblank)
                      nstate = FSM_BLANKING;
                 end
               else
                 begin
                    if(bt656_stream_i.HREF && !ignore_href_i)
                      begin
                         nstate = FSM_VDATA;
                      end
                 end
            end
        endcase; // case (state)
     end // always_comb

   // Input data FFs
   always_ff @(posedge clk)
     if(!rstn)
       begin
          int i;
          for(i = 0; i < input_data_ffs; i++)
            begin
               in_data[i] <= 'b0;
            end
       end
     else
       begin
          int i;
          in_data[0] <= bt656_stream_i.DATA;
          for(i = 1; i < input_data_ffs; i++)
            begin
               in_data[i] <= in_data[i-1];
            end
       end // else: !if(!rstn)

   // Input line/field/frame detection
   // Do header/SAV/EAV detection only if configured for 'pure bt656'
   assign hdr_det = pure_bt656_i &&
                    (in_data[input_data_ffs-1] == 8'hFF) &&
                    (in_data[input_data_ffs-2] == 8'h00) &&
                    (in_data[input_data_ffs-3] == 8'h00) &&
                    (in_data[input_data_ffs-4][bt656_stream_i.HDR_BIT_1] == 1'b1);
   assign eav_det = hdr_det &&
                    (in_data[input_data_ffs-4][bt656_stream_i.HDR_BIT_HBLANK] == 1'b1);
   assign sav_det = hdr_det &&
                    (in_data[input_data_ffs-4][bt656_stream_i.HDR_BIT_HBLANK] == 1'b0);

   // for VSYNC frame detection we invert the polarity - it wouldn't be necessary though
   assign in_new_frame
     = ((in_vsync_field_d && !in_vsync_field) && pure_bt656_i) ||
       ((!in_vsync_field_d && in_vsync_field && in_vsync_valid) && !pure_bt656_i && !ignore_vsync_i);

   always_ff @(posedge clk)
     if(!rstn)
       begin
          in_hblank          <= 1'b0;
          in_vblank          <= 1'b0;
          // This complication with in_vsync_valid is to avoid detection of new
          // frame if we are coming out of reset when VSYNC = 1'b1 (we first wait
          // it to be 1'b0 before start detecting frame
          in_vsync_valid     <= 1'b0;
          in_vsync_field     <= 1'b0;
          in_vsync_field_d   <= 1'b0;
          eav_det_d          <= 1'b0;
          sav_det_d          <= 1'b0;
       end
     else
       begin
          in_vsync_field_d <= in_vsync_field;
          if(pure_bt656_i)
            begin
               // in FSM we use [eav|sav]_det_d (delayed) so we always have also
               // registered values of in_hblank/in_vblank/in_vsync_field
               eav_det_d       <= eav_det;
               sav_det_d       <= sav_det;
               if(hdr_det)
                 begin
                    in_hblank
                      <= in_data[input_data_ffs-4][bt656_stream_i.HDR_BIT_HBLANK];
                    in_vblank
                      <= in_data[input_data_ffs-4][bt656_stream_i.HDR_BIT_VBLANK];
                    in_vsync_field
                      <= in_data[input_data_ffs-4][bt656_stream_i.HDR_BIT_FIELD];
                 end
            end // if (pure_bt656_i)
          else
            begin
               if(bt656_stream_i.VSYNC == 1'b0)
                 in_vsync_valid = 1'b1;
               in_vsync_field <= bt656_stream_i.VSYNC;
               in_hblank      <= bt656_stream_i.HREF;
            end // else: !if(pure_bt656_i)
       end // else: !if(!rstn)

   // Data packing and putting to FIFO
   // We receive:
   // Cb0 Y0 Cr0 Y1 and we must create two output data packets:
   // Cb0Cr0Y0 and Cb0Cr0Y1
   logic [7:0] cb, cr, y0;
   logic [11:0] pixel_cnt;
   logic [11:0] line_cnt;
   logic [31:0] packed_data [1:0];
   logic [11:0] width_cnt, height_cnt;

   // TODO: Add clear possibility for clear these flags
   logic        width_err, height_err;

   logic        fifo_full;
   logic        fifo_write;
   logic [31:0] size_fifo_data;
   logic        sync_rst_size_err;

   assign size_fifo_data = { width_err, {3{1'b0}}, width_cnt,
                             height_err, {3{1'b0}}, height_cnt };

   sync_pulse sync_rst_size_err_i
     (.rstn_i(rx_enable_i),

      .a_clk_i(axi_clk_i),
      .a_pulse_i(rst_size_err_i),

      .b_clk_i(clk),
      .b_pulse_o(sync_rst_size_err));

   always_ff @(posedge clk)
     if(!rstn)
       begin
          pixel_cnt <= '0;
          cb <= '0;
          cr <= '0;
          y0 <= '0;
          packed_data[0] <= '0;
          packed_data[1] <= '0;
          width_cnt  <= '1;
          height_cnt <= '1;
          width_err <= '0;
          height_err <= '0;
          line_cnt <= '0;
          fifo_write <= 1'b0;
       end
     else
       begin
          fifo_write <= 1'b0;
          if(sync_rst_size_err)
            begin
               width_err <= 1'b0;
               height_err <= 1'b0;
            end
          if(latch_pixel_cnt)
            begin
               // divide by two - we have 2 datas for each pixel - and add one
               // before that because we are counting from 0
               logic [12:0] pixel_cnt_msbs = (pixel_cnt+1);
               if(((width_cnt != pixel_cnt_msbs[12:1]) && (width_cnt != {12{1'b1}})))
                 width_err <= 1'b1;
               width_cnt <= pixel_cnt_msbs[12:1];
            end
          else if(rst_pixel_cnt)
            pixel_cnt <= '0;
          else if(state == FSM_VDATA)
            pixel_cnt <= pixel_cnt + 1;

          if(rst_line_cnt)
            begin
               if(((height_cnt != line_cnt) && (line_cnt != {12{1'b1}})))
                 height_err <= 1'b1;
               height_cnt <= line_cnt;
               line_cnt <= '0;
               if(height_cnt != {12{1'b1}})
                 fifo_write <= 1'b1;
            end
          else if(inc_line_cnt)
            line_cnt <= line_cnt + 1;

          // TODO: Change this code - no need to pack now - we send out the same YCbCr 4:2:2 as
          // we received (at least currently) - so we simply push to FIFO received data
          if((state == FSM_VDATA) && (!eav_det_d && pure_bt656_i))
            begin
               case(pixel_cnt[1:0])
                 2'b00: // Cb
                   cb <= in_data[1];
                 2'b01: // Y0
                   y0 <= in_data[1];
                 2'b10:
                   cr <= in_data[1];
                 2'b11:
                   begin
                      // TODO: Push to FIFO
                      packed_data[0] <= { cb, cr, y0 };
                      packed_data[1] <= { cb, cr, in_data[1] };
                   end
               endcase; // case (pixel_cnt)
            end
       end // else: !if(!rstn)

   // AXI clock domain

   // Length of frame with AXI clock
   logic [31:0] frame_cnts;
   logic        sync_in_new_frame;

   sync_pulse sync_new_frame_i
     (.rstn_i(rx_enable_i),

      // input
      .a_clk_i(clk),
      .a_pulse_i(in_new_frame),

      // output
      .b_clk_i(axi_clk_i),
      .b_pulse_o(sync_in_new_frame));

   always_ff @(posedge axi_clk_i)
     if(!axi_rstn_i)
       begin
          frame_cnts      <= '0;
          frame_cnts_o    <= '0;
       end
     else
       begin
          if(rx_enable_i)
            begin
               if(sync_in_new_frame)
                 begin
                    frame_cnts_o <= frame_cnts;
                    frame_cnts   <= '0;
                 end
               else
                 frame_cnts <= frame_cnts + 1;
            end
       end // else: !if(!axi_rstn_i)


   // Size status synchronization
   logic [31:0] size_status;
   logic        read_empty;
   logic        read_fifo;

   assign read_fifo = !read_empty;

   async_fifo #(.DATA_WIDTH(32), .ADDRESS_WIDTH(1)) size_status_fifo_i
   (// Reading - AXI4-Lite part
    .Data_out(size_status_o),
    .Empty_out(read_empty),
    .ReadEn_in(read_fifo),
    .RClk(axi_clk_i),

    // Writing - the PCLK part
    .Data_in(size_fifo_data),
    .Full_out(fifo_full),
    .WriteEn_in(fifo_write),
    .WClk(clk),

    // TODO: Probably not the best to use this async signal?
    .Clear_in(!rx_enable_i));

endmodule: bt656_to_axi_stream
