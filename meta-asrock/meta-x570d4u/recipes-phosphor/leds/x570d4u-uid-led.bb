SUMMARY = "X570D4U latched UID LED controller with physical state feedback"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/Apache-2.0;md5=89aea4e17d99a7cacdbeed46a0096b10"
SRC_URI = "file://uid-led.cpp file://50-x570d4u-uid.conf"
S = "${UNPACKDIR}"
DEPENDS = "systemd libgpiod"
RDEPENDS:${PN} += "phosphor-led-manager"
inherit pkgconfig

do_compile() {
    ${CXX} ${CXXFLAGS} ${LDFLAGS} ${UNPACKDIR}/uid-led.cpp -o x570d4u-uid-led $(pkg-config --cflags --libs libsystemd libgpiod)
}
do_install() {
    install -d ${D}${bindir} ${D}${systemd_system_unitdir}/xyz.openbmc_project.LED.GroupManager.service.d
    install -m 0755 x570d4u-uid-led ${D}${bindir}/x570d4u-uid-led
    install -m 0644 ${UNPACKDIR}/50-x570d4u-uid.conf ${D}${systemd_system_unitdir}/xyz.openbmc_project.LED.GroupManager.service.d/50-x570d4u-uid.conf
}
FILES:${PN} += "${systemd_system_unitdir}/xyz.openbmc_project.LED.GroupManager.service.d"
