FILESEXTRAPATHS:prepend := "${THISDIR}/host-interface:${THISDIR}/fan-mapping:${THISDIR}:"
SRC_URI:append:x570d4u = " file://0001-rndis-descriptors.patch file://0002-rndis-init.patch file://board.cfg file://0001-correct-primary-fan-tach-channels.patch file://0002-remap-fan-pwm-sysfs.patch"
