# Host IPMI watchdog

The X570D4U uses `x86-power-control` for host state. Its power transitions do
not start the generic OpenPOWER watchdog units, and the generic watchdog's
timeout target has no action on this board.

The image starts `phosphor-watchdog.service` at BMC boot. It exposes the
standard host watchdog at `/xyz/openbmc_project/watchdog/host0` for IPMI Set,
Get and Reset Watchdog Timer commands. Starting the service does not arm the
timer. A host driver or another IPMI client must configure and start it.
After expiration the daemon remains available and the timer stays disarmed
until a client starts it again.

| IPMI timeout action | `x86-power-control` request |
| --- | --- |
| Hard reset | `ForceWarmReboot`, which pulses the host reset GPIO |
| Power off | `Off` on the chassis, which forces power off |
| Power cycle | `PowerCycle` on the chassis, which uses the board's power cycle sequence |
| No action | No systemd unit is started |

The BMC service owns a single timer. On Proxmox, keep `watchdog-mux` as its
sole OS owner. Check which device it owns before switching the host from
`softdog` to `ipmi_watchdog`. The current Proxmox multiplexer sets a 10-second
timeout on the watchdog device. The BMC enforces a 60-second minimum for all
watchdog uses, including BIOS. Check the BMC timer with
`ipmitool mc watchdog get` from the host when `ipmitool` is installed. Once a
client owns the timer, verify that its time remaining rises periodically and
that the selected action matches the recovery policy.

Observe the first host boot after deploying the BMC image. BIOS can arm the
timer through IPMI before the OS takes over. Use a maintenance window to verify
real timeout and reset. The watchdog journal records the expired timer use,
action and systemd unit. Confirm the host actually rebooted; the journal alone
does not prove the GPIO reset worked.
Disarm the timer before planned out-of-band resets so a pending countdown
cannot interrupt the next boot. Recheck watchdog status after BMC or host
maintenance; the BMC does not persist timer configuration across its reboot.

## Check keepalives on the BMC

Run over SSH on the BMC:

```sh
service=xyz.openbmc_project.Watchdog
path=/xyz/openbmc_project/watchdog/host0
interface=xyz.openbmc_project.State.Watchdog

busctl get-property "$service" "$path" "$interface" \
    Enabled ExpireAction CurrentTimerUse Interval

while true; do
    busctl get-property "$service" "$path" "$interface" TimeRemaining || break
    sleep 0.3
done
```

With Proxmox feeding the timer, expect `Enabled` to be `true`, timer use to
be `SMSOS`, and the remaining time to repeatedly rise toward `60000` ms.
A disarmed timer reads `0`. Press Ctrl+C to stop the loop.

## Test missed keepalives from Proxmox

On Proxmox, check that `watchdog-mux` is using the `IPMI` driver. Save the
current boot ID to compare after the reset:

```sh
sudo journalctl --boot --unit=watchdog-mux.service --no-pager
cat /proc/sys/kernel/random/boot_id
```

Keep the BMC countdown visible in a separate terminal. With guests stopped
or migrated and backup or replication jobs finished, flush ZFS and pause the
mux. If the BMC shows `HardReset`, letting the timer expire resets the host.

```sh
pid=$(systemctl show --property=MainPID --value watchdog-mux.service)
if [ "$pid" -gt 1 ] && sudo zpool sync; then
    sudo kill --signal STOP "$pid"
fi
```

To cancel before the countdown reaches zero, run this in the same Proxmox
shell:

```sh
sudo kill --signal CONT "$pid"
```

A clean service stop disarms the watchdog. `SIGSTOP` leaves it armed and
halts keepalives. After an expiry test, check the BMC log and compare the
Proxmox boot ID with the saved value. The ID should change and the BMC
countdown should resume refreshing.

```sh
# On the BMC
journalctl --unit=phosphor-watchdog.service --no-pager --lines=30

# On Proxmox after it comes back
cat /proc/sys/kernel/random/boot_id
```
