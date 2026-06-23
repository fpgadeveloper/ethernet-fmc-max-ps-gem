# Opsero Electronic Design Inc. Copyright 2025
#
# Implementation init hook: waive known-benign CRITICAL WARNINGs.
#
# Wired into the implementation run as STEPS.INIT_DESIGN.TCL.PRE by build.tcl, so
# it runs inside the impl run process before constraints are read and clocks are
# derived -- i.e. before the messages it targets are issued. A set_msg_config in
# the build/xsa session would NOT work: the impl run is a separate process and
# set_msg_config is rejected inside .xdc files ([Designutils 20-1307]).
#
# [Timing 38-249] "Generated clock <...>/i_bufg_sgmii_clk_f/I has no logical paths
# from master clock txoutclk_out[0]_N":
#   The Ethernet PCS/PMA core (PG047) shares its clocking across ports -- only the
#   master port drives txoutclk, while the other ports reference the shared
#   i_bufg_sgmii_clk_f generated clock, which therefore has no logical path from
#   the master clock. This is a known-benign condition for multi-port SGMII.
#   Demote it to a WARNING (scoped via -string to these SGMII clocks only, so a
#   genuine 38-249 on any other clock stays CRITICAL) so it no longer trips the
#   build's critical-warning gate while remaining visible in the log.
set_msg_config -id {Timing 38-249} -string {i_bufg_sgmii_clk_f} -new_severity {WARNING}
