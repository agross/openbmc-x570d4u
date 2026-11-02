# IPMI monitoring compatibility

The provider preserves historical temperature and voltage names and gives board sensors fixed
IPMI sensor numbers and SDR record IDs. Both identities matter: `ipmi_exporter`
uses the SDR record ID for its `id` label.

The voltage D-Bus sensor `VBAT` is exposed as `BAT` in IPMI, matching the vendor
BMC and historical `ipmi_voltage_volts{name="BAT"}` series. The other twelve
voltage names match the names recorded in Prometheus on 2026-10-10 unchanged.
This is a display-name alias only: the D-Bus path, voltage conversion, thresholds,
and assigned sensor/SDR ID 112 remain unchanged.

| IDs | Sensors |
| --- | --- |
| 0–99 | Fans: FAN1–FAN6_1 use 0–5; Pwm_1–Pwm_6 use 50–55 |
| 100–199 | Voltages: existing rails use 100–111; VBAT uses 112 |
| 200–299 | Temperatures: existing sensors use 200–210; skip reserved ID 255 |

An absent sensor leaves its slot unused; later sensors keep their IDs. SDR
enumeration follows the next populated record rather than assuming consecutive
IDs. Non-sensor SDRs start at 1024, above the supported sensor LUNs.

The explicit path-to-number map in `0002-stable-sensor-ids.patch` is a board ABI:
new board sensors receive a free number in their category; never change or reuse
an assigned number. Unlisted sensors retain dynamic discovery starting at 300;
their IDs are not promised stable. Add an explicit mapping before relying on an
additional sensor's ID. IPMI reserves number 255 in each LUN, and LUN 2 is skipped.
Device SDR information reports the actual sensor count and populated LUNs, rather
than inferring them from consecutive numbering.

The regression harness compiles the actual patched numbering and SDR traversal
functions with fake D-Bus and record-construction boundaries. It checks fixed IDs
with and without PWM sensors, missing and returning sensors, sparse enumeration,
first/last requests, non-sensor records, invalid requests, and sparse LUN counts. Pass the recipe's
patched source directory:

```sh
python3 meta-asrock/meta-x570d4u/recipes-phosphor/ipmi/tests/test-stable-sensor-ids.py /path/to/phosphor-ipmi-host/source
python3 meta-asrock/meta-x570d4u/recipes-phosphor/ipmi/tests/test-sensor-names.py /path/to/phosphor-ipmi-host/source
```

A firmware update changes the SDR repository. Clients must refresh their SDR cache;
verify exported names and IDs after deployment.
