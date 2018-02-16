`timescale 1 ps / 1 ps

// TODO: Check where JJJ is in the comment - problem with using logic from interface axi4_lite_if during elaboration (only for simulation)

module sys_ctrl_axi #(
  int unsigned DW = 32, // Width of AXI data    bus
  int unsigned AW = 4, // Width of AXI address bus
  int unsigned VER = 'hdead0100, // Version of sys_ctrl module
  int          PWM_CNT_WIDTH = 24) // width of PWM counters
   (
    // AXI-Lite slave
    axi4_lite_if.s axi_bus,

    // PWM signals for drive/steer
    output logic pwm_mux_sel_o, // MUX selector for PWM sources (either ext or sys)

    output logic                     pwm_enable_o,
    output logic [PWM_CNT_WIDTH-1:0] pwm_period_o,
    output logic [PWM_CNT_WIDTH-1:0] pwm_active_0_o,
    output logic [PWM_CNT_WIDTH-1:0] pwm_active_1_o,

    output logic video_meas_en_o,
    input  logic [7:0] video_frames_cnt_i,
    input  logic [23:0] video_frame_len_i,
    input  logic [11:0] video_lines_cnt_i,
    input  logic [11:0] video_pixel_cnt_i
    );

   // Register map
   localparam VER_ADDR        = 6'h0; // RO  Version register
   localparam IOCTRL_ADDR     = 6'h1; // R/W IO Control register
   localparam PWM_CTRL_STATUS = 6'h2; // R/W PWM Ctrl/Status register
   localparam PWM_PERIOD      = 6'h3; // R/W PWM Period
   localparam PWM_ACTIVE_0    = 6'h4; // R/W PWM Active (high) for PWM0
   localparam PWM_ACTIVE_1    = 6'h5; // R/W PWM Active (high) for PWM1
   localparam VID_CTRL_STATUS = 6'h6; // R/W Video Input Measurement (debugging only)
   localparam VID_FRM_STATUS  = 6'h7; //  RO Video Input Measurement (Frame status)

   //----------------------------------------------
   //-- Signals for user logic register space example
   //------------------------------------------------
   localparam int unsigned ADDR_LSB = $clog2(DW/8);

   logic                   slv_reg_rden;
   logic                   slv_reg_wren;
   // AXI4LITE signals
   logic [AW-1:ADDR_LSB]   axi_awaddr;
   logic [AW-1:ADDR_LSB]   axi_araddr;

   logic            pwm_mux_sel;
   logic            pwm_enable;
   logic [PWM_CNT_WIDTH-1:0] pwm_period;
   logic [PWM_CNT_WIDTH-1:0] pwm_active_0;
   logic [PWM_CNT_WIDTH-1:0] pwm_active_1;

   logic                     video_meas_en;

   assign pwm_mux_sel_o  = pwm_mux_sel;
   assign pwm_enable_o   = pwm_enable;
   assign pwm_period_o   = pwm_period;
   assign pwm_active_0_o = pwm_active_0;
   assign pwm_active_1_o = pwm_active_1;

   assign video_meas_en_o = video_meas_en;

   // Write registers
   always_ff @(posedge axi_bus.ACLK)
     if (axi_bus.ARESETn == 1'b0)
       begin
          pwm_mux_sel <= 1'b0;
          video_meas_en <= 1'b0;
       end
     else if (slv_reg_wren)
       begin
          case (axi_awaddr)
            //VER_ADDR:
              // version register - read only
            IOCTRL_ADDR:
              pwm_mux_sel   <= axi_bus.WDATA[0];
            PWM_CTRL_STATUS:
              pwm_enable    <= axi_bus.WDATA[0];
            PWM_PERIOD:
              pwm_period    <= axi_bus.WDATA[PWM_CNT_WIDTH-1:0];
            PWM_ACTIVE_0:
              pwm_active_0  <= axi_bus.WDATA[PWM_CNT_WIDTH-1:0];
            PWM_ACTIVE_1:
              pwm_active_1  <= axi_bus.WDATA[PWM_CNT_WIDTH-1:0];
            VID_CTRL_STATUS:
              video_meas_en <= axi_bus.WDATA[31];
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
            IOCTRL_ADDR:
              axi_bus.RDATA <= { {31{1'b0}}, pwm_mux_sel };
            PWM_CTRL_STATUS:
              axi_bus.RDATA <= { {8{1'b0}}, PWM_CNT_WIDTH[7:0], {7{1'b0}}, pwm_enable};
            PWM_PERIOD:
              axi_bus.RDATA <= { {(32-PWM_CNT_WIDTH){1'b0}}, pwm_period };
            PWM_ACTIVE_0:
              axi_bus.RDATA <= { {(32-PWM_CNT_WIDTH){1'b0}}, pwm_active_0 };
            PWM_ACTIVE_1:
              axi_bus.RDATA <= { {(32-PWM_CNT_WIDTH){1'b0}}, pwm_active_1 };
            VID_CTRL_STATUS:
              axi_bus.RDATA <= { video_meas_en, {3{1'b0}},
                                 video_lines_cnt_i, {4{1'b0}},
                                 video_pixel_cnt_i };
            VID_FRM_STATUS:
              axi_bus.RDATA <= { video_frames_cnt_i, video_frame_len_i };
            default:
              axi_bus.RDATA <= 32'hdeadbeef;
          endcase
       end

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
     if (axi_bus.ARESETn == 1'b0)  axi_bus.BVALID  <= 0;
     else begin
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

endmodule : sys_ctrl_axi
