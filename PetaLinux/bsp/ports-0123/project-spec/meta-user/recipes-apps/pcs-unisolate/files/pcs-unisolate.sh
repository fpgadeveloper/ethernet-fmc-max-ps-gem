#!/bin/sh
#
# pcs-unisolate.sh  --  Opsero Electronic Design Inc.
#
# Clear the IEEE ISOLATE bit on the PL "1G/2.5G Ethernet PCS/PMA" (PG047, GEM
# mode) cores of the Ethernet FMC Max PS-GEM reference design, so the SGMII
# datapath passes traffic under Linux.
#
# ============================= WHY THIS EXISTS =============================
# Topology: each PS GEM drives the FPGA fabric over EMIO GMII into a PL PCS/PMA
# core (GEM mode), which bridges to SGMII over a GT to an on-card TI DP83867
# copper PHY.  The PCS/PMA cores are MDIO slaves at address (8 + port) on GEM0's
# MDIO bus; the external DP83867 PHYs are at addresses 1/3/12/15.
#
# Per IEEE 802.3 clause 22, a PHY/PCS strapped to a NON-ZERO MDIO address powers
# up ISOLATED (control register 0, bit 10 = 1).  In that state SGMII auto-neg
# still completes -- so the link reports "Up - 1Gbps/Full" -- but the GMII path
# to the MAC is cut, so RX is dead and DHCP never completes.  Clearing bit 10
# over MDIO is the standard, Xilinx-intended step (their own bare-metal and
# u-boot code does it; see get_Xilinx_pcs_pma_phy_speed()).
#
# Bare-metal and u-boot clear it themselves.  Under Linux the PCS core is an
# intermediate block that the macb driver does NOT manage (the GEM's phy-handle
# points at the DP83867, not the PCS), and -- as of linux-xlnx 6.12 / 2025.2 --
# there is no kernel owner for it: macb has no external-PCS support and
# drivers/net/pcs/ has no Xilinx PCS driver.  So nothing clears ISOLATE; we do
# it here, once, at boot.
#
# ====================== PROPER FIX / WHEN TO REMOVE =======================
# This service is a stop-gap.  The upstream-intended mechanism is Sean
# Anderson's "pcs-xilinx" PCS driver + Cadence-macb external-PCS support, which
# model the core as an "xlnx,pcs" MDIO node referenced by the GEM's "pcs-handle";
# phylink then clears ISOLATE and runs SGMII auto-neg with no userspace help.
# That series was NOT merged as of linux 6.12 (still in net-next review).
# WHEN A FUTURE KERNEL SHIPS IT: describe the PCS cores in the device tree
# (an "xlnx,pcs" MDIO node + a "pcs-handle" on each gem), delete this recipe,
# and remove the matching note in docs/source/advanced.md.
# ==========================================================================
#
# Mechanism: the cores are reached by driving GEM0's PHY-maintenance register
# (PHYMNTNC, base + 0x34) directly with busybox 'devmem' -- no extra packages.
# GEM0's interface is brought up first so the controller is clocked (the macb
# driver runtime-suspends the GEM while its interface is down).  If 'phytool' is
# installed it is used instead, which goes through the kernel MDIO bus and so
# serialises against the PHY-poll worker.

PATH=/usr/sbin:/usr/bin:/sbin:/bin

log() {
	echo "pcs-unisolate: $*"
	echo "pcs-unisolate: $*" > /dev/kmsg 2>/dev/null || true
}

# ---- GEM0 (the MDIO master) base address, by SoC family ----
if grep -aq 'xlnx,versal' /proc/device-tree/compatible 2>/dev/null; then
	GEM0_BASE=0xFF0C0000
elif grep -aq 'xlnx,zynqmp' /proc/device-tree/compatible 2>/dev/null; then
	GEM0_BASE=0xFF0B0000
else
	log "unrecognised SoC; not clearing PCS isolate"
	exit 0
fi
GEM0_NODE=$(printf '%x.ethernet' "$GEM0_BASE")   # e.g. ff0c0000.ethernet
PHYMNTNC=$(printf '0x%08X' $((GEM0_BASE + 0x34)))

# ---- find GEM0's netdev and bring it up so the GEM is clocked ----
IFACE=
for d in /sys/class/net/*; do
	[ -e "$d/device" ] || continue
	case "$(readlink -f "$d/device")" in
		*"$GEM0_NODE"*) IFACE=$(basename "$d"); break ;;
	esac
done
if [ -n "$IFACE" ]; then
	ifconfig "$IFACE" up 2>/dev/null || ip link set "$IFACE" up 2>/dev/null || true
	sleep 1
else
	log "warning: GEM0 netdev ($GEM0_NODE) not found; attempting MDIO anyway"
fi

# Use phytool (kernel MDIO bus, serialised) when available, else raw devmem.
USE_PHYTOOL=
METHOD=devmem
if command -v phytool >/dev/null 2>&1 && [ -n "$IFACE" ]; then
	USE_PHYTOOL=1
	METHOD=phytool
fi

# PHYMNTNC fields (Cadence GEM): [31:30]=01 [29:28]=op(read=10,write=01)
# [27:23]=phyaddr [22:18]=regaddr [17:16]=10 [15:0]=data
mdio_rd() {   # phyaddr reg -> prints 0xNNNN (16-bit), or nothing on failure
	if [ -n "$USE_PHYTOOL" ]; then
		phytool read "$IFACE/$1/$2" 2>/dev/null
	else
		devmem "$PHYMNTNC" 32 \
			"$(printf '0x%08X' $(( (1<<30)|(2<<28)|($1<<23)|($2<<18)|(2<<16) )))" 2>/dev/null
		v=$(devmem "$PHYMNTNC" 2>/dev/null)
		[ -n "$v" ] || return 0
		printf '0x%04X' $(( v & 0xFFFF ))
	fi
}
mdio_wr() {   # phyaddr reg data
	if [ -n "$USE_PHYTOOL" ]; then
		phytool write "$IFACE/$1/$2" "$3" 2>/dev/null
	else
		devmem "$PHYMNTNC" 32 \
			"$(printf '0x%08X' $(( (1<<30)|(1<<28)|($1<<23)|($2<<18)|(2<<16)|($3 & 0xFFFF) )))" 2>/dev/null
	fi
}

ISOLATE=0x0400      # control reg 0, bit 10
CLEARVAL=0x1340     # AN-enable + AN-restart + full-duplex + 1000Mbps, isolate=0

# PCS cores live at MDIO address (8 + port); external PHYs are at 1/3/12/15 and
# are deliberately not touched.  Reading 0xFFFF means no core at that address,
# so this loop is correct for both the 2-port and 4-port variants.
cleared=0
for a in 8 9 10 11; do
	v=$(mdio_rd "$a" 0)
	[ -n "$v" ] || continue
	vv=$(( v & 0xFFFF ))
	[ "$vv" -eq 65535 ] && continue
	if [ $(( vv & ISOLATE )) -ne 0 ]; then
		n=0; r=$v
		while [ "$n" -lt 3 ]; do
			mdio_wr "$a" 0 "$CLEARVAL"
			r=$(mdio_rd "$a" 0)
			[ -n "$r" ] && [ $(( r & ISOLATE )) -eq 0 ] && break
			n=$((n + 1))
		done
		log "PCS core @ MDIO $a: reg0 $v -> $r (ISOLATE cleared)"
		cleared=$((cleared + 1))
	else
		log "PCS core @ MDIO $a: reg0 $v (already de-isolated)"
	fi
done
log "done ($cleared core(s) cleared via $METHOD)"
exit 0
