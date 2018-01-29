`timescale 1 ps / 1 ps

typedef struct {
 int start_line;
 int stop_line;
 int field;
 int v_blank;
 } line_params_t;

// Simple BT.656 generator
module bt656_stream_gen
  (
   input logic ACLK,
   input logic ARESETn,

   bt656_stream_if.s bt656_stream_o,

   // some configuration
   input logic pure_bt656_i
   );

   // If pure_bt656_i == 0 the mimiced interface is from DVP of OV5642 sensor (without HSYNC)
   // http://www.uctronics.com/download/cam_module/OV5642DS.pdf
   // If pure_bt656_i == 1 then we do:
   // Change the values for V/HBlank here to produce:
   // These values taken from: http://www.itu.int/dms_pubrec/itu-r/rec/bt/R-REC-BT.656-5-200712-I!!PDF-E.pdf
   // But can be reduced for debugigng proposes
   // For nice reference table one can check also: https://www.intersil.com/content/dam/Intersil/documents/an97/an9728.pdf

   parameter num_hblank = 10;
   parameter num_pixels = 20; // number of pixels in line
   parameter num_lines = 16;
   const line_params_t line_params[7] = {
                              //   SL,  EL,  F,  Vs
                                {   1,   2,  0,  1 },
                                {   3,   6,  0,  0 },
                                {   7,   8,  0,  1 },
                                {   9,  10,  1,  1 },
                                {  11,  14,  1,  0 },
                                {  15,  16,  1,  1 },
                                {  -1,  -1, -1, -1 } };

//   // 525/60 Video stream
//   parameter num_hblank = 272;
//   parameter num_pixels = 640;
//   parameter num_lines = 525;
//   // Field & Vertical blanking
//   line_params_t line_params = {
//                              //   SL,  EL,  F,  V
//                                {   1,   3,  1,  1 },
//                                {   4,  20,  0,  1 },
//                                {  21, 263,  0,  0 },
//                                { 264, 265,  0,  1 },
//                                { 266, 282,  1,  1 },
//                                { 283, 525,  1,  0 } };
//
//   // 625/50 Video stream
//   parameter num_hblank = 344;
//   parameter num_pixels = 640;
//   parameter num_lines = 625;
//   // Field & Vertical blanking
//   line_params_t line_params =
//                              //   SL,  EL,  F,  V
//                                {   1,  22,  0,  1 },
//                                {  23, 310,  0,  0 },
//                                { 311, 312,  0,  1 },
//                                { 313, 335,  1,  1 },
//                                { 336, 623,  1,  0 },
//                                { 624, 625,  1,  1 } };

   logic clk, rstn;
   assign clk  = ACLK;
   // TODO: take care of the proper reset if needed
   assign rstn = ARESETn;

   // state machine signals
   enum logic [2:0]
        {
         FSM_IDLE     = 3'b000,
         FSM_SAV      = 3'b011,
         FSM_VDATA    = 3'b100,
         FSM_EAV      = 3'b001,
         FSM_BLANKING = 3'b010
         } state, nstate;

      always_ff @(posedge clk)
     if(!rstn)
       begin
          state <= #1 FSM_IDLE;
       end
     else
       begin
          state <= #1 nstate;
       end

   logic [31:0]  frame_num;
   logic [31:0]  pixel_num;
   logic [31:0]  line_num;
   logic         inc_pixel_num, rst_pixel_num;
   logic         inc_line_num, rst_line_num;
   logic         inc_frame_num, rst_frame_num;

   always_comb
     begin
        nstate = state;
        inc_pixel_num <= 1'b1;
        rst_pixel_num <= 1'b0;
        inc_line_num <= 1'b0;
        rst_line_num <= 1'b0;
        inc_frame_num <= 1'b0;
        rst_frame_num <= 1'b0;
        case (state)
          FSM_IDLE:
            begin
               // Wait for EAV to start new line
               rst_line_num  <= 1'b1;
               rst_pixel_num <= 1'b1;
               rst_frame_num <= 1'b1;

               nstate <= FSM_EAV;
            end
          FSM_EAV:
            begin
               if(pixel_num >= 4)
                 nstate <= FSM_BLANKING;
            end
          FSM_BLANKING:
            begin
               if(pixel_num >= (4+num_hblank))
                 nstate <= FSM_SAV;
            end
          FSM_SAV:
            begin
               if(pixel_num >= (8+num_hblank))
                 nstate <= FSM_VDATA;
            end
          FSM_VDATA:
            begin
               if(pixel_num >= (8+num_hblank+2*num_pixels))
                 begin
                    nstate        <= FSM_EAV;
                    rst_pixel_num <= 1'b1;

                    if(line_num == num_lines)
                      begin
                         rst_line_num <= 1'b1;
                         inc_frame_num <= 1'b1;
                      end
                    else
                      inc_line_num <= 1'b1;
                 end
            end
        endcase; // case (state)
     end // always_comb

   // Counters of pixels/lines
   always_ff @(posedge clk)
     if(!rstn)
       begin
          pixel_num <= 1;
          line_num  <= 1;
          frame_num <= 1;
       end
     else
       begin
          if(rst_pixel_num)
               pixel_num <= 1;
          else if(inc_pixel_num)
               pixel_num <= pixel_num + 1;

          if(rst_line_num)
            line_num <= 1;
          else if(inc_line_num)
            line_num <= line_num +1;

          if(rst_frame_num)
            frame_num <= 1;
          else if(inc_frame_num)
            frame_num <= frame_num + 1;
       end // else: !if(!rstn)


   logic       out_clk_en;
   logic [9:0] out_data;
   logic [9:0] hdr_data; // header data for EAV & SAV
   logic       hdr_f, hdr_v, hdr_h; // header info of current lines

   // for OV5642 (!pure_bt656_i generation)
   logic       dvp_vsync, dvp_href;

   assign bt656_stream_o.PCLK  = (out_clk_en) ? ACLK : 1'b0;
   assign bt656_stream_o.DATA  = out_data;
   assign bt656_stream_o.HSYNC = (pure_bt656_i == 1'b1) ? hdr_h : 1'b0;
   assign bt656_stream_o.VSYNC = (pure_bt656_i == 1'b1) ? hdr_v : dvp_vsync;
   assign bt656_stream_o.HREF  = (pure_bt656_i == 1'b1) ? 1'b0  : dvp_href;

   // Very simple frame formation :)
   always_ff @(posedge clk)
     if(!rstn)
       begin
          out_clk_en <= 1'b0;
          out_data   <= '0;
          hdr_data   <= '0;
          hdr_f <= 1'b0;
          hdr_v <= 1'b0;
          hdr_h <= 1'b0;
       end
     else
       begin
          if((state == FSM_EAV) || (state == FSM_SAV))
            begin
               // Find correct F & V parameters
               int i;
               hdr_h <= (state == FSM_EAV) ? 1'b1 : 1'b0;

               for(i = 0; line_params[i].start_line != -1; i++)
                 begin
                    if((line_params[i].start_line <= line_num) &&
                       (line_num <= line_params[i].stop_line))
                      begin
                         hdr_f <= line_params[i].field;
                         hdr_v <= line_params[i].v_blank;
                         break;
                      end
                 end // for (i = 0; line_params[i].start_line != -1; i++)

               // We have 3 clock cycles to set it
               hdr_data <= { 1'b1                  ,    // Always set to 1
                             hdr_f                 ,    // Field
                             hdr_v                 ,    // Vertical blanking
                             hdr_h                 ,    // Horizontal blanking - set for EAV
                             hdr_v ^ hdr_h         ,    // P3 - v xor h
                             hdr_f ^ hdr_h         ,    // P2 - f xor h
                             hdr_f ^ hdr_v         ,    // P1 - f xor v
                             hdr_f ^ hdr_v ^ hdr_h };   // P0 - f xor v xor h
            end

          out_clk_en <= 1'b1;
          dvp_href   <= 1'b0;
          dvp_vsync  <= 1'b0;

          // TODO: Check if we should produce something else if !pure_bt656
          case(state)
            FSM_EAV:
              begin
                 // We 'abuse' EAV stste - the VSYNC pulse is probably not the
                 // right length but it should suffice for our TB I guess -
                 //  otherwise complicate it and make it real :)
                 if(line_num == 1'b1)
                   dvp_vsync <= 1'b1;

                 case(pixel_num-1)
                   0: out_data <= 10'h3FF;
                   1: out_data <= 10'h000;
                   2: out_data <= 10'h000;
                   3: out_data <= hdr_data;
                   default: out_data <= 10'h3DE; // should not happen
                 endcase; // case (pixel_num)
              end
            FSM_BLANKING:
              begin
                 if((pixel_num[0]-1) == 1'b0)
                   out_data <= 10'h080;
                 else
                   out_data <= 10'h010;
              end
            FSM_SAV:
              begin
                 case(pixel_num-1-4-num_hblank)
                   0: out_data <= 10'h3FF;
                   1: out_data <= 10'h00;
                   2: out_data <= 10'h00;
                   3: out_data <= hdr_data;
                   default: out_data <= 10'h3DE; // should not happen
                 endcase; // case (pixel_num)
              end
            FSM_VDATA:
              begin
                 // TODO: Add real data here? If needed it should go here (be careful
                 // not to ignore vertical blanking
                 logic [9:0] pixel_num_lsb=pixel_num-1-8-num_hblank;

                 if(hdr_v == 1'b0)
                   dvp_href <= 1'b1;

                 case(pixel_num_lsb[1:0])
                   0: out_data <= (hdr_v == 1'b1) ? 10'h80 : { hdr_f, 9'b101001011 }; // Cb
                   1: out_data <= (hdr_v == 1'b1) ? 10'h10 : { pixel_num_lsb[9:0] }; //  Y0
                   2: out_data <= (hdr_v == 1'b1) ? 10'h80 : { line_num[9:0] }; // Cr
                   3: out_data <= (hdr_v == 1'b1) ? 10'h10 : { frame_num[9:0] }; //  Y1
                   default: out_data <= 8'hDE; // should not happen
                 endcase; // case (pixel_num_lsb[1:0])
              end
          endcase; // case state
       end
endmodule: bt656_stream_gen

