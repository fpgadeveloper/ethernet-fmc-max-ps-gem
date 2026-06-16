/*
 * Copyright (C) 2010 - 2022 Xilinx, Inc.
 * Copyright (C) 2022 - 2025 Advanced Micro Devices, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 */

/*****************************************************************************
* This file xemacpsif_physpeed.c implements functionalities to:
* - Detect the available PHYs connected to a MAC
* - Negotiate speed
* - Configure speed
* - Configure the SLCR registers for the negotiated speed
*
* In a typical use case, users of the APIs implemented in this file need to
* do the following.
* - Call the API detect_phy. It probes for the available PHYs connected to a MAC.
*   The MACs can be Emac0 (XPAR_XEMACPS_0_BASEADDR, 0xE000B000) or Emac1
*   (XPAR_XEMACPS_0_BASEADDR, 0xE000C000). It populates an array to notify
*   about the detected PHYs. The array phymapemac0 is used for Emac0 and
*   phymapemac1 is for Emac1.
* - The users need to parse the corresponding arrays, phymapemac0 or phymapemac1
*   to know the available PHYs for a MAC. The users then need to call
*   phy_setup_emacps to setup the PHYs for proper speed setting. The API
*   phy_setup_emacps should be called with the PHY address for which the speed
*   needs to be negotiated or configured. In a specific use case, if 2 PHYs are
*   connected to Emac0 with addresses of 7 and 11, then users get these address
*   details from phymapemac0 (after calling detect_phy) and then call
*   phy_setup_emacps twice, with ab address of 7 and 11.
* - Points to note: The MAC can operate at only one speed. If a MAC is connected
*   to multiple PHYs, then all PHYs must negotiate and configured for the same
*   speed.
* - This file implements static functions to set proper SLCR clocks. As stated
*   above, all PHYs connected to a PHY must operate at same speed and the SLCR
*   clock will be setup accordingly.
*
* This file implements the following PHY types.
* - The standard RGMII.
* - It provides support for GMII to RGMII converter Xilinx IP. This Xilinx IP
*   sits on the MDIO bus with a predefined PHY address. This IP exposes register
*   that needs to be programmed with the negotiated speed.
*   For example, in a typical design, the Emac0 or Emac1 exposes GMII interface.
*   The user can then use the Xilinx IP that converts GMII to RGMII.
*   The external PHY (most typically Marvell 88E1116R) negotiates for speed
*   with the remote PHY. The implementation in this file then programs the
*   Xilinx IP with this negotiated speed. The Xilinx IP has a predefined IP
*   address exposed through xparameters.h
* - The SGMII and 1000 BaseX PHY interfaces.
*   If the PHY interface is SGMII or 1000 BaseX a separate "get_IEEE_phy_speed"
*   is used which is different from standard RGMII "get_IEEE_phy_speed".
*   The 1000 BaseX always operates at 1000 Mbps. The SGMII interface can
*   negotiate speed accordingly.
*   For SGMII or 1000 BaseX interfaces, the detect_phy should not be called.
*   The phy addresses for these interfaces are fixed at the design time.
*
* Point to note:
* A MAC can not be connected to PHYs where there is a mix between
* SGMII or 1000 Basex or GMII/MII/RGMII.
* In a typical multiple PHY designs, it is expected that the PHYs connected
* will be RGMII or GMII.
*
* The users can choose not to negotiate speed from lwip settings GUI.
* If they opt to choose a particular PHY speed, then the PHY will hard code
* the speed to operate only at the corresponding speed. It will not advertise
* any other speeds. It is users responsibility to ensure that the remote PHY
* supports the speed programmed through the lwip gui.
*
* The following combination of MDIO/PHY are supported:
* - Multiple PHYs connected to the MDIO bus of a MAC. If Emac0 MDIO is connected
*   to single/multiple PHYs, it is supported. Similarly Emac1 MDIO connected to
*   single/multiple PHYs is supported.
* - A design where both the interfaces are present and are connected to their own
*   MDIO bus is supported.
*
* The following MDIO/PHY setup is not supported:
* - A design has both the MACs present. MDIO bus is available only for one MAC
*   (Emac0 or Emac1). This MDIO bus has multiple PHYs available for both the
*   MACs. The negotiated speed for PHYs sitting on the MDIO bus of one MAC will
*   not be see for the other MAC and hence the speed/SLCR settings of the other
*   MAC cannot be programmed. Hence this kind of design will not work for
*   this implementation.
*
********************************************************************************/

#include "netif/xemacpsif.h"
#include "lwipopts.h"
#include "xparameters_ps.h"
#include "xparameters.h"
#include "xemac_ieee_reg.h"

#if defined (__aarch64__)
#include "bspconfig.h"
#include "xil_smc.h"
#endif

#define PHY_BMCR				0x0000
#define PHY_DETECT_REG  						1
#define PHY_IDENTIFIER_1_REG					2
#define PHY_IDENTIFIER_2_REG					3
#define PHY_DETECT_MASK 					0x1808
#define PHY_MARVELL_IDENTIFIER				0x0141
#define PHY_TI_IDENTIFIER					0x2000
#define PHY_ADI_IDENTIFIER				0x0283
#define PHY_REALTEK_IDENTIFIER				0x001c
#define PHY_XILINX_PCS_PMA_ID1			0x0174
#define PHY_XILINX_PCS_PMA_ID2			0x0C00

#define XEMACPS_GMII2RGMII_SPEED1000_FD		0x140
#define XEMACPS_GMII2RGMII_SPEED100_FD		0x2100
#define XEMACPS_GMII2RGMII_SPEED10_FD		0x100
#define XEMACPS_GMII2RGMII_REG_NUM			0x10

#define PHY_REGCR		0x0D
#define PHY_ADDAR		0x0E
#define PHY_RGMIIDCTL	0x86
#define PHY_RGMIICTL	0x32
#define PHY_STS			0x11
#define PHY_TI_CR		0x10
#define PHY_TI_CTRL		0x1F
#define PHY_TI_CFG4		0x31

#define PHY_REGCR_ADDR	0x001F
#define PHY_REGCR_DATA	0x401F
#define PHY_TI_CRVAL	0x5048
#define PHY_TI_CFG4RESVDBIT7	0x80
#define PHY_TI_CFG4RESVDBIT8		0x100
#define PHY_TI_CFG4_AUTONEG_TIMER	0x60

#define PHY_TI_CTRL_SW_RESTART		0x4000
#define PHY_TI_BMCR_SW_RESET		0x8000

#define PHY_TI_PHYSTS_SPEED_SELECTION	0xC000
#define PHY_TI_PHYSTS_1000MBPS		0x8000
#define PHY_TI_PHYSTS_100MBPS		0x4000
#define SPEED_1000MBPS			1000
#define SPEED_100MBPS			100
#define SPEED_10MBPS			10

#define TI_PHY_CR_SGMII_EN		0x0800

/* TI DP83867 CFG2 (0x14) and port mirroring (Ethernet FMC Max) */
#define PHY_TI_CFGR2			0x14
#define PHY_TI_CFGR2_MASK		0x003F
#define PHY_TI_CFG2_SPEEDOPT_10EN	0x0040
#define PHY_TI_CFG2_SGMII_AUTONEGEN	0x0080
#define PHY_TI_CFG2_SPEEDOPT_ENH	0x0100
#define PHY_TI_CFG2_SPEEDOPT_CNT	0x0800
#define PHY_TI_CFG2_SPEEDOPT_INTLOW	0x2000
#define PHY_TI_PORT_MIRROR_EN		0x0001

/* Frequency setting */
#define SLCR_LOCK_ADDR			(XPS_SYS_CTRL_BASEADDR + 0x4)
#define SLCR_UNLOCK_ADDR		(XPS_SYS_CTRL_BASEADDR + 0x8)
#define SLCR_GEM0_CLK_CTRL_ADDR	(XPS_SYS_CTRL_BASEADDR + 0x140)
#define SLCR_GEM1_CLK_CTRL_ADDR	(XPS_SYS_CTRL_BASEADDR + 0x144)
#define SLCR_GEM_SRCSEL_EMIO	0x40
#define SLCR_LOCK_KEY_VALUE 	0x767B
#define SLCR_UNLOCK_KEY_VALUE	0xDF0D
#define SLCR_ADDR_GEM_RST_CTRL	(XPS_SYS_CTRL_BASEADDR + 0x214)
#define EMACPS_SLCR_DIV_MASK	0xFC0FC0FF

#define IEEE_CTRL_ISOLATE_DISABLE               0xFBFF

u32_t phymapemac0[32];
u32_t phymapemac1[32];

#if defined (CONFIG_LINKSPEED_AUTODETECT)
static u32_t get_IEEE_phy_speed(XEmacPs *xemacpsp, u32_t phy_addr);
#endif
static void SetUpSLCRDivisors(XEmacPs *xemacpsp __attribute__((unused)),
                              s32_t speed __attribute__((unused)));

#if defined (CONFIG_LINKSPEED1000) || defined (CONFIG_LINKSPEED100) \
	|| defined (CONFIG_LINKSPEED10)
static u32_t configure_IEEE_phy_speed(XEmacPs *xemacpsp, u32_t phy_addr, u32_t speed);
#endif

static void phy_identify(XEmacPs *xemacpsp, u32_t phy_addr, u32_t emacnum)
{
	u16_t phy_reg;
	u16_t phy_id;

	XEmacPs_PhyRead(xemacpsp, phy_addr, PHY_DETECT_REG,
			&phy_reg);
	XEmacPs_PhyRead(xemacpsp, phy_addr, PHY_IDENTIFIER_1_REG,
			&phy_id);

	if (((phy_reg != 0xFFFF) &&
	     ((phy_reg & PHY_DETECT_MASK) == PHY_DETECT_MASK)) ||
	    (phy_id == PHY_XILINX_PCS_PMA_ID1)) {
		/* Found a valid PHY address */
		LWIP_DEBUGF(NETIF_DEBUG, ("XEmacPs detect_phy: PHY detected at address %d.\r\n",
					  phy_addr));
		if (emacnum == 0) {
			phymapemac0[phy_addr] = TRUE;
		} else {
			phymapemac1[phy_addr] = TRUE;
		}

		XEmacPs_PhyRead(xemacpsp, phy_addr, PHY_IDENTIFIER_1_REG,
				&phy_reg);
		if ((phy_reg != PHY_MARVELL_IDENTIFIER) &&
		    (phy_reg != PHY_TI_IDENTIFIER) &&
		    (phy_reg != PHY_REALTEK_IDENTIFIER) &&
		    (phy_reg != PHY_ADI_IDENTIFIER)) {
			xil_printf("WARNING: Not a Marvell or TI or Realtek or Xilinx PCS PMA Ethernet PHY or ADI Ethernet PHY. Please verify the initialization sequence\r\n");
		}
	}
}

static u32_t get_Xilinx_pcs_pma_phy_speed(XEmacPs *xemacpsp, u32_t phy_addr)
{

#if XPAR_GIGE_PCS_PMA_1000BASEX_CORE_PRESENT == 1 || \
    XPAR_GIGE_PCS_PMA_SGMII_CORE_PRESENT == 1 || defined(SDT)
	u16_t temp;
#endif

	u16_t control;
	u16_t status;

	xil_printf("Start PHY autonegotiation \r\n");

	XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_CONTROL_REG_OFFSET, &control);
	control |= IEEE_CTRL_AUTONEGOTIATE_ENABLE;
	control |= IEEE_STAT_AUTONEGOTIATE_RESTART;
	control &= IEEE_CTRL_ISOLATE_DISABLE;
	XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_CONTROL_REG_OFFSET, control);

	xil_printf("Waiting for PHY to complete autonegotiation.\r\n");

	XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_STATUS_REG_OFFSET, &status);
	while ( !(status & IEEE_STAT_AUTONEGOTIATE_COMPLETE) ) {
		sleep(1);
		XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_STATUS_REG_OFFSET,
																&status);
	}
	xil_printf("autonegotiation complete \r\n");
#ifndef SDT
#if XPAR_GIGE_PCS_PMA_1000BASEX_CORE_PRESENT == 1
	XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_PAGE_ADDRESS_REGISTER, 1);
	XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_PARTNER_ABILITIES_1_REG_OFFSET, &temp);
	if ((temp & 0x0020) == 0x0020) {
		XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_PAGE_ADDRESS_REGISTER, 0);
		return 1000;
	}
	else {
		XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_PAGE_ADDRESS_REGISTER, 0);
		xil_printf("Link error, temp = %x\r\n", temp);
		return 0;
	}
#elif XPAR_GIGE_PCS_PMA_SGMII_CORE_PRESENT == 1
	xil_printf("Waiting for Link to be up; Polling for SGMII core Reg \r\n");
	XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_PARTNER_ABILITIES_1_REG_OFFSET, &temp);
	while(!(temp & 0x8000)) {
		XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_PARTNER_ABILITIES_1_REG_OFFSET, &temp);
	}
	if((temp & 0x0C00) == 0x0800) {
		return 1000;
	}
	else if((temp & 0x0C00) == 0x0400) {
		return 100;
	}
	else if((temp & 0x0C00) == 0x0000) {
		return 10;
	} else {
		xil_printf("get_IEEE_phy_speed(): Invalid speed bit value, Defaulting to Speed = 10 Mbps\r\n");
		XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_CONTROL_REG_OFFSET, &temp);
		XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_CONTROL_REG_OFFSET, 0x0100);
		return 10;
	}
#endif
#else
	if (strcmp(xemacpsp->Config.PhyType, "1000base-x") == 0) {
		XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_PAGE_ADDRESS_REGISTER, 1);
		XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_PARTNER_ABILITIES_1_REG_OFFSET, &temp);
		if ((temp & 0x0020) == 0x0020) {
			XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_PAGE_ADDRESS_REGISTER, 0);
			return 1000;
		}
		else {
			XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_PAGE_ADDRESS_REGISTER, 0);
			xil_printf("Link error, temp = %x\r\n", temp);
			return 0;
		}
	}
	/* Ethernet FMC Max (OP080): the PL PCS/PMA cores are in SGMII mode but
	 * the GEMs connect to them through EMIO GMII, so the hardware handoff
	 * reports the MAC-side interface (gmii) - treat anything that is not
	 * 1000base-x as SGMII. */
	if (strcmp(xemacpsp->Config.PhyType, "1000base-x") != 0) {
		xil_printf("Waiting for Link to be up; Polling for SGMII core Reg \r\n");
		XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_PARTNER_ABILITIES_1_REG_OFFSET, &temp);
		while(!(temp & 0x8000)) {
			XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_PARTNER_ABILITIES_1_REG_OFFSET, &temp);
		}
		if((temp & 0x0C00) == 0x0800) {
			return 1000;
		}
		else if((temp & 0x0C00) == 0x0400) {
			return 100;
		}
		else if((temp & 0x0C00) == 0x0000) {
			return 10;
		} else {
			xil_printf("get_IEEE_phy_speed(): Invalid speed bit value, Defaulting to Speed = 10 Mbps\r\n");
			XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_CONTROL_REG_OFFSET, &temp);
			XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_CONTROL_REG_OFFSET, 0x0100);
			return 10;
		}
	}
#endif
	return 0;
}

/*
 * Release all Ethernet FMC Max PHYs from reset on Versal.
 * PHY reset is active-low, directly driven by PMC GPIO EMIO bits 0-3
 * (one per port).  On Versal these are Bank 3 of the PMC GPIO controller.
 * This must be called once before any MDIO access.
 */
#if defined(versal) || defined(VERSAL_NET)
#include "xil_io.h"
#include "sleep.h"
#define PMC_GPIO_BASE        0xF1020000U
#define PMC_GPIO_EMIO_BANK   3U
static void release_phy_reset(void)
{
	static int done = 0;
	u32 dirm, oen;
	if (done) return;
	done = 1;

	/* EMIO bits 0-3 → PHY reset for ports 0-3 */
	const u32 mask = 0x0FU;
	const u32 bank_off = PMC_GPIO_EMIO_BANK * 0x40U;

	/* Set bits 0-3 as output */
	dirm = Xil_In32(PMC_GPIO_BASE + 0x204U + bank_off);
	Xil_Out32(PMC_GPIO_BASE + 0x204U + bank_off, dirm | mask);

	/* Enable output driver */
	oen = Xil_In32(PMC_GPIO_BASE + 0x208U + bank_off);
	Xil_Out32(PMC_GPIO_BASE + 0x208U + bank_off, oen | mask);

	/* Drive LOW (assert reset) using masked-write register:
	 * bits [31:16] = mask (0 = modify), bits [15:0] = data */
	Xil_Out32(PMC_GPIO_BASE + (PMC_GPIO_EMIO_BANK * 0x08U),
		  (~mask & 0xFFFFU) << 16 | 0U);
	xil_printf("PHY reset asserted (PMC GPIO EMIO bits 0-3 LOW)\r\n");
	usleep(10000);

	/* Drive HIGH (release reset) */
	Xil_Out32(PMC_GPIO_BASE + (PMC_GPIO_EMIO_BANK * 0x08U),
		  (~mask & 0xFFFFU) << 16 | mask);
	xil_printf("PHY reset released (PMC GPIO EMIO bits 0-3 HIGH)\r\n");
	usleep(10000);
}
#else
static void release_phy_reset(void) { }
#endif

/*
 * Raw access to the MDIO bus mastered by GEM0. In these designs a single
 * MDIO bus carries the Ethernet FMC Max PHYs (addresses 1,3,12,15) and
 * the management interfaces of the PL PCS/PMA cores (address 8+port).
 * Cores other than GEM0 have no MDIO bus of their own, so cross-GEM
 * access has to go through GEM0's registers directly.
 */
#include "xil_io.h"
#define MDIO0_GEM_BASE		XPAR_XEMACPS_0_BASEADDR
#define GEM_NWCTRL_OFFSET	0x000U
#define GEM_NWCFG_OFFSET	0x004U
#define GEM_NWSR_OFFSET		0x008U
#define GEM_PHYMNTNC_OFFSET	0x034U
#define GEM_NWCTRL_MDEN		(1U << 4)
#define GEM_NWSR_MDIOIDLE	(1U << 2)

static void mdio0_wait(void)
{
	while (!(Xil_In32(MDIO0_GEM_BASE + GEM_NWSR_OFFSET) & GEM_NWSR_MDIOIDLE));
}

static void mdio0_init(void)
{
	/* MDC = pclk/96, management port enable */
	Xil_Out32(MDIO0_GEM_BASE + GEM_NWCFG_OFFSET,
		  Xil_In32(MDIO0_GEM_BASE + GEM_NWCFG_OFFSET) | (5U << 18));
	Xil_Out32(MDIO0_GEM_BASE + GEM_NWCTRL_OFFSET,
		  Xil_In32(MDIO0_GEM_BASE + GEM_NWCTRL_OFFSET) | GEM_NWCTRL_MDEN);
}

static u16_t mdio0_read(u32_t phyad, u32_t regad)
{
	mdio0_wait();
	Xil_Out32(MDIO0_GEM_BASE + GEM_PHYMNTNC_OFFSET,
		  (1U << 30) | (2U << 28) | ((phyad & 0x1FU) << 23) |
		  ((regad & 0x1FU) << 18) | (2U << 16));
	mdio0_wait();
	return (u16_t)(Xil_In32(MDIO0_GEM_BASE + GEM_PHYMNTNC_OFFSET) & 0xFFFFU);
}

static void mdio0_write(u32_t phyad, u32_t regad, u16_t val)
{
	mdio0_wait();
	Xil_Out32(MDIO0_GEM_BASE + GEM_PHYMNTNC_OFFSET,
		  (1U << 30) | (1U << 28) | ((phyad & 0x1FU) << 23) |
		  ((regad & 0x1FU) << 18) | (2U << 16) | val);
	mdio0_wait();
}

/*
 * The PCS/PMA cores (PG047, "Ethernet MAC: GEM" mode) come out of reset
 * with the ISOLATE bit set (register 0, bit 10), which blocks the GMII
 * datapath both ways while leaving SGMII auto-negotiation operational.
 * Clear it (and restart the core's AN) before using the port.
 */
#define PCS_PMA_PHY_ADDR_BASE	8
#define IEEE_CTRL_ISOLATE	0x0400

static void pcs_pma_release_isolate(u32_t port)
{
	u32_t addr = PCS_PMA_PHY_ADDR_BASE + port;
	u16_t ctrl;

	mdio0_init();
	ctrl = mdio0_read(addr, IEEE_CONTROL_REG_OFFSET);
	if (ctrl == 0xFFFF) {
		xil_printf("WARNING: PCS/PMA core of port %u not found at MDIO address %u\r\n",
			   (unsigned)port, (unsigned)addr);
		return;
	}
	if (ctrl & IEEE_CTRL_ISOLATE) {
		ctrl &= (u16_t)~IEEE_CTRL_ISOLATE;
		ctrl |= IEEE_CTRL_AUTONEGOTIATE_ENABLE |
			IEEE_STAT_AUTONEGOTIATE_RESTART;
		mdio0_write(addr, IEEE_CONTROL_REG_OFFSET, ctrl);
		xil_printf("PCS/PMA core of port %u: isolate cleared\r\n",
			   (unsigned)port);
	}
}

/* Ethernet FMC Max (OP080): all four DP83867 PHYs share a single MDIO bus
 * (addresses 1,3,12,15) which is mastered by GEM0 in these designs. GEM0
 * drives port 0 of the FMC, whose PHY is at address 1. Restrict GEM0 to
 * that address, otherwise the generic scan would find all four PHYs and
 * phy_setup_emacps would block on auto-negotiation of the ports that have
 * no cable attached. GEM1-3 have no MDIO bus of their own; their PHYs and
 * PCS/PMA cores are reached through GEM0's bus (see mdio0_* above). */
#define ETH_FMC_MAX_PORT0_PHY_ADDR	1

void detect_phy(XEmacPs *xemacpsp)
{
	u32_t phy_addr = 0;
	u32_t emacnum;

	/* Release PHYs from reset (Versal only, runs once) */
	release_phy_reset();

	if (xemacpsp->Config.BaseAddress == XPAR_XEMACPS_0_BASEADDR)
		emacnum = 0;
	else
		emacnum = 1;

	if (emacnum == 0) {
		/* GEM0 masters the shared MDIO bus of the Ethernet FMC Max:
		 * use only the PHY of port 0 */
		phy_identify(xemacpsp, ETH_FMC_MAX_PORT0_PHY_ADDR, emacnum);
		return;
	}

#ifdef SDT
	phy_addr = xemacpsp->Config.PhyAddr;
#endif

	if (phy_addr != 0) {
		phy_identify(xemacpsp, phy_addr, emacnum);
	} else {
		for (phy_addr = 31; phy_addr > 0; phy_addr--) {
			phy_identify(xemacpsp, phy_addr, emacnum);
		}
	}
}

#ifdef SGMII_FIXED_LINK
u32_t pcs_setup_emacps (XEmacPs *xemacps)
{
	u32_t link_speed;

	SetUpSLCRDivisors(xemacps,1000);
	link_speed = 1000;
	sleep(1);
	return link_speed;
}
#endif

u32_t phy_setup_emacps (XEmacPs *xemacpsp, u32_t phy_addr)
{
	u32_t link_speed;
	u32_t conv_present = 0;
	u32_t convspeeddupsetting = 0;
	u32_t convphyaddr = 0;

#ifdef XPAR_GMII2RGMIICON_0N_ETH0_ADDR
	convphyaddr = XPAR_GMII2RGMIICON_0N_ETH0_ADDR;
	conv_present = 1;
#endif
#ifdef XPAR_GMII2RGMIICON_0N_ETH1_ADDR
	convphyaddr = XPAR_GMII2RGMIICON_0N_ETH1_ADDR;
	conv_present = 1;
#endif

	/* Clear the ISOLATE bit of this port's PCS/PMA core - the cores
	 * power up isolated and pass no data until it is cleared. The
	 * port number equals the GEM number in these designs. */
	pcs_pma_release_isolate(
		(xemacpsp->Config.BaseAddress - XPAR_XEMACPS_0_BASEADDR) >> 16);

#ifdef  CONFIG_LINKSPEED_AUTODETECT
	link_speed = get_IEEE_phy_speed(xemacpsp, phy_addr);
	if (link_speed == 1000) {
		SetUpSLCRDivisors(xemacpsp,1000);
		convspeeddupsetting = XEMACPS_GMII2RGMII_SPEED1000_FD;
	} else if (link_speed == 100) {
		SetUpSLCRDivisors(xemacpsp,100);
		convspeeddupsetting = XEMACPS_GMII2RGMII_SPEED100_FD;
	} else if (link_speed != XST_FAILURE){
		SetUpSLCRDivisors(xemacpsp,10);
		convspeeddupsetting = XEMACPS_GMII2RGMII_SPEED10_FD;
	} else {
		xil_printf("Phy setup error : link_speed invalid\r\n");
		return XST_FAILURE;
	}
#elif	defined(CONFIG_LINKSPEED1000)
	SetUpSLCRDivisors(xemacpsp,1000);
	link_speed = 1000;
	configure_IEEE_phy_speed(xemacpsp, phy_addr, link_speed);
	convspeeddupsetting = XEMACPS_GMII2RGMII_SPEED1000_FD;
	sleep(1);
#elif	defined(CONFIG_LINKSPEED100)
	SetUpSLCRDivisors(xemacpsp,100);
	link_speed = 100;
	configure_IEEE_phy_speed(xemacpsp, phy_addr, link_speed);
	convspeeddupsetting = XEMACPS_GMII2RGMII_SPEED100_FD;
	sleep(1);
#elif	defined(CONFIG_LINKSPEED10)
	SetUpSLCRDivisors(xemacpsp,10);
	link_speed = 10;
	configure_IEEE_phy_speed(xemacpsp, phy_addr, link_speed);
	convspeeddupsetting = XEMACPS_GMII2RGMII_SPEED10_FD;
	sleep(1);
#endif
	if (conv_present) {
		XEmacPs_PhyWrite(xemacpsp, convphyaddr,
		XEMACPS_GMII2RGMII_REG_NUM, convspeeddupsetting);
	}

#ifdef SDT
	if(xemacpsp->Config.GmiitoRgmiiConvPhyAddr != 0)
	{
		XEmacPs_PhyWrite(xemacpsp, xemacpsp->Config.GmiitoRgmiiConvPhyAddr,
		XEMACPS_GMII2RGMII_REG_NUM, convspeeddupsetting);
	}
#endif

	xil_printf("link speed for phy address %d: %d\r\n", phy_addr, link_speed);
	return link_speed;
}

#if defined CONFIG_LINKSPEED_AUTODETECT
static u32_t get_TI_phy_speed(XEmacPs *xemacpsp, u32_t phy_addr)
{
	u16_t control;
	u16_t status;
	u16_t status_speed;
	u32_t timeout_counter = 0;
	u32_t phyregtemp;
	u32_t RetStatus;

	xil_printf("Start PHY autonegotiation \r\n");

	XEmacPs_PhyRead(xemacpsp, phy_addr, 0x1F, (u16_t *)&phyregtemp);
	phyregtemp |= 0x4000;
	XEmacPs_PhyWrite(xemacpsp, phy_addr, 0x1F, phyregtemp);
	RetStatus = XEmacPs_PhyRead(xemacpsp, phy_addr, 0x1F, (u16_t *)&phyregtemp);
	if (RetStatus != XST_SUCCESS) {
		xil_printf("Error during sw reset \n\r");
		return XST_FAILURE;
	}

	XEmacPs_PhyRead(xemacpsp, phy_addr, 0, (u16_t *)&phyregtemp);
	phyregtemp |= 0x8000;
	XEmacPs_PhyWrite(xemacpsp, phy_addr, 0, phyregtemp);

	/*
	 * Delay
	 */
	sleep(1);

	RetStatus = XEmacPs_PhyRead(xemacpsp, phy_addr, 0, (u16_t *)&phyregtemp);
	if (RetStatus != XST_SUCCESS) {
		xil_printf("Error during reset \n\r");
		return XST_FAILURE;
	}

	/* FIFO depth */
	XEmacPs_PhyWrite(xemacpsp, phy_addr, PHY_TI_CR, PHY_TI_CRVAL);
	RetStatus = XEmacPs_PhyRead(xemacpsp, phy_addr, PHY_TI_CR, (u16_t *)&phyregtemp);
	if (RetStatus != XST_SUCCESS) {
		LWIP_DEBUGF(NETIF_DEBUG, ("Error writing to 0x10 \n\r"));
		return XST_FAILURE;
	}

	/* TX/RX tuning */
	/* Write to PHY_RGMIIDCTL */
	XEmacPs_PhyWrite(xemacpsp, phy_addr, PHY_REGCR, PHY_REGCR_ADDR);
	XEmacPs_PhyWrite(xemacpsp, phy_addr, PHY_ADDAR, PHY_RGMIIDCTL);
	XEmacPs_PhyWrite(xemacpsp, phy_addr, PHY_REGCR, PHY_REGCR_DATA);
	RetStatus = XEmacPs_PhyWrite(xemacpsp, phy_addr, PHY_ADDAR, 0xA8);
	if (RetStatus != XST_SUCCESS) {
		LWIP_DEBUGF(NETIF_DEBUG, ("Error in tuning"));
		return XST_FAILURE;
	}

	/* Read PHY_RGMIIDCTL */
	XEmacPs_PhyWrite(xemacpsp, phy_addr, PHY_REGCR, PHY_REGCR_ADDR);
	XEmacPs_PhyWrite(xemacpsp, phy_addr, PHY_ADDAR, PHY_RGMIIDCTL);
	XEmacPs_PhyWrite(xemacpsp, phy_addr, PHY_REGCR, PHY_REGCR_DATA);
	RetStatus = XEmacPs_PhyRead(xemacpsp, phy_addr, PHY_ADDAR, (u16_t *)&phyregtemp);
	if (RetStatus != XST_SUCCESS) {
		LWIP_DEBUGF(NETIF_DEBUG, ("Error in tuning"));
		return XST_FAILURE;
	}

	/* Write PHY_RGMIICTL */
	XEmacPs_PhyWrite(xemacpsp, phy_addr, PHY_REGCR, PHY_REGCR_ADDR);
	XEmacPs_PhyWrite(xemacpsp, phy_addr, PHY_ADDAR, PHY_RGMIICTL);
	XEmacPs_PhyWrite(xemacpsp, phy_addr, PHY_REGCR, PHY_REGCR_DATA);
	RetStatus = XEmacPs_PhyWrite(xemacpsp, phy_addr, PHY_ADDAR, 0xD3);
	if (RetStatus != XST_SUCCESS) {
		LWIP_DEBUGF(NETIF_DEBUG, ("Error in tuning"));
		return XST_FAILURE;
	}

	/* Read PHY_RGMIICTL */
	XEmacPs_PhyWrite(xemacpsp, phy_addr, PHY_REGCR, PHY_REGCR_ADDR);
	XEmacPs_PhyWrite(xemacpsp, phy_addr, PHY_ADDAR, PHY_RGMIICTL);
	XEmacPs_PhyWrite(xemacpsp, phy_addr, PHY_REGCR, PHY_REGCR_DATA);
	RetStatus = XEmacPs_PhyRead(xemacpsp, phy_addr, PHY_ADDAR, (u16_t *)&phyregtemp);
	if (RetStatus != XST_SUCCESS) {
		LWIP_DEBUGF(NETIF_DEBUG, ("Error in tuning"));
		return XST_FAILURE;
	}

	/* SW workaround for unstable link when RX_CTRL is not STRAP MODE 3 or 4 */
	XEmacPs_PhyWrite(xemacpsp, phy_addr, PHY_REGCR, PHY_REGCR_ADDR);
	XEmacPs_PhyWrite(xemacpsp, phy_addr, PHY_ADDAR, PHY_TI_CFG4);
	XEmacPs_PhyWrite(xemacpsp, phy_addr, PHY_REGCR, PHY_REGCR_DATA);
	RetStatus = XEmacPs_PhyRead(xemacpsp, phy_addr, PHY_ADDAR, (u16_t *)&phyregtemp);
	phyregtemp &= ~(PHY_TI_CFG4RESVDBIT7);
	XEmacPs_PhyWrite(xemacpsp, phy_addr, PHY_REGCR, PHY_REGCR_ADDR);
	XEmacPs_PhyWrite(xemacpsp, phy_addr, PHY_ADDAR, PHY_TI_CFG4);
	XEmacPs_PhyWrite(xemacpsp, phy_addr, PHY_REGCR, PHY_REGCR_DATA);
	RetStatus = XEmacPs_PhyWrite(xemacpsp, phy_addr, PHY_ADDAR, phyregtemp);

	XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_AUTONEGO_ADVERTISE_REG, &control);
	control |= IEEE_ASYMMETRIC_PAUSE_MASK;
	control |= IEEE_PAUSE_MASK;
	control |= ADVERTISE_100;
	control |= ADVERTISE_10;
	XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_AUTONEGO_ADVERTISE_REG, control);

	XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_1000_ADVERTISE_REG_OFFSET,
					&control);
	control |= ADVERTISE_1000;
	XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_1000_ADVERTISE_REG_OFFSET,
					control);

	XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_CONTROL_REG_OFFSET, &control);
	control |= IEEE_CTRL_AUTONEGOTIATE_ENABLE;
	control |= IEEE_STAT_AUTONEGOTIATE_RESTART;
	XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_CONTROL_REG_OFFSET, control);

	XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_CONTROL_REG_OFFSET, &control);
	XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_STATUS_REG_OFFSET, &status);

	xil_printf("Waiting for PHY to complete autonegotiation.\r\n");

	while ( !(status & IEEE_STAT_AUTONEGOTIATE_COMPLETE) ) {
		sleep(1);
		timeout_counter++;

		if (timeout_counter == 5) {
			xil_printf("Auto negotiation error \r\n");
			return XST_FAILURE;
		}
		XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_STATUS_REG_OFFSET, &status);
	}
	xil_printf("autonegotiation complete \r\n");

	XEmacPs_PhyRead(xemacpsp, phy_addr, PHY_STS, &status_speed);
	if ((status_speed & 0xC000) == 0x8000) {
		return 1000;
	} else if ((status_speed & 0xC000) == 0x4000) {
		return 100;
	} else {
		return 10;
	}

	return XST_SUCCESS;
}
/* Configuration sequence ported from the hardware-proven
 * get_phy_speed_TI_DP83867_SGMII() in ethernet-fmc-max-axi-eth. The
 * Ethernet FMC Max PHYs have their MDI channels mirrored on the PCB, so
 * port mirroring (lane swap) must be enabled in CFG4 — the PetaLinux
 * device trees do the same via the enet-phy-lane-swap property. */
static u32_t get_TI_phy_speed_sgmii(XEmacPs *xemacpsp, u32_t phy_addr)
{
	u16_t control;
	u16_t status;
	u16_t status_speed;
	u32_t timeout_counter = 0;
	u32_t phyregtemp;

	xil_printf("Start TI PHY autonegotiation in SGMII Mode \r\n");

	/* Enable port mirror mode (Ethernet FMC Max) */
	XEmacPs_PhyWrite(xemacpsp, phy_addr, PHY_REGCR, PHY_REGCR_ADDR);
	XEmacPs_PhyWrite(xemacpsp, phy_addr, PHY_ADDAR, PHY_TI_CFG4);
	XEmacPs_PhyWrite(xemacpsp, phy_addr, PHY_REGCR, PHY_REGCR_DATA);
	XEmacPs_PhyRead(xemacpsp, phy_addr, PHY_ADDAR, (u16_t *)&phyregtemp);
	phyregtemp |= PHY_TI_PORT_MIRROR_EN;
	XEmacPs_PhyWrite(xemacpsp, phy_addr, PHY_REGCR, PHY_REGCR_ADDR);
	XEmacPs_PhyWrite(xemacpsp, phy_addr, PHY_ADDAR, PHY_TI_CFG4);
	XEmacPs_PhyWrite(xemacpsp, phy_addr, PHY_REGCR, PHY_REGCR_DATA);
	XEmacPs_PhyWrite(xemacpsp, phy_addr, PHY_ADDAR, phyregtemp);

	/* Auto-negotiation enable, 1000M, full duplex */
	XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_CONTROL_REG_OFFSET, &control);
	control |= (IEEE_CTRL_AUTONEGOTIATE_ENABLE | IEEE_CTRL_LINKSPEED_1000M |
		    IEEE_CTRL_FULL_DUPLEX);
	XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_CONTROL_REG_OFFSET, control);

	/* CFG2: SGMII auto-negotiation and speed optimization */
	XEmacPs_PhyRead(xemacpsp, phy_addr, PHY_TI_CFGR2, &control);
	control &= PHY_TI_CFGR2_MASK;
	control |= (PHY_TI_CFG2_SPEEDOPT_10EN   |
		    PHY_TI_CFG2_SGMII_AUTONEGEN |
		    PHY_TI_CFG2_SPEEDOPT_ENH    |
		    PHY_TI_CFG2_SPEEDOPT_CNT    |
		    PHY_TI_CFG2_SPEEDOPT_INTLOW);
	XEmacPs_PhyWrite(xemacpsp, phy_addr, PHY_TI_CFGR2, control);

	/* Disable RGMII */
	XEmacPs_PhyWrite(xemacpsp, phy_addr, PHY_REGCR, PHY_REGCR_ADDR);
	XEmacPs_PhyWrite(xemacpsp, phy_addr, PHY_ADDAR, PHY_RGMIICTL);
	XEmacPs_PhyWrite(xemacpsp, phy_addr, PHY_REGCR, PHY_REGCR_DATA);
	XEmacPs_PhyWrite(xemacpsp, phy_addr, PHY_ADDAR, 0);

	/* Set SGMII mode in PHY Control register */
	XEmacPs_PhyWrite(xemacpsp, phy_addr, PHY_TI_CR, TI_PHY_CR_SGMII_EN);

	/* Restart auto-negotiation so the new configuration (port mirroring
	 * in particular) takes effect, then wait for a FRESH completion -
	 * the status bits are latched and a stale read passes immediately. */
	XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_CONTROL_REG_OFFSET, &control);
	control |= IEEE_CTRL_AUTONEGOTIATE_ENABLE |
		   IEEE_STAT_AUTONEGOTIATE_RESTART;
	XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_CONTROL_REG_OFFSET, control);

	xil_printf("Waiting for PHY to complete autonegotiation.\r\n");

	XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_STATUS_REG_OFFSET, &status);
	while ( !(status & IEEE_STAT_AUTONEGOTIATE_COMPLETE) ) {
		sleep(1);
		timeout_counter++;

		if (timeout_counter == 15) {
			xil_printf("Auto negotiation error \r\n");
			return XST_FAILURE;
		}
		XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_STATUS_REG_OFFSET,
				&status);
	}
	xil_printf("autonegotiation complete \r\n");

	/* SW workaround for unstable link when RX_CTRL is not STRAP MODE 3 or 4 */
	XEmacPs_PhyWrite(xemacpsp, phy_addr, PHY_REGCR, PHY_REGCR_ADDR);
	XEmacPs_PhyWrite(xemacpsp, phy_addr, PHY_ADDAR, PHY_TI_CFG4);
	XEmacPs_PhyWrite(xemacpsp, phy_addr, PHY_REGCR, PHY_REGCR_DATA);
	XEmacPs_PhyRead(xemacpsp, phy_addr, PHY_ADDAR, (u16_t *)&phyregtemp);
	phyregtemp &= ~(PHY_TI_CFG4RESVDBIT7);
	phyregtemp |= PHY_TI_CFG4RESVDBIT8;
	phyregtemp &= ~(PHY_TI_CFG4_AUTONEG_TIMER);
	phyregtemp |= PHY_TI_CFG4_AUTONEG_TIMER;
	XEmacPs_PhyWrite(xemacpsp, phy_addr, PHY_REGCR, PHY_REGCR_ADDR);
	XEmacPs_PhyWrite(xemacpsp, phy_addr, PHY_ADDAR, PHY_TI_CFG4);
	XEmacPs_PhyWrite(xemacpsp, phy_addr, PHY_REGCR, PHY_REGCR_DATA);
	XEmacPs_PhyWrite(xemacpsp, phy_addr, PHY_ADDAR, phyregtemp);

	XEmacPs_PhyRead(xemacpsp, phy_addr, PHY_STS, &status_speed);
	if ((status_speed & PHY_TI_PHYSTS_SPEED_SELECTION) == PHY_TI_PHYSTS_1000MBPS) {
		return SPEED_1000MBPS;
	} else if ((status_speed & PHY_TI_PHYSTS_SPEED_SELECTION) == PHY_TI_PHYSTS_100MBPS) {
		return SPEED_100MBPS;
	} else {
		return SPEED_10MBPS;
	}

	return XST_SUCCESS;
}


static u32_t get_Marvell_phy_speed(XEmacPs *xemacpsp, u32_t phy_addr)
{
	u16_t temp;
	u16_t control;
	u16_t status;
	u16_t status_speed;
	u32_t timeout_counter = 0;
	u32_t temp_speed;

	xil_printf("Start PHY autonegotiation \r\n");

	XEmacPs_PhyWrite(xemacpsp,phy_addr, IEEE_PAGE_ADDRESS_REGISTER, 2);
	XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_CONTROL_REG_MAC, &control);
	control |= IEEE_RGMII_TXRX_CLOCK_DELAYED_MASK;
	XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_CONTROL_REG_MAC, control);

	XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_PAGE_ADDRESS_REGISTER, 0);

	XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_AUTONEGO_ADVERTISE_REG, &control);
	control |= IEEE_ASYMMETRIC_PAUSE_MASK;
	control |= IEEE_PAUSE_MASK;
	control |= ADVERTISE_100;
	control |= ADVERTISE_10;
	XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_AUTONEGO_ADVERTISE_REG, control);

	XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_1000_ADVERTISE_REG_OFFSET,
					&control);
	control |= ADVERTISE_1000;
	XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_1000_ADVERTISE_REG_OFFSET,
					control);

	XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_PAGE_ADDRESS_REGISTER, 0);
	XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_COPPER_SPECIFIC_CONTROL_REG,
																&control);
	control |= (7 << 12);	/* max number of gigabit attempts */
	control |= (1 << 11);	/* enable downshift */
	XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_COPPER_SPECIFIC_CONTROL_REG,
																control);
	XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_CONTROL_REG_OFFSET, &control);
	control |= IEEE_CTRL_AUTONEGOTIATE_ENABLE;
	control |= IEEE_STAT_AUTONEGOTIATE_RESTART;
	XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_CONTROL_REG_OFFSET, control);

	XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_CONTROL_REG_OFFSET, &control);
	control |= IEEE_CTRL_RESET_MASK;
	XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_CONTROL_REG_OFFSET, control);

	while (1) {
		XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_CONTROL_REG_OFFSET, &control);
		if (control & IEEE_CTRL_RESET_MASK)
			continue;
		else
			break;
	}

	XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_STATUS_REG_OFFSET, &status);

	xil_printf("Waiting for PHY to complete autonegotiation.\r\n");

	while ( !(status & IEEE_STAT_AUTONEGOTIATE_COMPLETE) ) {
		sleep(1);
		XEmacPs_PhyRead(xemacpsp, phy_addr,
						IEEE_COPPER_SPECIFIC_STATUS_REG_2,  &temp);
		timeout_counter++;

		if (timeout_counter == 5) {
			xil_printf("Auto negotiation error \r\n");
			return XST_FAILURE;
		}
		XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_STATUS_REG_OFFSET, &status);
	}
	xil_printf("autonegotiation complete \r\n");

	XEmacPs_PhyRead(xemacpsp, phy_addr,IEEE_SPECIFIC_STATUS_REG,
					&status_speed);
	if (status_speed & 0x400) {
		temp_speed = status_speed & IEEE_SPEED_MASK;

		if (temp_speed == IEEE_SPEED_1000)
			return 1000;
		else if(temp_speed == IEEE_SPEED_100)
			return 100;
		else
			return 10;
	}

	return XST_SUCCESS;
}

static u32_t get_Realtek_phy_speed(XEmacPs *xemacpsp, u32_t phy_addr)
{
	u16_t control;
	u16_t status;
	u16_t status_speed;
	u32_t timeout_counter = 0;
	u32_t temp_speed;

	xil_printf("Start PHY autonegotiation \r\n");

	XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_AUTONEGO_ADVERTISE_REG, &control);
	control |= IEEE_ASYMMETRIC_PAUSE_MASK;
	control |= IEEE_PAUSE_MASK;
	control |= ADVERTISE_100;
	control |= ADVERTISE_10;
	XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_AUTONEGO_ADVERTISE_REG, control);

	XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_1000_ADVERTISE_REG_OFFSET,
					&control);
	control |= ADVERTISE_1000;
	XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_1000_ADVERTISE_REG_OFFSET,
					control);

	XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_CONTROL_REG_OFFSET, &control);
	control |= IEEE_CTRL_AUTONEGOTIATE_ENABLE;
	control |= IEEE_STAT_AUTONEGOTIATE_RESTART;
	XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_CONTROL_REG_OFFSET, control);

	XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_CONTROL_REG_OFFSET, &control);
	control |= IEEE_CTRL_RESET_MASK;
	XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_CONTROL_REG_OFFSET, control);

	while (1) {
		XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_CONTROL_REG_OFFSET, &control);
		if (control & IEEE_CTRL_RESET_MASK)
			continue;
		else
			break;
	}

	XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_STATUS_REG_OFFSET, &status);

	xil_printf("Waiting for PHY to complete autonegotiation.\r\n");

	while ( !(status & IEEE_STAT_AUTONEGOTIATE_COMPLETE) ) {
		sleep(1);
		timeout_counter++;

		if (timeout_counter == 5) {
			xil_printf("Auto negotiation error \r\n");
			return XST_FAILURE;
		}
		XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_STATUS_REG_OFFSET, &status);
	}
	xil_printf("autonegotiation complete \r\n");

	XEmacPs_PhyRead(xemacpsp, phy_addr,IEEE_SPECIFIC_STATUS_REG,
					&status_speed);
	if (status_speed & 0x400) {
		temp_speed = status_speed & IEEE_SPEED_MASK;

		if (temp_speed == IEEE_SPEED_1000)
			return 1000;
		else if(temp_speed == IEEE_SPEED_100)
			return 100;
		else
			return 10;
	}

	return XST_FAILURE;
}

#define ADIN1300_PHY_CTRL1	0x0012
#define ADIN1300_PHY_CTRL2	0x0016
#define ADIN1300_PHY_CTRL3	0x0017
#define ADIN1300_EXT_ADDR	0x0010
#define ADIN1300_EXT_DATA	0x0011
#define ADIN1300_PHY_STS1	0x001A

#define ADIN1300_RGMII_CFG	0xFF23
#define ADIN1300_RMII_CFG	0xFF24

#define ADIN1300_AUTO_MDI_EN	0x400
#define ADIN1300_MAN_MDIX_EN	0x200
#define ADIN1300_DIAG_CLK_EN	0x4

#define ADIN1300_LINKING_EN	0x2000

#define ADIN1300_RGMII_EN		0x0001
#define ADIN1300_RGMII_TXRX_TUNING_EN	0x0006
#define ADIN1300_RGMII_RX_DELAY_MASK	0x01C0
#define ADIN1300_RGMII_TX_DELAY_MASK	0x0038
#define ADIN1300_RGMII_RX_DELAY_VAL_2000PS	0x0
#define ADIN1300_RGMII_TX_DELAY_VAL_2000PS	0x0

#define ADIN1300_SPEED_RETRY_MASK	0x1C00
#define ADIN1300_SPEED_RETRY_FOUR	0x1000
#define ADIN1300_DOWNSPEED_EN		0x0C00

#define ADIN1300_AN_DONE	0x1000
#define ADIN1300_SPEED_MASK	0x0380
#define ADIN1300_SPEED_1G	0x0280
#define ADIN1300_SPEED_100M	0x0180
#define ADIN1300_SPEED_10M	0x0080

static u32_t get_Adi_phy_speed(XEmacPs *xemacpsp, u32_t phy_addr)
{
	u16_t temp;
	u16_t control;
	u16_t status;
	u16_t status_speed;
	u16_t phyreg;
	u32_t timeout_counter = 0;
	u32_t temp_speed;

	xil_printf("Start PHY autonegotiation \r\n");

	/* PHY soft reset */
	XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_CONTROL_REG_OFFSET, &control);
	control |= IEEE_CTRL_RESET_MASK;
	XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_CONTROL_REG_OFFSET, control);

	/* Delay for PHY to be accessible */
	sleep(1);

	/* RGMII TX/RX tuning */
	XEmacPs_PhyWrite(xemacpsp, phy_addr, ADIN1300_EXT_ADDR, ADIN1300_RGMII_CFG);
	XEmacPs_PhyRead(xemacpsp, phy_addr, ADIN1300_EXT_DATA, &phyreg);
	phyreg |= (ADIN1300_RGMII_EN | ADIN1300_RGMII_TXRX_TUNING_EN);
	phyreg &= ~(ADIN1300_RGMII_RX_DELAY_MASK | ADIN1300_RGMII_TX_DELAY_MASK);
	phyreg |= ((ADIN1300_RGMII_RX_DELAY_VAL_2000PS << 6) | (ADIN1300_RGMII_TX_DELAY_VAL_2000PS << 3));
	XEmacPs_PhyWrite(xemacpsp, phy_addr, ADIN1300_EXT_ADDR, ADIN1300_RGMII_CFG);
	XEmacPs_PhyWrite(xemacpsp, phy_addr, ADIN1300_EXT_DATA, phyreg);

	/* Downspeed */
	XEmacPs_PhyRead(xemacpsp, phy_addr, ADIN1300_PHY_CTRL3, &phyreg);
	phyreg &= ~(ADIN1300_SPEED_RETRY_MASK);
	phyreg |= ADIN1300_SPEED_RETRY_FOUR;
	XEmacPs_PhyWrite(xemacpsp, phy_addr, ADIN1300_PHY_CTRL3, phyreg);
	XEmacPs_PhyRead(xemacpsp, phy_addr, ADIN1300_PHY_CTRL2, &phyreg);
	phyreg |= ADIN1300_DOWNSPEED_EN;
	XEmacPs_PhyWrite(xemacpsp, phy_addr, ADIN1300_PHY_CTRL2, phyreg);

	/* Diag clock disable */
	XEmacPs_PhyRead(xemacpsp, phy_addr, ADIN1300_PHY_CTRL1, &phyreg);
	phyreg &= ~ADIN1300_DIAG_CLK_EN;
	XEmacPs_PhyWrite(xemacpsp, phy_addr, ADIN1300_PHY_CTRL1, phyreg);
	/* Linking Enable */
	XEmacPs_PhyRead(xemacpsp, phy_addr, ADIN1300_PHY_CTRL3, &phyreg);
	phyreg |= ADIN1300_LINKING_EN;
	XEmacPs_PhyWrite(xemacpsp, phy_addr, ADIN1300_PHY_CTRL3, phyreg);
	/* Auto MDIX by default */
	XEmacPs_PhyRead(xemacpsp, phy_addr, ADIN1300_PHY_CTRL1, &phyreg);
	phyreg &= ~ADIN1300_MAN_MDIX_EN;
	phyreg |= ADIN1300_AUTO_MDI_EN;
	XEmacPs_PhyWrite(xemacpsp, phy_addr, ADIN1300_PHY_CTRL1, phyreg);

	XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_AUTONEGO_ADVERTISE_REG, &control);
	control |= IEEE_ASYMMETRIC_PAUSE_MASK;
	control |= IEEE_PAUSE_MASK;
	control |= ADVERTISE_100;
	control |= ADVERTISE_10;
	XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_AUTONEGO_ADVERTISE_REG, control);

	XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_1000_ADVERTISE_REG_OFFSET,
					&control);
	control |= ADVERTISE_1000;
	XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_1000_ADVERTISE_REG_OFFSET,
					control);

	XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_CONTROL_REG_OFFSET, &control);
	control |= IEEE_CTRL_AUTONEGOTIATE_ENABLE;
	control |= IEEE_STAT_AUTONEGOTIATE_RESTART;
	XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_CONTROL_REG_OFFSET, control);

	while (1) {
		XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_CONTROL_REG_OFFSET, &control);
		if (control & IEEE_CTRL_RESET_MASK)
			continue;
		else
			break;
	}

	XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_STATUS_REG_OFFSET, &status);

	xil_printf("Waiting for PHY to complete autonegotiation.\r\n");

	while ( !(status & IEEE_STAT_AUTONEGOTIATE_COMPLETE) ) {
		sleep(1);
		XEmacPs_PhyRead(xemacpsp, phy_addr,
						IEEE_COPPER_SPECIFIC_STATUS_REG_2,  &temp);
		timeout_counter++;

		if (timeout_counter == 30) {
			xil_printf("Auto negotiation error \r\n");
			return XST_FAILURE;
		}
		XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_STATUS_REG_OFFSET, &status);
	}
	xil_printf("autonegotiation complete \r\n");

	XEmacPs_PhyRead(xemacpsp, phy_addr,ADIN1300_PHY_STS1,
					&status_speed);
	if (status_speed & ADIN1300_AN_DONE) {
		temp_speed = status_speed & ADIN1300_SPEED_MASK;

		if (temp_speed == ADIN1300_SPEED_1G)
			return 1000;
		else if(temp_speed == ADIN1300_SPEED_100M)
			return 100;
		else
			return 10;
	}

	return XST_SUCCESS;
}

static u32_t get_IEEE_phy_speed(XEmacPs *xemacpsp, u32_t phy_addr)
{
	u16_t phy_identity;
	u32_t RetStatus;
	char *PhyType;

#ifdef SDT
	PhyType = xemacpsp->Config.PhyType;
#endif

	XEmacPs_PhyRead(xemacpsp, phy_addr, PHY_IDENTIFIER_1_REG,
					&phy_identity);
	if (phy_identity == PHY_TI_IDENTIFIER) {
		/* Ethernet FMC Max (OP080): the DP83867 PHYs are always in SGMII
		 * mode in these designs. The GEMs connect to them through EMIO
		 * GMII and a PL PCS/PMA core, so the hardware handoff reports
		 * the MAC-side interface (gmii) - always take the SGMII path. */
		RetStatus = get_TI_phy_speed_sgmii(xemacpsp, phy_addr);
		(void)PhyType;
	} else if (phy_identity == PHY_REALTEK_IDENTIFIER) {
		RetStatus = get_Realtek_phy_speed(xemacpsp, phy_addr);
	} else if (phy_identity == PHY_XILINX_PCS_PMA_ID1) {
		RetStatus = get_Xilinx_pcs_pma_phy_speed(xemacpsp, phy_addr);
	} else if (phy_identity == PHY_ADI_IDENTIFIER) {
		RetStatus = get_Adi_phy_speed(xemacpsp, phy_addr);
	} else {
		RetStatus = get_Marvell_phy_speed(xemacpsp, phy_addr);
	}

	return RetStatus;
}
#endif

#if defined (CONFIG_LINKSPEED1000) || defined (CONFIG_LINKSPEED100) \
	|| defined (CONFIG_LINKSPEED10)
static u32_t configure_IEEE_phy_speed(XEmacPs *xemacpsp, u32_t phy_addr, u32_t speed)
{
	u16_t control;
	u16_t autonereg;

	XEmacPs_PhyWrite(xemacpsp,phy_addr, IEEE_PAGE_ADDRESS_REGISTER, 2);
	XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_CONTROL_REG_MAC, &control);
	control |= IEEE_RGMII_TXRX_CLOCK_DELAYED_MASK;
	XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_CONTROL_REG_MAC, control);

	XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_PAGE_ADDRESS_REGISTER, 0);

	XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_AUTONEGO_ADVERTISE_REG, &autonereg);
	autonereg |= IEEE_ASYMMETRIC_PAUSE_MASK;
	autonereg |= IEEE_PAUSE_MASK;
	XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_AUTONEGO_ADVERTISE_REG, autonereg);

	XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_CONTROL_REG_OFFSET, &control);
	control &= ~IEEE_CTRL_LINKSPEED_1000M;
	control &= ~IEEE_CTRL_LINKSPEED_100M;
	control &= ~IEEE_CTRL_LINKSPEED_10M;

	if (speed == 1000) {
		control |= IEEE_CTRL_LINKSPEED_1000M;

		/* Don't advertise PHY speed of 100 Mbps */
		XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_AUTONEGO_ADVERTISE_REG, &autonereg);
		autonereg &= (~ADVERTISE_100);
		XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_AUTONEGO_ADVERTISE_REG, autonereg);

		/* Don't advertise PHY speed of 10 Mbps */
		XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_AUTONEGO_ADVERTISE_REG, &autonereg);
		autonereg &= (~ADVERTISE_10);
		XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_AUTONEGO_ADVERTISE_REG, autonereg);

		/* Advertise PHY speed of 1000 Mbps */
		XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_1000_ADVERTISE_REG_OFFSET, &autonereg);
		autonereg |= ADVERTISE_1000;
		XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_1000_ADVERTISE_REG_OFFSET, autonereg);
	}

	else if (speed == 100) {
		control |= IEEE_CTRL_LINKSPEED_100M;

		/* Don't advertise PHY speed of 1000 Mbps */
		XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_1000_ADVERTISE_REG_OFFSET, &autonereg);
		autonereg &= (~ADVERTISE_1000);
		XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_1000_ADVERTISE_REG_OFFSET, autonereg);

		/* Don't advertise PHY speed of 10 Mbps */
		XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_AUTONEGO_ADVERTISE_REG, &autonereg);
		autonereg &= (~ADVERTISE_10);
		XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_AUTONEGO_ADVERTISE_REG, autonereg);

		/* Advertise PHY speed of 100 Mbps */
		XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_AUTONEGO_ADVERTISE_REG, &autonereg);
		autonereg |= ADVERTISE_100;
		XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_AUTONEGO_ADVERTISE_REG, autonereg);
	}

	else if (speed == 10) {
		control |= IEEE_CTRL_LINKSPEED_10M;

		/* Don't advertise PHY speed of 1000 Mbps */
		XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_1000_ADVERTISE_REG_OFFSET, &autonereg);
		autonereg &= (~ADVERTISE_1000);
		XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_1000_ADVERTISE_REG_OFFSET, autonereg);

		/* Don't advertise PHY speed of 100 Mbps */
		XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_AUTONEGO_ADVERTISE_REG, &autonereg);
		autonereg &= (~ADVERTISE_100);
		XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_AUTONEGO_ADVERTISE_REG, autonereg);

		/* Advertise PHY speed of 10 Mbps */
		XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_AUTONEGO_ADVERTISE_REG, &autonereg);
		autonereg |= ADVERTISE_10;
		XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_AUTONEGO_ADVERTISE_REG, autonereg);
	}

	XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_CONTROL_REG_OFFSET,
											control | IEEE_CTRL_RESET_MASK);
	{
		volatile s32_t wait;
		for (wait=0; wait < 100000; wait++);
	}
	return 0;
}
#endif

static void SetUpSLCRDivisors(XEmacPs *xemacpsp __attribute__((unused)),
                              s32_t speed __attribute__((unused)))
{
#ifndef SDT
	volatile UINTPTR slcrBaseAddress;
	u32_t SlcrDiv0 = 0;
	u32_t SlcrDiv1 = 0;
	u32_t SlcrTxClkCntrl;
	u32_t gigeversion;
	volatile UINTPTR CrlApbBaseAddr;
	u32_t CrlApbDiv0 = 0;
	u32_t CrlApbDiv1 = 0;
	u32_t CrlApbGemCtrl;

	UINTPTR mac_baseaddr = xemacpsp->Config.BaseAddress;

#if defined (__aarch64__) && (EL1_NONSECURE == 1)
	u32_t ClkId;
#endif

	gigeversion = ((Xil_In32(mac_baseaddr + 0xFC)) >> 16) & 0xFFF;
	if (gigeversion == 2) {

		Xil_Out32(SLCR_UNLOCK_ADDR, SLCR_UNLOCK_KEY_VALUE);

		if (mac_baseaddr == ZYNQ_EMACPS_0_BASEADDR) {
			slcrBaseAddress = SLCR_GEM0_CLK_CTRL_ADDR;
		} else {
			slcrBaseAddress = SLCR_GEM1_CLK_CTRL_ADDR;
		}

		if(Xil_In32(slcrBaseAddress) &
			SLCR_GEM_SRCSEL_EMIO) {
				return;
		}

		if (speed == 1000) {
			if (mac_baseaddr == ZYNQ_EMACPS_0_BASEADDR) {
#ifdef XPAR_PS7_ETHERNET_0_ENET_SLCR_1000MBPS_DIV0
				SlcrDiv0 = XPAR_PS7_ETHERNET_0_ENET_SLCR_1000MBPS_DIV0;
				SlcrDiv1 = XPAR_PS7_ETHERNET_0_ENET_SLCR_1000MBPS_DIV1;
#endif
			} else {
#ifdef XPAR_PS7_ETHERNET_1_ENET_SLCR_1000MBPS_DIV0
				SlcrDiv0 = XPAR_PS7_ETHERNET_1_ENET_SLCR_1000MBPS_DIV0;
				SlcrDiv1 = XPAR_PS7_ETHERNET_1_ENET_SLCR_1000MBPS_DIV1;
#endif
			}
		} else if (speed == 100) {
			if (mac_baseaddr == ZYNQ_EMACPS_0_BASEADDR) {
#ifdef XPAR_PS7_ETHERNET_0_ENET_SLCR_100MBPS_DIV0
				SlcrDiv0 = XPAR_PS7_ETHERNET_0_ENET_SLCR_100MBPS_DIV0;
				SlcrDiv1 = XPAR_PS7_ETHERNET_0_ENET_SLCR_100MBPS_DIV1;
#endif
			} else {
#ifdef XPAR_PS7_ETHERNET_1_ENET_SLCR_100MBPS_DIV0
				SlcrDiv0 = XPAR_PS7_ETHERNET_1_ENET_SLCR_100MBPS_DIV0;
				SlcrDiv1 = XPAR_PS7_ETHERNET_1_ENET_SLCR_100MBPS_DIV1;
#endif
			}
		} else {
			if (mac_baseaddr == ZYNQ_EMACPS_0_BASEADDR) {
#ifdef XPAR_PS7_ETHERNET_0_ENET_SLCR_10MBPS_DIV0
				SlcrDiv0 = XPAR_PS7_ETHERNET_0_ENET_SLCR_10MBPS_DIV0;
				SlcrDiv1 = XPAR_PS7_ETHERNET_0_ENET_SLCR_10MBPS_DIV1;
#endif
			} else {
#ifdef XPAR_PS7_ETHERNET_1_ENET_SLCR_10MBPS_DIV0
				SlcrDiv0 = XPAR_PS7_ETHERNET_1_ENET_SLCR_10MBPS_DIV0;
				SlcrDiv1 = XPAR_PS7_ETHERNET_1_ENET_SLCR_10MBPS_DIV1;
#endif
			}
		}

		if (SlcrDiv0 != 0 && SlcrDiv1 != 0) {
			SlcrTxClkCntrl = Xil_In32(slcrBaseAddress);
			SlcrTxClkCntrl &= EMACPS_SLCR_DIV_MASK;
			SlcrTxClkCntrl |= (SlcrDiv1 << 20);
			SlcrTxClkCntrl |= (SlcrDiv0 << 8);
			Xil_Out32(slcrBaseAddress, SlcrTxClkCntrl);
			Xil_Out32(SLCR_LOCK_ADDR, SLCR_LOCK_KEY_VALUE);
		} else {
			xil_printf("Clock Divisors incorrect - Please check\r\n");
		}
	} else if (gigeversion == GEM_VERSION_ZYNQMP) {
		/* Setup divisors in CRL_APB for Zynq Ultrascale+ MPSoC */
		if (mac_baseaddr == ZYNQMP_EMACPS_0_BASEADDR) {
			CrlApbBaseAddr = CRL_APB_GEM0_REF_CTRL;
		} else if (mac_baseaddr == ZYNQMP_EMACPS_1_BASEADDR) {
			CrlApbBaseAddr = CRL_APB_GEM1_REF_CTRL;
		} else if (mac_baseaddr == ZYNQMP_EMACPS_2_BASEADDR) {
			CrlApbBaseAddr = CRL_APB_GEM2_REF_CTRL;
		} else if (mac_baseaddr == ZYNQMP_EMACPS_3_BASEADDR) {
			CrlApbBaseAddr = CRL_APB_GEM3_REF_CTRL;
		}

		if (speed == 1000) {
			if (mac_baseaddr == ZYNQMP_EMACPS_0_BASEADDR) {
#ifdef XPAR_PSU_ETHERNET_0_ENET_SLCR_1000MBPS_DIV0
				CrlApbDiv0 = XPAR_PSU_ETHERNET_0_ENET_SLCR_1000MBPS_DIV0;
				CrlApbDiv1 = XPAR_PSU_ETHERNET_0_ENET_SLCR_1000MBPS_DIV1;
#endif
			} else if (mac_baseaddr == ZYNQMP_EMACPS_1_BASEADDR) {
#ifdef XPAR_PSU_ETHERNET_1_ENET_SLCR_1000MBPS_DIV0
				CrlApbDiv0 = XPAR_PSU_ETHERNET_1_ENET_SLCR_1000MBPS_DIV0;
				CrlApbDiv1 = XPAR_PSU_ETHERNET_1_ENET_SLCR_1000MBPS_DIV1;
#endif
			} else if (mac_baseaddr == ZYNQMP_EMACPS_2_BASEADDR) {
#ifdef XPAR_PSU_ETHERNET_2_ENET_SLCR_1000MBPS_DIV0
				CrlApbDiv0 = XPAR_PSU_ETHERNET_2_ENET_SLCR_1000MBPS_DIV0;
				CrlApbDiv1 = XPAR_PSU_ETHERNET_2_ENET_SLCR_1000MBPS_DIV1;
#endif
			} else if (mac_baseaddr == ZYNQMP_EMACPS_3_BASEADDR) {
#ifdef XPAR_PSU_ETHERNET_3_ENET_SLCR_1000MBPS_DIV0
				CrlApbDiv0 = XPAR_PSU_ETHERNET_3_ENET_SLCR_1000MBPS_DIV0;
				CrlApbDiv1 = XPAR_PSU_ETHERNET_3_ENET_SLCR_1000MBPS_DIV1;
#endif
			}
		} else if (speed == 100) {
			if (mac_baseaddr == ZYNQMP_EMACPS_0_BASEADDR) {
#ifdef XPAR_PSU_ETHERNET_0_ENET_SLCR_100MBPS_DIV0
				CrlApbDiv0 = XPAR_PSU_ETHERNET_0_ENET_SLCR_100MBPS_DIV0;
				CrlApbDiv1 = XPAR_PSU_ETHERNET_0_ENET_SLCR_100MBPS_DIV1;
#endif
			} else if (mac_baseaddr == ZYNQMP_EMACPS_1_BASEADDR) {
#ifdef XPAR_PSU_ETHERNET_1_ENET_SLCR_100MBPS_DIV0
				CrlApbDiv0 = XPAR_PSU_ETHERNET_1_ENET_SLCR_100MBPS_DIV0;
				CrlApbDiv1 = XPAR_PSU_ETHERNET_1_ENET_SLCR_100MBPS_DIV1;
#endif
			} else if (mac_baseaddr == ZYNQMP_EMACPS_2_BASEADDR) {
#ifdef XPAR_PSU_ETHERNET_2_ENET_SLCR_100MBPS_DIV0
				CrlApbDiv0 = XPAR_PSU_ETHERNET_2_ENET_SLCR_100MBPS_DIV0;
				CrlApbDiv1 = XPAR_PSU_ETHERNET_2_ENET_SLCR_100MBPS_DIV1;
#endif
			} else if (mac_baseaddr == ZYNQMP_EMACPS_3_BASEADDR) {
#ifdef XPAR_PSU_ETHERNET_3_ENET_SLCR_100MBPS_DIV0
				CrlApbDiv0 = XPAR_PSU_ETHERNET_3_ENET_SLCR_100MBPS_DIV0;
				CrlApbDiv1 = XPAR_PSU_ETHERNET_3_ENET_SLCR_100MBPS_DIV1;
#endif
			}
		} else {
			if (mac_baseaddr == ZYNQMP_EMACPS_0_BASEADDR) {
#ifdef XPAR_PSU_ETHERNET_0_ENET_SLCR_10MBPS_DIV0
				CrlApbDiv0 = XPAR_PSU_ETHERNET_0_ENET_SLCR_10MBPS_DIV0;
				CrlApbDiv1 = XPAR_PSU_ETHERNET_0_ENET_SLCR_10MBPS_DIV1;
#endif
			} else if (mac_baseaddr == ZYNQMP_EMACPS_1_BASEADDR) {
#ifdef XPAR_PSU_ETHERNET_1_ENET_SLCR_10MBPS_DIV0
				CrlApbDiv0 = XPAR_PSU_ETHERNET_1_ENET_SLCR_10MBPS_DIV0;
				CrlApbDiv1 = XPAR_PSU_ETHERNET_1_ENET_SLCR_10MBPS_DIV1;
#endif
			} else if (mac_baseaddr == ZYNQMP_EMACPS_2_BASEADDR) {
#ifdef XPAR_PSU_ETHERNET_2_ENET_SLCR_10MBPS_DIV0
				CrlApbDiv0 = XPAR_PSU_ETHERNET_2_ENET_SLCR_10MBPS_DIV0;
				CrlApbDiv1 = XPAR_PSU_ETHERNET_2_ENET_SLCR_10MBPS_DIV1;
#endif
			} else if (mac_baseaddr == ZYNQMP_EMACPS_3_BASEADDR) {
#ifdef XPAR_PSU_ETHERNET_3_ENET_SLCR_10MBPS_DIV0
				CrlApbDiv0 = XPAR_PSU_ETHERNET_3_ENET_SLCR_10MBPS_DIV0;
				CrlApbDiv1 = XPAR_PSU_ETHERNET_3_ENET_SLCR_10MBPS_DIV1;
#endif
			}
		}

		if (CrlApbDiv0 != 0 && CrlApbDiv1 != 0) {
		#if defined (__aarch64__) && (EL1_NONSECURE == 1)
			XSmc_OutVar RegRead;
			RegRead = Xil_Smc(MMIO_READ_SMC_FID, (u64)(CrlApbBaseAddr),
								0, 0, 0, 0, 0, 0);
			CrlApbGemCtrl = RegRead.Arg0 >> 32;
		#else
			CrlApbGemCtrl = Xil_In32(CrlApbBaseAddr);
        #endif
			CrlApbGemCtrl &= ~CRL_APB_GEM_DIV0_MASK;
			CrlApbGemCtrl |= CrlApbDiv0 << CRL_APB_GEM_DIV0_SHIFT;
			CrlApbGemCtrl &= ~CRL_APB_GEM_DIV1_MASK;
			CrlApbGemCtrl |= CrlApbDiv1 << CRL_APB_GEM_DIV1_SHIFT;
		#if defined (__aarch64__) && (EL1_NONSECURE == 1)
			Xil_Smc(MMIO_WRITE_SMC_FID, (u64)(CrlApbBaseAddr) | ((u64)(0xFFFFFFFF) << 32),
				(u64)CrlApbGemCtrl, 0, 0, 0, 0, 0);
			do {
			RegRead = Xil_Smc(MMIO_READ_SMC_FID, (u64)(CrlApbBaseAddr),
				0, 0, 0, 0, 0, 0);
			} while((RegRead.Arg0 >> 32) != CrlApbGemCtrl);
		#else
			Xil_Out32(CrlApbBaseAddr, CrlApbGemCtrl);
        #endif
		} else {
			xil_printf("Clock Divisors incorrect - Please check\r\n");
		}
	} else if (gigeversion == GEM_VERSION_VERSAL) {
		/* Setup divisors in CRL for Versal */
		if (mac_baseaddr == VERSAL_EMACPS_0_BASEADDR) {
			CrlApbBaseAddr = VERSAL_CRL_GEM0_REF_CTRL;
#if defined (__aarch64__) && (EL1_NONSECURE == 1)
			ClkId = CLK_GEM0_REF;
#endif
		} else if (mac_baseaddr == VERSAL_EMACPS_1_BASEADDR) {
			CrlApbBaseAddr = VERSAL_CRL_GEM1_REF_CTRL;
#if defined (__aarch64__) && (EL1_NONSECURE == 1)
			ClkId = CLK_GEM1_REF;
#endif
		} else if (mac_baseaddr == VERSAL_NET_EMACPS_0_BASEADDR) {
#ifdef VERSAL_NET_CRL_GEM0_REF_CTRL
			CrlApbBaseAddr = VERSAL_NET_CRL_GEM0_REF_CTRL;
#endif
#if defined (__aarch64__) && (EL1_NONSECURE == 1)
			ClkId = CLK_GEM0_REF;
#endif
		} else if (mac_baseaddr == VERSAL_NET_EMACPS_1_BASEADDR) {
#ifdef VERSAL_NET_CRL_GEM1_REF_CTRL
			CrlApbBaseAddr = VERSAL_NET_CRL_GEM1_REF_CTRL;
#endif
#if defined (__aarch64__) && (EL1_NONSECURE == 1)
			ClkId = CLK_GEM1_REF;
#endif
		}

		if (speed == 1000) {
			if (mac_baseaddr == VERSAL_EMACPS_0_BASEADDR) {
#ifdef XPAR_VERSAL_CIPS_0_PSPMC_0_PSV_ETHERNET_0_ENET_SLCR_1000MBPS_DIV0
				CrlApbDiv0 = XPAR_VERSAL_CIPS_0_PSPMC_0_PSV_ETHERNET_0_ENET_SLCR_1000MBPS_DIV0;
#endif
			} else if (mac_baseaddr == VERSAL_EMACPS_1_BASEADDR) {
#ifdef XPAR_VERSAL_CIPS_0_PSPMC_0_PSV_ETHERNET_1_ENET_SLCR_1000MBPS_DIV0
				CrlApbDiv0 = XPAR_VERSAL_CIPS_0_PSPMC_0_PSV_ETHERNET_1_ENET_SLCR_1000MBPS_DIV0;
#endif
			} else if (mac_baseaddr == VERSAL_NET_EMACPS_0_BASEADDR) {
#ifdef XPAR_PSX_WIZARD_0_PSXL_0_PSX_ETHERNET_0_ENET_SLCR_1000MBPS_DIV0
				CrlApbDiv0 =  XPAR_PSX_WIZARD_0_PSXL_0_PSX_ETHERNET_0_ENET_SLCR_1000MBPS_DIV0;
#endif
			} else if (mac_baseaddr == VERSAL_NET_EMACPS_1_BASEADDR) {
#ifdef  XPAR_PSX_WIZARD_0_PSXL_0_PSX_ETHERNET_1_ENET_SLCR_1000MBPS_DIV0
				CrlApbDiv0 =  XPAR_PSX_WIZARD_0_PSXL_0_PSX_ETHERNET_1_ENET_SLCR_1000MBPS_DIV0;
#endif
			}
		} else if (speed == 100) {
			if (mac_baseaddr == VERSAL_EMACPS_0_BASEADDR) {
#ifdef XPAR_VERSAL_CIPS_0_PSPMC_0_PSV_ETHERNET_0_ENET_SLCR_100MBPS_DIV0
				CrlApbDiv0 = XPAR_VERSAL_CIPS_0_PSPMC_0_PSV_ETHERNET_0_ENET_SLCR_100MBPS_DIV0;
#endif
			} else if (mac_baseaddr == VERSAL_EMACPS_1_BASEADDR) {
#ifdef XPAR_VERSAL_CIPS_0_PSPMC_0_PSV_ETHERNET_1_ENET_SLCR_100MBPS_DIV0
				CrlApbDiv0 = XPAR_VERSAL_CIPS_0_PSPMC_0_PSV_ETHERNET_1_ENET_SLCR_100MBPS_DIV0;
#endif
			} else if (mac_baseaddr == VERSAL_NET_EMACPS_0_BASEADDR) {
#ifdef XPAR_PSX_WIZARD_0_PSXL_0_PSX_ETHERNET_0_ENET_SLCR_100MBPS_DIV0
				CrlApbDiv0 = XPAR_PSX_WIZARD_0_PSXL_0_PSX_ETHERNET_0_ENET_SLCR_100MBPS_DIV0;
#endif
			} else if (mac_baseaddr == VERSAL_NET_EMACPS_1_BASEADDR) {
#ifdef XPAR_PSX_WIZARD_0_PSXL_0_PSX_ETHERNET_1_ENET_SLCR_100MBPS_DIV0
				CrlApbDiv0 = XPAR_PSX_WIZARD_0_PSXL_0_PSX_ETHERNET_1_ENET_SLCR_100MBPS_DIV0;
#endif
			}
		} else {
			if (mac_baseaddr == VERSAL_EMACPS_0_BASEADDR) {
#ifdef XPAR_VERSAL_CIPS_0_PSPMC_0_PSV_ETHERNET_0_ENET_SLCR_10MBPS_DIV0
				CrlApbDiv0 = XPAR_VERSAL_CIPS_0_PSPMC_0_PSV_ETHERNET_0_ENET_SLCR_10MBPS_DIV0;
#endif
			} else if (mac_baseaddr == VERSAL_EMACPS_1_BASEADDR) {
#ifdef XPAR_VERSAL_CIPS_0_PSPMC_0_PSV_ETHERNET_1_ENET_SLCR_10MBPS_DIV0
				CrlApbDiv0 = XPAR_VERSAL_CIPS_0_PSPMC_0_PSV_ETHERNET_1_ENET_SLCR_10MBPS_DIV0;
#endif
			} else if (mac_baseaddr == VERSAL_NET_EMACPS_0_BASEADDR) {
#ifdef XPAR_PSX_WIZARD_0_PSXL_0_PSX_ETHERNET_0_ENET_SLCR_10MBPS_DIV0
				CrlApbDiv0 = XPAR_PSX_WIZARD_0_PSXL_0_PSX_ETHERNET_0_ENET_SLCR_10MBPS_DIV0;
#endif
			} else if (mac_baseaddr == VERSAL_NET_EMACPS_1_BASEADDR) {
#ifdef XPAR_PSX_WIZARD_0_PSXL_0_PSX_ETHERNET_1_ENET_SLCR_10MBPS_DIV0
				CrlApbDiv0 = XPAR_PSX_WIZARD_0_PSXL_0_PSX_ETHERNET_1_ENET_SLCR_10MBPS_DIV0;
#endif
			}
		}

		if (CrlApbDiv0 != 0) {
			if ((mac_baseaddr == VERSAL_EMACPS_0_BASEADDR) ||
			    (mac_baseaddr == VERSAL_EMACPS_1_BASEADDR)) {
#if defined (__aarch64__) && (EL1_NONSECURE == 1)
			Xil_Smc(PM_SET_DIVIDER_SMC_FID, (((u64)CrlApbDiv0 << 32) | ClkId), 0, 0, 0, 0, 0, 0);
#else
			CrlApbGemCtrl = Xil_In32(CrlApbBaseAddr);
			CrlApbGemCtrl &= ~VERSAL_CRL_GEM_DIV_MASK;
			CrlApbGemCtrl |= CrlApbDiv0 << VERSAL_CRL_APB_GEM_DIV_SHIFT;

			Xil_Out32(CrlApbBaseAddr, CrlApbGemCtrl);
#endif
			} else if((mac_baseaddr == VERSAL_NET_EMACPS_0_BASEADDR) ||
				  (mac_baseaddr == VERSAL_NET_EMACPS_1_BASEADDR)) {
#if defined (__aarch64__) && (EL1_NONSECURE == 1)
			Xil_Smc(PM_SET_DIVIDER_SMC_FID, (((u64)CrlApbDiv0 << 32) | ClkId), 0, 0, 0, 0, 0, 0);
#else
			CrlApbGemCtrl = Xil_In32(CrlApbBaseAddr);
			CrlApbGemCtrl &= ~VERSAL_NET_CRL_GEM_DIV_MASK;
			CrlApbGemCtrl |= CrlApbDiv0 << VERSAL_NET_CRL_APB_GEM_DIV_SHIFT;

			Xil_Out32(CrlApbBaseAddr, CrlApbGemCtrl);
#endif
			}
		} else {
			xil_printf("Clock Divisors incorrect - Please check\r\n");
		}
	}
#else

#if defined(PLATFORM_ZYNQMP) && defined(XCLOCKING)
	XClockRate SetClockRate = 0;
	u32_t ClockId;

	ClockId = xemacpsp->Config.RefClk;

	if (speed == 1000)
	{
		Xil_ClockSetRate(ClockId, CLOCK_FREQ_1000MBPS, &SetClockRate);
	}
	else if (speed == 100)
	{
		Xil_ClockSetRate(ClockId, CLOCK_FREQ_100MBPS, &SetClockRate);
	}
	else if (speed == 10)
	{
		Xil_ClockSetRate(ClockId, CLOCK_FREQ_10MBPS, &SetClockRate);
	}
#else
	xil_printf("Using default Speed from design\r\n");
#endif

#endif
	return;
}
/*****************************************************************************/
/**
* Check if the connected PHY is an external PCS-PMA.
*
* This function reads the PHY identifier registers to determine whether the
* PHY connected to the given address is an external Xilinx PCS-PMA device.
*
* @param xemacpsp is a pointer to the Ethernet MAC instance.
* @param phy_addr is the address of the PHY device.
*
* @return
*   - 1 if the PHY is an external PCS-PMA.
*   - 0 if the PHY is assumed to be an internal PCS-PMA.
*
******************************************************************************/
int isphy_pcspma_external(XEmacPs *xemacpsp, u32_t phy_addr) {
	u16 phy_id;

	XEmacPs_PhyRead(xemacpsp, phy_addr, PHY_IDENTIFIER_1_REG, &phy_id);
	if (phy_id == PHY_XILINX_PCS_PMA_ID1) {
		XEmacPs_PhyRead(xemacpsp, phy_addr, PHY_IDENTIFIER_2_REG, &phy_id);
		if (phy_id == PHY_XILINX_PCS_PMA_ID2) {
			return 1;// External PCS-PMA detected
		}
	}

	return 0;// Internal PCS-PMA assumed
}

/*****************************************************************************/
/**
* Configure the MAC for SGMII PCS mode.
*
* This function configures the Ethernet MAC for operation with an SGMII
* (Serial Gigabit Media Independent Interface) PHY. It first checks whether
* the PHY is external using the `isphy_pcspma_external` function. If the PHY
* is not an external PCS-PMA device, the function enables the SGMII option
* and sets the Auto-Negotiation bit in the PCS control register.
*
* @param xemacpsp is a pointer to the Ethernet MAC instance.
* @param phy_addr is the address of the PHY device.
*
******************************************************************************/
void MacConfig_SgmiiPcs(XEmacPs *xemacpsp, u32_t phy_addr) {
#ifdef SDT
	const char *PhyType = xemacpsp->Config.PhyType;
	if (!strcmp(PhyType, "sgmii") && !isphy_pcspma_external(xemacpsp, phy_addr)){

		/* Enable SGMII option */
		XEmacPs_SetOptions(xemacpsp, XEMACPS_SGMII_ENABLE_OPTION);

		/* Read the current PCS_CONTROL register value */
		u32 status = XEmacPs_ReadReg(xemacpsp->Config.BaseAddress, XEMACPS_PCS_CONTROL_OFFSET);

		/* Set the Enable Auto-Negotiation bit (bit 12) */
		status |= XEMACPS_PCS_CON_AUTO_NEG_MASK;

		/* Write the updated value back to the PCS_CONTROL register */
		XEmacPs_WriteReg(xemacpsp->Config.BaseAddress, XEMACPS_PCS_CONTROL_OFFSET, status);
	}
#endif
}
