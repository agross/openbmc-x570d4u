FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"
SRC_URI:append = " file://0001-nct6796-schema.patch file://x570d4u.json"
do_install:append() {
    install -m 0644 ${UNPACKDIR}/x570d4u.json ${D}${datadir}/entity-manager/configurations/asrock/x570d4u.json
}
