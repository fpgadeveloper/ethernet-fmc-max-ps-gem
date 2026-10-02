#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Opsero Electronic Design Inc.
"""
Generate the block diagrams for the Opsero Ethernet FMC Max (OP080) PS GEM reference
design docs, one per device family:

    psgem-zynqmp-block-diagram.png   Zynq UltraScale+ targets (uzev, zcu102_hpc0,
                                     zcu106_hpc0, zcu111): 4 ports, GEM0-3
    psgem-versal-block-diagram.png   Versal target (vck190_fmcp1): 2 ports, GEM0-1

Both drawings follow the block designs built by Vivado/src/bd/bd_zynqmp.tcl and
Vivado/src/bd/bd_versal.tcl; the box titles are the names of the block-design cells.
Per port: PS GEM <-EMIO GMII-> gig_ethernet_pcs_pma (PG047, SGMII, "Ethernet MAC: GEM"
mode) <-> one transceiver lane <-FMC DPn-> DP83867 PHY <-> RJ45. GEM0's MDIO master
drives the mdio_bus combiner, which puts the four external PHYs (MDIO 1, 3, 12, 15)
and the management interfaces of the PCS/PMA cores (MDIO 8 + port) on one bus.

The palette and the drawing helpers are shared with the other Opsero reference-design
block diagrams. The output PNGs are written next to this script (i.e. into
docs/source/images/).

Usage (from anywhere):
    python3 docs/source/images/gen_block_diagram.py
"""

import os
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Polygon, FancyBboxPatch, FancyArrowPatch
from matplotlib.lines import Line2D

# ---- palette (shared with the other Opsero reference-design block diagrams) --
C_PS_FILL      = "#D9D9D9"; C_PS_EDGE      = "#7F7F7F"   # processor column
C_FAB_FILL     = "#F2F2F2"; C_FAB_EDGE     = "#BFBFBF"   # FPGA fabric container
C_MAC_FILL     = "#E8E8F2"; C_MAC_EDGE     = "#8C8CC0"   # soft IP in the PL (lavender)
C_GT_FILL      = "#F3EFE2"; C_GT_EDGE      = "#BFB585"   # hard blocks: GEM, GT (cream)
C_FMC_FILL     = "#DCE6F2"; C_FMC_EDGE     = "#9DB7D4"   # external FMC (blue-grey)
C_CAGE_FILL    = "#FFFFFF"                                # parts on the FMC (white)
C_CLK_FILL     = "#FDE9D9"; C_CLK_EDGE     = "#E0B090"   # clocking (peach)
C_CTRL_FILL    = "#ECECEC"; C_CTRL_EDGE    = "#BFBFBF"   # control plane
C_AXARR_FILL   = "#EDF3D4"; C_AXARR_EDGE   = "#A6B85A"   # data arrows (pale green)
C_LINKARR_FILL = "#DAE8F5"; C_LINKARR_EDGE = "#6F9FCF"   # link arrows (pale blue)
C_REFCLK_LINE  = "#C8823C"                                # refclk arrows (orange)
TXT = "#1A1A1A"
# additions for this design
C_MDIO         = "#7B5AA6"                                # MDIO bus (purple)
C_RST          = "#B04A4A"                                # PHY resets (red)
C_MUTED        = "#8C8C8C"                                # unused ports / notes
C_UNUSED_FILL  = "#F7F7F7"

PHY_ADDR = {0: 1, 1: 3, 2: 12, 3: 15}                     # DP83867 MDIO addresses
MAC_ADDR = {0: "00:0a:35:00:01:22", 1: "00:0a:35:00:01:23",
            2: "00:0a:35:00:01:24", 3: "00:0a:35:00:01:25"}


def box(ax, x, y, w, h, fc, ec, label, fs=10, rot=0, lw=1.2, weight="normal",
        txtcolor=None, ls="-", z=2):
    ax.add_patch(plt.Rectangle((x, y), w, h, fc=fc, ec=ec, lw=lw, ls=ls, zorder=z))
    if label:
        ax.text(x + w / 2, y + h / 2, label, ha="center", va="center",
                fontsize=fs, rotation=rot, color=txtcolor or TXT, weight=weight,
                zorder=z + 1, linespacing=1.25)


def titled_box(ax, x, y, w, h, fc, ec, title, body, title_fs=9.5, body_fs=7.6,
               lw=1.2, txtcolor=None, title_dy=2.6, ls="-", z=2):
    """A box() with a bold title line at the top and a smaller body below it."""
    box(ax, x, y, w, h, fc, ec, "", lw=lw, ls=ls, z=z)
    cx = x + w / 2
    ax.text(cx, y + h - title_dy, title, ha="center", va="center",
            fontsize=title_fs, weight="bold", color=txtcolor or TXT, zorder=z + 1)
    ax.text(cx, y + (h - title_dy * 1.9) / 2, body, ha="center", va="center",
            fontsize=body_fs, color=txtcolor or TXT, zorder=z + 1, linespacing=1.3)


def harrow(ax, x0, x1, yc, label, fc, ec, double=True, bh=2.0, hh=3.4, hl=3.2,
           fs=8.5, lw=1.1, lab_dy=0.0, lab_color=None, weight="normal"):
    """Horizontal block arrow from x0 to x1 (double-headed, or head at x1)."""
    if double:
        pts = [(x0, yc), (x0 + hl, yc + hh), (x0 + hl, yc + bh),
               (x1 - hl, yc + bh), (x1 - hl, yc + hh), (x1, yc),
               (x1 - hl, yc - hh), (x1 - hl, yc - bh),
               (x0 + hl, yc - bh), (x0 + hl, yc - hh)]
    else:
        s = 1.0 if x1 >= x0 else -1.0
        neck = x1 - s * hl
        pts = [(x0, yc + bh), (neck, yc + bh), (neck, yc + hh),
               (x1, yc), (neck, yc - hh), (neck, yc - bh), (x0, yc - bh)]
    ax.add_patch(Polygon(pts, closed=True, fc=fc, ec=ec, lw=lw, zorder=2))
    if label:
        ax.text((x0 + x1) / 2, yc + lab_dy, label, ha="center", va="center",
                fontsize=fs, color=lab_color or TXT, zorder=3, linespacing=1.15,
                weight=weight)


def route(ax, pts, color, lw=1.6, head=True, ls="-"):
    """Thin elbow line through pts (arrow head at the last point if head)."""
    if head:
        xs, ys = zip(*pts[:-1])
        ax.add_line(Line2D(xs, ys, color=color, lw=lw, ls=ls, zorder=3,
                           solid_capstyle="butt", solid_joinstyle="miter"))
        ax.add_patch(FancyArrowPatch(pts[-2], pts[-1], arrowstyle="-|>",
                                     mutation_scale=10, lw=lw, color=color,
                                     zorder=3, shrinkA=0, shrinkB=0))
    else:
        xs, ys = zip(*pts)
        ax.add_line(Line2D(xs, ys, color=color, lw=lw, ls=ls, zorder=3,
                           solid_capstyle="butt", solid_joinstyle="miter"))


def dot(ax, x, y, color):
    ax.add_patch(plt.Circle((x, y), 0.55, color=color, zorder=4))


def label(ax, x, y, text, fs=7.0, color=TXT, ha="center", weight="normal", rot=0):
    ax.text(x, y, text, ha=ha, va="center", fontsize=fs, color=color,
            weight=weight, rotation=rot, zorder=4, linespacing=1.2)


def draw(family):
    versal = family == "versal"
    ports = [0, 1] if versal else [0, 1, 2, 3]

    fig, ax = plt.subplots(figsize=(19.5, 13.4), dpi=120)
    ax.set_xlim(0, 195)
    ax.set_ylim(0, 134)
    ax.axis("off")

    # vertical plan: one row per FMC port (port 0 on top), control strip below
    row_h = 15.0
    row_yc = {0: 108.0, 1: 89.0, 2: 70.0, 3: 51.0}
    unused_y0, unused_y1 = 62.0, 76.0          # Versal: band for ports 2 and 3

    # ---- processing system column ------------------------------------------
    ps_x0, ps_x1 = 2.0, 36.0
    box(ax, ps_x0, 4.0, ps_x1 - ps_x0, 124.0, C_PS_FILL, C_PS_EDGE, "", lw=1.3)
    pcx = 11.5
    if versal:
        ps_title, ps_cpu, ps_axi = "Versal PS\n(CIPS)", "Arm Cortex-A72", "M_AXI_LPD"
    else:
        ps_title, ps_cpu, ps_axi = ("Zynq\nUltraScale+\nPS", "Arm Cortex-A53",
                                    "M_AXI_HPM0_FPD")
    label(ax, pcx, 121.0 if versal else 119.5, ps_title, fs=11.0, weight="bold")
    label(ax, pcx, 66.0,
          ps_cpu + "\n\nLinux:\nmacb driver\n(PetaLinux or\nYocto)\n\n"
          "bare-metal:\nemacps +\nlwIP echo\nserver", fs=7.4)

    gem_x0, gem_w = 22.0, 13.0
    for p in ports:
        yc = row_yc[p]
        gem_body = ("hard MAC\n10/100/1000\n\nMDIO master\n(EMIO)" if p == 0
                    else "hard MAC\n10/100/1000\n\nMDIO unused")
        titled_box(ax, gem_x0, yc - row_h / 2, gem_w, row_h, C_GT_FILL, C_GT_EDGE,
                   f"GEM{p}", gem_body, title_fs=9.6, body_fs=6.8, title_dy=2.4)
    if versal:
        titled_box(ax, gem_x0, unused_y0, gem_w, unused_y1 - unused_y0,
                   C_UNUSED_FILL, C_MUTED, "no GEM2/3", "the Versal PS\nhas two GEMs",
                   title_fs=8.0, body_fs=6.8, txtcolor=C_MUTED, ls="--")

    # ---- PL fabric container ------------------------------------------------
    fab_x0, fab_x1 = 40.0, 124.0
    ax.add_patch(plt.Rectangle((fab_x0, 4.0), fab_x1 - fab_x0, 124.0,
                               fc=C_FAB_FILL, ec=C_FAB_EDGE, lw=1.3, zorder=1))
    fab_title = "Versal PL  (XCVC1902, VCK190)" if versal else "Zynq UltraScale+ PL"
    ax.text((fab_x0 + fab_x1) / 2, 128.6, fab_title, ha="center", va="bottom",
            fontsize=13, weight="bold", color=TXT)

    pcs_x0, pcs_w = 52.0, 29.0
    gt_x0, gt_w = 92.0, 14.0
    for p in ports:
        yc = row_yc[p]
        y0 = yc - row_h / 2
        body = ("PG047, SGMII over GT\n\"Ethernet MAC: GEM\" mode\n"
                f"auto-negotiation on\nmgmt MDIO address {8 + p}")
        if p == 0 and not versal:
            body += "\nshared logic: clocks ports 1-3"
        titled_box(ax, pcs_x0, y0, pcs_w, row_h, C_MAC_FILL, C_MAC_EDGE,
                   f"pcs_pma_{p}", body, title_fs=9.4, body_fs=6.8, title_dy=2.3)
        # GEM <-> PCS/PMA: EMIO GMII
        harrow(ax, gem_x0 + gem_w, pcs_x0, yc + 1.5, "", C_AXARR_FILL,
               C_AXARR_EDGE, bh=1.5, hh=2.7, hl=1.8)
        label(ax, (gem_x0 + gem_w + pcs_x0) / 2 + 1.5, yc + 6.0, "EMIO GMII", fs=6.9)
        label(ax, (gem_x0 + gem_w + pcs_x0) / 2 + 1.5, yc - 3.6,
              "GMII clocks\nfrom PCS/PMA", fs=6.0, color="#404040")
        # PCS/PMA <-> GT lane
        harrow(ax, pcs_x0 + pcs_w, gt_x0, yc, "", C_AXARR_FILL, C_AXARR_EDGE,
               bh=1.4, hh=2.5, hl=1.6)
        if not versal:
            titled_box(ax, gt_x0, y0, gt_w, row_h, C_GT_FILL, C_GT_EDGE,
                       "GT lane", "GTH / GTY\n1.25 Gb/s\nSGMII",
                       title_fs=8.6, body_fs=6.9, title_dy=2.3)
    if versal:
        box(ax, pcs_x0, unused_y0, pcs_w, unused_y1 - unused_y0,
            C_UNUSED_FILL, C_MUTED, "ports 2 and 3: not used\n"
            "no GEM, no PCS/PMA core,\nGT lanes unconnected,\nPHY resets tied low",
            fs=7.2, txtcolor=C_MUTED, ls="--")
        q_y0, q_y1 = row_yc[1] - row_h / 2, row_yc[0] + row_h / 2
        titled_box(ax, gt_x0, q_y0, gt_w, q_y1 - q_y0, C_GT_FILL, C_GT_EDGE,
                   "gt_quad_base", "GTY quad\n\nch0 → port 0\nch1 → port 1\n\n"
                   "1.25 Gb/s\nSGMII\n\n4 BUFG_GT\nper channel\n(user clocks)",
                   title_fs=8.2, body_fs=6.8, title_dy=2.4)

    # ---- mdio_bus combiner ----------------------------------------------------
    mb_x0, mb_w, mb_h = 52.0, 29.0, 10.0
    mb_y0 = 44.0 if versal else 28.0
    titled_box(ax, mb_x0, mb_y0, mb_w, mb_h, C_MAC_FILL, C_MDIO, "mdio_bus",
               "RTL combiner: one MDIO bus for\nPHYs 1, 3, 12, 15 + PCS/PMA 8-11",
               title_fs=8.8, body_fs=6.7, title_dy=2.2, lw=1.5)
    # GEM0 MDIO master -> mdio_bus (down the gap between the PS and the PL)
    g0_y = row_yc[0] - row_h / 2 + 2.0
    route(ax, [(gem_x0 + gem_w, g0_y), (38.0, g0_y), (38.0, mb_y0 + 5.0),
               (mb_x0, mb_y0 + 5.0)], C_MDIO, lw=1.7)
    label(ax, 46.0, mb_y0 + mb_h + 2.4, "GEM0 MDIO (EMIO)", fs=6.6,
          color=C_MDIO, weight="bold")
    # mdio_bus -> PCS/PMA management interfaces
    bus_x = 84.5
    mg_y = mb_y0 + 8.0
    top_y = row_yc[0] - row_h / 2 + 2.5
    route(ax, [(mb_x0 + mb_w, mg_y), (bus_x, mg_y), (bus_x, top_y)], C_MDIO,
          lw=1.2, head=False)
    for p in ports:
        yy = row_yc[p] - row_h / 2 + 2.5
        route(ax, [(bus_x, yy), (pcs_x0 + pcs_w, yy)], C_MDIO, lw=1.2)
        dot(ax, bus_x, yy, C_MDIO)
    label(ax, bus_x + 1.5, 58.0 if versal else (row_yc[3] - row_h / 2 + mg_y) / 2,
          "mgmt\nMDIO", fs=6.0, color=C_MDIO, ha="left")

    # ---- external: Ethernet FMC Max ------------------------------------------
    fmc_x0, fmc_x1 = 132.0, 168.0
    ax.add_patch(plt.Rectangle((fmc_x0, 4.0), fmc_x1 - fmc_x0, 124.0,
                               fc=C_FMC_FILL, ec=C_FMC_EDGE, lw=1.3, zorder=1))
    ax.text((fmc_x0 + 194.0) / 2, 128.6, "External to the FPGA", ha="center",
            va="bottom", fontsize=12, weight="bold", color=TXT)
    slot = "FMCP1" if versal else "HPC / HPC0 / FMCP"
    label(ax, (fmc_x0 + fmc_x1) / 2, 123.0,
          f"Ethernet FMC Max (OP080)\non {slot}", fs=9.4, weight="bold")
    phy_x0, phy_w = 136.0, 22.0
    for p in (0, 1, 2, 3):
        yc = row_yc[p]
        used = p in ports
        titled_box(ax, phy_x0, yc - row_h / 2, phy_w, row_h,
                   C_CAGE_FILL if used else C_UNUSED_FILL,
                   C_FMC_EDGE if used else C_MUTED, f"Port {p}: DP83867",
                   (f"MDIO address {PHY_ADDR[p]}\nSGMII ↔ 1000BASE-T\n"
                    f"MAC {MAC_ADDR[p]}" if used else
                    f"MDIO address {PHY_ADDR[p]}\nheld in reset\n(not used)"),
                   title_fs=8.4, body_fs=6.6, title_dy=2.3,
                   txtcolor=None if used else C_MUTED, ls="-" if used else "--")
        box(ax, phy_x0 + phy_w, yc - 2.5, 4.0, 5.0, "#FFFFFF",
            C_FMC_EDGE if used else C_MUTED, "RJ45", fs=5.6,
            txtcolor=None if used else C_MUTED)
        if used:
            harrow(ax, gt_x0 + gt_w, phy_x0, yc, "", C_LINKARR_FILL,
                   C_LINKARR_EDGE, bh=1.6, hh=2.9, hl=1.8)
            label(ax, (gt_x0 + gt_w + phy_x0) / 2, yc + 5.0, f"FMC DP{p}", fs=6.9)
            label(ax, (gt_x0 + gt_w + phy_x0) / 2, yc - 5.0, "SGMII 1.25 Gb/s",
                  fs=6.4, color="#404040")

    # shared MDIO from the PL to the PHYs (FMC LA17_CC pair)
    mx = 130.0
    ext_y = mb_y0 + 3.0
    lo_y = row_yc[ports[-1]] - row_h / 2 + 1.5
    hi_y = row_yc[0] - row_h / 2 + 1.5
    route(ax, [(mb_x0 + mb_w, ext_y), (mx, ext_y), (mx, hi_y)], C_MDIO, lw=1.7,
          head=False)
    for p in ports:
        yy = row_yc[p] - row_h / 2 + 1.5
        route(ax, [(mx, yy), (phy_x0, yy)], C_MDIO, lw=1.2)
        dot(ax, mx, yy, C_MDIO)
    label(ax, 104.0, ext_y + 1.8, "MDC / MDIO  (FMC LA17_CC)", fs=6.4,
          color=C_MDIO, weight="bold")

    # Si511 GT reference clock on the FMC
    si_y0, si_y1 = 21.0, 31.0
    titled_box(ax, phy_x0, si_y0, phy_w + 4.0, si_y1 - si_y0, C_CLK_FILL,
               C_CLK_EDGE, "Si511", "125 MHz GT reference\n→ FMC GBTCLK0",
               title_fs=8.6, body_fs=6.8, title_dy=2.2)
    rc_y = 25.0
    if versal:
        ib_x0, ib_w, ib_y0, ib_h = 108.0, 14.0, 20.5, 8.0
        titled_box(ax, ib_x0, ib_y0, ib_w, ib_h, C_CLK_FILL, C_CLK_EDGE,
                   "util_ds_buf_0", "IBUFDSGTE", title_fs=7.4, body_fs=6.8,
                   title_dy=2.0)
        route(ax, [(phy_x0, rc_y), (ib_x0 + ib_w, rc_y)], C_REFCLK_LINE, lw=1.8)
        route(ax, [(ib_x0 + 4.0, ib_y0 + ib_h), (ib_x0 + 4.0, 85.0),
                   (gt_x0 + gt_w, 85.0)], C_REFCLK_LINE, lw=1.8)
        label(ax, ib_x0 + 2.4, 66.0, "GT_REFCLK0", fs=6.4, color=C_REFCLK_LINE,
              weight="bold", rot=90)
    else:
        # Si511 -> pcs_pma_0 gtrefclk_in (the core with the shared logic)
        rc_x = 88.5
        route(ax, [(phy_x0, rc_y), (rc_x, rc_y), (rc_x, row_yc[0] + 4.5),
                   (pcs_x0 + pcs_w, row_yc[0] + 4.5)], C_REFCLK_LINE, lw=1.8)
        label(ax, 107.0, rc_y - 1.9, "gt_ref_clk 125 MHz → pcs_pma_0 gtrefclk_in",
              fs=6.2, color=C_REFCLK_LINE, weight="bold")

    # link partner
    lp_x0 = 172.0
    lp_y0 = row_yc[ports[-1]] - row_h / 2
    lp_y1 = row_yc[0] + row_h / 2
    titled_box(ax, lp_x0, lp_y0, 22.0, lp_y1 - lp_y0, C_CAGE_FILL, C_LINKARR_EDGE,
               "Link partner",
               "PC NIC, switch\nor DHCP router\n\nCat5e / Cat6\ncable per port\n\n"
               "1000BASE-T\n(10/100 also)\n\ne.g. iperf3\nserver",
               title_fs=9.0, body_fs=7.0, title_dy=2.8, lw=1.3)
    for p in ports:
        harrow(ax, phy_x0 + phy_w + 4.0, lp_x0, row_yc[p], "", C_LINKARR_FILL,
               C_LINKARR_EDGE, bh=1.2, hh=2.3, hl=1.4)

    # ---- control plane, resets, clocks (bottom strip) -------------------------
    rx = 128.0                                   # PHY reset bus
    harrow(ax, ps_x1, 44.0, 12.5, "", C_CTRL_FILL, C_PS_EDGE, double=False,
           bh=1.3, hh=2.5, hl=1.8)
    label(ax, (ps_x1 + 44.0) / 2 + 0.3, 17.6, ps_axi, fs=5.6, color="#404040")
    if versal:
        titled_box(ax, 44.0, 6.0, 30.0, 13.0, C_CTRL_FILL, C_CTRL_EDGE,
                   "axi_smc  (SmartConnect)",
                   "M00 → axi_apb_bridge_0\n→ gt_quad_base APB3\nM01 → axi_gpio_0",
                   title_fs=7.8, body_fs=6.6, title_dy=2.2)
        titled_box(ax, 76.0, 6.0, 28.0, 13.0, C_CTRL_FILL, C_CTRL_EDGE,
                   "axi_gpio_0", "10 inputs:\nFMC power-good ×2\nPHY GPIO ×8",
                   title_fs=7.8, body_fs=6.6, title_dy=2.2)
        # PHY resets from PMC GPIO EMIO
        titled_box(ax, 44.0, 25.0, 37.0, 12.0, C_CTRL_FILL, C_RST,
                   "PHY resets: PMC GPIO (EMIO) bits 0, 1",
                   "pulsed by U-Boot (Linux images) or by the\n"
                   "standalone app, after VADJ is switched on",
                   title_fs=7.6, body_fs=6.5, title_dy=2.1)
        harrow(ax, ps_x1, 44.0, 31.0, "", C_CTRL_FILL, C_RST, double=False,
               bh=1.0, hh=2.0, hl=1.6)
        route(ax, [(81.0, 31.0), (rx, 31.0), (rx, row_yc[0] - row_h / 2 + 4.0)],
              C_RST, lw=1.4, head=False)
        for p in ports:
            yy = row_yc[p] - row_h / 2 + 4.0
            route(ax, [(rx, yy), (phy_x0, yy)], C_RST, lw=1.4)
            dot(ax, rx, yy, C_RST)
        label(ax, rx - 1.6, 58.0, "PHY reset", fs=6.4, color=C_RST, weight="bold",
              rot=90)
        titled_box(ax, phy_x0, 6.0, phy_w + 4.0, 13.0, C_CLK_FILL, C_RST,
                   "VADJ = 1.5 V", "VCK190 regulator, switched on\nover I2C by U-Boot "
                   "or\nby the standalone app", title_fs=8.2, body_fs=6.4,
                   title_dy=2.2)
        label(ax, 18.5, 15.0,
              "Clocks (CIPS):\npl0_ref_clk 100 MHz\n→ AXI, APB, GPIO\n\n"
              "pl1_ref_clk 50 MHz\n→ PCS/PMA\nindependent (DRP)\nclock",
              fs=6.4, color="#404040")
    else:
        titled_box(ax, 44.0, 6.0, 28.0, 13.0, C_CTRL_FILL, C_CTRL_EDGE,
                   "axi_gpio_0", "10 inputs:\nFMC power-good ×2\nPHY GPIO ×8",
                   title_fs=7.8, body_fs=6.6, title_dy=2.2)
        titled_box(ax, 76.0, 6.0, 42.0, 13.0, C_CTRL_FILL, C_RST,
                   "rst_100m  (proc_sys_reset)",
                   "peripheral_aresetn → PHY resets 0-3\n(released when the PS is up)\n"
                   "peripheral_reset → PCS/PMA reset",
                   title_fs=7.8, body_fs=6.6, title_dy=2.2)
        route(ax, [(118.0, 12.5), (rx, 12.5), (rx, row_yc[0] - row_h / 2 + 4.0)],
              C_RST, lw=1.4, head=False)
        for p in ports:
            yy = row_yc[p] - row_h / 2 + 4.0
            route(ax, [(rx, yy), (phy_x0, yy)], C_RST, lw=1.4)
            dot(ax, rx, yy, C_RST)
        label(ax, rx - 1.6, 60.5, "PHY reset", fs=6.4, color=C_RST, weight="bold",
              rot=90)
        titled_box(ax, phy_x0, 6.0, phy_w + 4.0, 13.0, C_CLK_FILL, C_CLK_EDGE,
                   "VADJ = 1.8 V", "provided by the carrier\n(FMC I/O: LVCMOS18)",
                   title_fs=8.2, body_fs=6.6, title_dy=2.2)
        label(ax, 18.5, 15.0,
              "Clocks (PS):\npl_clk0 100 MHz\n→ AXI, GPIO, resets\n\n"
              "pl_clk1 50 MHz\n→ PCS/PMA\nindependent (DRP)\nclock",
              fs=6.4, color="#404040")

    out = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                       f"psgem-{family}-block-diagram.png")
    fig.savefig(out, bbox_inches="tight", pad_inches=0.15, facecolor="white")
    plt.close(fig)
    print("wrote", out)


def main():
    draw("zynqmp")
    draw("versal")


if __name__ == "__main__":
    main()
