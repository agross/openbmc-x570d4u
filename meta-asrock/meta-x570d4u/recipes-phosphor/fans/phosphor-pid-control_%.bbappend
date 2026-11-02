FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"
SRC_URI += "file://x570d4u.conf"
PACKAGECONFIG:append = " offline-failsafe handle-missing-object-paths"

do_install:append() {
    install -d ${D}${systemd_system_unitdir}/phosphor-pid-control.service.d
    install -m 0644 ${UNPACKDIR}/x570d4u.conf ${D}${systemd_system_unitdir}/phosphor-pid-control.service.d/x570d4u.conf
    sed -i 's|/usr/libexec/fan-full-speed|${libexecdir}/fan-full-speed|g' ${D}${systemd_system_unitdir}/phosphor-pid-control.service.d/x570d4u.conf
}
FILES:${PN}:append = " ${systemd_system_unitdir}/phosphor-pid-control.service.d"
