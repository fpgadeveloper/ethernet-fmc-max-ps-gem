# Yocto

The Yocto flow builds the Linux image with AMD's Embedded Development Framework (EDF), the
announced successor to PetaLinux. It is built with the same `build.py` runner as the other
flows and produces a Linux image that drives the Ethernet FMC Max ports through the PS GEMs, in
the same way as the PetaLinux image.

```{note}
For 2025.2 both the PetaLinux and Yocto flows are supported. From the next tool version
onward, the PetaLinux flow for this repository will be retired and Yocto will be the only
supported Linux flow.
```

The Yocto flow is supported for all of the targets in this repository:
{% for design in data.designs if design.yocto and design.publish %} `{{ design.label }}`{{ ", " if not loop.last else "." }} {% endfor %}

## Requirements

To build the Yocto images you need a physical or virtual machine running one of the
[supported Linux distributions], with:

* Vivado 2025.2 and Vitis 2025.2. The flow converts the Vivado XSA into a System Device Tree with
  the `sdtgen` tool, which ships with Vitis. PetaLinux Tools are **not** needed.
* [Google's repo tool](https://gerrit.googlesource.com/git-repo/) on your `PATH`.
* The usual Yocto host packages. On Ubuntu 22.04 / 24.04:
  ```
  sudo apt-get install repo gawk wget git diffstat unzip texinfo gcc \
      build-essential chrpath socat cpio python3 python3-pip python3-pexpect \
      xz-utils debianutils iputils-ping python3-git python3-jinja2 \
      python3-subunit zstd liblz4-tool file locales libacl1 bmap-tools
  ```
* Disk space: a Yocto workspace takes 40-60 GB per target.

```{attention}
You cannot build the Yocto images on Windows. Windows users can build the Vivado and
Vitis parts of the design on Windows and the Yocto image on a Linux machine or virtual machine.
```

## How to build

The build runner locates and sources the Vivado and Vitis settings itself, so there is no need
to source them by hand.

1. From a command terminal, clone the Git repository and `cd` into it:
   ```
   git clone https://github.com/fpgadeveloper/ethernet-fmc-max-ps-gem.git
   cd ethernet-fmc-max-ps-gem
   ```
2. Build the Yocto image for your target, replacing `<target>` with one of the target labels
   listed in the [build instructions](build_instructions.md#build-yocto):
   ```
   ./build.sh yocto --target <target>
   ```

This command also builds the Vivado project and exports the hardware (XSA) if that has not
already been done. The first build of a target downloads several GB of sources (`repo sync`)
and builds the whole image, so it takes a long time; later builds are incremental.

To gather the SD card image into a zip under `bootimages/`, run
`./build.sh package --target <target>` (or build with `./build.sh all --target <target>`,
which runs every stage the target supports and then packages them).

### Faster builds with the AMD sstate-cache

AMD publishes the 2025.2 sstate-cache and downloads mirror on the Embedded Design Tools
download page ("sstate-cache & Downloads - 2025.2"). Extract it, and write the absolute path
of the extracted directory, on a single line, into a file called `Yocto/offline.txt`. The
build then uses the mirror for every sub-directory it finds there (`aarch64/` for
Zynq UltraScale+, `cortexa72/` for Versal, `microblaze/` for the ZynqMP PMU firmware, and
`downloads/` for the source tarballs).

### Build outputs

The output products are gathered into `Yocto/<target>/images/linux/`:

| File | Description |
| --- | --- |
| `rootfs.wic.xz` | Complete SD card disk image (all partitions) — this is what you write to the card |
| `rootfs.wic.bmap` | Block map of the disk image, for fast writing with `bmaptool` |
| `BOOT.BIN` | Boot image: boot loaders, the PL bitstream and U-Boot |
| `boot.scr` | U-Boot boot script (Zynq UltraScale+ only; it is also inside the disk image) |
| `Image` | Linux kernel |
| `system.dtb` | Linux device tree |
| `u-boot.elf` | U-Boot (for JTAG boot) |
| `rootfs.tar.gz` | Root filesystem tarball |

The packaged zip, `bootimages/ethernet-fmc-max-ps-gem_<target>_yocto-2025-2.zip`, contains
`rootfs.wic.xz`, `rootfs.wic.bmap`, `BOOT.BIN`, on the VCK190 also `BOOTAA64.EFI`, and a
`readme.txt` with the SD card instructions below.

## Prepare the SD card

Unlike the PetaLinux flow, which produces separate boot files for a card that you partition
yourself, the Yocto flow produces a **complete SD card disk image** (`rootfs.wic.xz`). You
write that image to the card's raw device and then copy `BOOT.BIN` (and on the VCK190
`BOOTAA64.EFI`) onto the card's first partition.

```{important}
The disk image is slightly larger than 8 GiB, so use an SD card of 16 GB or larger.
```

```{warning}
Writing to a raw block device cannot be undone. Make absolutely sure that you have
identified the SD card's device node before running the commands below. If you use the wrong
device you can destroy the data on one of your hard drives.
```

1. Identify the SD card device. With the card **un**plugged, run `lsblk -o NAME,SIZE,RM,TYPE`,
   insert the card, and run it again. The new entry, typically `/dev/sdX` with `RM=1`
   (removable) and a size matching your card, is your card. Replace `sdX` with that device in
   the commands below.
2. Unmount any partitions that your desktop mounted automatically:
   ```
   for p in /dev/sdX?*; do sudo umount "$p" 2>/dev/null; done
   ```
3. Write the disk image to the raw device. With `bmaptool` (fast, writes only the used blocks):
   ```
   sudo bmaptool copy --bmap Yocto/<target>/images/linux/rootfs.wic.bmap \
                             Yocto/<target>/images/linux/rootfs.wic.xz \
                             /dev/sdX
   ```
   Or, without `bmaptool` (slower):
   ```
   xzcat Yocto/<target>/images/linux/rootfs.wic.xz \
       | sudo dd of=/dev/sdX bs=4M status=progress conv=fsync
   ```
4. **Copy `BOOT.BIN` onto the first partition.** The boot ROM loads `BOOT.BIN` from the first
   FAT partition of the card, but the disk image does not carry it there, so a card without this
   step does not boot:
   ```
   sudo partprobe /dev/sdX
   sudo mkdir -p /mnt/sd_p1
   sudo mount /dev/sdX1 /mnt/sd_p1
   sudo cp Yocto/<target>/images/linux/BOOT.BIN /mnt/sd_p1/BOOT.BIN
   ```
5. **VCK190 only: copy the systemd-boot loader.** On Versal, U-Boot starts Linux through the
   systemd-boot EFI loader, but the disk image leaves the `EFI/BOOT/` directory of the first
   partition empty, so U-Boot stops at its prompt. Copy `BOOTAA64.EFI` from the
   `vck190_fmcp1` Yocto zip into it:
   ```
   sudo mkdir -p /mnt/sd_p1/EFI/BOOT
   sudo cp BOOTAA64.EFI /mnt/sd_p1/EFI/BOOT/BOOTAA64.EFI
   ```
   If you built the image yourself, `./build.sh package --target vck190_fmcp1` puts
   `BOOTAA64.EFI` into the zip; it is also in the root filesystem at
   `usr/lib/systemd/boot/efi/systemd-bootaa64.efi`.
6. Unmount and eject the card so that all writes are flushed:
   ```
   sync
   sudo umount /mnt/sd_p1 && sudo rmdir /mnt/sd_p1
   sudo eject /dev/sdX
   ```

## Boot

1. Plug the SD card into the target board and set the boot mode switches to SD card boot. The
   switch settings are the same as for PetaLinux; see [Boot PetaLinux](petalinux.md#boot-petalinux)
   and the documentation of your board.
2. Connect the [Ethernet FMC Max] to the FMC connector used by your target (see the
   [target designs](build_instructions.md#target-designs) table).
3. Connect Ethernet cables from the ports that you want to use to your network or PC. The image
   requests an IP address by DHCP on every port that has a link, so a port connected to a
   network with a DHCP server gets an address without any configuration.
4. Connect the USB-UART to your PC and open a terminal emulator at 115200 baud, 8N1 (see
   [UART terminal](petalinux.md#uart-terminal)).
5. Power up the board.

### What you should see

On the Zynq UltraScale+ boards, U-Boot finds the four GEMs, all with their PHYs on GEM0's MDIO
bus (`mdio bus ff0b0000`), then runs the boot script from the SD card:

```
ZYNQ GEM: ff0b0000, mdio bus ff0b0000, phyaddr 1, interface gmii
ZYNQ GEM: ff0c0000, mdio bus ff0b0000, phyaddr 3, interface gmii
ZYNQ GEM: ff0d0000, mdio bus ff0b0000, phyaddr 12, interface gmii
ZYNQ GEM: ff0e0000, mdio bus ff0b0000, phyaddr 15, interface gmii
...
Found U-Boot script /boot.scr
Booting /Image
EFI stub: Booting Linux Kernel...
```

On the VCK190, U-Boot switches on VADJ and releases the PHY resets, then starts the
systemd-boot menu, which boots the **EDF Linux** entry after 5 seconds.

The kernel command line contains the design's arguments at the end (`cma=…`). On the ZCU106,
for example:

```
Kernel command line: earlycon console=ttyPS0,115200 clk_ignore_unused init_fatal_sh=1 root=/dev/mmcblk0p3 ro rootwait uio_pdrv_genirq.of_id=generic-uio cma=1536M
```

On the UltraZed-EV the root filesystem is on `/dev/mmcblk1p3` and the argument is `cma=1000M`.
On the VCK190 the command line is
`console=ttyAMA0 earlycon=pl011,mmio32,0xFF000000,115200n8 root=PARTUUID=<uuid> ro rootwait uio_pdrv_genirq.of_id=generic-uio clk_ignore_unused cma=1536M`.

During the boot, the Linux driver registers one interface per GEM with the fixed MAC addresses
of the design, and the `pcs-unisolate` service clears the ISOLATE bit of every PCS/PMA core
(ZCU106 shown; the VCK190 shows two GEMs and two cores):

```
macb ff0b0000.ethernet eth0: Cadence GEM rev 0x50070106 at 0xff0b0000 irq 45 (00:0a:35:00:01:22)
macb ff0c0000.ethernet eth1: Cadence GEM rev 0x50070106 at 0xff0c0000 irq 46 (00:0a:35:00:01:23)
macb ff0d0000.ethernet eth2: Cadence GEM rev 0x50070106 at 0xff0d0000 irq 47 (00:0a:35:00:01:24)
macb ff0e0000.ethernet eth3: Cadence GEM rev 0x50070106 at 0xff0e0000 irq 48 (00:0a:35:00:01:25)
...
macb ff0b0000.ethernet end3: PHY [ff0b0000.ethernet-ffffffff:01] driver [TI DP83867] (irq=POLL)
pcs-unisolate: PCS core @ MDIO 8: reg0 0x1540 -> 0x1140 (ISOLATE cleared)
pcs-unisolate: PCS core @ MDIO 9: reg0 0x1540 -> 0x1140 (ISOLATE cleared)
pcs-unisolate: PCS core @ MDIO 10: reg0 0x1540 -> 0x1140 (ISOLATE cleared)
pcs-unisolate: PCS core @ MDIO 11: reg0 0x1540 -> 0x1140 (ISOLATE cleared)
pcs-unisolate: done (4 core(s) cleared via phytool)
...
macb ff0b0000.ethernet end3: Link is Up - 1Gbps/Full - flow control off
```

The boot ends at a login prompt that shows the hostname of the image:

```
zcu106-psgem-2025-2 login:
```

The hostname is `<board>-psgem-2025-2`: `uzev-psgem-2025-2`, `zcu102-psgem-2025-2`,
`zcu106-psgem-2025-2`, `zcu111-psgem-2025-2` or `vck190-psgem-2025-2`.

### Log in

Log in as **`amd-edf`**. On the first login you are asked to choose a new password. The
`amd-edf` user can run commands as root with `sudo` (using that password).

```{note}
AMD EDF prints a notice at login that it is a reference distribution for development and
testing. For a product, create your own Yocto distribution.
```

## Interface names and FMC ports

The EDF root filesystem uses systemd's predictable interface names (`end0`, `end1`, …) instead
of `eth0`, `eth1`, …, and on the Zynq UltraScale+ boards the numbering runs in the **opposite**
order to the GEM and FMC port numbers:

| FMC port | GEM (base address) | MAC address | PHY MDIO addr | Zynq UltraScale+ interface | VCK190 interface |
|----------|--------------------|-------------|---------------|----------------------------|------------------|
| 0 | GEM0 | `00:0a:35:00:01:22` | 1  | `end3` (`ff0b0000.ethernet`) | `end0` (`ff0c0000.ethernet`) |
| 1 | GEM1 | `00:0a:35:00:01:23` | 3  | `end2` (`ff0c0000.ethernet`) | `end1` (`ff0d0000.ethernet`) |
| 2 | GEM2 | `00:0a:35:00:01:24` | 12 | `end1` (`ff0d0000.ethernet`) | not used |
| 3 | GEM3 | `00:0a:35:00:01:25` | 15 | `end0` (`ff0e0000.ethernet`) | not used |

On the UltraZed-EV, GEM3 is also FMC port 3 (`end0`): in this design all four GEMs are routed
to the Ethernet FMC Max, so the carrier's own RJ45 port is not available.

You can always confirm the mapping on your board from the MAC address (`ip -br link`) or from
the GEM that each interface belongs to:

```
zcu106-psgem-2025-2:~$ ls -l /sys/class/net/*/device
```

The MAC addresses are fixed in the device tree (`port-config.dtsi`). If you connect more than
one board to the same network, change them so that they are unique.

## Use and test the ports

The examples below were taken on a ZCU106 with port 0 (`end3`) and port 3 (`end0`) connected
to a network with a DHCP server. Substitute the interface names for your board and ports.

### Check the link and the address

```
zcu106-psgem-2025-2:~$ ip -br link | grep ^e
end3             UP             00:0a:35:00:01:22 <BROADCAST,MULTICAST,UP,LOWER_UP>
end2             DOWN           00:0a:35:00:01:23 <NO-CARRIER,BROADCAST,MULTICAST,UP>
end1             DOWN           00:0a:35:00:01:24 <NO-CARRIER,BROADCAST,MULTICAST,UP>
end0             UP             00:0a:35:00:01:25 <BROADCAST,MULTICAST,UP,LOWER_UP>
zcu106-psgem-2025-2:~$ ip -br addr | grep ^e
end3             UP             192.168.1.62/24 metric 10 fe80::20a:35ff:fe00:122/64
end2             DOWN
end1             DOWN
end0             UP             192.168.2.136/24 metric 10 fe80::20a:35ff:fe00:125/64
```

Ports without a cable show `NO-CARRIER`. Use `ethtool` to see the negotiated speed:

```
zcu106-psgem-2025-2:~$ sudo ethtool end3 | grep -E 'Speed|Link detected'
	Speed: 1000Mb/s
	Link detected: yes
```

To give a port a fixed address instead (for example when it is connected directly to a PC):

```
zcu106-psgem-2025-2:~$ sudo ip addr add 192.168.10.10/24 dev end0
zcu106-psgem-2025-2:~$ sudo ip link set end0 up
```

```{tip}
Put each port on its own IP subnet. If two ports have addresses on the same subnet, Linux
may send the traffic for both out of one port.
```

### Check the PCS/PMA cores and the PHYs

The `pcs-unisolate` service must have run for the ports to pass traffic:

```
zcu106-psgem-2025-2:~$ systemctl is-active pcs-unisolate
active
zcu106-psgem-2025-2:~$ dmesg | grep pcs-unisolate
```

`phytool` reads the registers of any device on GEM0's MDIO bus, through GEM0's interface
(`end3` on the Zynq UltraScale+ boards, `end0` on the VCK190). The PCS/PMA cores are at MDIO
addresses 8 + port. Control register 0 should read `0x1140` (auto-negotiation enabled,
ISOLATE bit 10 clear); `0x1540` means that the core is still isolated:

```
zcu106-psgem-2025-2:~$ for a in 8 9 10 11; do sudo phytool read end3/$a/0; done
0x1140
0x1140
0x1140
0x1140
```

The DP83867 PHYs are at MDIO addresses 1, 3, 12 and 15 (ports 0 to 3). `phytool print` decodes
their standard registers, for example for the PHY of port 3:

```
zcu106-psgem-2025-2:~$ sudo phytool print end3/15
```

### Test the ports with ping

```
zcu106-psgem-2025-2:~$ ping -c 5 -I end3 192.168.1.100
...
5 packets transmitted, 5 received, 0% packet loss, time 4084ms
```

### Test the ports with iperf3

Run an `iperf3` server on a PC that is reachable through the port under test:

```
iperf3 -s
```

On the board, measure both directions on one port; `--bind-dev` makes sure that the traffic
uses that port (with older `iperf3` versions, use `-B <port's IP address>` instead):

```
zcu106-psgem-2025-2:~$ iperf3 -c 192.168.1.100 --bind-dev end3 -t 10
...
[  5]   0.00-10.01  sec  1.10 GBytes   943 Mbits/sec    0            sender
[  5]   0.00-10.03  sec  1.10 GBytes   941 Mbits/sec                  receiver
zcu106-psgem-2025-2:~$ iperf3 -c 192.168.1.100 --bind-dev end3 -t 10 -R
...
[  5]   0.00-10.00  sec  1.09 GBytes   936 Mbits/sec    0            sender
[  5]   0.00-10.00  sec  1.09 GBytes   934 Mbits/sec                  receiver
```

With a 1000BASE-T link partner, expect about **940 Mbit/s** from the board to the PC and about
**935 Mbit/s** from the PC to the board, on every port, with no errors in the interface
statistics (`ip -s link show end3`). These rates apply to all of the targets, Zynq
UltraScale+ and Versal alike. A rate far below that points to the link partner, the cabling or a
port that negotiated 100 Mbit/s (check with `ethtool`).

## How the image is put together

The per-target configuration of the Yocto flow lives under `Yocto/bsp/`:

* **`<board>/conf/local.conf.append`** sets the hostname and the extra kernel arguments of the
  board (`BSP_EXTRA_BOOTARGS`, the `cma=` size, and `clk_ignore_unused` on the VCK190).
  The EDF boot flow does not read the usual `APPEND` variable, so these arguments are added
  to the U-Boot boot script (`boot.scr`, Zynq UltraScale+) or to the systemd-boot entry
  (VCK190) by the bbappends in `meta-user`.
* **`port-configs/ports-0123/` and `port-configs/ports-01xx/`** are the port-config overlays:
  `ports-0123` for the four-port Zynq UltraScale+ targets and `ports-01xx` for the two-port
  VCK190. Their `port-config.dtsi` adds the MAC address, `phy-handle`, MDIO bus and
  `phy-mode = "gmii"` of each GEM (the external PHYs are not in the XSA). All four DP83867
  PHYs are described on the MDIO bus of GEM0. Each overlay also contains the
  `pcs-unisolate` recipe (see
  [PCS/PMA ISOLATE bit](advanced.md#pcspma-isolate-bit-and-the-pcs-unisolate-service)).
* **`<board>/meta-user/recipes-bsp/device-tree/files/system-user.dtsi`** contains board fixes:
  the UART numbering on the ZCU102, ZCU106 and ZCU111 (so that the console on UART0 is
  `ttyPS0`), and the UltraZed-EV peripherals (I2C, clocks, eMMC, SD, SATA/USB3 GT clocks).
* **`<board>/meta-user/recipes-kernel/linux/linux-xlnx/bsp.cfg`** enables the TI DP83867 and
  Xilinx PHY drivers.
* **`<board>/meta-user/recipes-core/images/edf-linux-disk-image.bbappend`** adds the
  test tools to the root filesystem: `ethtool`, `iperf3`, `phytool`, `mtd-utils`,
  `can-utils`, `nfs-utils`, `pciutils` and the `pcs-unisolate` service.
* **VCK190 only:** `vck190/meta-user/recipes-bsp/u-boot/` replaces the U-Boot boot command so
  that, like the PetaLinux image, U-Boot switches the FMC VADJ rail on at 1.5 V (IR38164
  regulator, over I2C) and then pulses the Ethernet FMC Max PHY resets (PMC GPIO bank 3, EMIO)
  before it boots Linux. The PHY reset signals are FMC I/Os, so they have no effect until VADJ
  is on.

See the `Yocto/README.md` file of the repository for a description of the build scripts.

[Ethernet FMC Max]: https://docs.opsero.com/op080/datasheet/overview/
[supported Linux distributions]: https://docs.amd.com/r/en-US/ug1144-petalinux-tools-reference-guide/Setting-Up-Your-Environment
