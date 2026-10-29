# Board sensors

The Entity Manager append installs `x570d4u.json` and adds NCT6796 schema
support. The associated [dbus-sensors patch](../sensors/dbus-sensors/0001-nct6796.patch)
adds the corresponding sensor support. The IPMI append enables dynamic sensor
discovery and preserves historical temperature names.

The board configuration includes six fan-header sensor objects, four JC42
DIMM temperatures, and twelve ADC voltage rails. DIMM addresses on bus 7 are
A2 `0x18`, A1 `0x1a`, B2 `0x19`, and B1 `0x1b`; their IPMI names retain the
`DDR4_*_Temp` spelling. Voltage scales and thresholds are derived from stock
firmware. Host-dependent readings are gated by host power state. Battery voltage
is intentionally omitted because its acquisition has not been established.
An unpopulated fan may report unavailable rather than zero.

The fan controller and sensor lifecycle are described in
[fan control](../fans/README.md).
