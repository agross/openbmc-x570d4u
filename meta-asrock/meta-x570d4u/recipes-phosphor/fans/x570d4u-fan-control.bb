SUMMARY = "X570D4U hardware monitor recovery and fan safety"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/Apache-2.0;md5=89aea4e17d99a7cacdbeed46a0096b10"
SRC_URI = "file://enable-temperature-sensors.sh file://fan-full-speed.sh file://nct6796-enable.service"
S = "${UNPACKDIR}"
inherit systemd
SYSTEMD_SERVICE:${PN} = "nct6796-enable.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"
RDEPENDS:${PN} += "phosphor-pid-control"
do_install() {
    install -d ${D}${sysconfdir} ${D}${libexecdir} ${D}${systemd_system_unitdir}
    install -m 0644 ${UNPACKDIR}/enable-temperature-sensors.sh ${D}${sysconfdir}/enable-temperature-sensors.sh
    install -m 0755 ${UNPACKDIR}/fan-full-speed.sh ${D}${libexecdir}/fan-full-speed
    install -m 0644 ${UNPACKDIR}/nct6796-enable.service ${D}${systemd_system_unitdir}/nct6796-enable.service
}
