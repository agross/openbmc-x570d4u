FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"
PACKAGECONFIG:append = " dynamic-sensors"
SRC_URI:append = " file://0001-preserve-temperature-names.patch file://0002-stable-sensor-ids.patch"
