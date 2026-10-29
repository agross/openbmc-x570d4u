# Kernel changes

`linux-aspeed_%.bbappend` applies board-specific configuration and patches.
The primary tachometer channels for FAN4, FAN5, and FAN6 are corrected to
zero-based channels 3, 4, and 5. The PWM sysfs mapping is corrected so FAN5
uses `pwm5` and FAN6 uses `pwm6`; earlier firmware used the reverse mapping.
RPM feedback and PWM control are separate mappings.

The host-interface patches support the BIOS-compatible RNDIS USB gadget.
`board.cfg` supplies the required kernel configuration. Kernel changes require
an image rebuild and BMC restart; changing runtime sensor configuration alone
cannot apply them.
