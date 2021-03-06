`timescale 1ps / 1ps

module video_ctrl_tb
  #(
    // time periods
    realtime TP = 20.0ns, // 50MHz
    realtime V_TP = 41.667ns // 24MHz
    // RTL config
    );

   ////////////////////////////////////////////////////////////////////////////////
   // signal generation
   ////////////////////////////////////////////////////////////////////////////////

   logic                                    clk ;
   logic                                    rstn;
   logic                                    s_clk;
   logic                                    s_rstn;
   logic                                    b_clk;
   logic                                    b_rstn;

   logic                                    pure_bt656;
   logic                                    axi_vdma_introut;
   logic [5:0]                              s2mm_frame_ptr_in;
   logic [5:0]                              s2mm_frame_ptr_out;
   logic                                    vdma_frame_int;
   // to check either BT656 generation or OV DVP like interface
   assign pure_bt656 = 1'b0;

   // Clock
   initial        clk = 1'b1;
   always #(TP/2) clk = ~clk;

   // Stream clock (currently hard-coded to twice the AXI speed)
   initial        s_clk = 1'b1;
   always #(TP/2) s_clk = ~s_clk;

   // BT656 PCLK
   initial b_clk = 1'b1;
   always #(V_TP/2) b_clk = ~b_clk;

   // Reset
   initial begin
      rstn = 1'b0;
      repeat(10) @(posedge clk);
      rstn = 1'b1;
      repeat(20) @(posedge clk);
   end

   initial begin
      s_rstn = 1'b0;
      repeat(10) @(posedge s_clk);
      s_rstn = 1'b1;
   end

   initial begin
      b_rstn = 1'b0;
      repeat(10) @(posedge b_clk);
      b_rstn = 1'b1;
   end

   ////////////////////////////////////////////////////////////////////////////////
   // test sequence
   ////////////////////////////////////////////////////////////////////////////////

   logic [32-1:0] ver;
   logic [32-1:0] video_meas1, video_meas2;
   logic [32-1:0] vdma_status, vdma_version;
   logic [32-1:0] rx_ctrl_reg;

   // BE CAREFUL: Should match the size coming from bt656_stream_gen if not using
   // internal VIDEO_CTRL TP generator (bt656_stream_gen is not very programmable
   // from outside - it has this fancy array of blanking dpeending on hblank/pixels/lines/...
   logic [11:0] sim_height = 12'h010;
   logic [11:0] sim_weight = 12'h018;
   logic [7:0] sim_num_frames = 20;
   logic [23:0] tp_blanking = sim_height * sim_weight * 2;

   logic        use_tp_gen = 0;

   logic [5:0]  bin_ptr;
   logic [5:0]  prot_buffer_ptr;

   parameter    num_buffers = 3;
   const logic[32-1:0] dst_addr[num_buffers] =
                       {
                        32'h27c0_0000,
                        32'h28c0_0000,
                        32'h29c0_0000
                        };

   initial begin
      repeat(20) @(posedge clk);
      video_stream.TREADY <= 1'b1;
      axi_read('h00, ver);

      // dummy frame (we have only 3 FBs so we set currently slave read to something illegal

      $display("Video controller version = 0x%08x", ver);

      axi_vdma_read('h2C, vdma_version);
      $display("VDMA version=0x%08x", vdma_version);

      // Reset VDMA S2MM engine (and wait for it to be released)
      axi_vdma_write('h30, {{29{1'b0}}, 1'b1, 2'b00}); // VDMA reset

      do
        begin
           axi_vdma_read('h30, vdma_status);
        end
      while(vdma_status & 4);

      // Clear potential error bits
      axi_vdma_write('h34, 0);

      // Start the engine and wait for stat bit to be asserted
      axi_vdma_write('h28, 32'h0000_0100 );
//      axi_vdma_write('h30, { {8{1'b0}}, sim_num_frames, 16'h701b });
      axi_vdma_write('h30, { 32'h0000400b });
//      axi_vdma_write('h30, { {8{1'b0}}, 8'h1, 16'h701b });

      do
        begin
           axi_vdma_read('h34, vdma_status);
        end
      while(vdma_status & 1); // HALT goes low in STATUS_REG

      // REG_INDEX - write 0
      axi_vdma_write('h44, 0);

      // START_ADDRESS from 0xAC - 0xE8
//      axi_vdma_write('hAC, 32'h27c0_0000);
//      axi_vdma_write('hB0, 32'h28c0_0000);
//      axi_vdma_write('hB4, 32'h29c0_0000);
      for(int i = 0; i < num_buffers; i++)
        begin
           axi_vdma_write('hAC + i*4, dst_addr[i][32-1:0]);
        end

      // PART_PTR
      axi_vdma_write('h28, 0);

      // HSIZE=0xA4, FRMDLY_STRIDE=0xA8
      axi_vdma_write('hA8, {{20{1'b0}}, sim_weight[10:0], 1'b0}); // weight * 2 (2 bytes for pixel)
      axi_vdma_write('hA4, {{20{1'b0}}, sim_weight[10:0], 1'b0}); // weight * 2 (2 bytes for pixel)

      // VSIZE must be last!
      axi_vdma_write('hA0, {{20{1'b0}}, sim_height});

      axi_vdma_read('h30, vdma_status);
      $display("VDMA CTRL (0x30)=0x%08x", vdma_status);
      axi_vdma_read('hA0, vdma_status);
      $display("VDMA VSIZE (0xA0)=0x%08x", vdma_status);
      axi_vdma_read('hA4, vdma_status);
      $display("VDMA HSIZE (0xA4)=0x%08x", vdma_status);
      axi_vdma_read('hA8, vdma_status);
      $display("VDMA FRMDLY_STRIDE (0xA8)=0x%08x", vdma_status);
      axi_vdma_read('h34, vdma_status);
      $display("VDMA status=0x%08x", vdma_status);

      axi_sys_write('h18, 32'h8000_0000); // Enable video measurement in sys_ctrl

      // Enable also source of the video streaming
      if(use_tp_gen == 1'b1)
        begin
           // VIDEO_CTRL Test pattern generator
           axi_write('h08, {{4{1'b0}}, sim_height, {3{1'b0}}, sim_weight[10:0], 1'b0} );
           axi_write('h04, { tp_blanking, 8'h01 }); // AXI FIFO write set to line * 2
//           axi_write('h08, 32'h01e0_0500); // AXI FIFO write set to line * 2
//           axi_write('h04, 32'h000a_0007 );

        end
      else
        begin
           // BT656 to Stream module
           axi_write('h1C, { {5{1'b0}}, sim_weight, {5{1'b0}}, sim_weight[10:0], 1'b0 } );
           axi_write('h0C, 32'h0000_0015);
        end

      do
        begin
           @(posedge axi_vdma_introut or posedge vdma_frame_int)
             begin
                axi_vdma_read('h34, vdma_status);
                $display("Interrupt detected: 0x%08x", vdma_status);

                axi_read('h0C, rx_ctrl_reg);
                $display("RX Control register: 0x%08x", rx_ctrl_reg);

                gray2bin(rx_ctrl_reg[29:24], bin_ptr);

                $display("Buffer available from pointer %d, address: 0x%08x",
                         bin_ptr, dst_addr[bin_ptr]);

                // protect this buffer for two frames
                if(sim_num_frames[1:0] == 2'b00)
                  begin
                     bin2gray(bin_ptr, prot_buffer_ptr);
                     $display("Protecting read buffer %d (%d), address: 0x%08x",
                              bin_ptr, prot_buffer_ptr, dst_addr[bin_ptr]);
                     axi_write('h0C,  {rx_ctrl_reg[31:22], prot_buffer_ptr,
                                      rx_ctrl_reg[15:0]});
                  end
                else
                  begin
                     axi_write('h0C, rx_ctrl_reg);
                  end
                sim_num_frames <= sim_num_frames - 1;

                if(axi_vdma_introut)
                  begin
                     $display("AXI VDMA Interrupt detected, quitting");
                     $finish;
                  end
             end // do begin
        end // do begin
      while(sim_num_frames > 0);

      axi_sys_read('h18, video_meas1);
      axi_sys_read('h1C, video_meas2);
      $display("SYS Video measurement 0x%08x 0x%08x", video_meas1, video_meas2);


      axi_read('h14, video_meas1);
      axi_read('h18, video_meas2);
      $display("VIDEO_CTRL measurement 0x%08x 0x%08x", video_meas1, video_meas2);


      $finish();
   end

   task axi_write (
                   input logic [32-1:0] adr,
                   input logic [32-1:0] dat
                   );
      int                               b;
      bus_master.WriteTransaction (
                                   .AWDelay (1),  .aw ('{
                                                         addr: adr,
                                                         prot: 0
                                                         }),
                                   .WDelay (0),   .w ('{
                                                        data: dat,
                                                        strb: '1
                                                        }),
                                   .BDelay (0),   .b (b)
                                   );
   endtask: axi_write

   task axi_read (
                  input logic [32-1:0]  adr,
                  output logic [32-1:0] dat
                  );
      logic [32+2-1:0]                  r;
      bus_master.ReadTransaction (
                                  .ARDelay (0),  .ar ('{
                                                        addr: adr,
                                                        prot: 0
                                                        }),
                                  .RDelay (0),   .r (r)
                                  //     .RDelay (0),   .r ('{
                                  //                          data: dat,
                                  //                          resp: rsp
                                  //                         })
                                  );
      dat = r >> 2;
   endtask: axi_read

   task axi_sys_write (
                   input logic [32-1:0] adr,
                   input logic [32-1:0] dat
                   );
      int                               b;
      bus_sys_master.WriteTransaction (
                                   .AWDelay (1),  .aw ('{
                                                         addr: adr,
                                                         prot: 0
                                                         }),
                                   .WDelay (0),   .w ('{
                                                        data: dat,
                                                        strb: '1
                                                        }),
                                   .BDelay (0),   .b (b)
                                   );
   endtask: axi_sys_write

   task axi_sys_read (
                  input logic [32-1:0]  adr,
                  output logic [32-1:0] dat
                  );
      logic [32+2-1:0]                  r;
      bus_sys_master.ReadTransaction (
                                  .ARDelay (0),  .ar ('{
                                                        addr: adr,
                                                        prot: 0
                                                        }),
                                  .RDelay (0),   .r (r)
                                  //     .RDelay (0),   .r ('{
                                  //                          data: dat,
                                  //                          resp: rsp
                                  //                         })
                                  );
      dat = r >> 2;
   endtask: axi_sys_read

   task axi_vdma_write (
                   input logic [32-1:0] adr,
                   input logic [32-1:0] dat
                   );
      int                               b;
      bus_vdma_master.WriteTransaction (
                                   .AWDelay (1),  .aw ('{
                                                         addr: adr,
                                                         prot: 0
                                                         }),
                                   .WDelay (0),   .w ('{
                                                        data: dat,
                                                        strb: '1
                                                        }),
                                   .BDelay (0),   .b (b)
                                   );
   endtask: axi_vdma_write

   task axi_vdma_read (
                  input logic [32-1:0]  adr,
                  output logic [32-1:0] dat
                  );
      logic [32+2-1:0]                  r;
      bus_vdma_master.ReadTransaction (
                                  .ARDelay (0),  .ar ('{
                                                        addr: adr,
                                                        prot: 0
                                                        }),
                                  .RDelay (0),   .r (r)
                                  //     .RDelay (0),   .r ('{
                                  //                          data: dat,
                                  //                          resp: rsp
                                  //                         })
                                  );
      dat = r >> 2;
   endtask: axi_vdma_read

   task gray2bin (input logic [5:0] gray,
                  output logic [5:0] bin);
     bin[5] = gray[5];
     for(int i=4; i >= 0; i=i-1)
        begin : bin_out_gen
          bin[i] = gray[i] ^ bin[i+1];
        end
      bin = bin -1; // Gray really starts with 1
      bin = bin % num_buffers; // Gray is always done on ^2 so we need to 'wrap' it

      repeat(5) @(posedge clk);
   endtask: gray2bin

   task bin2gray (input logic [5:0] bin,
                  output logic [5:0] gray);
      bin = bin + 1;
      gray <= {bin[5], bin[4:0] ^ bin[5:1]};

      repeat(5) @(posedge clk);
   endtask: bin2gray

   ////////////////////////////////////////////////////////////////////////////////
   // module instances
   ////////////////////////////////////////////////////////////////////////////////
   axi4_lite_if axi4_lite (.ACLK (clk), .ARESETn (rstn));
   axi4_lite_if axi4_sys_lite (.ACLK (clk), .ARESETn (rstn));
   axi4_lite_if axi4_vdma_lite (.ACLK (clk), .ARESETn (rstn));

   axi4_stream_if #(.DW(8)) video_stream (.ACLK(s_clk), .ARESETn(s_rstn));
   bt656_stream_if bt656_stream();

   axi4_lite_master bus_master (.intf (axi4_lite));
   axi4_lite_master bus_sys_master (.intf (axi4_sys_lite));
   axi4_lite_master bus_vdma_master (.intf (axi4_vdma_lite));

   video_ctrl_top video_ctrl_top_i
     (
      .axi_bus(axi4_lite),
      .axi_video_o(video_stream),
      .bt656_video_i(bt656_stream),
      .vdma_frame_ptr_i(s2mm_frame_ptr_out),
      .vdma_frame_ptr_o(s2mm_frame_ptr_in),
      .vdma_frame_int_o(vdma_frame_int)
      );

   sys_ctrl_top sys_ctrl_top_i
     (.axi_bus(axi4_sys_lite),
      .pwm0_pad_o(),
      .pwm1_pad_o(),
      .pwm0_pad_i(1'b0),
      .pwm1_pad_i(1'b0),
      .axi_video_i(video_stream));

   bt656_stream_gen bt656_stream_gen_i
     (.ACLK(b_clk),
      .ARESETn(b_rstn),
      .bt656_stream_o(bt656_stream),
      .pure_bt656_i(pure_bt656));


   // 'modeled'
   logic                                m_axi_s2mm_bvalid;
   logic                                m_axi_s2mm_wlast;
   bd_system_axi_vdma_0_0 axi_vdma_i
     (.s_axi_lite_aclk(axi4_vdma_lite.ACLK),
      .m_axi_s2mm_aclk(video_stream.ACLK),
      .s_axis_s2mm_aclk(video_stream.ACLK),
      .axi_resetn(axi4_vdma_lite.ARESETn),
      .s_axi_lite_awvalid(axi4_vdma_lite.AWVALID),
      .s_axi_lite_awready(axi4_vdma_lite.AWREADY),
      .s_axi_lite_awaddr(axi4_vdma_lite.AWADDR),
      .s_axi_lite_wvalid(axi4_vdma_lite.WVALID),
      .s_axi_lite_wready(axi4_vdma_lite.WREADY),
      .s_axi_lite_wdata(axi4_vdma_lite.WDATA),
      .s_axi_lite_bresp(axi4_vdma_lite.BRESP),
      .s_axi_lite_bvalid(axi4_vdma_lite.BVALID),
      .s_axi_lite_bready(axi4_vdma_lite.BREADY),
      .s_axi_lite_arvalid(axi4_vdma_lite.ARVALID),
      .s_axi_lite_arready(axi4_vdma_lite.ARREADY),
      .s_axi_lite_araddr(axi4_vdma_lite.ARADDR),
      .s_axi_lite_rvalid(axi4_vdma_lite.RVALID),
      .s_axi_lite_rready(axi4_vdma_lite.RREADY),
      .s_axi_lite_rdata(axi4_vdma_lite.RDATA),
      .s_axi_lite_rresp(axi4_vdma_lite.RRESP),
      .s_axis_s2mm_tdata(video_stream.TDATA),
      .s_axis_s2mm_tkeep(video_stream.TKEEP),
      .s_axis_s2mm_tuser(video_stream.TUSER),
      .s_axis_s2mm_tvalid(video_stream.TVALID),
      .s_axis_s2mm_tready(video_stream.TREADY),
      .s_axis_s2mm_tlast(video_stream.TLAST),
      .s2mm_introut(axi_vdma_introut),

      .s2mm_frame_ptr_in(s2mm_frame_ptr_in),
      .s2mm_frame_ptr_out(s2mm_frame_ptr_out),
      .m_axi_s2mm_awaddr(), // JJJ
      .m_axi_s2mm_awlen(), // JJJ
      .m_axi_s2mm_awsize(), // JJJ
      .m_axi_s2mm_awburst(), // JJJ
      .m_axi_s2mm_awprot(), // JJJ
      .m_axi_s2mm_awcache(), // JJJ
      .m_axi_s2mm_awvalid(), // JJJ
      .m_axi_s2mm_awready(1'b1), // JJJ - be always ready
      .m_axi_s2mm_wdata(), // JJJ
      .m_axi_s2mm_wstrb(), // JJJ
      .m_axi_s2mm_wlast(m_axi_s2mm_wlast), // JJJ
      .m_axi_s2mm_wvalid(), // JJJ
      .m_axi_s2mm_wready(1'b1), // JJJ
      .m_axi_s2mm_bresp(2'b00), // JJJ
      .m_axi_s2mm_bvalid(m_axi_s2mm_bvalid), // JJJ
      .m_axi_s2mm_bready());

   // BVALID = WLAST delayed for one clock cycle - seems to work (I know not real
   // interface verification :) )
   always_ff @(posedge axi4_vdma_lite.ACLK)
        if(!axi4_vdma_lite.ARESETn)
          begin
             m_axi_s2mm_bvalid = '0;
          end
        else
          begin
             m_axi_s2mm_bvalid = m_axi_s2mm_wlast;
          end
      ////////////////////////////////////////////////////////////////////////////////
   // waveforms
   ////////////////////////////////////////////////////////////////////////////////
//
//   initial begin
//      $dumpfile("video_ctrl_tb.vcd");
//      $dumpvars(0, video_ctrl_tb);
//   end

endmodule: video_ctrl_tb
