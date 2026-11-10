FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"

# x86-power-control does not run the OpenPOWER host startup targets. Keep the
# daemon available while letting IPMI configure and arm the timer explicitly.
SYSTEMD_ENVIRONMENT_FILE:${PN}:remove:x570d4u = "obmc/watchdog/poweron"
SYSTEMD_OVERRIDE:${PN}:remove:x570d4u = "poweron.conf:phosphor-watchdog@poweron.service.d/poweron.conf"
SYSTEMD_LINK:${PN}:x570d4u = ""
SYSTEMD_SERVICE:${PN}:x570d4u = " \
    phosphor-watchdog.service \
    phosphor-watchdog-host-reset.service \
    phosphor-watchdog-host-poweroff.service \
    phosphor-watchdog-host-powercycle.service \
"
