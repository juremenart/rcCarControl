`timescale 1 ps / 1 ps

module video_meas
  (
   axi4_stream_if.m axi_video_i,

   input logic video_meas_en_i,

   output logic [7:0] video_frames_cnt_o,
   output logic [23:0] video_frame_len_o,
   output logic [11:0] video_lines_cnt_o,
   output logic [11:0] video_pixel_cnt_o);

   logic               clk, rstn;

   logic               valid_data, sof, eol;

   logic [7:0] frame_cnt;
   logic [23:0] frame_len;
   logic [11:0] lines_cnt;
   logic [11:0] pixel_cnt;

   assign clk = axi_video_i.ACLK;
   assign rstn = axi_video_i.ARESETn;

   assign valid_data = (axi_video_i.TREADY & axi_video_i.TVALID);

   assign sof = valid_data & axi_video_i.TUSER[0];
   assign eol = valid_data & axi_video_i.TLAST;

   assign video_frames_cnt_o = frame_cnt;

   always_ff @(posedge clk)
     if(!rstn)
       begin
          frame_cnt <= '0;
          frame_len <= '0;
          lines_cnt <= '0;
          pixel_cnt <= '0;

          video_lines_cnt_o <= '0;
          video_pixel_cnt_o <= '0;
          video_frame_len_o <= '0;
       end
     else
       begin
          if(video_meas_en_i)
            begin
               if(valid_data)
                 begin
                    pixel_cnt <= pixel_cnt + 1;
                    frame_len <= frame_len + 1;

                    if(sof)
                      begin
                         frame_cnt         <= frame_cnt + 1;
                         video_frame_len_o <= frame_len;
                         frame_len         <= '0;
                         pixel_cnt         <= '0;
                         video_lines_cnt_o <= lines_cnt;
                         lines_cnt         <= '0;
                      end
                    if(eol)
                      begin
                         lines_cnt <= lines_cnt + 1;
                         pixel_cnt <= '0;
                         video_pixel_cnt_o <= pixel_cnt;
                      end
                 end // if (valid_data)
            end
          else
            begin
               frame_cnt <= '0;
               frame_len <= '0;
               pixel_cnt <= '0;
               lines_cnt <= '0;
            end
       end // else: !if(!rstn)

endmodule: video_meas
