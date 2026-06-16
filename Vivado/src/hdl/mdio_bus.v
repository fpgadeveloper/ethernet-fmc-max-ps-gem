// Opsero Electronic Design Inc. Copyright 2026
//
// MDIO bus combiner: places the management interfaces of up to four
// PCS/PMA cores AND the external (Ethernet FMC Max) MDIO bus on the
// single MDIO master of GEM0. MDIO is an open-drain wired-AND bus:
// the master sees the AND of every potential driver; a device that is
// not driving presents logic 1 (mdio_t = 1).
//
// This exists because the PCS/PMA cores (PG047, "Ethernet MAC: GEM"
// mode) come out of reset with the ISOLATE bit (reg 0, bit 10) set,
// so software MUST be able to reach every core over MDIO to clear it.
//
// The external bus is exposed as an mdio_rtl interface (separate
// O/T/I signals): the tri-state pad merge is done by the generated
// HDL wrapper at the top level. Putting an inout inside this module
// does NOT work - it is synthesized out-of-context and the tri-state
// gets converted to logic (Synth 8-5799), leaving the pad's input
// path unconnected.
//
// Unused slave ports must be tied HIGH (mdio_o = 1, mdio_t = 1).

module mdio_bus (
    // GEM MDIO master (EMIO)
    input  wire gem_mdc,
    input  wire gem_mdio_o,
    input  wire gem_mdio_t,
    output wire gem_mdio_i,
    // Common to all PCS/PMA management slaves
    output wire slave_mdc,
    output wire slave_mdio_i,
    // Per-slave outputs (PCS/PMA mdio_o / mdio_t)
    input  wire s0_mdio_o,
    input  wire s0_mdio_t,
    input  wire s1_mdio_o,
    input  wire s1_mdio_t,
    input  wire s2_mdio_o,
    input  wire s2_mdio_t,
    input  wire s3_mdio_o,
    input  wire s3_mdio_t,
    // External MDIO bus (board pull-up keeps it high when undriven)
    (* X_INTERFACE_INFO = "xilinx.com:interface:mdio:1.0 ext_mdio MDC" *)
    output wire ext_mdc,
    (* X_INTERFACE_INFO = "xilinx.com:interface:mdio:1.0 ext_mdio MDIO_O" *)
    output wire ext_mdio_o,
    (* X_INTERFACE_INFO = "xilinx.com:interface:mdio:1.0 ext_mdio MDIO_T" *)
    output wire ext_mdio_t,
    (* X_INTERFACE_INFO = "xilinx.com:interface:mdio:1.0 ext_mdio MDIO_I" *)
    input  wire ext_mdio_i
);

    // Value the master is driving (1 when tri-stated = idle high)
    wire master_drive = gem_mdio_t ? 1'b1 : gem_mdio_o;

    assign slave_mdc    = gem_mdc;
    assign ext_mdc      = gem_mdc;
    assign slave_mdio_i = master_drive;

    assign ext_mdio_o = gem_mdio_o;
    assign ext_mdio_t = gem_mdio_t;

    // Wired-AND of every internal slave (idle/tri-state reads as 1)
    wire slaves_drive = (s0_mdio_o | s0_mdio_t) &
                        (s1_mdio_o | s1_mdio_t) &
                        (s2_mdio_o | s2_mdio_t) &
                        (s3_mdio_o | s3_mdio_t);

    assign gem_mdio_i = ext_mdio_i & slaves_drive & master_drive;

endmodule
