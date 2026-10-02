# Build instructions

## Source code

The source code for the reference designs is managed on this Github repository:

* [https://github.com/fpgadeveloper/ethernet-fmc-max-ps-gem](https://github.com/fpgadeveloper/ethernet-fmc-max-ps-gem)

To get the code, you can follow the link and use the **Download ZIP** option, or you can clone it
using this command:
```
git clone https://github.com/fpgadeveloper/ethernet-fmc-max-ps-gem.git
```

## License requirements

Some of the designs in this repository target dev boards for which a license is required to generate a bitstream. 
Others can be built with the Vivado ML Standard Edition **without a license**. The table of target designs in the 
following section contains a column specifying which designs require a license, and which can be built without a 
license.

Additionally, some designs use IP cores that are licensed separately from the Vivado edition itself (for example: TEMAC, XXV Ethernet, HDMI). The **IP License** column in the tables below indicates the designs that require such a license to generate a bitstream; evaluation licenses are generally available from AMD for testing.

## Target designs

This repo contains several designs that target the various supported development boards and their
FMC connectors. The table below lists the target design name, the Ethernet ports supported by the design and 
the FMC connector on which to connect the mezzanine card.

{% for group in data.groups %}
    {% set designs_in_group = [] %}
    {% for design in data.designs %}
        {% if design.group == group.label and design.publish %}
            {% set _ = designs_in_group.append(design.label) %}
        {% endif %}
    {% endfor %}
    {% if designs_in_group | length > 0 %}
### {{ group.name }} designs

| Target board        | Target design     | Ports   | FMC Slot    | Standalone<br> Echo Server | PetaLinux | Yocto | Vivado<br> Edition | IP<br>License |
|---------------------|-------------------|---------|-------------|-----|-----|-----|-----|-----|
{% for design in data.designs %}{% if design.group == group.label and design.publish %}| [{{ design.board }}]({{ design.link }}) | `{{ design.label }}` | {{ design.lanes | length }}x | {{ design.connector }} | {% if design.baremetal %} ✅ {% else %} ❌ {% endif %} | {% if design.petalinux %} ✅ {% else %} ❌ {% endif %} | {% if design.yocto %} ✅ {% else %} ❌ {% endif %} | {{ "Enterprise" if design.license else "Standard 🆓" }} | {{ "Required" if design.ip_license else "-" }} |
{% endif %}{% endfor %}
{% endif %}
{% endfor %}

Notes:

1. The Vivado Edition column indicates which designs are supported by the Vivado *Standard* Edition, the
   FREE edition which can be used without a license. Vivado *Enterprise* Edition requires
   a license however a 30-day evaluation license is available from the AMD Xilinx Licensing site.
2. The VCK190 design supports ports 0 and 1 of the Ethernet FMC Max only, because the Versal PS
   has two GEM controllers. The PHYs of ports 2 and 3 are held in reset in that design.

## Cross-platform build runner

All builds are driven by the `build.py` runner at the root of the repository,
on **both Windows and Linux** — the build instructions are the same for the
two operating systems. Each command builds whatever it depends on
automatically, skips anything that is already built, and locates the AMD
tools itself, so there is no need to source the settings scripts beforehand.

On Linux and on Windows (git bash), commands are run with the `build.sh`
shim, which finds a suitable Python 3 automatically (including the
interpreter bundled with the AMD tools). Windows users who prefer not to
use git bash can run the same commands from Command Prompt or PowerShell
using `build.bat` instead — the commands and arguments are otherwise
identical, for example `build.bat xsa --target <target>`.

To see the available targets and the state of a build:

```
./build.sh list                       # list the targets and their attributes
./build.sh status --target <target>   # show the per-stage artifact state
./build.sh clean --target <target>    # delete a target's generated outputs
```

```{note}
The embedded Linux images (PetaLinux and Yocto) can only be built on a
native Linux machine; everything else builds on Windows too. On Windows, the
runner refuses the Linux-only stages up front and prints the exact command
to run on the Linux machine. For Versal targets on Windows, the runner also
verifies that the project path fits within the 260-character Windows path
limit before building, and explains the `subst` workaround if it does not.
```

```{attention}
The legacy `make` interface described in previous versions of
this documentation still works on Linux — each Makefile is now a thin
wrapper around `build.sh` — but it is deprecated and will be removed at the
next version update.
```

### Build Vivado project

This single command creates the Vivado project, generates the bitstream and
exports the hardware to an XSA file:

```
./build.sh xsa --target <target>
```

Valid targets are:
{% for design in data.designs if design.publish %} `{{ design.label }}`{{ ", " if not loop.last else "." }} {% endfor %}

If you want the Vivado project and block design without generating a
bitstream — for example, to explore or modify the design in the Vivado GUI —
run `./build.sh project --target <target>` instead, then open the project
from `Vivado/<target>/`.

### Build Vitis workspace

This creates the Vitis workspace and compiles the standalone application,
producing the baremetal boot file (`BOOT.BIN` or bit file, depending on the
device family). The Vivado XSA is built first if it does not already exist:

```
./build.sh standalone --target <target>
```

Valid targets for the standalone application are:
{% for design in data.designs if design.baremetal and design.publish %} `{{ design.label }}`{{ ", " if not loop.last else "." }} {% endfor %}

The workspace is created in `Vitis/<target>_workspace` and the boot files
are gathered in `Vitis/boot/<target>/`.

### Build PetaLinux

The PetaLinux build requires a native Linux machine (one of the [supported
Linux distributions]) with PetaLinux Tools 2025.2 installed. The runner
locates and sources the PetaLinux `settings.sh` itself, and builds the
Vivado XSA first if it does not already exist:

```
./build.sh petalinux --target <target>
```

Valid targets for PetaLinux are:
{% for design in data.designs if design.petalinux and design.publish %} `{{ design.label }}`{{ ", " if not loop.last else "." }} {% endfor %}

The output products are written to `PetaLinux/<target>/images/linux/`.

#### PetaLinux offline build

If you need to build the PetaLinux projects offline (without an internet
connection), you can follow these instructions.

1. Download the sstate-cache artefacts from the Xilinx downloads site (the
   same page where you downloaded PetaLinux tools). There are two of them:
   * aarch64 sstate-cache (for the ZynqMP and Versal designs)
   * Downloads (for all designs)
2. Extract the contents of those files to a single location on your hard
   drive, for this example we'll say `/home/user/petalinux-sstate`. That
   should leave you with the following directory structure:
   ```
   /home/user/petalinux-sstate
                             +---  aarch64
                             +---  downloads
   ```
3. Create a text file called `offline.txt` in the `PetaLinux` directory of
   the project repository. The file should contain a single line of text
   specifying the path where you extracted the sstate-cache files. In this
   example, the contents of the file would be:
   ```
   /home/user/petalinux-sstate
   ```
   It is important that the file contain only one line and that the path is
   written with NO TRAILING FORWARD SLASH.

The PetaLinux builds will then be configured for offline build.

### Build Yocto

The Yocto (AMD EDF) build also requires a native Linux machine (one of the
[supported Linux distributions]). It needs Vivado and Vitis 2025.2 (the flow
uses the `sdtgen` tool that ships with Vitis to turn the XSA into a System
Device Tree) and [Google's repo tool](https://gerrit.googlesource.com/git-repo/)
on your `PATH`; PetaLinux Tools are not needed. The runner sources the Vivado
and Vitis settings itself and builds the XSA first if it does not already
exist:

```
./build.sh yocto --target <target>
```

Valid targets for Yocto are:
{% for design in data.designs if design.yocto and design.publish %} `{{ design.label }}`{{ ", " if not loop.last else "." }} {% endfor %}

The output products are written to `Yocto/<target>/images/linux/`. The first
build of a target downloads several GB of sources and builds the image from
scratch, so it takes a long time and needs 40-60 GB of disk space; later builds
are incremental. See [Yocto](yocto.md) for the outputs, how to write them to an
SD card, and how to test the ports.

### Build everything

This builds everything that the target supports — the Vivado project and XSA,
the standalone application, the PetaLinux image and the Yocto image — and
gathers the boot images into `bootimages/*.zip`:

```
./build.sh all --target <target>
./build.sh all --target all      # every target in the repo
```

On Windows, `all` builds everything that the host can build and reports the
Linux-only stages as `BLOCKED` rather than failing.

The boot image zips are named `<project>_<target>_<flow>-2025-2.zip`, for
example `ethernet-fmc-max-ps-gem_zcu106_hpc0_yocto-2025-2.zip`:

| Zip | Contents |
|-----|----------|
| `…_standalone-2025-2.zip` | `BOOT.BIN` (and the `.bif` used to make it) for the lwIP echo server |
| `…_petalinux-2025-2.zip` | `boot/` (`BOOT.BIN`, `image.ub`, `boot.scr`) for the FAT32 partition, `root/rootfs.tar.gz` for the ext4 partition |
| `…_yocto-2025-2.zip` | `rootfs.wic.xz` + `rootfs.wic.bmap` (complete SD card image), `BOOT.BIN`, on Versal also `BOOTAA64.EFI`, and a `readme.txt` |

`./build.sh package --target <target>` gathers the zips again. When a stage has
been rebuilt since its zip was made, `package` rewrites the zip; a zip whose
artifacts are all older is left as it is.

### Free up disk space

After a successful build, the intermediate files (Vivado project, Vitis
workspace, PetaLinux and Yocto build trees) can be deleted while keeping
everything that is needed to boot the board:

```
./build.sh clean --target <target> --keep-boot
```

The target still reports as built afterwards (`./build.sh status --target
<target>`). Without `--keep-boot`, `clean` deletes all of the target's
generated outputs after asking for confirmation.

[supported Linux distributions]: https://docs.amd.com/r/en-US/ug1144-petalinux-tools-reference-guide/Setting-Up-Your-Environment
