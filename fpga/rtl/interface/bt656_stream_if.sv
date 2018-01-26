interface bt656_stream_if
  #(
    int unsigned DW = 8 // data width
    )
   (
    );

   // Header (EAV/SAV) definitions for 4th byte
   parameter HDR_BIT_1      = 7; // Bit7 - always set to '1'
   parameter HDR_BIT_FIELD  = 6; // Bit6 Field bit
   parameter HDR_BIT_VBLANK = 5; // Bit5 - Vertical blanking (this line will be blanking)
   parameter HDR_BIT_HBLANK = 4; // Bit4 - Horizontal blanking (if 0 = SAV else = EAV)
   parameter HDR_BIT_P3     = 3; // Bit3 - Protection bits - currently not used
   parameter HDR_BIT_P2     = 2; // Bit2 - Protection bits - currently not used
   parameter HDR_BIT_P1     = 1; // Bit1 - Protection bits - currently not used
   parameter HDR_BIT_P0     = 0; // Bit0 - Protection bits - currently not used

   logic          LLC;      // line-locked clock
   logic [DW-1:0] DATA;     // data
   logic          HREF;     // horizontal ref - ignored with current implementation
   logic          HSYNC;    // horizontal sync - ignored with current implementation
   logic          VSYNC;    // vertical sync - ignored with current implementation

   // Using this chip: http://www.analog.com/media/en/technical-documentation/data-sheets/ADV7280.PDF
   // BT.656 recommendation:
   // http://www.itu.int/dms_pubrec/itu-r/rec/bt/R-REC-BT.656-5-200712-I!!PDF-E.pdf

   modport s (
              output LLC,
              output DATA,
              output HREF,
              output HSYNC,
              output VSYNC
              );

   // drain
   modport d (
              input LLC,
              input DATA,
              input HREF,
              input HSYNC,
              input VSYNC
              );

   // monitor
   modport m (
              input LLC,
              input DATA,
              input HREF,
              input HSYNC,
              input VSYNC
              );

endinterface: bt656_stream_if
