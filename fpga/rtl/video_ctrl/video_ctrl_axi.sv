`timescale 1 ps / 1 ps

// TODO: Check where JJJ is in the comment - problem with using logic from interface axi4_lite_if during elaboration (only for simulation)

module video_ctrl_axi #(
  int unsigned DW = 32,          // Width of AXI data    bus
  int unsigned AW = 8,           // Width of AXI address bus
  int unsigned VER = 'hdead0100) // Version of video_ctrl module
   (
    // AXI-Lite slave
    axi4_lite_if.s axi_bus,

    // test pattern generator settings
    output logic        tp_en_gen_o,
    output logic [1:0]  tp_type_o,
    output logic [10:0] tp_width_o,
    output logic [10:0] tp_height_o,

    output logic        rx_enable_o,
    output logic        pure_bt656_o,
    input  logic [31:0] rx_size_status_i,
    output logic        rx_rst_size_err_o,
    input  logic [31:0] rx_frame_cnts_i
    );

   // Register map
   localparam VER_ADDR           = 6'h0; // RO  Version register
   localparam TP_CTRL_ADDR       = 6'h1; // R/W Test Pattern Control register
   localparam TP_SIZE_ADDR       = 6'h2; // R/W Test Size register (width, height)
   localparam RX_CTRL_ADDR       = 6'h3; // R/W Receiver control register
   localparam RX_SIZE_STAT_ADDR  = 6'h4; // R/C  Size Status register
   localparam RX_FRAME_CNTS_ADDR = 6'h5; // RO  Frame counter (with AXI clock)

   //----------------------------------------------
   //-- Signals for user logic register space example
   //------------------------------------------------
   localparam int unsigned ADDR_LSB = $clog2(DW/8);

   logic                   slv_reg_rden;
   logic                   slv_reg_wren;
   // AXI4LITE signals
   logic [AW-1:ADDR_LSB]   axi_awaddr;
   logic [AW-1:ADDR_LSB]   axi_araddr;

   // Test pattern registers
   reg                     tp_en_gen_r;
   reg [1:0]               tp_type_r;
   reg [10:0]              tp_width_r, tp_height_r;

   // BT656 receiver registers
   reg                     rx_enable;
   reg                     pure_bt656;

   // Write registers
   always_ff @(posedge axi_bus.ACLK)
     if (axi_bus.ARESETn == 1'b0)
       begin
          tp_en_gen_r <= 1'b0;
          tp_type_r   <= 2'b00;
          tp_width_r  <= 640;
          tp_height_r <= 480;
          rx_enable   <= 1'b0;
          pure_bt656  <= 1'b0;
       end
     else if (slv_reg_wren)
       begin
          rx_rst_size_err_o <= 1'b0;
          case (axi_awaddr)
            //VER_ADDR:
              // version register - read only
            TP_CTRL_ADDR:
              begin
                 tp_en_gen_r <= axi_bus.WDATA[0];
                 tp_type_r   <= axi_bus.WDATA[5:4];
              end
            TP_SIZE_ADDR:
              begin
                 tp_width_r  <= axi_bus.WDATA[10:0];
                 tp_height_r <= axi_bus.WDATA[26:16];
              end
            RX_CTRL_ADDR:
              begin
                 rx_enable   <= axi_bus.WDATA[0];
                 pure_bt656  <= axi_bus.WDATA[1];
              end
            RX_SIZE_STAT_ADDR:
              begin
                 rx_rst_size_err_o <= 1'b1;
              end
          endcase
       end

   // Address decoding for reading registers
   // Output register or memory read data
   // When there is a valid read address (ARVALID) with
   // acceptance of read address by the slave (ARREADY),
   always_ff @(posedge axi_bus.ACLK)
     if (slv_reg_rden)
       begin
          case (axi_araddr)
            VER_ADDR:
              axi_bus.RDATA <= VER;
            TP_CTRL_ADDR:
              axi_bus.RDATA <= { {26{1'b0}}, tp_type_r, {3{1'b0}}, tp_en_gen_r };
            TP_SIZE_ADDR:
              axi_bus.RDATA <= { {5{1'b0}}, tp_height_r, {5{1'b0}}, tp_width_r };
            RX_CTRL_ADDR:
              axi_bus.RDATA <= { {30{1'b0}}, pure_bt656, rx_enable };
            RX_SIZE_STAT_ADDR:
              axi_bus.RDATA <= rx_size_status_i;
            RX_FRAME_CNTS_ADDR:
              axi_bus.RDATA <= rx_frame_cnts_i;
            default:
              axi_bus.RDATA <= 32'hdeadbeef;
          endcase
       end

   // Assign outputs
   // TODO: Add synchornization to target clock domain
   // Test Pattern outputs
   assign tp_en_gen_o = tp_en_gen_r;
   assign tp_type_o   = tp_type_r;
   assign tp_width_o  = tp_width_r;
   assign tp_height_o = tp_height_r;

   assign rx_enable_o   = rx_enable;
   assign pure_bt656_o  = pure_bt656;

   // Example-specific design signals
   // local parameter for addressing 32 bit / 64 bit DW

   // Implement AWREADY generation
   // AWREADY is asserted for one ACLK clock cycle when both
   // AWVALID and WVALID are asserted. AWREADY is
   // de-asserted when reset is low.

   // slave is ready to accept write address when
   // there is a valid write address and write data
   // on the write address and data bus. This design
   // expects no outstanding transactions.
   // TODO: implement pipelining
   always_ff @(posedge axi_bus.ACLK)
     if (~axi_bus.ARESETn)
       axi_bus.AWREADY <= 1'b0;
     else
       axi_bus.AWREADY <= ~axi_bus.AWREADY & axi_bus.AWVALID & axi_bus.WVALID;

   // Implement axi_awaddr latching
   // This process is used to latch the address when both
   // AWVALID and WVALID are valid.

   // Write Address latching
   // TODO: remove reset
   // TODO: combine control signal
   always_ff @(posedge axi_bus.ACLK)
     if (~axi_bus.AWREADY & axi_bus.AWVALID & axi_bus.WVALID)
       axi_awaddr <= axi_bus.AWADDR [AW-1:ADDR_LSB];

   // Implement WREADY generation
   // WREADY is asserted for one ACLK clock cycle when both
   // AWVALID and WVALID are asserted. WREADY is
   // de-asserted when reset is low.

   // slave is ready to accept write data when
   // there is a valid write address and write data
   // on the write address and data axi_bus. This design
   // expects no outstanding transactions.
   always_ff @(posedge axi_bus.ACLK)
     if (~axi_bus.ARESETn)
       axi_bus.WREADY <= 1'b0;
     else
       axi_bus.WREADY <= ~axi_bus.WREADY & axi_bus.WVALID & axi_bus.AWVALID;

   // Implement memory mapped register select and write logic generation
   // The write data is accepted and written to memory mapped registers when
   // AWREADY, WVALID, WREADY and WVALID are asserted. Write strobes are used to
   // select byte enables of slave registers while writing.
   // These registers are cleared when reset (active low) is applied.
   // Slave register write enable is asserted when valid address and data are available
   // and the slave is ready to accept the write address and write data.
//JJJ   assign slv_reg_wren = axi_bus.Wtransfer & axi_bus.AWtransfer;
   assign slv_reg_wren = axi_bus.WVALID & axi_bus.WREADY &
                         axi_bus.AWVALID & axi_bus.AWREADY;


   // Implement write response logic generation
   // The write response and response valid signals are asserted by the slave
   // when WREADY, WVALID, WREADY and WVALID are asserted.
   // This marks the acceptance of address and indicates the status of
   // write transaction.

   always_ff @(posedge axi_bus.ACLK)
//JJJ     if (axi_bus.AWtransfer & ~axi_bus.BVALID & axi_bus.Wtransfer) begin
     if (slv_reg_wren & ~axi_bus.BVALID) begin
        // indicates a valid write response is available
        axi_bus.BRESP  <= 2'b0; // 'OKAY' response
        // work error responses in future
     end

   always_ff @(posedge axi_bus.ACLK)
     if (axi_bus.ARESETn == 1'b0)
       axi_bus.BVALID  <= 0;
     else
       begin
//JJJ        if (axi_bus.AWtransfer & ~axi_bus.BVALID & axi_bus.Wtransfer) begin
          if (slv_reg_wren & ~axi_bus.BVALID) begin
             // indicates a valid write response is available
             axi_bus.BVALID <= 1'b1;
          end else if (axi_bus.BREADY & axi_bus.BVALID) begin
             //check if bready is asserted while bvalid is high)
             //(there is a possibility that bready is always asserted high)
             axi_bus.BVALID <= 1'b0;
          end
       end

   // Implement ARREADY generation
   // ARREADY is asserted for one ACLK clock cycle when
   // ARVALID is asserted. AWREADY is
   // de-asserted when reset (active low) is asserted.
   // The read address is also latched when ARVALID is
   // asserted. axi_araddr is reset to zero on reset assertion.

   // indicates that the slave has acceped the valid read address
   always_ff @(posedge axi_bus.ACLK)
     if (axi_bus.ARESETn == 1'b0)
       axi_bus.ARREADY <= 1'b0;
     else
       axi_bus.ARREADY <= ~axi_bus.ARREADY & axi_bus.ARVALID;

   // Read address latching
   always_ff @(posedge axi_bus.ACLK)
     if (~axi_bus.ARREADY & axi_bus.ARVALID)
       axi_araddr <= axi_bus.ARADDR [AW-1:ADDR_LSB];

   // Implement axi_arvalid generation
   // RVALID is asserted for one ACLK clock cycle when both
   // ARVALID and ARREADY are asserted. The slave registers
   // data are available on the RDATA bus at this instance. The
   // assertion of RVALID marks the validity of read data on the
   // bus and RRESP indicates the status of read transaction.RVALID
   // is deasserted on reset (active low). RRESP and RDATA are
   // cleared to zero on reset (active low).
   always_ff @(posedge axi_bus.ACLK)
     if (slv_reg_rden)
       begin
        // Valid read data is available at the read data bus
        axi_bus.RRESP  <= 2'b0; // 'OKAY' response
     end

   always_ff @(posedge axi_bus.ACLK)
     if (axi_bus.ARESETn == 1'b0)
       begin
          axi_bus.RVALID <= 0;
       end
     else
       begin
        if (slv_reg_rden)
          begin
             // Valid read data is available at the read data bus
             axi_bus.RVALID <= 1'b1;
          end
//JJJ        else if (axi_bus.Rtransfer)
        else if (axi_bus.RVALID & axi_bus.RREADY)
          begin
             // Read data is accepted by the master
             axi_bus.RVALID <= 1'b0;
          end
       end

   // Implement memory mapped register select and read logic generation
   // Slave register read enable is asserted when valid address is available
   // and the slave is ready to accept the read address.
//JJJ   assign slv_reg_rden = axi_bus.ARtransfer & ~axi_bus.RVALID;
      assign slv_reg_rden = axi_bus.ARVALID & axi_bus.ARREADY & ~axi_bus.RVALID;

endmodule : video_ctrl_axi
