interface axi4_stream_if
  #(
    int unsigned DW = 32 // data width
    )
   (
    input logic ACLK ,    // clock
    input logic ARESETn   // reset - active low
    );

   logic [DW-1:0] TDATA ;   // data
   logic [3:0]    TKEEP ;   // keep
   logic          TLAST ;   // last
   logic          TREADY;   // ready
   logic [0:0]    TUSER;    // tuser
   logic          TVALID;   // valid

   logic          transf;

   assign transf = TVALID & TREADY;

   // From: UG1037 - Vivado AXI Reference Guide
   // When AXI4-Stream Video is used the following applies
   // Video data           = TDATA
   // Valid                = TVALID
   // Ready                = TREADY
   // Start of Frame (SOF) = TUSER (only bit 0 of TUSER is used others ignored))
   // End Of Line (EOL)    = TLAST
   // TKEEP/TSTRB are ignored and should use default values TKEEP=1 and
   // TSTRB=1
   modport s (
              input  ACLK ,
              input  ARESETn,
              output TDATA ,
              output TKEEP ,
              output TLAST ,
              output TVALID,
              output TUSER ,
              input  TREADY
              );

   // drain
   modport d (
              input  ACLK ,
              input  ARESETn,
              input  TDATA ,
              input  TKEEP ,
              input  TLAST ,
              input  TVALID,
              input  TUSER ,
              output TREADY
              );

   // monitor
   modport m (
              input ACLK ,
              input ARESETn,
              input TDATA ,
              input TKEEP ,
              input TLAST ,
              input TVALID,
              input TUSER ,
              input TREADY
              );

endinterface: axi4_stream_if
