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

   rx_cfg_if.d  rx_cfg
   );

   logic              clk;
   logic              rstn;

   assign bt656_stream_i.RSTN = rx_cfg.cam_rstn;
   assign bt656_stream_i.PWDN = rx_cfg.cam_pwdn;

   // TODO: Should we have internal clock of the same speed as external for this
   // or would it be unnecessary complication? Be sure to connect external clock
   // to pin connected to clock network - this should do the trick
   assign clk  = bt656_stream_i.PCLK;
   assign rstn = rx_cfg.rx_enable;

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
   assign inc_line_cnt = (!rx_cfg.pure_bt656 && !in_hblank && bt656_stream_i.HREF) ||
                         (rx_cfg.pure_bt656 && sav_det);

   always_ff @(posedge clk)
     if(!rstn)
       begin
          state <= FSM_IDLE;
       end
     else
       begin
          if(!rx_cfg.rx_enable)
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
               if(in_new_frame && rx_cfg.pure_bt656)
                 nstate = FSM_SAV;
               else if(in_new_frame && !rx_cfg.pure_bt656)
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
               if(rx_cfg.pure_bt656)
                 begin
                    if(eav_det_d)
                      begin
                         latch_pixel_cnt <= 1'b1;
                         nstate = FSM_BLANKING;
                      end
                 end
               else
                 begin
                    if(!bt656_stream_i.HREF)
                      begin
                         latch_pixel_cnt <= 1'b1;
                         nstate = FSM_BLANKING;
                      end
                 end
            end
          FSM_BLANKING:
            begin
               if(rx_cfg.pure_bt656)
                 begin
                    if(sav_det_d && !in_vblank)
                      nstate = FSM_VDATA;
                    else if(sav_det_d && in_vblank)
                      nstate = FSM_BLANKING;
                 end
               else
                 begin
                    if(bt656_stream_i.HREF)
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
   assign hdr_det = rx_cfg.pure_bt656 &&
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
     = ((in_vsync_field_d && !in_vsync_field) && rx_cfg.pure_bt656) ||
       ((!in_vsync_field_d && in_vsync_field && in_vsync_valid) &&
        !rx_cfg.pure_bt656);

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
          if(rx_cfg.pure_bt656)
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
            end // if (rx_cfg.pure_bt656)
          else
            begin
               if(bt656_stream_i.VSYNC == 1'b0)
                 in_vsync_valid = 1'b1;
               in_vsync_field <= bt656_stream_i.VSYNC;
               in_hblank      <= bt656_stream_i.HREF;
            end // else: !if(rx_cfg.pure_bt656)
       end // else: !if(!rstn)

   // Data packing and putting to FIFO
   // We receive:
   // Cb0 Y0 Cr0 Y1 and we must create two output data packets:
   // Cb0Cr0Y0 and Cb0Cr0Y1
   logic [11:0] pixel_cnt;
   logic [11:0] line_cnt;
   logic [11:0] width_cnt, height_cnt;

   logic        width_err, height_err;

   logic        fifo_full;
   logic        fifo_write;
   logic [31:0] size_fifo_data;
   logic        sync_rst_size_err;

   // Write DATA FIFO signals
   // See data_fifo_i instantiation and READ signals below

   logic        data_fifo_write;
   logic        data_fifo_full;

   // Size FIFO (only status)
   assign size_fifo_data = { height_err, {3{1'b0}}, height_cnt,
                             width_err, {3{1'b0}}, width_cnt };

   assign data_fifo_write = (state == FSM_VDATA) && !data_fifo_full;

   sync_pulse sync_rst_size_err_i
     (.rstn_i(rx_cfg.rx_enable),

      .a_clk_i(axi_clk_i),
      .a_pulse_i(rx_cfg.rst_size_err),

      .b_clk_i(clk),
      .b_pulse_o(sync_rst_size_err));

   always_ff @(posedge clk)
     if(!rstn)
       begin
          pixel_cnt       <= '0;
          width_cnt       <= '1;
          height_cnt      <= '1;
          width_err       <= '0;
          height_err      <= '0;
          line_cnt        <= '0;
          fifo_write      <= 1'b0;
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
       end // else: !if(!rstn)

   // AXI clock domain

   // Length of frame with AXI clock
   logic [31:0] frame_length;
   logic        sync_in_new_frame;

   sync_pulse sync_new_frame_i
     (.rstn_i(rx_cfg.rx_enable),

      // input
      .a_clk_i(clk),
      .a_pulse_i(in_new_frame),

      // output
      .b_clk_i(axi_clk_i),
      .b_pulse_o(sync_in_new_frame));

   always_ff @(posedge axi_clk_i)
     if(!axi_rstn_i)
       begin
          frame_length           <= '0;
          rx_cfg.frame_length    <= '0;
          rx_cfg.frame_cnts      <= '0;
       end
     else
       begin
          if(rx_cfg.rx_enable)
            begin
               if(sync_in_new_frame)
                 begin
                    rx_cfg.frame_cnts   <= rx_cfg.frame_cnts + 1;
                    rx_cfg.frame_length <= frame_length;
                    frame_length   <= '0;
                 end
               else
                 frame_length <= frame_length + 1;
            end
       end // else: !if(!axi_rstn_i)


   // Size status synchronization
   logic [31:0] size_status;
   logic        read_empty;
   logic        read_fifo;

   assign read_fifo = !read_empty;

   // Size status FIFO (status only)
   // TODO: Change this FIFO with normal sync of this (we don't need FIFO really)
   async_fifo #(.DATA_WIDTH(32), .ADDRESS_WIDTH(2)) size_status_fifo_i
   (// Reading - AXI4-Lite part
    .Data_out(rx_cfg.size_status),
    .Empty_out(read_empty),
    .ReadEn_in(read_fifo),
    .RClk(axi_clk_i),

    // Writing - the PCLK part
    .Data_in(size_fifo_data),
    .Full_out(fifo_full),
    .WriteEn_in(fifo_write),
    .WClk(clk),

    // TODO: Probably not the best to use this async signal?
    .Clear_in(!rx_cfg.rx_enable),
    .Read_WordCount_out());

   // DATA FIFO
   // Current size is 8 x 2048 (should be more then comfortable for 720p - 1280x720
   // resoltuion to store ~1.5 lines)
   // Remember each pixel is actually 16 bit, so two entries
   logic        data_fifo_empty, data_fifo_empty_d, data_fifo_read, data_fifo_read_d;
   logic [7:0]  data_fifo_read_data;
   logic [10:0] data_fifo_read_words;
   logic        stream_in_new_frame; // we already have sync_in_new_frame for AXI-Lite :)

   logic        fifo_start_read; // start read when write pointer >= fifo_start_read configuration

   // TODO: FIFO Debugging status - sync it correctly to AXI-Lite or remove it!
   assign rx_cfg.data_fifo_rd_read = data_fifo_read;
   assign rx_cfg.data_fifo_rd_empty = data_fifo_empty;
   assign rx_cfg.data_fifo_rd_data = data_fifo_read_data;
   assign rx_cfg.data_fifo_rd_words = data_fifo_read_words;
   assign rx_cfg.data_fifo_rd_state = axi_state;
   assign rx_cfg.data_fifo_wr_write = data_fifo_write;
   assign rx_cfg.data_fifo_wr_full = data_fifo_full;
   assign rx_cfg.data_fifo_wr_data = in_data[0];

   async_fifo #(.DATA_WIDTH(8), .ADDRESS_WIDTH(11)) data_fifo_i
     ( // Read part - fast AXI Stream running @ 100MHz
      .Data_out(data_fifo_read_data),
      .Empty_out(data_fifo_empty),
      .ReadEn_in(data_fifo_read),
      .RClk(axi_stream_o.ACLK),

       // Write part - slow input video @ 24 MHz
       .Data_in(in_data[0]),
       .WriteEn_in(data_fifo_write),
       .Full_out(data_fifo_full),
       .WClk(clk),

       .Clear_in(!rstn),

       // Read pointer at fast AXI stream clock domain
       .Read_WordCount_out(data_fifo_read_words));

   always_ff @(posedge axi_stream_o.ACLK)
     if(!rstn)
       begin
          fifo_start_read <= 1'b0;
       end
     else
       begin
          // NOTE: data_fifo_start is in AXI-Lite clock domain but
          // it should be counted as static setting (??) during
          // operation so one can false-path it
          if(data_fifo_read_words >= rx_cfg.data_fifo_start)
            fifo_start_read <= 1'b1;
          else
            fifo_start_read <= 1'b0;
       end

   sync_pulse sync_stream_new_frame_i
     (.rstn_i(rx_cfg.rx_enable),

      // input
      .a_clk_i(clk),
      .a_pulse_i(in_new_frame),

      // output
      .b_clk_i(axi_stream_o.ACLK),
      .b_pulse_o(stream_in_new_frame));

   // We perform read until when we get start_read (which must be set
   // high enough to be sure full line can be transfered - and then we
   // readout full line and stop reading (until next time)

   // We check this in this nice simple state machine
   enum logic [1:0]
        {
         AXI_FSM_IDLE = 2'b00,
         AXI_FSM_SOF  = 2'b01,
         AXI_FSM_DATA = 2'b10
         } axi_state, axi_nstate;

   logic [10:0] fifo_pixel_cnt;
   logic        fifo_pixel_cnt_eol;
   logic        rst_fifo_pixel_cnt;
   logic        inc_fifo_pixel_cnt;

   logic        stream_frame_latch;
   logic        stream_clear_frame_latch;

   always_ff @(posedge axi_stream_o.ACLK)
     if(!rstn)
       begin
          stream_frame_latch <= 1'b0;
       end
     else
       begin
          if(stream_in_new_frame)
            stream_frame_latch <= 1'b1;
          else if(stream_clear_frame_latch)
            stream_frame_latch <= 1'b0;
       end // else: !if(!rstn)

   always_ff @(posedge axi_stream_o.ACLK)
     if(!rstn)
       begin
          axi_state <= AXI_FSM_IDLE;
          fifo_pixel_cnt <= '0;
          fifo_pixel_cnt_eol <= 1'b0;
          data_fifo_empty_d  <= 1'b0;
          data_fifo_read_d   <= 1'b0;
       end
     else
       begin
          data_fifo_empty_d  <= data_fifo_empty;
          data_fifo_read_d   <= data_fifo_read;

          if(!rx_cfg.rx_enable)
            axi_state <= AXI_FSM_IDLE;
          else
            axi_state <= axi_nstate;

          if(rst_fifo_pixel_cnt)
            begin
               fifo_pixel_cnt <= '0;
            end
          else if(inc_fifo_pixel_cnt)
            begin
               fifo_pixel_cnt_eol <= 1'b0;
               if(fifo_pixel_cnt == (rx_cfg.data_fifo_line_len-1))
                 begin
                    fifo_pixel_cnt_eol <= 1'b1;
                    fifo_pixel_cnt <= '0;
                 end
               else
                 fifo_pixel_cnt <= fifo_pixel_cnt + 1;
            end
       end

   assign axi_stream_o.TDATA = (axi_state != AXI_FSM_IDLE) ? data_fifo_read_data : 'X;

   always_comb
     begin
        axi_stream_o.TKEEP    = 1'b1;

        axi_stream_o.TVALID   = 1'b0;
        axi_stream_o.TUSER[0] = 1'b0;
        axi_stream_o.TLAST    = 1'b0;

        axi_nstate = axi_state;

        data_fifo_read = 1'b1;
        stream_clear_frame_latch = 1'b0;

        inc_fifo_pixel_cnt = 1'b0;
        rst_fifo_pixel_cnt = 1'b0;

        case(axi_state)
          AXI_FSM_IDLE:
            begin
               data_fifo_read = 1'b0;
               rst_fifo_pixel_cnt = 1'b1;

               if(fifo_start_read)
                 begin
                    // Check if it's new frame or just new line
                    axi_nstate            = AXI_FSM_SOF;
                    data_fifo_read        = 1'b1;
                    rst_fifo_pixel_cnt    = 1'b0;
                    inc_fifo_pixel_cnt    = 1'b1;
                 end
            end
          AXI_FSM_SOF:
            begin
               // TODO: Danger is we have FIFO empty at this stage?
               axi_nstate            = AXI_FSM_DATA;
               data_fifo_read        = 1'b1;
               axi_stream_o.TVALID   = 1'b1;
               inc_fifo_pixel_cnt    = 1'b1;

               if(stream_frame_latch)
                 begin
                    axi_stream_o.TUSER[0] = 1'b1;
                    stream_clear_frame_latch <= 1'b1;
                 end
            end
          AXI_FSM_DATA:
            begin
               data_fifo_read        = 1'b1;
               axi_stream_o.TVALID   = 1'b1;
               inc_fifo_pixel_cnt    = 1'b1;

               // Always when we restart reading we must wait 1 clock cycle
               if(!data_fifo_read_d)
                 begin
                    inc_fifo_pixel_cnt  = 1'b0;
                    axi_stream_o.TVALID = 1'b0;
                 end

               if(fifo_pixel_cnt_eol)
                 begin
                    rst_fifo_pixel_cnt  = 1'b1;
                    data_fifo_read      = 1'b0;
                    axi_stream_o.TLAST  = 1'b1;
                    axi_nstate          = AXI_FSM_IDLE;
                 end
               else if(!axi_stream_o.TREADY || data_fifo_empty_d)
                 begin
                    // If not ready wait for bus to become ready
                    inc_fifo_pixel_cnt  = 1'b0;
                    data_fifo_read      = 1'b0;
                    axi_stream_o.TVALID = 1'b0;
                 end
               end
            endcase; // case: AXI_FSM_DATA
        end // always_comb

endmodule: bt656_to_axi_stream
