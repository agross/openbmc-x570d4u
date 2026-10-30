# X570D4U-2L2T fan PWM mapping

The stock firmware's board project configuration names PWM channel `0x05` for
FAN5 and `0x04` for FAN6. FAN1–FAN4 use `0x00`–`0x03` respectively. Source:
[stock X570D4U-2L2T_K5.PRJ, fan table](rootfs-partial-macos/info/X570D4U-2L2T_K5.PRJ#L916-L936).

The upstream Linux X570D4U board device tree independently labels `fan@4`
with `reg = <0x04>` as FAN6 and `fan@5` with `reg = <0x05>` as FAN5:
[Linux board device tree](https://github.com/torvalds/linux/blob/master/arch/arm/boot/dts/aspeed/aspeed-bmc-asrock-x570d4u.dts#L331-L340).
The ASPEED driver reads each fan node's `reg` as the PWM port and creates that
port ([driver](https://github.com/torvalds/linux/blob/master/drivers/hwmon/aspeed-pwm-tacho.c#L858-L871)).
Its sysfs attributes are one-based: `pwm5` selects port 4 and `pwm6` selects
port 5 ([driver attributes](https://github.com/torvalds/linux/blob/master/drivers/hwmon/aspeed-pwm-tacho.c#L649-L654)).
Thus both board descriptions support **FAN6 → pwm5** and **FAN5 → pwm6**.

PWM outputs and tachometer inputs are distinct. The local kernel patch changes
only the `aspeed,fan-tach-ch` values for the 6-pin headers, not their `reg` PWM
ports ([patch](../../meta-asrock/meta-x570d4u/recipes-kernel/linux/linux-aspeed/0001-correct-primary-fan-tach-channels.patch)).
The [monitoring-r3 record](../../artifacts/monitoring-r3-bmc-verification.txt#L60-L67)
shows pwm5 set to 128 and pwm6 to 255, but that snapshot alone does not prove
which physical fan responds to either output.

The sources do not explain why the board designer assigned the headers in this
order. On 2026-10-29, a controlled live test confirmed the physical response:
the owner observed FAN1 through FAN4 respond to pwm1 through pwm4, FAN6 to
pwm5, and FAN5 to pwm6. Each channel was tested individually and restored.
The local firmware patch remaps the two sysfs names for future images; this
test used the original, unremapped firmware.
