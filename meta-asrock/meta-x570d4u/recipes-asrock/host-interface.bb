FILESEXTRAPATHS:prepend := "${THISDIR}/host-interface/files:"

SUMMARY = "X570D4U BIOS host interface and inventory integration"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/Apache-2.0;md5=89aea4e17d99a7cacdbeed46a0096b10"

SRC_URI = "file://x570d4u-smbios-import.cpp file://x570d4u-smbios-import.service file://x570d4u-smbios-import.path file://kcs-access.cpp file://x570d4u-inventory-publisher.cpp file://x570d4u-inventory-publisher.service file://x570d4u-redfish-host-network file://x570d4u-redfish-host-network.service file://10-x570d4u-rndis.network"
S = "${UNPACKDIR}"

DEPENDS += "nlohmann-json phosphor-ipmi-host systemd"
RDEPENDS:${PN} += "systemd smbios-mdr"
FILES:${PN} += "${systemd_system_unitdir}/x570d4u-smbios-import.service ${libdir}/ipmid-providers/libx570d4u-ami-inventory.so ${libexecdir}/x570d4u-inventory-publisher ${nonarch_libdir}/systemd/network/10-x570d4u-rndis.network"

inherit systemd

SYSTEMD_SERVICE:${PN} = "x570d4u-smbios-import.path x570d4u-redfish-host-network.service x570d4u-inventory-publisher.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"

do_compile() {
    ${CXX} ${CXXFLAGS} ${LDFLAGS} -std=gnu++23 -shared -fPIC ${UNPACKDIR}/kcs-access.cpp -o x570d4u-ami-inventory.so -lsystemd
    ${CXX} ${CXXFLAGS} ${LDFLAGS} -std=gnu++23 ${UNPACKDIR}/x570d4u-smbios-import.cpp -o x570d4u-smbios-import -lsystemd
    ${CXX} ${CXXFLAGS} ${LDFLAGS} -std=gnu++23 ${UNPACKDIR}/x570d4u-inventory-publisher.cpp -o x570d4u-inventory-publisher -lsystemd
}

do_install() {
    install -d ${D}${libdir}/ipmid-providers ${D}${libexecdir} ${D}${systemd_system_unitdir}
    install -m 0755 x570d4u-ami-inventory.so ${D}${libdir}/ipmid-providers/libx570d4u-ami-inventory.so
    install -m 0755 x570d4u-inventory-publisher ${D}${libexecdir}/x570d4u-inventory-publisher
    install -m 0644 ${UNPACKDIR}/x570d4u-inventory-publisher.service ${D}${systemd_system_unitdir}/x570d4u-inventory-publisher.service
    install -m 0755 ${UNPACKDIR}/x570d4u-redfish-host-network ${D}${libexecdir}/x570d4u-redfish-host-network
    install -m 0644 ${UNPACKDIR}/x570d4u-redfish-host-network.service ${D}${systemd_system_unitdir}/x570d4u-redfish-host-network.service
    install -m 0755 x570d4u-smbios-import ${D}${libexecdir}/x570d4u-smbios-import
    install -m 0644 ${UNPACKDIR}/x570d4u-smbios-import.service ${UNPACKDIR}/x570d4u-smbios-import.path ${D}${systemd_system_unitdir}/
    install -d ${D}${nonarch_libdir}/systemd/network
    install -m 0644 ${UNPACKDIR}/10-x570d4u-rndis.network ${D}${nonarch_libdir}/systemd/network/10-x570d4u-rndis.network
}
