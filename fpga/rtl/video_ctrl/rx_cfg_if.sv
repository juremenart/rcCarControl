// Running in AXI4-Lite domain
interface rx_cfg_if
  (
   );

   logic rx_enable;
   logic pure_bt656;  // 'pure' is probably not correct name - this means use header from data to control the stream and ignore all control signals

   logic [31:0] size_status;
   logic        rst_size_err;
   logic [31:0] frame_cnts;
   logic [31:0] frame_length;
   logic        cam_pwdn;
   logic        cam_rstn;
   logic [10:0] data_fifo_start;
   logic [10:0] data_fifo_line_len;

   // TODO: This is not sync'd - it's directly connected from FIFO (debugging
   // purposes - later either make it correct or remove it
   logic        data_fifo_rd_read;
   logic        data_fifo_rd_empty;
   logic [7:0]  data_fifo_rd_data;
   logic [10:0] data_fifo_rd_words;
   logic [1:0]  data_fifo_rd_state;

   logic        data_fifo_wr_write;
   logic        data_fifo_wr_full;
   logic [7:0]  data_fifo_wr_data;

   // Source
   modport s (
              output rx_enable,
              output pure_bt656,
              input  size_status,
              output rst_size_err,
              input  frame_cnts,
              input  frame_length,
              output cam_pwdn,
              output cam_rstn,
              output data_fifo_start,
              output data_fifo_line_len,

              // fifo debugging status
              input  data_fifo_rd_read,
              input  data_fifo_rd_empty,
              input  data_fifo_rd_data,
              input  data_fifo_rd_words,
              input  data_fifo_rd_state,
              input  data_fifo_wr_write,
              input  data_fifo_wr_full,
              input  data_fifo_wr_data
              );

   // drain
   modport d (
              input  rx_enable,
              input  pure_bt656,
              output size_status,
              input  rst_size_err,
              output frame_cnts,
              output frame_length,
              input  cam_pwdn,
              input  cam_rstn,
              input  data_fifo_start,
              input  data_fifo_line_len,

              // fifo debugging status
              output data_fifo_rd_read,
              output data_fifo_rd_empty,
              output data_fifo_rd_data,
              output data_fifo_rd_words,
              output data_fifo_rd_state,
              output data_fifo_wr_write,
              output data_fifo_wr_full,
              output data_fifo_wr_data
              );


   // monitor
   modport m (
              input rx_enable,
              input pure_bt656,
              input size_status,
              input rst_size_err,
              input frame_cnts,
              input frame_length,
              input cam_pwdn,
              input cam_rstn,
              input data_fifo_start,
              input data_fifo_line_len,

              // fifo debugging status
              input data_fifo_rd_read,
              input data_fifo_rd_empty,
              input data_fifo_rd_data,
              input data_fifo_rd_words,
              input data_fifo_rd_state,
              input data_fifo_wr_write,
              input data_fifo_wr_full,
              input data_fifo_wr_data
              );


endinterface: rx_cfg_if
