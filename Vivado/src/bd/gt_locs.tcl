
# Opsero Electronic Design Inc. Copyright 2025

# GT LOC constraints are required in the configuration of the PCS/PMA (PG047) IPs.
# The following code constructs a nested dictionary that contains the GT assignments for
# each target board for ports 0,1,2,3 of the Ethernet FMC Max.
# The Versal targets do not use this dictionary - on Versal the GT quad placement is
# derived from the package pin constraints of the GT serial port.

# To use the dictionary:
#   * Get the GT coordinate:    dict get $gt_loc_dict <target> <port number>

dict set gt_loc_dict uzev 0 X0Y8
dict set gt_loc_dict uzev 1 X0Y9
dict set gt_loc_dict uzev 2 X0Y10
dict set gt_loc_dict uzev 3 X0Y11
dict set gt_loc_dict zcu102_hpc0 0 X1Y10
dict set gt_loc_dict zcu102_hpc0 1 X1Y9
dict set gt_loc_dict zcu102_hpc0 2 X1Y11
dict set gt_loc_dict zcu102_hpc0 3 X1Y8
dict set gt_loc_dict zcu106_hpc0 0 X0Y14
dict set gt_loc_dict zcu106_hpc0 1 X0Y13
dict set gt_loc_dict zcu106_hpc0 2 X0Y15
dict set gt_loc_dict zcu106_hpc0 3 X0Y12
dict set gt_loc_dict zcu111 0 X0Y8
dict set gt_loc_dict zcu111 1 X0Y9
dict set gt_loc_dict zcu111 2 X0Y10
dict set gt_loc_dict zcu111 3 X0Y11
