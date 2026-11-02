# Stock 3.09.00 battery-voltage sampling

Static investigation, 2026-11-02. No stock executable was executed; no live BMC or host setting was changed.

## Finding

BAT is gated. Stock firmware drives GPIO **48 (0x30), AST2500 GPIOG0, high**, waits **200 ms**, reads **ADC9**, then drives GPIOG0 **low** again. Reading ADC9 while this gate is closed is not a valid battery measurement. This supplies the missing software mechanism behind the previously observed near-zero ADC9 input; actual electrical response still needs hardware validation.

The gate is implemented in sensor-monitor hooks in `libipmipdk.so.6.28.0`, outside the platform library's voltage conversion function. Inspecting only that conversion misses the enable/disable sequence.

## Artifact identity

- Firmware: `/Users/agross/lab/openbmc/reverse-engineering/X570D4U-2L2T_3.09.00.ima`; SHA-256 `660c22ae2516fc4e1819d067ba93210f39223ffd0a2ea0af616c27d8df7e8d07`, freshly checked.
- PDK: `binaries/libipmipdk.so.6.28.0`; SHA-256 `d30370c6d127ce2249f203b3e58a161a8fd47c9fcab83335de7a6a613df96d82`. Fresh extraction matches the earlier local copy.
- Platform: `voltage-platform-wolfpass.so`; SHA-256 `91222fc918e8286c23faa26ac830a9b0bacc5bb93f18202c3cf3d933e60d4aad`.
- HAL: `binaries/libipmihalhw.so.6.3.0`; SHA-256 `adb4d9eaddc1371127afa2cc7881a417bf5296d7fb5b79c45eb7bd0301da1974`.
- Rootfs freshly extracted from SquashFS offset `0x4c0000` inside Linux analysis container `stock-battery-analysis`, `/analysis/rootfs`. Container stopped after analysis. The older macOS extraction remains incomplete.

## Observed call path

1. `PDK_RegisterSensorHooks` at `0xb99c` installs per-sensor hook pairs. Table entry at `0x13a8c` contains little-endian words `10`, `0x124f0`, `0x12580`: the BAT sensor number and its two hook addresses.
2. `PDK_PreMonitorSensor_PBAT` at `0x124f0` calls `g_HALGPIOHandle + 0x1c` with argument `0x30`, then `usleep(0x30d40)`, i.e. 200,000 microseconds.
3. HAL table `g_HALGPIOInit` at `0x160ac` explicitly maps slot 7 (`+0x1c`) to `set_gpio_data_high`, slot 8 (`+0x20`) to `set_gpio_data_low`. `GetHALHandles` at `0x1344` uses the two-byte slot index and symbol string from each 258-byte table entry, resolves it with `dlsym`, and writes the function pointer into the indexed handle slot. Thus gate polarity is established through the initializer, not guessed from the log text.
4. Platform BAT function `dev_asrr_voltage_v0dot1_10_0_32_read_device` at `0xf80c` invokes `dev_ast2500_adc_9_read` at `0xf7e8`. The latter sets request offset `0x3c` to channel 9, offsets `0x40` and `0x04` to zero, and calls `adc_read`. BAT-specific platform init at `0xde28` and ADC9 init at `0xf7e0` both return immediately; gate control does not occur there.
5. Stock ADC hardware driver samples channel 9 from physical register **`0x1e6e9020`, bits 25:16**, with 10-bit masking and its normal compensation. See [voltage findings](voltage-findings.md) for the full driver path.
6. `PDK_PostMonitorSensor_PBAT` at `0x12580` processes/caches the reading and calls `g_HALGPIOHandle + 0x20` with argument `0x30`, closing the gate, whenever that callback exists.

The libgpio high/low wrappers submit literal data 1/0 with GPIO number to ioctl `0x7fff4b08`. The GPIO hardware driver's `ast2050_writegpio` at `.text+0x1198` selects register offset `0x20` for GPIO48, bit `48 & 31 = 16`, setting or clearing that bit. Its GPIO base is `0x1e780000`, so this is GPIO data register `0x1e780020`, bit 16. GPIOG0 is the standard eight-pins-per-bank interpretation of line 48. These data writes alone do not configure output direction.

## Conversion and reporting

Let C be the compensated stock ADC code:

```
Vbattery = (C + 1) * 1.8 / 1024 * 300000 / 100000
IPMI_raw = integer(Vbattery / 0.02)
Displayed_V = IPMI_raw * 0.02
```

The arithmetic-helper semantics are inferred from the stock soft-float implementation, consistent with the earlier voltage reconstruction. The constants, calls, channel and gate operations are directly observed in disassembly.

For an OpenBMC IIO input already expressed in ADC-pin millivolts: `Vbattery = input_mV * 3 / 1000`. Do not add stock's raw-count +1 again. An ADC consumer that divides by `ScaleFactor` needs `ScaleFactor = 1/3`.

Stock BAT SDR lower-critical threshold is 2.70 V (raw 135); lower-nonrecoverable is 2.00 V. These are stock policy, not independently verified electrical limits.

## Cache and retry policy

Assembly, rather than Hopper's malformed pseudocode for byte packing and indirect calls, establishes:

- `g_BAT` is a countdown. Initial sentinel -1 is changed to zero on the first pre-hook.
- At zero, or when decrementing reaches zero, pre-hook opens the gate, waits, and returns zero.
- While countdown remains positive, pre-hook returns -1 without opening the gate. Post-hook's nonzero-countdown branch restores the cached reading into the sensor result.
- A fresh reading **strictly above** the lower-critical raw threshold is cached; countdown becomes **86400** (`0x15180`).
- At or below that threshold, retry counts 0 through 4 increment and schedule another sample after **5 hook invocations**, substituting the previous cached value meanwhile.
- Once retry count exceeds 4, the low result is accepted/cached, retry count reset, countdown set to 86400.
- Post-hook lowers GPIO48 on all its normal branches when the low callback is present, including branches using the cache.

86400 and 5 are observed **invocation counts**. A once-per-second hook cadence would mean approximately one day and five seconds; this investigation did not establish that cadence and does not claim those as measured wall-clock intervals. It also did not fully trace the generic monitor's pre-hook return handling.

## OpenBMC implication and remaining limits

An OpenBMC implementation needs exclusive ownership of GPIOG0 as an output, low between samples, high for at least stock's 200 ms settling interval before ADC9 acquisition, and low again on success or error. Scale only a sample acquired with the gate enabled. Coordinate gate state with asynchronous ADC reads; a permanently closed gate plus a correct scale still gives an invalid result.

Hardware validation remains: verify GPIOG0 pinmux/direction/ownership, enable it briefly, wait, read ADC9, restore low, and check that a plausible battery voltage appears. No GPIO was toggled during this investigation. Board circuitry and stock boot-time direction setup were not reconstructed; gate duty-cycle policy may limit battery drain, but that electrical rationale is inference. Runtime platform selection remains as qualified in the earlier voltage report.

## Evidence ledger

Native REA/Hopper Evidence (static observations; not execution traces):

- `battery-pdk-evidence.json`: pre-hook `ev_6d8aae879f9654c68524988a19e6de31125a8e01fe63af7e144dc2845c6eda5f`; post-hook `ev_f25b7e852671f0fd9bc1fac3c302163ad894a2c420450aba6791803edc71fe01`; sensor table `ev_52c2a362e1a80cee28f7742d42a318a32ed79955f248675abaf868db1a018910`.
- `battery-hal-evidence.json`: full GPIO initializer table `ev_38bc6c5cdc745a1f04b9a6feb62d18a7d8f57c39fd5e5bcf220d081043a98722`; resolver `ev_231763247a666a2459fd98a432f4fcf8bd26855e56664f53ccdab23f978d8aac`.
- `battery-platform-evidence.json`: conversion `ev_ff2cd3b16d9bcd8809fd76d4f1d9acd426173772b826e8716bbaad154f861d23`; ADC9 request `ev_b15c07d6884738113509b594dff2c891894f17160cc6d44842ed0b4ad6ce2fb8`; ADC9 init `ev_0e4b35d88b56a87646814ed2f2be07b9b14494c8f3460ce19637449c13dd0c39`.
- `battery-gpio-disassembly.txt`, `battery-adc-read-disassembly.txt`: supplementary GNU ARM objdump output from freshly extracted bytes; these are ordinary static artifacts, not REA Evidence.

Answered: gate line/polarity/settling delay, ADC channel/register, scaling, cache/retry counters. Unresolved: live electrical confirmation, boot-time GPIO setup, exact monitoring cadence.

## Live OpenBMC verification, 2026-11-02

The subsequent GPIO-gated Entity Manager configuration was built and deployed
with explicit user authorization. Firmware build `20261102193652`, version
`3.1.0-dev-1649-g5c21ebb175-x570d4u-2026-11-02-19-36-39`, was verified running
on `172.16.0.253` after BMC reboot. All 6263 build tasks succeeded; the build
reported the existing undefined `render` group warning.

- VBAT D-Bus value: **3.198 V**, Available true, critical alarms false.
- Kernel GPIO debug state: GPIO48, consumer `adcsensor`, **output low** after read.
- IPMI Get Sensor Reading for sensor 112 completed successfully (completion 0).
- SDR record 112 completed successfully and names the sensor `VBAT`.
- ADC, bmcweb and NCT6796 enable services active; host state Running.

This validates the gate/channel/scaling combination on the actual board.
The 200 ms edge-to-sample timing was established from implementation, not
measured with an external instrument. The daily scheduled reread has not yet
elapsed. Logs: `output/vbat-build.log`, `output/vbat-deploy.log`,
`output/vbat-verification.txt` (generated artifacts, excluded from Git).
