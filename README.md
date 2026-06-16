# PS GEM Reference Designs for the Opsero Ethernet FMC Max (OP080)

## Description

This project demonstrates the use of the Opsero [Ethernet FMC Max] (OP080) with the hard
gigabit Ethernet MACs (GEMs) of the Zynq UltraScale+ and Versal processing systems.
Each port of the mezzanine card is driven by a PS GEM routed through EMIO GMII into the
programmable logic, where an Ethernet PCS/PMA or SGMII core (PG047) converts the GMII to
SGMII over a gigabit transceiver lane connected to the port's TI DP83867 PHY. No soft MAC
or DMA IP is used — the ports are handled by the standard PS GEM software drivers.

![Ethernet FMC Max with ZCU106](docs/source/images/zcu106-with-op080_03.jpg)

Important links:

* The user guide for these reference designs is hosted here: [Ethernet FMC Max PS GEM docs](https://psgem-sgmii.ethernetfmc.com "Ethernet FMC Max PS GEM docs")
* To report a bug: [Report an issue](https://github.com/fpgadeveloper/ethernet-fmc-max-ps-gem/issues "Report an issue").
* For technical support: [Contact Opsero](https://opsero.com/contact-us "Contact Opsero").
* To purchase the mezzanine card: [Ethernet FMC Max order page](https://opsero.com/product/ethernet-fmc-max "Ethernet FMC Max order page").

## Requirements

This project is designed for version 2025.2 of the Xilinx tools (Vivado/Vitis/PetaLinux). 
If you are using an older version of the Xilinx tools, then refer to the 
[release tags](https://github.com/fpgadeveloper/ethernet-fmc-max-ps-gem/tags "releases")
to find the version of this repository that matches your version of the tools.

In order to test this design on hardware, you will need the following:

* Vivado 2025.2
* Vitis 2025.2
* PetaLinux Tools 2025.2
* [Ethernet FMC Max]
* One of the target platforms listed below

## Target designs

This repo contains several designs that target various supported development boards and their
FMC connectors. The table below lists the target design name, the number of ports supported by the design and 
the FMC connector on which to connect the mezzanine card. Some of the target designs
require a license to generate a bitstream with the AMD Xilinx tools.

<!-- updater start -->
### Zynq UltraScale+ designs

| Target board          | Target design      | Ports       | FMC Slot    | Standalone<br> Echo Server | PetaLinux | Vivado<br> Edition |
|-----------------------|--------------------|-------------|-------------|-------|-------|-------|
| [UltraZed-EV Carrier] | `uzev`             | 4x          | HPC         | :white_check_mark: | :white_check_mark: | Standard :free: |
| [ZCU102]              | `zcu102_hpc0`      | 4x          | HPC0        | :white_check_mark: | :white_check_mark: | Enterprise |
| [ZCU106]              | `zcu106_hpc0`      | 4x          | HPC0        | :white_check_mark: | :white_check_mark: | Standard :free: |
| [ZCU111]              | `zcu111`           | 4x          | FMCP        | :white_check_mark: | :white_check_mark: | Enterprise |

### Versal designs

| Target board          | Target design      | Ports       | FMC Slot    | Standalone<br> Echo Server | PetaLinux | Vivado<br> Edition |
|-----------------------|--------------------|-------------|-------------|-------|-------|-------|
| [VCK190]              | `vck190_fmcp1`     | 2x          | FMCP1       | :white_check_mark: | :white_check_mark: | Enterprise |

[UltraZed-EV Carrier]: https://www.xilinx.com/products/boards-and-kits/1-1s78dxb.html
[ZCU102]: https://www.xilinx.com/zcu102
[ZCU106]: https://www.xilinx.com/zcu106
[ZCU111]: https://www.xilinx.com/zcu111
[VCK190]: https://www.xilinx.com/vck190
<!-- updater end -->

### UltraZed-EV board files

The board definition files for the UltraZed-EV Carrier are not currently included in the AMD Xilinx Board Store.
To enable Vivado to recognize this board, the required board files have been included in this
repository as a Git submodule (`submodules/avnet-bdf`), which is a fork of
[Avnet's BDF repository](https://github.com/Avnet/bdf). When cloning this repo, use the `--recursive`
flag to ensure the board files are downloaded:

```
git clone --recursive <repo-url>
```

Notes:

1. The Vivado Edition column indicates which designs are supported by the Vivado *Standard* Edition, the
   FREE edition which can be used without a license. Vivado *Enterprise* Edition requires
   a license however a 30-day evaluation license is available from the AMD Xilinx Licensing site.
2. The VCK190 design supports ports 0 and 1 of the Ethernet FMC Max only, because the Versal PS
   has two GEM controllers. The PHYs of ports 2 and 3 are held in reset in that design.
3. Because the GEMs are routed to the FMC through EMIO in these designs, the development
   board's onboard Ethernet ports are not available.

## Software

These reference designs can be driven by either a standalone application or within a PetaLinux environment. 
The repository includes all necessary scripts and code to build both environments. The table 
below outlines the corresponding applications available in each environment:

| Environment      | Available Applications  |
|------------------|-------------------------|
| Standalone       | lwIP Echo Server |
| PetaLinux        | Built-in Linux commands<br>Additional tools: ethtool, phytool, iperf3 |

## Build instructions

Clone the repo:
```
git clone --recursive https://github.com/fpgadeveloper/ethernet-fmc-max-ps-gem.git
```

Source Vivado and PetaLinux tools:

```
source <path-to-petalinux>/2025.2/settings.sh
source <path-to-xilinx-tools>/2025.2/Vivado/settings64.sh
```

To build the standalone lwIP echo server application (Vivado project and Vitis workspace):

```
cd ethernet-fmc-max-ps-gem/Vitis
make workspace TARGET=zcu106_hpc0
```

To build the PetaLinux image (Vivado project and PetaLinux):

```
cd ethernet-fmc-max-ps-gem/PetaLinux
make petalinux TARGET=zcu106_hpc0
```

Replace the target label in these commands with the one corresponding to the target design of your
choice from the tables above.

## Troubleshooting

### PetaLinux build fails with `bitbake petalinux-image-minimal failed` and sstate fetch errors

If a `make petalinux TARGET=<board>` run ends with errors like

```
ERROR: <package>-<ver>-r0 do_..._setscene: Fetcher failure: Unable to find file file://.../sstate:...
[ERROR] Command bitbake petalinux-image-minimal failed
```

the actual build is not broken. These `_setscene` errors come from
bitbake trying to pull prebuilt artifacts from the public Xilinx
sstate-cache mirror, which occasionally returns 404 for individual
packages. Bitbake falls back to building those packages locally and
succeeds, but still exits non-zero because of the failed fetches —
so the Makefile stops before the `petalinux-package` step that
produces `BOOT.BIN`.

**Fix: just re-run the same command.** The second attempt finds the
missing packages in the local sstate cache (populated by the first
run) and completes cleanly, producing `BOOT.BIN`. The reference
design itself is fine; this is a transient issue with the public
mirror.


## Contribute

We strongly encourage community contribution to these projects. Please make a pull request if you
would like to share your work:
* if you've spotted and fixed any issues
* if you've added designs for other target platforms

Thank you to everyone who supports us!

## About us

This project was developed by [Opsero Inc.](https://opsero.com "Opsero Inc."),
a tight-knit team of FPGA experts delivering FPGA products and design services to start-ups and tech companies. 
Follow our blog, [FPGA Developer](https://www.fpgadeveloper.com "FPGA Developer"), for news, tutorials and
updates on the awesome projects we work on.

[Ethernet FMC Max]: https://docs.opsero.com/op080/datasheet/overview/
