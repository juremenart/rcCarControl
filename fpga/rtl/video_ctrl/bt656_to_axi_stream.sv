`timescale 1 ps / 1 ps

module bt656_to_axi_stream
  (
   bt656_stream_if.d bt656_stream_i,

   axi4_stream_if.s axi_stream_o,

   // AXI4-Lite configuration
   input logic axi_clk_i,
   input logic axi_rstn_i,

   input logic rx_enable_i
   // configuration
   // ...
   );

   logic              clk;

   // TODO: Should we have internal clock of the same speed as external for this
   // or would it be unnecessary complication? Be sure to connect external clock
   // to pin connected to clock network - this should do the trick
   assign clk  = bt656_stream_i.LLC;

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
   logic [8-1:0] in_data [input_data_ffs-1:0];

   // header EAV/SAV/blanking/field detection logic
   logic         hdr_det, eav_det, sav_det;
   logic         eav_det_d, sav_det_d;
   logic         in_hblank, in_vblank, in_field, in_field_d, in_new_frame;
   logic         rst_pixel_cnt;

   always_ff @(posedge clk)
     if(!axi_rstn_i)
       begin
          state <= FSM_IDLE;
       end
     else
       begin
          if(!rx_enable_i)
            rx_enable_i <= FSM_IDLE;
          else
            state <= nstate;
       end

   always_comb
     begin
        nstate = state;
        rst_pixel_cnt = 1'b1;
        case (state)
          FSM_IDLE:
            begin
               // Wait for start of new frame (ignore all other headers)
               // When new frame is detected wait for SAV
               if(in_new_frame)
                 nstate = FSM_SAV;
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
               if(eav_det_d)
                 nstate = FSM_BLANKING;
            end
          FSM_BLANKING:
            begin
               if(sav_det_d && !in_vblank)
                 nstate = FSM_VDATA;
               else if(sav_det_d && in_vblank)
                 nstate = FSM_BLANKING;
            end
        endcase; // case (state)
     end // always_comb

   // Input data FFs
   always_ff @(posedge clk)
     if(!axi_rstn_i)
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
       end // else: !if(!axi_rstn_i)

   // Input line/field/frame detection
   assign hdr_det = (in_data[input_data_ffs-1] == 8'hFF) &&
                    (in_data[input_data_ffs-2] == 8'h00) &&
                    (in_data[input_data_ffs-3] == 8'h00) &&
                    (in_data[input_data_ffs-4][bt656_stream_i.HDR_BIT_1] == 1'b1);
   assign eav_det = hdr_det &&
                    (in_data[input_data_ffs-4][bt656_stream_i.HDR_BIT_HBLANK] == 1'b1);
   assign sav_det = hdr_det &&
                    (in_data[input_data_ffs-4][bt656_stream_i.HDR_BIT_HBLANK] == 1'b0);

   assign in_new_frame = (in_field_d && !in_field);

   always_ff @(posedge clk)
     if(!axi_rstn_i)
       begin
          in_hblank    <= 1'b0;
          in_vblank    <= 1'b0;
          in_field     <= 1'b0;
          in_field_d   <= 1'b0;
          eav_det_d    <= 1'b0;
          sav_det_d    <= 1'b0;
       end
     else
       begin
          // in FSM we use [eav|sav]_det_d (delayed) so we always have also
          // registered values of in_hblank/in_vblank/in_field
          in_field_d <= in_field;
          eav_det_d  <= eav_det;
          sav_det_d  <= sav_det;
          if(hdr_det)
            begin
               in_hblank <= in_data[input_data_ffs-4][bt656_stream_i.HDR_BIT_HBLANK];
               in_vblank <= in_data[input_data_ffs-4][bt656_stream_i.HDR_BIT_VBLANK];
               in_field <= in_data[input_data_ffs-4][bt656_stream_i.HDR_BIT_FIELD];
            end
       end // else: !if(!axi_rstn_i)

   // Data packing and putting to FIFO
   // We receive:
   // Cb0 Y0 Cr0 Y1 and we must create two output data packets:
   // Cb0Cr0Y0 and Cb0Cr0Y1
   logic [7:0] cb, cr, y0;
   logic [1:0] pixel_cnt;
   logic [24:0] packed_data [1:0];

   always_ff @(posedge clk)
     if(!axi_rstn_i)
       begin
          pixel_cnt <= '0;
          cb <= '0;
          cr <= '0;
          y0 <= '0;
          packed_data[0] <= '0;
          packed_data[1] <= '0;
       end
     else
       begin
          if(rst_pixel_cnt)
            pixel_cnt <= '0;
          else if(state == FSM_VDATA)
            pixel_cnt <= pixel_cnt + 1;

          // TODO: Change this code - no need to pack now - we send out the same YCbCr 4:2:2 as
          // we received (at least currently) - so we simply push to FIFO received data
          if((state == FSM_VDATA) && !eav_det_d)
            begin
               case(pixel_cnt)
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
       end
endmodule: bt656_to_axi_stream
