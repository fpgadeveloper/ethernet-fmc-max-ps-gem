################################################################
# Block diagram build script for Zynq UltraScale+ designs
#
# PS GEM design for the Ethernet FMC Max (OP080):
# Each of the 4 ports is driven by a PS GEM through EMIO GMII and
# a 1G/2.5G Ethernet PCS/PMA or SGMII core (PG047) in SGMII mode
# over a gigabit transceiver. The PCS/PMA cores are configured in
# "Ethernet MAC: GEM" mode, in which the core generates the GMII
# TX/RX clocks for the GEM and adapts the data rate to the speed
# resolved by SGMII auto-negotiation with the DP83867 PHY.
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

# Check if IP exists in design
proc ip_exists {ip_name} {
    set cells [get_bd_cells -quiet $ip_name]
    return [llength $cells]
}

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

# Initialize the list of unused ports
set unused_ports {}

# Work out which ports of the Ethernet FMC Max are not used in this design
foreach port {0 1 2 3} {
    # Check if the current port is not in the ports list
    if { [lsearch -exact $ports $port] == -1 } {
        # Add the port to the unused_ports list
        lappend unused_ports $port
    }
}

# The port with the shared logic (locked to port 0 which masters the MDIO bus)
set port_with_shared_logic [lindex $ports 0]

# Add the Processor System and apply board preset
create_bd_cell -type ip -vlnv xilinx.com:ip:zynq_ultra_ps_e zynq_ultra_ps_e_0
apply_bd_automation -rule xilinx.com:bd_rule:zynq_ultra_ps_e -config {apply_board_preset "1" }  [get_bd_cells zynq_ultra_ps_e_0]

# Configure the PS
# Enable the PL1 CLK 50MHz for the PCS/PMA independent (DRP) clock
set_property -dict [list CONFIG.PSU__USE__M_AXI_GP0 {1} \
CONFIG.PSU__USE__M_AXI_GP1 {0} \
CONFIG.PSU__USE__S_AXI_GP2 {0} \
CONFIG.PSU__TTC0__PERIPHERAL__ENABLE {1} \
CONFIG.PSU__TTC0__PERIPHERAL__IO {EMIO} \
CONFIG.PSU__CRL_APB__PL1_REF_CTRL__FREQMHZ {50.000} \
CONFIG.PSU__FPGA_PL1_ENABLE {1} \
] [get_bd_cells zynq_ultra_ps_e_0]

# Enable the PS GEMs on EMIO, one per port
foreach port $ports {
  set_property -dict [list CONFIG.PSU__ENET${port}__PERIPHERAL__ENABLE {1} \
  CONFIG.PSU__ENET${port}__PERIPHERAL__IO {EMIO} \
  CONFIG.PSU__ENET${port}__GRP_MDIO__ENABLE {1} \
  CONFIG.PSU__ENET${port}__GRP_MDIO__IO {EMIO} \
  ] [get_bd_cells zynq_ultra_ps_e_0]
}

# PCS/PMA independent (DRP) clock
set indep_clk "zynq_ultra_ps_e_0/pl_clk1"

# UltraZed-EV Carrier: Must use IOPLL clock source for the 50MHz clock (pl_clk1)
# For reasons unknown, we have needed to source the 50MHz clock from IOPLL (instead of RPLL)
# for the Ethernet ports to function correctly in PetaLinux.
# We found that the PL1 clock would not be set to the correct frequency in PetaLinux when
# sourced from RPLL.
#   - Verified using command: sudo cat /sys/kernel/debug/clk/pl1_ref/clk_rate
# This problem did not affect baremetal applications (ie. using RPLL is fine in baremetal).
# This problem does not affect the Xilinx boards (ZCU102, ZCU106, ...).
# It is likely that the UltraZed-EV BSP changes the RPLL configuration, but this requires
# further examination.
if {$board_name == "ultrazed_7ev_cc"} {
  set_property -dict [list CONFIG.PSU__CRL_APB__PL1_REF_CTRL__SRCSEL {IOPLL} ] [get_bd_cells zynq_ultra_ps_e_0]
}

# Connect the FCLK_CLK0 to the PS GP0
connect_bd_net [get_bd_pins zynq_ultra_ps_e_0/pl_clk0] [get_bd_pins zynq_ultra_ps_e_0/maxihpm0_fpd_aclk]

# Add proc system reset for system clock
create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset rst_100m
connect_bd_net [get_bd_pins zynq_ultra_ps_e_0/pl_clk0] [get_bd_pins rst_100m/slowest_sync_clk]
connect_bd_net [get_bd_pins zynq_ultra_ps_e_0/pl_resetn0] [get_bd_pins rst_100m/ext_reset_in]

# Constants shared by all PCS/PMA cores
# configuration_vector/valid tied to zero: the cores come out of reset with
# auto-negotiation enabled (Auto_Negotiation true in the IP configuration)
create_bd_cell -type inline_hdl -vlnv xilinx.com:inline_hdl:ilconstant:1.0 const_low
set_property CONFIG.CONST_VAL {0} [get_bd_cells const_low]
create_bd_cell -type inline_hdl -vlnv xilinx.com:inline_hdl:ilconstant:1.0 const_high
set_property CONFIG.CONST_VAL {1} [get_bd_cells const_high]
create_bd_cell -type inline_hdl -vlnv xilinx.com:inline_hdl:ilconstant:1.0 const_config_vector
set_property -dict [list CONFIG.CONST_VAL {0} CONFIG.CONST_WIDTH {5}] [get_bd_cells const_config_vector]
# SGMII AN advertisement: 1Gbps full-duplex link up (0xD801)
create_bd_cell -type inline_hdl -vlnv xilinx.com:inline_hdl:ilconstant:1.0 const_an_adv
set_property -dict [list CONFIG.CONST_VAL {55297} CONFIG.CONST_WIDTH {16}] [get_bd_cells const_an_adv]
# MDIO bus combiner: GEM0 masters one MDIO bus carrying the external
# PHYs of the Ethernet FMC Max and the management interfaces of all
# PCS/PMA cores (address 8+port). Software must clear the ISOLATE bit
# (register 0, bit 10) of every core over this bus.
create_bd_cell -type module -reference mdio_bus mdio_bus_0
connect_bd_net [get_bd_pins zynq_ultra_ps_e_0/emio_enet0_mdio_mdc] [get_bd_pins mdio_bus_0/gem_mdc]
connect_bd_net [get_bd_pins zynq_ultra_ps_e_0/emio_enet0_mdio_o] [get_bd_pins mdio_bus_0/gem_mdio_o]
connect_bd_net [get_bd_pins zynq_ultra_ps_e_0/emio_enet0_mdio_t] [get_bd_pins mdio_bus_0/gem_mdio_t]
connect_bd_net [get_bd_pins mdio_bus_0/gem_mdio_i] [get_bd_pins zynq_ultra_ps_e_0/emio_enet0_mdio_i]
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
  # Add the PCS/PMA core
  create_bd_cell -type ip -vlnv xilinx.com:ip:gig_ethernet_pcs_pma pcs_pma_$port

  # Get the GT location
  set gt_loc [dict get $gt_loc_dict $target $port]

  # Configure the PCS/PMA core
  # SGMII over transceiver, "Ethernet MAC: GEM" mode, auto-negotiation enabled
  if {$port == $port_with_shared_logic} {
    set_property -dict [list CONFIG.Standard {SGMII} \
                              CONFIG.Physical_Interface {Transceiver} \
                              CONFIG.EMAC_IF_TEMAC {GEM} \
                              CONFIG.Auto_Negotiation {true} \
                              CONFIG.GT_Location $gt_loc \
                              CONFIG.DrpClkRate {50} \
                              CONFIG.SupportLevel {Include_Shared_Logic_in_Core} \
                              ] [get_bd_cells pcs_pma_$port]
    # GT ref clock (125MHz from the Ethernet FMC Max Si511)
    create_bd_intf_port -mode Slave -vlnv xilinx.com:interface:diff_clock_rtl:1.0 gt_ref_clk
    set_property CONFIG.FREQ_HZ 125000000 [get_bd_intf_ports /gt_ref_clk]
    connect_bd_intf_net [get_bd_intf_pins pcs_pma_$port/gtrefclk_in] [get_bd_intf_ports gt_ref_clk]
  } else {
    set_property -dict [list CONFIG.Standard {SGMII} \
                              CONFIG.Physical_Interface {Transceiver} \
                              CONFIG.EMAC_IF_TEMAC {GEM} \
                              CONFIG.Auto_Negotiation {true} \
                              CONFIG.GT_Location $gt_loc \
                              CONFIG.DrpClkRate {50} \
                              CONFIG.SupportLevel {Include_Shared_Logic_in_Example_Design} \
                              ] [get_bd_cells pcs_pma_$port]
    # Shared clocks from the core with the shared logic
    connect_bd_net [get_bd_pins pcs_pma_$port_with_shared_logic/gtrefclk_out] [get_bd_pins pcs_pma_$port/gtrefclk]
    connect_bd_net [get_bd_pins pcs_pma_$port_with_shared_logic/userclk_out] [get_bd_pins pcs_pma_$port/userclk]
    connect_bd_net [get_bd_pins pcs_pma_$port_with_shared_logic/userclk2_out] [get_bd_pins pcs_pma_$port/userclk2]
    connect_bd_net [get_bd_pins pcs_pma_$port_with_shared_logic/rxuserclk_out] [get_bd_pins pcs_pma_$port/rxuserclk]
    connect_bd_net [get_bd_pins pcs_pma_$port_with_shared_logic/rxuserclk2_out] [get_bd_pins pcs_pma_$port/rxuserclk2]
    connect_bd_net [get_bd_pins pcs_pma_$port_with_shared_logic/pma_reset_out] [get_bd_pins pcs_pma_$port/pma_reset]
    connect_bd_net [get_bd_pins pcs_pma_$port_with_shared_logic/mmcm_locked_out] [get_bd_pins pcs_pma_$port/mmcm_locked]
  }

  # MDIO: this core's management interface sits on the shared MDIO bus
  # mastered by GEM0 (PHY address 8+port)
  connect_bd_net [get_bd_pins mdio_bus_0/slave_mdc] [get_bd_pins pcs_pma_$port/mdc]
  connect_bd_net [get_bd_pins mdio_bus_0/slave_mdio_i] [get_bd_pins pcs_pma_$port/mdio_i]
  connect_bd_net [get_bd_pins pcs_pma_$port/mdio_o] [get_bd_pins mdio_bus_0/s${port}_mdio_o]
  connect_bd_net [get_bd_pins pcs_pma_$port/mdio_t] [get_bd_pins mdio_bus_0/s${port}_mdio_t]

  # Management PHY address on the shared MDIO bus: 8+port
  create_bd_cell -type inline_hdl -vlnv xilinx.com:inline_hdl:ilconstant:1.0 const_phyaddr_$port
  set_property -dict [list CONFIG.CONST_VAL [expr {8 + $port}] CONFIG.CONST_WIDTH {5}] [get_bd_cells const_phyaddr_$port]
  connect_bd_net [get_bd_pins const_phyaddr_$port/dout] [get_bd_pins pcs_pma_$port/phyaddr]

  # Independent (DRP) clock
  connect_bd_net [get_bd_pins $indep_clk] [get_bd_pins pcs_pma_$port/independent_clock_bufg]

  # Reset
  connect_bd_net [get_bd_pins rst_100m/peripheral_reset] [get_bd_pins pcs_pma_$port/reset]

  # GMII interface to the PS GEM (the GMII TX/RX clocks generated by the
  # PCS/PMA core in GEM mode are carried by the interface connection)
  connect_bd_intf_net [get_bd_intf_pins zynq_ultra_ps_e_0/GMII_ENET${port}] [get_bd_intf_pins pcs_pma_$port/gmii_gem_pcs_pma]

  # Configuration and AN advertisement (cores run on their reset defaults)
  connect_bd_net [get_bd_pins const_config_vector/dout] [get_bd_pins pcs_pma_$port/configuration_vector]
  connect_bd_net [get_bd_pins const_low/dout] [get_bd_pins pcs_pma_$port/configuration_valid]
  connect_bd_net [get_bd_pins const_an_adv/dout] [get_bd_pins pcs_pma_$port/an_adv_config_vector]
  connect_bd_net [get_bd_pins const_low/dout] [get_bd_pins pcs_pma_$port/an_adv_config_val]
  connect_bd_net [get_bd_pins const_low/dout] [get_bd_pins pcs_pma_$port/an_restart_config]

  # signal_detect tied HIGH
  connect_bd_net [get_bd_pins const_high/dout] [get_bd_pins pcs_pma_$port/signal_detect]

  # Make the SGMII and PHY RESET ports external
  # SGMII
  create_bd_intf_port -mode Master -vlnv xilinx.com:interface:sgmii_rtl:1.0 sgmii_port_${port}
  connect_bd_intf_net [get_bd_intf_pins pcs_pma_${port}/sgmii] [get_bd_intf_ports sgmii_port_${port}]
  # RESET (active low, released when the PS comes up)
  create_bd_port -dir O -type rst reset_port_${port}
  connect_bd_net [get_bd_pins rst_100m/peripheral_aresetn] [get_bd_ports reset_port_${port}]
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
connect_bd_net [get_bd_pins zynq_ultra_ps_e_0/pl_clk0] [get_bd_pins axi_gpio_0/s_axi_aclk]
connect_bd_net [get_bd_pins rst_100m/peripheral_aresetn] [get_bd_pins axi_gpio_0/s_axi_aresetn]
apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config { Clk_master {Auto} Clk_slave {Auto} Clk_xbar {Auto} Master {/zynq_ultra_ps_e_0/M_AXI_HPM0_FPD} Slave {/axi_gpio_0/S_AXI} ddr_seg {Auto} intc_ip {Auto} master_apm {0}}  [get_bd_intf_pins axi_gpio_0/S_AXI]
create_bd_intf_port -mode Master -vlnv xilinx.com:interface:gpio_rtl:1.0 gpio
connect_bd_intf_net [get_bd_intf_pins axi_gpio_0/GPIO] [get_bd_intf_ports gpio]

# Assign addresses
assign_bd_address

# Restore current instance
current_bd_instance $oldCurInst

save_bd_design
