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

## Serial over LAN

The X570D4U-2L2T host sees the AST2500 virtual UART as `ttyS0` at I/O
address `0x3f8`, IRQ 4. With the driver's default active-low polarity,
the host's IRQ counter stayed flat and a serial getty received no input or
showed a login prompt. Setting the host UART to polling made SOL work in
both directions, which isolated the failure to interrupt-driven operation.

The board device tree sets SIRQ4 active-high. After deploying the rebuilt
image and restarting the BMC, pressing Enter in SOL produced the host
login prompt and advanced the host IRQ 4 counter. The host stayed up.

This verifies interactive SOL. The effect on the observed host boot
slowdown with a serial kernel console still needs a timed boot test.
