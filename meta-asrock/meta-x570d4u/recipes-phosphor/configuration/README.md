# Board sensors

The Entity Manager append installs `x570d4u.json` and adds NCT6796 schema
support. The associated [dbus-sensors patch](../sensors/dbus-sensors/0001-nct6796.patch)
adds the corresponding sensor support. The IPMI append enables dynamic sensor
discovery and preserves historical temperature names.

The board configuration includes six fan-header sensor objects, four JC42
DIMM temperatures, and thirteen ADC voltage rails. DIMM addresses on bus 7 are
A2 `0x18`, A1 `0x1a`, B2 `0x19`, and B1 `0x1b`; their IPMI names retain the
`DDR4_*_Temp` spelling. Voltage scales and thresholds are derived from stock
firmware. Host-dependent readings are gated by host power state. VBAT uses ADC9
with `ScaleFactor = 1/3` and the existing dbus-sensors `BridgeGpio` support:
`output-hwm-vbat-enable` (GPIOG0) is driven high, allowed to settle for 200 ms,
then driven low after the read. The first sample is taken when the sensor starts;
subsequent samples are taken once per day, including with the host off, to limit
battery drain. The latest sample remains visible between reads. Critical limits
are 2.7 V and 3.4 V, matching stock firmware. See the
[battery reconstruction](../../../../reverse-engineering/stock-3.09.00/battery-findings.md).
No new daemon or kernel change is required. Build `20261102193652` was verified
on hardware: VBAT reported 3.198 V, IPMI sensor/SDR 112 was present, and the gate
returned low. After future deployments, verify the reading and gate state again.
An unpopulated fan may report unavailable rather than zero.

The fan controller and sensor lifecycle are described in
[fan control](../fans/README.md).
