SUMMARY = "X570D4U temperature driver and local fan policy"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/Apache-2.0;md5=89aea4e17d99a7cacdbeed46a0096b10"
SRC_URI = "file://enable-temperature-sensors.sh file://fan-policy.yaml file://fan-policy.awk file://fan-controller.sh file://nct6796-enable.service file://fan-speeds.service"
S = "${UNPACKDIR}"
inherit systemd
SYSTEMD_SERVICE:${PN} = "nct6796-enable.service fan-speeds.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"
RDEPENDS:${PN} += "bash"
do_install() {
    install -d ${D}${sysconfdir} ${D}${systemd_system_unitdir}
    for script in enable-temperature-sensors.sh fan-policy.yaml fan-policy.awk fan-controller.sh; do
        install -m 0644 ${UNPACKDIR}/$script ${D}${sysconfdir}/$script
    done
    for service in nct6796-enable.service fan-speeds.service; do
        install -m 0644 ${UNPACKDIR}/$service ${D}${systemd_system_unitdir}/$service
    done
}
CONFFILES:${PN} += "${sysconfdir}/fan-policy.yaml"
FILES:${PN} += "${systemd_system_unitdir}/fan-speeds.service"
