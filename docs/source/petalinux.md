# PetaLinux

PetaLinux can be built for these reference designs by using the Makefile in the `PetaLinux` directory
of the repository.

## Requirements

To build the PetaLinux projects, you will need a physical or virtual machine running one of the 
[supported Linux distributions] as well as the Vitis Core Development Kit installed.

```{attention} You cannot build the PetaLinux projects in the Windows operating system. Windows
users are advised to use a Linux virtual machine to build the PetaLinux projects.
```

## How to build

1. From a command terminal, clone the Git repository and `cd` into it.
   ```
   git clone --recursive https://github.com/fpgadeveloper/ethernet-fmc-max-ps-gem.git
   cd ethernet-fmc-max-ps-gem
   ```
2. Launch PetaLinux by sourcing the `settings.sh` bash script, eg:
   ```
   source <path-to-petalinux-install>/2025.2/settings.sh
   ```
3. Launch Vivado by sourcing the `settings64.sh` bash script, eg:
   ```
   source <path-to-xilinx-tools>/2025.2/Vivado/settings64.sh
   ```
4. Build the Vivado and PetaLinux project for your specific target platform by running the following
   commands and replacing `<target>` with one of the target labels listed in the target designs table
   in the build instructions.
   ```
   cd PetaLinux
   make petalinux TARGET=<target>
   ```
   
The last command will launch the build process for the corresponding Vivado project if that project
has not already been built and it's hardware exported.

## Boot from SD card

### Prepare the SD card

Once the build process is complete, you must prepare the SD card for booting PetaLinux.
All targets in this repository (Zynq UltraScale+ and Versal) are configured to boot from
the SD card and use it for the root filesystem.

1. The SD card must first be prepared with two partitions: one for the boot files and another 
   for the root file system.

   * Plug the SD card into your computer and find it's device name using the `dmesg` command.
     The SD card should be found at the end of the log, and it's device name should be something
     like `/dev/sdX`, where `X` is a letter such as a,b,c,d, etc. Note that you should replace
     the `X` in the following instructions.
     
```{warning} Do not continue these steps until you are certain that you have found the correct
device name for the SD card. If you use the wrong device name in the following steps, you risk
losing data on one of your hard drives.
```
   * Run `fdisk` by typing the command `sudo fdisk /dev/sdX`
   * Make the `boot` partition: typing `n` to create a new partition, then type `p` to make 
     it primary, then use the default partition number and first sector. For the last sector, type 
     `+1G` to allocate 1GB to this partition.
   * Make the `boot` partition bootable by typing `a`
   * Make the `root` partition: typing `n` to create a new partition, then type `p` to make 
     it primary, then use the default partition number, first sector and last sector.
   * Save the partition table by typing `w`
   * Format the `boot` partition (FAT32) by typing `sudo mkfs.vfat -F 32 -n boot /dev/sdX1`
   * Format the `root` partition (ext4) by typing `sudo mkfs.ext4 -L root /dev/sdX2`

2. Copy the following files to the `boot` partition of the SD card:
   Assuming the `boot` partition was mounted to `/media/user/boot`, follow these instructions:
   ```
   $ cd /media/user/boot/
   $ sudo cp /<petalinux-project>/images/linux/BOOT.BIN .
   $ sudo cp /<petalinux-project>/images/linux/boot.scr .
   $ sudo cp /<petalinux-project>/images/linux/image.ub .
   ```

3. Create the root file system by extracting the `rootfs.tar.gz` file to the `root` partition.
   Assuming the `root` partition was mounted to `/media/user/root`, follow these instructions:
   ```
   $ cd /media/user/root/
   $ sudo cp /<petalinux-project>/images/linux/rootfs.tar.gz .
   $ sudo tar xvf rootfs.tar.gz -C .
   $ sync
   ```
   
   Once the `sync` command returns, you will be able to eject the SD card from the machine.

### Boot PetaLinux

1. Plug the SD card into your target board.
2. Ensure that the target board is configured to boot from SD card:
   * **VCK190:** DIP switch SW1 is set to 1000 (1=ON,2=OFF,3=OFF,4=OFF)
   * **UltraZed-EV:** DIP switch SW2 (on the SoM) is set to 1000 (1=ON,2=OFF,3=OFF,4=OFF)
   * **ZCU102, ZCU106, ZCU111:** DIP switch SW6 must be set to 1000 (1=ON,2=OFF,3=OFF,4=OFF)
3. Connect the [Ethernet FMC Max] to the FMC connector of the target board.
4. Connect the USB-UART to your PC and then open a UART terminal set to 115200 baud and the 
   comport that corresponds to your target board.
5. Connect and power your hardware.

## Boot via JTAG

```{tip} You need to install the cable drivers before being able to boot via JTAG.
Note that the Vitis installer does not automatically install the cable drivers, it must be done separately.
For instructions, read section 
[installing the cable drivers](https://docs.amd.com/r/en-US/ug973-vivado-release-notes-install-license/Installing-Cable-Drivers) 
from the Vivado release notes.
```

```{warning} If you boot these designs via JTAG, you must still
first prepare the SD card. The reason is because all of the designs in this repository are
configured to use the SD card to store the root filesystem. If you boot via JTAG without
preparing and connecting the SD card, the boot will hang at a message similar to this:
`Waiting for root device /dev/mmcblk0p2...`
```

### Setup hardware

1. Prepare the SD card according to the [instructions above](#prepare-the-sd-card)
   and plug it into the target board.
2. Ensure that the target board is configured to boot from JTAG:
   * **VCK190:** DIP switch SW1 is set to 1111 (1=ON,2=ON,3=ON,4=ON)
   * **UltraZed-EV:** DIP switch SW2 (on the SoM) is set to 1111 (1=ON,2=ON,3=ON,4=ON)
   * **ZCU102, ZCU106, ZCU111:** DIP switch SW6 must be set to 1111 (1=ON,2=ON,3=ON,4=ON)
3. Connect the [Ethernet FMC Max] to the FMC connector of the target board.
4. Connect the USB-UART to your PC and then open a UART terminal set to 115200 baud and the 
   comport that corresponds to your target board.
5. Connect and power your hardware.

### Boot PetaLinux

To boot PetaLinux on hardware via JTAG, use the following commands in a Linux command terminal:

1. Change current directory to the PetaLinux project directory for your target design:
   ```
   cd <project-dir>/PetaLinux/<target>
   ```
2. Download bitstream to the FPGA:
   ```
   petalinux-boot --jtag --kernel --fpga
   ```

An explanation of the above command is provided by the `petalinux-boot` command:
```none
For Zynq UltraScale+, it will download the bitstream, PMUFW and FSBL,
and then boot the kernel with help of linux-boot.elf to set kernel
start and dtb addresses.
```

## UART terminal

You will need to setup a terminal emulator to use the PetaLinux command line over the USB-UART connection.
Connect with a baud rate of 115200.

### In Windows

You will need to find the comport for the USB-UART in Windows Device Manager. As a terminal emulator, you
can use the open source and free [Putty](https://www.putty.org/).

### In Linux

In Linux, you can find the USB-UART device by running `dmesg | grep tty`. Typically, the device will be
`/dev/ttyUSB0` or it could be followed by a different number. To open a terminal emulator, you can use
the following command:

```
sudo screen /dev/ttyUSB0 115200
```

## Port configurations

In these designs the network interfaces map directly onto the Ethernet FMC Max ports: each
port is driven by the PS GEM of the same index, and the GEMs enumerate in order, so
interface `ethN` is port N. All four PHYs are on the single shared MDIO bus mastered by
GEM0; the device tree (`PetaLinux/bsp/ports-0123/.../port-config.dtsi` and
`ports-01xx/.../port-config.dtsi` for the VCK190) declares the PHY nodes under `gem0`'s
MDIO bus and cross-references them from the other GEMs via `phy-handle`. The `phy-mode`
is `gmii` — the GEMs connect to the PHYs through EMIO GMII and the PL PCS/PMA cores,
which handle the SGMII auto-negotiation in hardware.

The default interfaces table (`/etc/network/interfaces`) brings up `eth0` at
boot, so it is convenient to wire port 0 to a DHCP-enabled link before powering
the board.

### Zynq UltraScale+ designs (uzev, zcu102_hpc0, zcu106_hpc0, zcu111)

* `eth0`: Ethernet FMC Max Port 0 (GEM0, PHY @ MDIO addr 1)
* `eth1`: Ethernet FMC Max Port 1 (GEM1, PHY @ MDIO addr 3)
* `eth2`: Ethernet FMC Max Port 2 (GEM2, PHY @ MDIO addr 12)
* `eth3`: Ethernet FMC Max Port 3 (GEM3, PHY @ MDIO addr 15)

### Versal design (vck190_fmcp1)

The Versal PS has two GEM controllers, so this design supports ports 0 and 1 of the
Ethernet FMC Max (the PHYs of ports 2 and 3 are held in reset):

* `eth0`: Ethernet FMC Max Port 0 (GEM0, PHY @ MDIO addr 1)
* `eth1`: Ethernet FMC Max Port 1 (GEM1, PHY @ MDIO addr 3)

```{note} The development board's onboard Ethernet ports are normally driven by the
same PS GEMs through MIO. In these designs the GEMs are routed to the FMC through
EMIO instead, so the onboard Ethernet ports are not available.
```

## Example Usage

The examples below are from a ZCU106 PetaLinux session and are representative of all
targets in this repo.

### Log in

Log in with the username `petalinux`. On first boot you will be asked to choose a
password; the examples below assume that the `sudo` prefix is used where needed.

### Enable port

This example will bring up port 1 of the Ethernet FMC Max.

```
zcu106-psgem-sgmii-2025-2:~$ sudo ifconfig eth1 up
[   48.243899] macb ff0c0000.ethernet eth1: PHY [ff0b0000.ethernet-ffffffff:03] driver [TI DP83867] (irq=POLL)
[   48.254056] macb ff0c0000.ethernet eth1: configuring for phy/gmii link mode
[   52.345672] macb ff0c0000.ethernet eth1: Link is Up - 1Gbps/Full - flow control off
[   52.353437] IPv6: ADDRCONF(NETDEV_CHANGE): eth1: link becomes ready
```

Note in the first kernel message that the PHY is found on GEM0's MDIO bus
(`ff0b0000.ethernet`, address 0x03) even though the interface is driven by GEM1
(`ff0c0000.ethernet`) — all four PHYs share the one MDIO bus mastered by GEM0.

### Enable port with fixed IP address

This example sets a fixed IP address to a port.

```
zcu106-psgem-sgmii-2025-2:~$ sudo ifconfig eth1 192.168.2.30 up
```

### Enable port using DHCP

This example enables a port and obtains an IP address for the port via DHCP. Note that the
port must be connected to a DHCP enabled router.

```
zcu106-psgem-sgmii-2025-2:~$ sudo udhcpc -i eth1
udhcpc: started, v1.36.1
[   68.814013] macb ff0c0000.ethernet eth1: Link is Up - 1Gbps/Full - flow control off
[   68.822670] IPv6: ADDRCONF(NETDEV_CHANGE): eth1: link becomes ready
udhcpc: sending discover
udhcpc: sending select for 192.168.2.72
udhcpc: lease of 192.168.2.72 obtained, lease time 259200
/etc/udhcpc.d/50default: Adding DNS 192.168.2.1
```

### Check port status

In this example, we use the ``ifconfig`` command with no arguments to check the port status.
Trimmed excerpt — `eth1` is Ethernet FMC Max port 1 brought up at 192.168.2.30:

```
zcu106-psgem-sgmii-2025-2:~$ ifconfig
eth1      Link encap:Ethernet  HWaddr 00:0A:35:00:01:23
          inet addr:192.168.2.30  Bcast:192.168.2.255  Mask:255.255.255.0
          inet6 addr: fe80::20a:35ff:fe00:123/64 Scope:Link
          UP BROADCAST RUNNING  MTU:1500  Metric:1
          RX packets:38 errors:0 dropped:0 overruns:0 frame:0
          TX packets:26 errors:0 dropped:0 overruns:0 carrier:0
          collisions:0 txqueuelen:1000
          RX bytes:6033 (5.8 KiB)  TX bytes:3302 (3.2 KiB)

lo        Link encap:Local Loopback
          inet addr:127.0.0.1  Mask:255.0.0.0
          inet6 addr: ::1/128 Scope:Host
          UP LOOPBACK RUNNING  MTU:65536  Metric:1
          ...
```

We can also use ``ethtool`` to check the port status as follows (trimmed excerpt).

```
zcu106-psgem-sgmii-2025-2:~$ ethtool eth1
Settings for eth1:
        Supported ports: [ TP MII ]
        Supported link modes:   10baseT/Half 10baseT/Full
                                100baseT/Half 100baseT/Full
                                1000baseT/Half 1000baseT/Full
        Supports auto-negotiation: Yes
        Advertised link modes:  10baseT/Half 10baseT/Full
                                100baseT/Half 100baseT/Full
                                1000baseT/Half 1000baseT/Full
        Advertised auto-negotiation: Yes
        ...
        Speed: 1000Mb/s
        Duplex: Full
        Port: MII
        PHYAD: 3
        Transceiver: external
        Auto-negotiation: on
        Link detected: yes
```

### Ping link partner using specific port

In this example we ping the link partner at IP address 192.168.2.98 from interface eth1.

```
zcu106-psgem-sgmii-2025-2:~$ ping -I eth1 192.168.2.98
PING 192.168.2.98 (192.168.2.98): 56 data bytes
64 bytes from 192.168.2.98: seq=0 ttl=64 time=0.359 ms
64 bytes from 192.168.2.98: seq=1 ttl=64 time=0.199 ms
64 bytes from 192.168.2.98: seq=2 ttl=64 time=0.231 ms
64 bytes from 192.168.2.98: seq=3 ttl=64 time=0.161 ms
```


[Ethernet FMC Max]: https://docs.opsero.com/op080/datasheet/overview/
[supported Linux distributions]: https://docs.amd.com/r/en-US/ug1144-petalinux-tools-reference-guide/Setting-Up-Your-Environment
