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

   logic          PCLK;     // pixel clock, line-locked clock
   logic [DW-1:0] DATA;     // data
   logic          HREF;     // horizontal ref
   logic          HSYNC;    // horizontal sync - ignored with current implementation
   logic          VSYNC;    // vertical sync

   logic          RSTN;     // Sensor RESETn (active low)
   logic          PWDN;     // Sensor Power Down (active high)

   // Using this chip: http://www.analog.com/media/en/technical-documentation/data-sheets/ADV7280.PDF
   // BT.656 recommendation:
   // http://www.itu.int/dms_pubrec/itu-r/rec/bt/R-REC-BT.656-5-200712-I!!PDF-E.pdf
   // Was never tested with pure BT.656 but rather with OV5642 DVP

   modport s (
              output PCLK,
              output DATA,
              output HREF,
              output HSYNC,
              output VSYNC,
              input  RSTN,
              input  PWDN
              );

   // drain
   modport d (
              input PCLK,
              input DATA,
              input HREF,
              input HSYNC,
              input VSYNC,
              output RSTN,
              output PWDN
              );

   // monitor
   modport m (
              input PCLK,
              input DATA,
              input HREF,
              input HSYNC,
              input VSYNC,
              input RSTN,
              input PWDN
              );

endinterface: bt656_stream_if
