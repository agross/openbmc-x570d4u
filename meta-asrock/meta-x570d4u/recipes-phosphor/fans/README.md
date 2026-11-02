# Fan control

`phosphor-pid-control` (`swampd`) owns normal fan control. Its zones and fan
PWM connectors are defined in the Entity Manager `x570d4u.json` configuration.
The board append enables missing-sensor handling and offline failsafe output.
Do not install a static swampd `config.json`: it bypasses Entity Manager's
sensor discovery and configuration reload path.

FAN1 runs at 75%, FAN4 at 40%, and FAN6 at 50%, each in an independent zone.
FAN2 and FAN3 share the CPU zone, using `CPU Temp` with these steps:

| CPU temperature | PWM |
| --- | --- |
| Below 60°C | 20% |
| 60–69°C | 50% |
| 70–79°C | 60% |
| 80–89°C | 70% |
| 90°C and above | 100% |

Fan controllers use feed-forward gain 1 and zero proportional/integral gains:
the zone setpoint is a duty percentage, not a target RPM. Tachometer sensors
remain inputs for failure reporting. Controlled fan tachs are read regardless
of host power state so fixed outputs remain available with the host off.
FAN5 is uninstalled and has no controller or PWM connector. Header mapping is
described in the [kernel recipe](../../recipes-kernel/linux/README.md).
These duties are specific to the original fan configuration.

Each zone has a 100% failsafe. Missing or unavailable CPU temperature drives
the CPU fans to full speed, including when the host is off. Entity Manager
configuration enables swampd to rebuild its controllers when sensors appear or
vanish.

`fan-full-speed.sh` complements swampd's `offline-failsafe` option, which writes
failsafe duty during orderly controller teardown. That code cannot run after
a crash or `SIGKILL`, and controllers may not exist yet when configuration
loading fails. The systemd drop-in therefore invokes the separate hook through
`ExecStartPre` and `ExecStopPost`: it sets every available ASPEED PWM channel to
255 (100%) before startup and after any exit, independently of swampd's policy
or process cleanup. During the initial migration, empty stepwise arrays prevented
controller creation; fans stayed at full speed until configuration was corrected.

Only swampd writes the normal policy; the old `fan-speeds.service` and
shell/YAML policy are removed. Its former `/etc/fan-policy.yaml` is no longer
used or preserved by `deploy.sh`. Change the board JSON and rebuild to change
the packaged policy.

Keep every `Stepwise.Reading` and `Stepwise.Output` entry as a JSON
floating-point literal (for example `50.0`). The pinned Entity Manager
exports writable numeric arrays as doubles but copies only JSON double values;
integer literals otherwise produce empty D-Bus arrays.

`x570d4u-fan-control.bb` retains `nct6796-enable.service` and the full-speed
safety hook. The recovery service manages I2C device `1-002d`, checking for the
`TSI0_TEMP` label every five seconds. If a host reboot leaves an unusable
device behind, it removes it and retries creation. This hardware recovery is
still required independently of swampd. Once temperature labels return, the
monitor uses `systemctl try-restart` to refresh a running `hwmontempsensor`:
device recreation invalidates its open sysfs handles even when paths stay the
same. Failed refresh requests are retried on the next monitor pass; a healthy,
unchanged device does not cause restarts. CPU temperature comes from D-Bus;
the former direct-hwmon `CPUTIN` fallback is replaced by failsafe output.

Before deploying a changed policy, verify the physical PWM mapping, normal
steps and fixed duties, missing/reappearing CPU sensor, host power cycle,
and full-speed output on service stop/restart. A build does not verify these
hardware behaviors.
