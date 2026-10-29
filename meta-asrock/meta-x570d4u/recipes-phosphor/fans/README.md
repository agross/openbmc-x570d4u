# Fan control

`x570d4u-fan-control.bb` installs the policy, validator, controller, and two
systemd services. `nct6796-enable.service` manages I2C device `1-002d` and
checks for the `TSI0_TEMP` label every five seconds. If a host reboot leaves
an unusable device behind, it removes it and retries creation.

`fan-speeds.service` reads `/etc/fan-policy.yaml`. The packaged policy sets
FAN1 to 75%, FAN4 to 40%, and FAN6 to 50%; FAN5 is uninstalled and has no
policy entry. FAN2 and FAN3 use the CPU curve 50°C/20%, 60°C/50%, 70°C/60%,
80°C/70%, and 90°C/100%, interpolating between points and holding endpoint
values outside the range. These settings are specific to the original fan
configuration, not a universal board default.

The controller locates hwmon devices by name, tries `TSI0_TEMP` then `CPUTIN`,
and polls every five seconds. A missing or invalid temperature sets the
corresponding temperature-controlled fan to full speed. Invalid policy or
controller shutdown sets all available PWM channels to full speed. The policy
is a preserved configuration file; restart `fan-speeds.service` after editing.
The supported policy syntax is shown in the packaged YAML and checked by
`fan-policy.awk`. Header mapping is described in the
[kernel recipe](../../recipes-kernel/linux/README.md).
