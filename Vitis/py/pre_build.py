"""Pre-build fixup for lwip_echo_server template apps with multiple PS GEMs.

Fix: the template's lwip_echo_server.cmake builds TOTAL_MAC_INSTANCES from all
  EMACPS instances and picks index 0 when generating platform_config.h, so the
  echo server always runs on GEM0 (port 0 of the Ethernet FMC Max).

  This script patches platform_config.h.in to replace the cmake-generated
  PLATFORM_EMAC_BASEADDR with a port selection block that lets the user choose
  the Ethernet FMC Max port via the XPAR_XEMACPS_N_BASEADDR defines.
  To retarget the echo server to a different port, edit ETHERNET_PORT in
  platform_config.h.in (in the workspace echo_server/src directory).

  Note: on the Versal targets (VCK190) only ports 0 and 1 exist, because the
  Versal PS has two GEMs.

Usage: called by build-vitis.py with app_src as the first argument.
"""

import os, sys

PORT_CONFIG = """\
/*
 * Ethernet FMC port selection
 *
 * Change ETHERNET_PORT to select which Ethernet FMC port to use.
 * Each port is driven by the PS GEM of the same index.
 * Valid values: 0, 1, 2, 3 (0, 1 on Versal targets)
 */

#ifndef PORT_CONFIG_H
#define PORT_CONFIG_H

#define ETHERNET_PORT 0

#include "xparameters.h"

#if ETHERNET_PORT == 0
#define PLATFORM_EMAC_BASEADDR XPAR_XEMACPS_0_BASEADDR
#elif ETHERNET_PORT == 1
#define PLATFORM_EMAC_BASEADDR XPAR_XEMACPS_1_BASEADDR
#elif ETHERNET_PORT == 2
#define PLATFORM_EMAC_BASEADDR XPAR_XEMACPS_2_BASEADDR
#elif ETHERNET_PORT == 3
#define PLATFORM_EMAC_BASEADDR XPAR_XEMACPS_3_BASEADDR
#else
#error "Invalid ETHERNET_PORT value. Must be 0, 1, 2, or 3."
#endif

#endif"""

def main():
    if len(sys.argv) < 2:
        print("Usage: pre_build.py <app_src_dir>")
        sys.exit(1)

    app_src = sys.argv[1]
    config_in = os.path.join(app_src, "platform_config.h.in")

    if not os.path.isfile(config_in):
        print(f"WARNING: {config_in} not found; skipping patch")
        return

    with open(config_in, "r") as f:
        content = f.read()

    old = "#cmakedefine PLATFORM_EMAC_BASEADDR @PLATFORM_EMAC_BASEADDR@"

    if old not in content:
        print(f"NOTE: cmake PLATFORM_EMAC_BASEADDR line not found in {config_in}; skipping")
        return

    content = content.replace(old, PORT_CONFIG)
    with open(config_in, "w") as f:
        f.write(content)
    print(f"Patched {config_in}: replaced cmake PLATFORM_EMAC_BASEADDR with port selection")

if __name__ == "__main__":
    main()
