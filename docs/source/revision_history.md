# Revision History

## 2025.2 update (September 2026)

New:

* **Yocto (AMD EDF) build flow** for all targets: `./build.sh yocto --target <target>`. It
  builds a complete SD card disk image (`rootfs.wic.xz`) whose Yocto MACHINE and device tree
  are generated from the target's own XSA. The image includes the same test tools as the
  PetaLinux image (`ethtool`, `iperf3`, `phytool`) plus `mtd-utils`, `can-utils`,
  `nfs-utils` and `pciutils`, and the `pcs-unisolate` boot service. See [Yocto](yocto.md).
* The Yocto boot image zip (`bootimages/…_yocto-2025-2.zip`) contains `BOOT.BIN`, and on the
  VCK190 also `BOOTAA64.EFI`, next to the disk image. Neither file is in the disk image where
  the boot ROM (`BOOT.BIN`) or U-Boot (`BOOTAA64.EFI`) looks for it, so a card that has only
  the disk image does not boot; copy them onto the first partition.
* `./build.sh clean --target <target> --keep-boot` deletes the intermediate build files and
  keeps everything needed to boot the board.
* Block diagrams of the Zynq UltraScale+ and Versal designs on the
  [Description](description.md#block-diagrams) page, with a how-to for using and testing the
  ports under Yocto and PetaLinux.

Fixes:

* **Ports linked up but passed no traffic in the Yocto image.** The `pcs-unisolate` service
  identified the SoC from the root `compatible` of the device tree only; the Yocto device tree
  names just the board there, so the service printed `unrecognised SoC; not clearing PCS
  isolate`, the PCS/PMA cores stayed isolated and the ports never obtained an address. The
  service now also checks the `compatible` of the GEM nodes. The same script is used in the
  PetaLinux image.
* **UltraZed-EV: GEM3 described as the carrier's RJ45 port.** The board device tree described
  `gem3` as the carrier's onboard Ethernet port (RGMII PHY at address 0, MAC address from the
  EEPROM). In this design GEM3 drives FMC port 3, and that description was merged into the same
  node as the FMC port 3 settings. It has been removed from the PetaLinux and Yocto BSPs.
* **Yocto: hostname and kernel arguments not applied.** The images booted with the default
  hostname `amd-edf` and without the design's kernel arguments (`cma=…`, and
  `clk_ignore_unused` on the VCK190). The EDF boot flow ignores the usual `APPEND` variable;
  the arguments are now added to the U-Boot boot script (Zynq UltraScale+) or the systemd-boot
  entry (VCK190), and the hostname is now `<board>-psgem-2025-2`.
* **Yocto, VCK190: VADJ and PHY reset.** Like the PetaLinux image, the Yocto image's U-Boot now
  switches on the FMC VADJ rail (1.5 V) and then pulses the Ethernet FMC Max PHY resets before
  it boots Linux. Without this, VADJ could still be off when Linux probed the ports, and the
  PHYs never received a valid reset.
* **Yocto, UltraZed-EV: device-tree build error.** The board device tree used kernel
  `dt-bindings` header includes that the EDF device-tree recipe cannot resolve; the few
  constants that it needs are now defined in the file.
* **`./build.sh package` kept an out-of-date zip.** `package` skipped a target whenever its
  zip existed, so after a rebuild the zip still held the old image. It now rewrites a zip that
  is older than the files it contains.
* Documentation: U-Boot does not clear the PCS/PMA ISOLATE bit (the standalone application and
  the Linux `pcs-unisolate` service do), and the description of the lwIP patches and of the
  Versal PHY reset has been corrected.

## 2025.2 (June 2026)

* First release of the Ethernet FMC Max PS GEM reference designs, for
  Vivado/Vitis/PetaLinux 2025.2.
