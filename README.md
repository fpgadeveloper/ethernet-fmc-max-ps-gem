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

| Target board          | Target design      | Ports       | FMC Slot    | Standalone<br> Echo Server | PetaLinux | Yocto | Vivado<br> Edition | IP<br>License |
|-----------------------|--------------------|-------------|-------------|-------|-------|-------|-------|-------|
| [UltraZed-EV Carrier] | `uzev`             | 4x          | HPC         | :white_check_mark: | :white_check_mark: | :white_check_mark: | Standard :free: | -     |
| [ZCU102]              | `zcu102_hpc0`      | 4x          | HPC0        | :white_check_mark: | :white_check_mark: | :white_check_mark: | Enterprise | -     |
| [ZCU106]              | `zcu106_hpc0`      | 4x          | HPC0        | :white_check_mark: | :white_check_mark: | :white_check_mark: | Standard :free: | -     |
| [ZCU111]              | `zcu111`           | 4x          | FMCP        | :white_check_mark: | :white_check_mark: | :white_check_mark: | Enterprise | -     |

### Versal designs

| Target board          | Target design      | Ports       | FMC Slot    | Standalone<br> Echo Server | PetaLinux | Yocto | Vivado<br> Edition | IP<br>License |
|-----------------------|--------------------|-------------|-------------|-------|-------|-------|-------|-------|
| [VCK190]              | `vck190_fmcp1`     | 2x          | FMCP1       | :white_check_mark: | :white_check_mark: | :white_check_mark: | Enterprise | -     |

[UltraZed-EV Carrier]: https://www.xilinx.com/products/boards-and-kits/1-1s78dxb.html
[ZCU102]: https://www.xilinx.com/zcu102
[ZCU106]: https://www.xilinx.com/zcu106
[ZCU111]: https://www.xilinx.com/zcu111
[VCK190]: https://www.xilinx.com/vck190
<!-- updater end -->

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

Clone the repo and change into its directory:
```
git clone https://github.com/fpgadeveloper/ethernet-fmc-max-ps-gem.git
cd ethernet-fmc-max-ps-gem
```

### Cross-platform build runner

All builds are driven by `build.py` at the repo root, on both Windows
(git bash) and Linux. The `build.sh` / `build.bat` shim finds a suitable
Python 3 automatically (including the one bundled with the AMD tools).
Pick a target design label from the tables above (or run `./build.sh
list`), then run the build command for the stage(s) you want — each
command builds whatever it depends on automatically and skips anything
already built. On Windows without git bash, run the same commands from
Command Prompt or PowerShell using `build.bat` (e.g. `build.bat xsa
--target <target>`).

You don't need to source the AMD tools first — the build runner finds
Vivado, Vitis and PetaLinux automatically in their standard install
locations and sets up the environment each stage needs. If your tools
are installed somewhere non-standard and the runner can't find them,
source the tool settings yourself before running the build.

#### Build the Vivado project (bitstream + XSA)

```
./build.sh xsa --target <target>
```

#### Build the standalone application

Builds the Vitis workspace and the baremetal boot file (`BOOT.BIN` or
bit file, depending on the device family):

```
./build.sh standalone --target <target>
```

#### Build PetaLinux (Linux only)

```
./build.sh petalinux --target <target>
```

#### Build everything

Builds all of the above that the target supports, then gathers the boot
images into `bootimages/*.zip`:

```
./build.sh all --target <target>
./build.sh all --target all          # every target in the repo
```

Also available: `status`, `clean`, `project` — see
`./build.sh --help`. On Windows, the PetaLinux and Yocto stages require a
Linux machine; the runner says so and prints the hand-off command. The
legacy `make` interface still works on Linux (each Makefile now wraps
`build.sh`) but is deprecated and will be removed at the next version
update.

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
