#
# pcs-unisolate -- boot-time workaround for the PL PCS/PMA ISOLATE bit.
#
# Clears the IEEE ISOLATE bit (control reg 0, bit 10) on the PL 1G/2.5G Ethernet
# PCS/PMA (PG047, GEM mode) cores so the SGMII datapath passes traffic under
# Linux. Required until the kernel gains an external-PCS owner (Sean Anderson's
# "pcs-xilinx" driver + Cadence-macb "pcs-handle" support, not merged as of
# linux 6.12 / 2025.2). See docs/source/advanced.md -- remove this recipe when
# the device tree can describe the PCS instead.
#
SUMMARY = "Clear the PL PCS/PMA ISOLATE bit at boot (Ethernet FMC Max)"
SECTION = "PETALINUX/apps"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://pcs-unisolate.sh \
           file://pcs-unisolate.service \
          "

S = "${WORKDIR}"

inherit systemd

SYSTEMD_SERVICE:${PN} = "pcs-unisolate.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"

RDEPENDS:${PN} = "busybox"

do_install() {
	install -d ${D}${sbindir}
	install -m 0755 ${S}/pcs-unisolate.sh ${D}${sbindir}/pcs-unisolate.sh

	install -d ${D}${systemd_system_unitdir}
	install -m 0644 ${S}/pcs-unisolate.service ${D}${systemd_system_unitdir}/pcs-unisolate.service
}

FILES:${PN} += "${systemd_system_unitdir}/pcs-unisolate.service"
