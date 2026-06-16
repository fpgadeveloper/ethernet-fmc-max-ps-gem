################################################################
# Block design build script for Versal designs
#
# PS GEM design for the Ethernet FMC Max (OP080):
# The Versal PS has two GEMs, so this design drives ports 0 and 1
# of the Ethernet FMC Max; ports 2 and 3 are unused and their PHYs
# are held in reset. Each used port is driven by a PS GEM through
# EMIO GMII and a 1G/2.5G Ethernet PCS/PMA or SGMII core (PG047)
# in SGMII mode over a GTY transceiver (gt_quad_base).
#
# The PCS/PMA cores are configured in "Ethernet MAC: GEM" mode, in
# which the core generates the GMII TX/RX clocks for the GEM and
# adapts the data rate to the speed resolved by SGMII
# auto-negotiation with the DP83867 PHY.
#
# MDIO: the Ethernet FMC Max has a single shared MDIO bus for its
# four PHYs (addresses 1,3,12,15). GEM0 masters that external bus,
# and the management interfaces of ALL PCS/PMA cores sit on the same
# bus (mdio_bus combiner module) at PHY address 8+port. This is
# required because the PCS/PMA cores come out of reset with the
# ISOLATE bit set (register 0, bit 10): software must clear it over
# MDIO before any data can pass through a core. The MDIO masters of
# GEM1-3 are unused.
################################################################

# CHECKING IF PROJECT EXISTS
if { [get_projects -quiet] eq "" } {
   puts "ERROR: Please open or create a project!"
   return 1
}

set cur_design [current_bd_design -quiet]
set list_cells [get_bd_cells -quiet]

create_bd_design $block_name

current_bd_design $block_name

set parentCell [get_bd_cells /]

# Get object for parentCell
set parentObj [get_bd_cells $parentCell]
if { $parentObj == "" } {
   puts "ERROR: Unable to find parent cell <$parentCell>!"
   return
}

# Make sure parentObj is hier blk
set parentType [get_property TYPE $parentObj]
if { $parentType ne "hier" } {
   puts "ERROR: Parent <$parentObj> has TYPE = <$parentType>. Expected to be <hier>."
   return
}

# Save current instance; Restore later
set oldCurInst [current_bd_instance .]

# Set parent object as current
current_bd_instance $parentObj

# PCS/PMA core management PHY addresses are 8+port on the shared
# MDIO bus mastered by GEM0 (see mdio_bus below)

# Number of ports
set num_ports [llength $ports]

# The port with the shared logic (locked to port 0 which masters the MDIO bus)
set port_with_shared_logic [lindex $ports 0]

# Initialize the list of unused ports
set unused_ports {}
foreach port {0 1 2 3} {
    if { [lsearch -exact $ports $port] == -1 } {
        lappend unused_ports $port
    }
}

# Add the CIPS
create_bd_cell -type ip -vlnv xilinx.com:ip:versal_cips versal_cips_0

# Configure the CIPS using automation feature
apply_bd_automation -rule xilinx.com:bd_rule:cips -config { \
  board_preset {Yes} \
  boot_config {Custom} \
  configure_noc {Add new AXI NoC} \
  debug_config {JTAG} \
  design_flow {Full System} \
  mc_type {DDR} \
  num_mc_ddr {1} \
  num_mc_lpddr {None} \
  pl_clocks {None} \
  pl_resets {None} \
}  [get_bd_cells versal_cips_0]

# Extra PS PMC config for this design
# -----------------------------------
# - GEM0 and GEM1: enable on EMIO, MDIO on EMIO
# - PMC GPIO EMIO: enable (PHY resets)
# - Clocking -> Output clocks -> PMC domain clocks -> PL Fabric clocks -> PL CLK0: Enable 100MHz
# - Clocking -> Output clocks -> PMC domain clocks -> PL Fabric clocks -> PL CLK1: Enable 50MHz
# - PL resets: 1
# - M_AXI_LPD: enable

set_property -dict [list \
  CONFIG.CLOCK_MODE {Custom} \
  CONFIG.PS_BOARD_INTERFACE {Custom} \
  CONFIG.PS_PL_CONNECTIVITY_MODE {Custom} \
  CONFIG.PS_PMC_CONFIG { \
    CLOCK_MODE {Custom} \
    DDR_MEMORY_MODE {Connectivity to DDR via NOC} \
    DEBUG_MODE {JTAG} \
    DESIGN_MODE {1} \
    PMC_CRP_PL0_REF_CTRL_FREQMHZ {100} \
    PMC_CRP_PL1_REF_CTRL_FREQMHZ {50} \
    PMC_GPIO0_MIO_PERIPHERAL {{ENABLE 1} {IO {PMC_MIO 0 .. 25}}} \
    PMC_GPIO1_MIO_PERIPHERAL {{ENABLE 1} {IO {PMC_MIO 26 .. 51}}} \
    PMC_GPIO_EMIO_PERIPHERAL_ENABLE {1} \
    PMC_MIO37 {{AUX_IO 0} {DIRECTION out} {DRIVE_STRENGTH 8mA} {OUTPUT_DATA high} {PULL pullup} {SCHMITT 0} {SLEW slow} {USAGE GPIO}} \
    PMC_OSPI_PERIPHERAL {{ENABLE 0} {IO {PMC_MIO 0 .. 11}} {MODE Single}} \
    PMC_QSPI_COHERENCY {0} \
    PMC_QSPI_FBCLK {{ENABLE 1} {IO {PMC_MIO 6}}} \
    PMC_QSPI_PERIPHERAL_DATA_MODE {x4} \
    PMC_QSPI_PERIPHERAL_ENABLE {1} \
    PMC_QSPI_PERIPHERAL_MODE {Dual Parallel} \
    PMC_REF_CLK_FREQMHZ {33.3333} \
    PMC_SD1 {{CD_ENABLE 1} {CD_IO {PMC_MIO 28}} {POW_ENABLE 1} {POW_IO {PMC_MIO 51}} {RESET_ENABLE 0} {RESET_IO {PMC_MIO 12}} {WP_ENABLE 0} {WP_IO {PMC_MIO 1}}} \
    PMC_SD1_COHERENCY {0} \
    PMC_SD1_DATA_TRANSFER_MODE {8Bit} \
    PMC_SD1_PERIPHERAL {{CLK_100_SDR_OTAP_DLY 0x3} {CLK_200_SDR_OTAP_DLY 0x2} {CLK_50_DDR_ITAP_DLY 0x36} {CLK_50_DDR_OTAP_DLY 0x3} {CLK_50_SDR_ITAP_DLY 0x2C} {CLK_50_SDR_OTAP_DLY 0x4} {ENABLE 1} {IO {PMC_MIO 26 .. 36}}} \
    PMC_SD1_SLOT_TYPE {SD 3.0} \
    PMC_USE_PMC_NOC_AXI0 {1} \
    PS_BOARD_INTERFACE {Custom} \
    PS_CAN1_PERIPHERAL {{ENABLE 1} {IO {PMC_MIO 40 .. 41}}} \
    PS_CRL_CAN1_REF_CTRL_FREQMHZ {160} \
    PS_ENET0_MDIO {{ENABLE 1} {IO {EMIO}}} \
    PS_ENET0_PERIPHERAL {{ENABLE 1} {IO {EMIO}}} \
    PS_ENET1_MDIO {{ENABLE 1} {IO {EMIO}}} \
    PS_ENET1_PERIPHERAL {{ENABLE 1} {IO {EMIO}}} \
    PS_GEN_IPI0_ENABLE {1} \
    PS_GEN_IPI0_MASTER {A72} \
    PS_GEN_IPI1_ENABLE {1} \
    PS_GEN_IPI2_ENABLE {1} \
    PS_GEN_IPI3_ENABLE {1} \
    PS_GEN_IPI4_ENABLE {1} \
    PS_GEN_IPI5_ENABLE {1} \
    PS_GEN_IPI6_ENABLE {1} \
    PS_HSDP_EGRESS_TRAFFIC {JTAG} \
    PS_HSDP_INGRESS_TRAFFIC {JTAG} \
    PS_HSDP_MODE {NONE} \
    PS_I2C0_PERIPHERAL {{ENABLE 1} {IO {PMC_MIO 46 .. 47}}} \
    PS_I2C1_PERIPHERAL {{ENABLE 1} {IO {PMC_MIO 44 .. 45}}} \
    PS_MIO19 {{AUX_IO 0} {DIRECTION in} {DRIVE_STRENGTH 8mA} {OUTPUT_DATA default} {PULL disable} {SCHMITT 0} {SLEW slow} {USAGE Reserved}} \
    PS_MIO21 {{AUX_IO 0} {DIRECTION in} {DRIVE_STRENGTH 8mA} {OUTPUT_DATA default} {PULL disable} {SCHMITT 0} {SLEW slow} {USAGE Reserved}} \
    PS_MIO7 {{AUX_IO 0} {DIRECTION in} {DRIVE_STRENGTH 8mA} {OUTPUT_DATA default} {PULL disable} {SCHMITT 0} {SLEW slow} {USAGE Reserved}} \
    PS_MIO9 {{AUX_IO 0} {DIRECTION in} {DRIVE_STRENGTH 8mA} {OUTPUT_DATA default} {PULL disable} {SCHMITT 0} {SLEW slow} {USAGE Reserved}} \
    PS_NUM_FABRIC_RESETS {1} \
    PS_PCIE_EP_RESET1_IO {PMC_MIO 38} \
    PS_PCIE_EP_RESET2_IO {PMC_MIO 39} \
    PS_PCIE_RESET {ENABLE 1} \
    PS_PL_CONNECTIVITY_MODE {Custom} \
    PS_TTC0_PERIPHERAL_ENABLE {1} \
    PS_UART0_PERIPHERAL {{ENABLE 1} {IO {PMC_MIO 42 .. 43}}} \
    PS_USB3_PERIPHERAL {{ENABLE 1} {IO {PMC_MIO 13 .. 25}}} \
    PS_USE_FPD_CCI_NOC {1} \
    PS_USE_FPD_CCI_NOC0 {1} \
    PS_USE_M_AXI_LPD {1} \
    PS_USE_NOC_LPD_AXI0 {1} \
    PS_USE_PMCPL_CLK0 {1} \
    PS_USE_PMCPL_CLK1 {1} \
    PS_USE_PMCPL_CLK2 {0} \
    PS_USE_PMCPL_CLK3 {0} \
    SMON_ALARMS {Set_Alarms_On} \
    SMON_ENABLE_TEMP_AVERAGING {0} \
    SMON_TEMP_AVERAGING_SAMPLES {0} \
  } \
] [get_bd_cells versal_cips_0]

# System clock (100MHz)
set sys_clk "versal_cips_0/pl0_ref_clk"

# PCS/PMA independent (DRP) clock (50MHz)
set indep_clk "versal_cips_0/pl1_ref_clk"

# Connect the AXI interface clocks
connect_bd_net [get_bd_pins $sys_clk] [get_bd_pins versal_cips_0/m_axi_lpd_aclk]

# Proc system reset for main clock
create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset rst_pl0
connect_bd_net [get_bd_pins $sys_clk] [get_bd_pins rst_pl0/slowest_sync_clk]
connect_bd_net [get_bd_pins versal_cips_0/pl0_resetn] [get_bd_pins rst_pl0/ext_reset_in]

# AXI SmartConnect for AXI Lite interfaces (GT quad APB bridge + GPIO)
create_bd_cell -type ip -vlnv xilinx.com:ip:smartconnect axi_smc
set_property -dict [list CONFIG.NUM_SI {1} CONFIG.NUM_MI {2} ] [get_bd_cells axi_smc]
connect_bd_net [get_bd_pins $sys_clk] [get_bd_pins axi_smc/aclk]
connect_bd_net [get_bd_pins rst_pl0/interconnect_aresetn] [get_bd_pins axi_smc/aresetn]
connect_bd_intf_net [get_bd_intf_pins versal_cips_0/M_AXI_LPD] [get_bd_intf_pins axi_smc/S00_AXI]

# GT ref clock and utility buffer
create_bd_intf_port -mode Slave -vlnv xilinx.com:interface:diff_clock_rtl:1.0 gt_ref_clk
set_property CONFIG.FREQ_HZ 125000000 [get_bd_intf_ports /gt_ref_clk]
create_bd_cell -type ip -vlnv xilinx.com:ip:util_ds_buf util_ds_buf_0
set_property CONFIG.C_BUF_TYPE {IBUFDSGTE} [get_bd_cells util_ds_buf_0]
connect_bd_intf_net [get_bd_intf_ports gt_ref_clk] [get_bd_intf_pins util_ds_buf_0/CLK_IN_D]

# GT Quad base (Transceiver wizard)
create_bd_cell -type ip -vlnv xilinx.com:ip:gt_quad_base gt_quad_base_0
connect_bd_net [get_bd_pins util_ds_buf_0/IBUF_OUT] [get_bd_pins gt_quad_base_0/GT_REFCLK0]
connect_bd_net [get_bd_pins $sys_clk] [get_bd_pins gt_quad_base_0/apb3clk]
connect_bd_net [get_bd_pins rst_pl0/peripheral_aresetn] [get_bd_pins gt_quad_base_0/apb3presetn]

# APB Bridge
create_bd_cell -type ip -vlnv xilinx.com:ip:axi_apb_bridge axi_apb_bridge_0
set_property CONFIG.C_APB_NUM_SLAVES {1} [get_bd_cells axi_apb_bridge_0]
connect_bd_intf_net [get_bd_intf_pins axi_apb_bridge_0/APB_M] [get_bd_intf_pins gt_quad_base_0/APB3_INTF]
connect_bd_net [get_bd_pins $sys_clk] [get_bd_pins axi_apb_bridge_0/s_axi_aclk]
connect_bd_net [get_bd_pins rst_pl0/peripheral_aresetn] [get_bd_pins axi_apb_bridge_0/s_axi_aresetn]
connect_bd_intf_net [get_bd_intf_pins axi_smc/M00_AXI] [get_bd_intf_pins axi_apb_bridge_0/AXI4_LITE]

# SGMII (GT) interface
create_bd_intf_port -mode Master -vlnv xilinx.com:interface:gt_rtl:1.0 sgmii_port
connect_bd_intf_net [get_bd_intf_pins gt_quad_base_0/GT_Serial] [get_bd_intf_ports sgmii_port]

# Constants shared by all PCS/PMA cores and BUFG GTs
create_bd_cell -type inline_hdl -vlnv xilinx.com:inline_hdl:ilconstant:1.0 const_low
set_property CONFIG.CONST_VAL {0} [get_bd_cells const_low]
create_bd_cell -type inline_hdl -vlnv xilinx.com:inline_hdl:ilconstant:1.0 const_high
set_property CONFIG.CONST_VAL {1} [get_bd_cells const_high]
create_bd_cell -type inline_hdl -vlnv xilinx.com:inline_hdl:ilconstant:1.0 const_config_vector
set_property -dict [list CONFIG.CONST_VAL {0} CONFIG.CONST_WIDTH {5}] [get_bd_cells const_config_vector]
# SGMII AN advertisement: 1Gbps full-duplex link up (0xD801)
create_bd_cell -type inline_hdl -vlnv xilinx.com:inline_hdl:ilconstant:1.0 const_an_adv
set_property -dict [list CONFIG.CONST_VAL {55297} CONFIG.CONST_WIDTH {16}] [get_bd_cells const_an_adv]
# BUFG GT constants
create_bd_cell -type inline_hdl -vlnv xilinx.com:inline_hdl:ilconstant:1.0 const_bufgtdiv
set_property -dict [list CONFIG.CONST_VAL {1} CONFIG.CONST_WIDTH {3}] [get_bd_cells const_bufgtdiv]

# MDIO bus combiner: GEM0 masters one MDIO bus carrying the external
# PHYs of the Ethernet FMC Max and the management interfaces of all
# PCS/PMA cores (address 8+port). Software must clear the ISOLATE bit
# (register 0, bit 10) of every core over this bus.
create_bd_cell -type module -reference mdio_bus mdio_bus_0
connect_bd_net [get_bd_pins versal_cips_0/GEM0_MDIO_mdc] [get_bd_pins mdio_bus_0/gem_mdc]
connect_bd_net [get_bd_pins versal_cips_0/GEM0_MDIO_mdio_o] [get_bd_pins mdio_bus_0/gem_mdio_o]
connect_bd_net [get_bd_pins versal_cips_0/GEM0_MDIO_mdio_t] [get_bd_pins mdio_bus_0/gem_mdio_t]
connect_bd_net [get_bd_pins mdio_bus_0/gem_mdio_i] [get_bd_pins versal_cips_0/GEM0_MDIO_mdio_i]
# External MDIO bus: interface port so the generated wrapper performs
# the tri-state pad merge (port names mdio_io_* match the XDC)
create_bd_intf_port -mode Master -vlnv xilinx.com:interface:mdio_rtl:1.0 mdio_io
connect_bd_intf_net [get_bd_intf_pins mdio_bus_0/ext_mdio] [get_bd_intf_ports mdio_io]
# Slave inputs of unused ports idle HIGH (wired-AND bus)
foreach port $unused_ports {
  connect_bd_net [get_bd_pins const_high/dout] [get_bd_pins mdio_bus_0/s${port}_mdio_o]
  connect_bd_net [get_bd_pins const_high/dout] [get_bd_pins mdio_bus_0/s${port}_mdio_t]
}

# Add and configure the PCS/PMA cores
foreach port $ports {
  # BUFG GTs (4 per channel, matching AMD versal_bd_automation reference)
  # bufg_gt_rxoutclk  -> rxuserclk  (62.5 MHz, also feeds gt_rxusrclk)
  # bufg_gt_rxoutclk2 -> rxuserclk2 (62.5 MHz, separate buffer per AMD ref)
  # bufg_gt_txoutclk_div2 -> userclk (62.5 MHz, also feeds gt_txusrclk)
  # bufg_gt_txoutclk  -> userclk2   (125 MHz)
  create_bd_cell -type ip -vlnv xilinx.com:ip:bufg_gt bufg_gt_rxoutclk_${port}
  set_property CONFIG.FREQ_HZ {62500000} [get_bd_cells bufg_gt_rxoutclk_${port}]
  connect_bd_net [get_bd_pins bufg_gt_rxoutclk_${port}/usrclk] [get_bd_pins gt_quad_base_0/ch${port}_rxusrclk]
  connect_bd_net [get_bd_pins gt_quad_base_0/ch${port}_rxoutclk] [get_bd_pins bufg_gt_rxoutclk_${port}/outclk]

  create_bd_cell -type ip -vlnv xilinx.com:ip:bufg_gt bufg_gt_rxoutclk2_${port}
  set_property CONFIG.FREQ_HZ {62500000} [get_bd_cells bufg_gt_rxoutclk2_${port}]
  connect_bd_net [get_bd_pins gt_quad_base_0/ch${port}_rxoutclk] [get_bd_pins bufg_gt_rxoutclk2_${port}/outclk]

  create_bd_cell -type ip -vlnv xilinx.com:ip:bufg_gt bufg_gt_txoutclk_div2_${port}
  set_property CONFIG.FREQ_HZ {62500000} [get_bd_cells bufg_gt_txoutclk_div2_${port}]
  connect_bd_net [get_bd_pins bufg_gt_txoutclk_div2_${port}/usrclk] [get_bd_pins gt_quad_base_0/ch${port}_txusrclk]
  connect_bd_net [get_bd_pins gt_quad_base_0/ch${port}_txoutclk] [get_bd_pins bufg_gt_txoutclk_div2_${port}/outclk]

  create_bd_cell -type ip -vlnv xilinx.com:ip:bufg_gt bufg_gt_txoutclk_${port}
  set_property CONFIG.FREQ_HZ {125000000} [get_bd_cells bufg_gt_txoutclk_${port}]
  connect_bd_net [get_bd_pins gt_quad_base_0/ch${port}_txoutclk] [get_bd_pins bufg_gt_txoutclk_${port}/outclk]

  # Add the PCS/PMA core
  create_bd_cell -type ip -vlnv xilinx.com:ip:gig_ethernet_pcs_pma pcs_pma_$port

  # Configure the PCS/PMA core
  # SGMII over transceiver, "Ethernet MAC: GEM" mode, auto-negotiation enabled.
  # IS_GT_WIZ_OLD=1 enables the gt_tx/rx_interface ports which are required to
  # connect the core to the gt_quad_base (GT data, control and clock signals).
  # This parameter will be deprecated in 2026.1.
  set_property -dict [list \
    CONFIG.Standard {SGMII} \
    CONFIG.Physical_Interface {Transceiver} \
    CONFIG.EMAC_IF_TEMAC {GEM} \
    CONFIG.Auto_Negotiation {true} \
    CONFIG.DrpClkRate {50} \
    CONFIG.IS_GT_WIZ_OLD {1} \
  ] [get_bd_cells pcs_pma_$port]

  # PCS/PMA management PHY address
  # Management PHY address on the shared MDIO bus: 8+port
  create_bd_cell -type inline_hdl -vlnv xilinx.com:inline_hdl:ilconstant:1.0 const_phyaddr_$port
  set_property -dict [list CONFIG.CONST_VAL [expr {8 + $port}] CONFIG.CONST_WIDTH {5}] [get_bd_cells const_phyaddr_$port]
  connect_bd_net [get_bd_pins const_phyaddr_$port/dout] [get_bd_pins pcs_pma_$port/phyaddr]

  # Independent (DRP) clock
  connect_bd_net [get_bd_pins $indep_clk] [get_bd_pins pcs_pma_$port/independent_clock_bufg]

  # Resets
  connect_bd_net [get_bd_pins rst_pl0/peripheral_reset] [get_bd_pins pcs_pma_$port/reset]
  connect_bd_net [get_bd_pins rst_pl0/peripheral_reset] [get_bd_pins pcs_pma_$port/pma_reset]
  connect_bd_net [get_bd_pins const_high/dout] [get_bd_pins pcs_pma_$port/mmcm_locked]

  # Connect clocks (each PCS/PMA clock gets its own BUFG_GT)
  connect_bd_net [get_bd_pins bufg_gt_rxoutclk_${port}/usrclk] [get_bd_pins pcs_pma_${port}/rxuserclk]
  connect_bd_net [get_bd_pins bufg_gt_rxoutclk2_${port}/usrclk] [get_bd_pins pcs_pma_${port}/rxuserclk2]
  connect_bd_net [get_bd_pins bufg_gt_txoutclk_div2_${port}/usrclk] [get_bd_pins pcs_pma_${port}/userclk]
  connect_bd_net [get_bd_pins bufg_gt_txoutclk_${port}/usrclk] [get_bd_pins pcs_pma_${port}/userclk2]

  # Connect GT Quad to the PCS/PMA core via interface connections
  connect_bd_intf_net [get_bd_intf_pins gt_quad_base_0/RX${port}_GT_IP_Interface] [get_bd_intf_pins pcs_pma_${port}/gt_rx_interface]
  connect_bd_intf_net [get_bd_intf_pins gt_quad_base_0/TX${port}_GT_IP_Interface] [get_bd_intf_pins pcs_pma_${port}/gt_tx_interface]

  # GT reset-done and status signals (not part of the GT interface bundles;
  # the rx/tx mstresetdone signals ARE carried by the GT interface bundles)
  connect_bd_net [get_bd_pins gt_quad_base_0/ch${port}_rxprogdivresetdone] [get_bd_pins pcs_pma_${port}/gtwiz_reset_rx_done_in]
  connect_bd_net [get_bd_pins gt_quad_base_0/ch${port}_txprogdivresetdone] [get_bd_pins pcs_pma_${port}/gtwiz_reset_tx_done_in]
  connect_bd_net [get_bd_pins gt_quad_base_0/gtpowergood] [get_bd_pins pcs_pma_${port}/gtpowergood_in]
  connect_bd_net [get_bd_pins gt_quad_base_0/hsclk0_lcplllock] [get_bd_pins pcs_pma_${port}/cplllock_in]

  # GMII interface to the PS GEM (the GMII TX/RX clocks generated by the
  # PCS/PMA core in GEM mode are carried by the interface connection)
  connect_bd_intf_net [get_bd_intf_pins versal_cips_0/GEM${port}_GMII] [get_bd_intf_pins pcs_pma_$port/gmii_gem_pcs_pma]

  # MDIO: this core's management interface sits on the shared MDIO bus
  # mastered by GEM0 (PHY address 8+port)
  connect_bd_net [get_bd_pins mdio_bus_0/slave_mdc] [get_bd_pins pcs_pma_$port/mdc]
  connect_bd_net [get_bd_pins mdio_bus_0/slave_mdio_i] [get_bd_pins pcs_pma_$port/mdio_i]
  connect_bd_net [get_bd_pins pcs_pma_$port/mdio_o] [get_bd_pins mdio_bus_0/s${port}_mdio_o]
  connect_bd_net [get_bd_pins pcs_pma_$port/mdio_t] [get_bd_pins mdio_bus_0/s${port}_mdio_t]

  # Configuration and AN advertisement (cores run on their reset defaults)
  connect_bd_net [get_bd_pins const_config_vector/dout] [get_bd_pins pcs_pma_$port/configuration_vector]
  connect_bd_net [get_bd_pins const_low/dout] [get_bd_pins pcs_pma_$port/configuration_valid]
  connect_bd_net [get_bd_pins const_an_adv/dout] [get_bd_pins pcs_pma_$port/an_adv_config_vector]
  connect_bd_net [get_bd_pins const_low/dout] [get_bd_pins pcs_pma_$port/an_adv_config_val]
  connect_bd_net [get_bd_pins const_low/dout] [get_bd_pins pcs_pma_$port/an_restart_config]

  # signal_detect tied HIGH
  connect_bd_net [get_bd_pins const_high/dout] [get_bd_pins pcs_pma_$port/signal_detect]
}

# Connect constant values to BUFG GTs
foreach port $ports {
  connect_bd_net [get_bd_pins const_bufgtdiv/dout] [get_bd_pins bufg_gt_txoutclk_div2_${port}/gt_bufgtdiv]
  foreach i {"rxoutclk" "rxoutclk2" "txoutclk_div2" "txoutclk"} {
    connect_bd_net [get_bd_pins const_high/dout] [get_bd_pins bufg_gt_${i}_${port}/gt_bufgtce]
    connect_bd_net [get_bd_pins const_high/dout] [get_bd_pins bufg_gt_${i}_${port}/gt_bufgtcemask]
    connect_bd_net [get_bd_pins const_high/dout] [get_bd_pins bufg_gt_${i}_${port}/gt_bufgtclrmask]
    connect_bd_net [get_bd_pins const_low/dout] [get_bd_pins bufg_gt_${i}_${port}/gt_bufgtclr]
  }
}

# Configure the GT quad protocols (one 1G Ethernet lane per used port)
set_property -dict [list CONFIG.PROT1_PRESET.VALUE_MODE MANUAL \
  CONFIG.PROT0_PRESET.VALUE_MODE MANUAL \
] [get_bd_cells gt_quad_base_0]
set_property -dict [list \
  CONFIG.PROT0_NO_OF_LANES {1} \
  CONFIG.PROT0_PRESET {GTY-Ethernet_1G} \
  CONFIG.PROT1_ENABLE {true} \
  CONFIG.PROT1_PRESET {GTY-Ethernet_1G} \
] [get_bd_cells gt_quad_base_0]

# PHY reset signals (PMC GPIO EMIO)
foreach port $ports {
  create_bd_cell -type inline_hdl -vlnv xilinx.com:inline_hdl:ilslice:1.0 xlslice_phy${port}
  set_property -dict [list \
    CONFIG.DIN_FROM $port \
    CONFIG.DIN_TO $port \
    CONFIG.DIN_WIDTH {64} \
  ] [get_bd_cells xlslice_phy${port}]
  connect_bd_net [get_bd_pins versal_cips_0/PMC_GPIO_o] [get_bd_pins xlslice_phy${port}/Din]
  # External PHY RESET
  create_bd_port -dir O -type rst reset_port_${port}
  connect_bd_net [get_bd_pins /xlslice_phy${port}/Dout] [get_bd_ports reset_port_${port}]
}

# Correctly tie off the unused ports
foreach port $unused_ports {
  # PHY RESET - hold LOW - keep unused PHYs in reset
  create_bd_port -dir O -type rst reset_port_${port}
  connect_bd_net [get_bd_pins const_low/dout] [get_bd_ports reset_port_${port}]
}

# Add the AXI GPIO for the power good and PHY GPIO signals
create_bd_cell -type ip -vlnv xilinx.com:ip:axi_gpio axi_gpio_0
set_property -dict [list \
  CONFIG.C_ALL_INPUTS {1} \
  CONFIG.C_GPIO_WIDTH {10} \
] [get_bd_cells axi_gpio_0]
connect_bd_net [get_bd_pins $sys_clk] [get_bd_pins axi_gpio_0/s_axi_aclk]
connect_bd_net [get_bd_pins rst_pl0/peripheral_aresetn] [get_bd_pins axi_gpio_0/s_axi_aresetn]
connect_bd_intf_net [get_bd_intf_pins axi_smc/M01_AXI] [get_bd_intf_pins axi_gpio_0/S_AXI]
create_bd_intf_port -mode Master -vlnv xilinx.com:interface:gpio_rtl:1.0 gpio
connect_bd_intf_net [get_bd_intf_pins axi_gpio_0/GPIO] [get_bd_intf_ports gpio]

# Assign any addresses that haven't already been assigned
assign_bd_address

validate_bd_design
save_bd_design
