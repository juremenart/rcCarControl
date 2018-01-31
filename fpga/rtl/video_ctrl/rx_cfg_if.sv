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
              output data_fifo_line_len
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
              input  data_fifo_line_len
              );


   // monitor
   modport m (
              input  rx_enable,
              input  pure_bt656,
              input  size_status,
              input  rst_size_err,
              input  frame_cnts,
              input  frame_length,
              input  cam_pwdn,
              input  cam_rstn,
              input  data_fifo_start,
              input  data_fifo_line_len
              );


endinterface: rx_cfg_if
