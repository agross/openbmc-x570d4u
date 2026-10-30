# Stock 3.09.00 voltage sensor reconstruction

All work is static and read-only. No vendor executable was run and no BMC setting was changed. The independent fresh ADC millivolt sample below was supplied by the parent investigation.

## Artifact identity and profile scope

- Firmware: `/Users/agross/Downloads/X570D4U-2L2T_3.09.00.ima`, SHA256 `660c22ae2516fc4e1819d067ba93210f39223ffd0a2ea0af616c27d8df7e8d07`.
- Full extracted stock rootfs: Docker volume `openbmc-stock-309-analysis`, `/analysis/rootfs`.
- `/usr/local/lib/ipmi/platform.conf` contains `wolfpass_BCM`; this is a shipped default, not proof of the selected runtime profile on this particular board.
- Both `etc/defconfig/BMC1/wolfpass/SDR.dat` and `etc/defconfig/BMC1/wolfpass_BCM/SDR.dat` contain the same first 13 full voltage SDR records, byte for byte (including headers). BCM adds `BCM_V10PHYDIG` and `BCM_VDD1P8`, which must not be assumed present on the Intel 2L2T board.
- The 13 matching conversion function bodies and their constant pools are also identical across the two platform binaries after masking only ARM BL relative displacements. See `voltage-profile-comparison.json`. Therefore the shared 13-channel calibration does not depend on resolving this profile-selection ambiguity.

| Artifact | SHA256 |
|---|---|
| `voltage-platform-wolfpass.so` | `91222fc918e8286c23faa26ac830a9b0bacc5bb93f18202c3cf3d933e60d4aad` |
| `voltage-platform-wolfpass-BCM.so` | `f9004c8790fff28cc9eebd216c509e7be21495df3495d5a34411710a968fdf09` |
| `voltage-wolfpass-SDR.dat` | `1b6dc8f8bb3a626c164828c66c9307aade37069910ebb4d7b27c945f573aad3b` |
| `voltage-wolfpass-BCM-SDR.dat` | `93c6afab2f5a161def0e6a7dc85a6fdf3f85f830d810ab1192369cc264cdc5ec` |

## Two separate conversions

**Observed:** stock platform functions read ADC channel N-1 for voltage sensor N, add 1 to the returned 16-bit ADC reading, pass literal double constants through soft-float helpers, convert the result back to an integer, and store a 16-bit result. Register setup writes the channel number at request structure offset `0x3c`, and zeroes offsets `0x40` and `0x04` before `adc_read`.

**Inferred with high confidence from the arithmetic helper implementations and literal arguments:** let C be the compensated ADC code returned by the stock driver. The formula is `pin_V = (C + 1) * 1.8 / 1024`, then `rail_V = pin_V * divider_multiplier`, then `IPMI_raw = integer(rail_V / SDR_step_V)`. The ADC has 10-bit resolution and a nominal 1800 mV reference, confirmed separately by the stock driver. Integer quantization can make stock displayed readings differ slightly from continuous OpenBMC values.

**Observed SDR conversion:** all 13 records use unsigned readings, linearization 0, B=0, Bexp=0. Their voltage is `(M * IPMI_raw) * 10^Rexp`. This describes the already scaled and quantized IPMI value; it is not the resistor divider multiplier and must not be used directly as an OpenBMC ADC scale.

For an OpenBMC input already in ADC-pin millivolts, use `rail_V = input_mV / 1000 * divider_multiplier`. If the consuming ADC sensor implementation divides by `ScaleFactor`, use the reciprocal shown below. Verify that implementation before applying it. Do not add the stock +1 raw-count offset again to an already converted millivolt input.

| ADC | SDR sensor/name | Divider multiplier | Reciprocal ScaleFactor | SDR M,Rexp / step V | Function VA = file offset (wolfpass) |
|---:|---|---:|---:|---|---|
| 0 | 1 / 3VSB | 2890/1150 = 2.513043478 | 0.397923875433 | 2,-2 / 0.02 | `0xf1b4` (132 bytes) |
| 1 | 2 / 5VSB | 4100/1100 = 3.727272727 | 0.268292682927 | 3,-2 / 0.03 | `0xf274` (132 bytes) |
| 2 | 3 / VCPU | 4000/3000 = 1.333333333 | 0.75 | 1,-2 / 0.01 | `0xf334` (132 bytes) |
| 3 | 4 / VSOC | 4000/3000 = 1.333333333 | 0.75 | 1,-2 / 0.01 | `0xf3f4` (132 bytes) |
| 4 | 5 / VCCM | 1000/1000 = 1 | 1 | 1,-2 / 0.01 | `0xf4b4` (128 bytes) |
| 5 | 6 / APU_VDDP | 1000/1000 = 1 | 1 | 1,-2 / 0.01 | `0xf570` (128 bytes) |
| 6 | 7 / PM_VDD_CLDO | 1/1 = 1 | 1 | 1,-2 / 0.01 | `0xf62c` (100 bytes) |
| 7 | 8 / PM_VDDCR_S5 | 1/1 = 1 | 1 | 1,-2 / 0.01 | `0xf6cc` (100 bytes) |
| 8 | 9 / PM_VDDCR | 1/1 = 1 | 1 | 1,-2 / 0.01 | `0xf76c` (100 bytes) |
| 9 | 10 / BAT | 300000/100000 = 3 | 0.333333333333 | 2,-2 / 0.02 | `0xf80c` (132 bytes) |
| 10 | 11 / 3V | 2890/1150 = 2.513043478 | 0.397923875433 | 2,-2 / 0.02 | `0xf8cc` (132 bytes) |
| 11 | 12 / 5V | 300000/100000 = 3 | 0.333333333333 | 3,-2 / 0.03 | `0xf98c` (132 bytes) |
| 12 | 13 / 12V | 120000/10000 = 12 | 0.0833333333333 | 1,-1 / 0.1 | `0xfa4c` (132 bytes) |

The function suffix is `dev_asrr_voltage_v0dot1_<sensor>_0_32_read_device`, except sensors 7, 8, 9, which use `v0dot2`. Unity channels 4 and 5 explicitly multiply and divide by 1000; channels 6, 7, 8 omit these two operations. This is an implementation difference with the same net divider ratio.

## SDR record offsets and thresholds

The AMI SDR file starts with 16 `ff` bytes. Each standard SDR header is preceded by the two-byte AMI wrapper `5a 40`. Header offsets below refer to the standard IPMI header, not the wrapper. Full-record body offsets: sensor number `+2`, type `+7`, units `+16`, linearization `+18`, M `+19/+20[7:6]`, B `+21/+22[7:6]`, Rexp/Bexp `+24`, ID-length `+42`, name `+43`. All offsets are zero based from the full-record body, which starts five bytes after its header.

Threshold readable mask is body `+13`, settable mask `+14`. Bits 0..5 are LNC, LC, LNR, UNC, UC, UNR. Threshold values occur at body `+31` UNR, `+32` UC, `+33` UNC, `+34` LNR, `+35` LC, `+36` LNC. Every record except VCPU has mask `0x36` (LC, LNR, UC, UNR); VCPU has `0x30` (UC, UNR). Noncritical thresholds are disabled, not 0 V alarms.

| ADC/name | SDR header offset | LNR V (raw) | LC V (raw) | UC V (raw) | UNR V (raw) |
|---|---|---:|---:|---:|---:|
| 0 / 3VSB | `0x12` | 2.88 (144) | 3.04 (152) | 3.7 (185) | 3.88 (194) |
| 1 / 5VSB | `0x52` | 4.26 (142) | 4.5 (150) | 5.49 (183) | 5.73 (191) |
| 2 / VCPU | `0x92` | disabled | disabled | 1.65 (165) | 1.73 (173) |
| 3 / VSOC | `0xd2` | 0.34 (34) | 0.36 (36) | 1.54 (154) | 1.61 (161) |
| 4 / VCCM | `0x112` | 1.02 (102) | 1.08 (108) | 1.32 (132) | 1.38 (138) |
| 5 / APU_VDDP | `0x152` | 0.77 (77) | 0.81 (81) | 1.16 (116) | 1.21 (121) |
| 6 / PM_VDD_CLDO | `0x192` | 1.02 (102) | 1.08 (108) | 1.32 (132) | 1.38 (138) |
| 7 / PM_VDDCR_S5 | `0x1d2` | 0.8 (80) | 0.89 (89) | 1.16 (116) | 1.21 (121) |
| 8 / PM_VDDCR | `0x212` | 0.8 (80) | 0.89 (89) | 1.16 (116) | 1.21 (121) |
| 9 / BAT | `0x252` | 2 (100) | 2.7 (135) | 3.4 (170) | 3.56 (178) |
| 10 / 3V | `0x292` | 2.8 (140) | 2.98 (149) | 3.62 (181) | 3.78 (189) |
| 11 / 5V | `0x2d2` | 4.26 (142) | 4.5 (150) | 5.49 (183) | 5.73 (191) |
| 12 / 12V | `0x312` | 10.2 (102) | 10.8 (108) | 13.2 (132) | 13.8 (138) |

LNR/UNR mean lower/upper nonrecoverable; LC/UC mean lower/upper critical. These are stock thresholds and can be reproduced; they are not independently validated electrical limits. A system with overclocked memory or altered CPU voltages may legitimately need different limits.

## ADC hardware register and compensation evidence

`lib/modules/generic/misc/adc_hw.ko` disassembly is preserved in `voltage-adc-hw-disassembly.txt`. This is an ARM relocatable object: addresses here are offsets within `.text`, not runtime virtual addresses.

- `adc_get_resolution` at `.text+0x00` returns 10. `adc_get_reference_voltage` at `.text+0x24` returns literal `0x708` = 1800.
- `init_module` at `.text+0x2ec` maps a `0x1000` byte region at physical `0x1e6e9000` (literal at `.text+0x440`). It writes `0x140` at register offset `0x0c`, initializes control with `0x0f`, waits for control bit `0x100`, performs compensation, and enables bits 16..31 for 16 channels.
- `adc_measure` at `.text+0x184` reads register offset `0x10 + 4*(channel>>1)`. Even channels use bits 9:0; odd channels use bits 25:16. Thus ADC0/1 map to `0x1e6e9010`, ADC2/3 to `0x1e6e9014`, ADC4/5 to `0x1e6e9018`, ADC6/7 to `0x1e6e901c`, ADC8/9 to `0x1e6e9020`, ADC10/11 to `0x1e6e9024`, ADC12/13 to `0x1e6e9028`.
- `adc_read_channel` at `.text+0x228` enables control bit `16+channel` if needed, sleeps 1 ms, calls `adc_measure`, and stores a 16-bit result.
- `adc_read_compensating_value` at `.text+0xc0` takes ten internal reference measurements, computes `512 - (measurement & 1023)` per sample, and averages them with signed arithmetic. It switches control values `0x1f`, `0x1001f`, and `0x0f` with 50 ms delays. `adc_measure` adds this stored compensation to nonzero readings if nonzero, clamping negative corrected results to zero. Consequently C in the stock formula is a compensated code, not necessarily the raw hardware register value. The OpenBMC kernel may calibrate separately; the divider ratios remain applicable independently of this offset.

## Native REA corroboration

Parent-created Hopper dossiers establish native instruction/call/literal structure for both function variants:

- `rea-voltage-1-function.json`, Evidence `ev_e102c0d982f084b4ab7b0a58cb159abbf3ec32af05eddd431d7d60fe64f57d5b`, procedure `0xf1b4`: calls `dev_ast2500_adc_0_read`, adds 1, and applies the constants `1.8`, `1/1024`, `2890`, `1150`, `0.02`.
- `rea-voltage-7-function.json`, Evidence `ev_01619e5a2f5bf7ab51c815b9468a96e652fe5aed02d0f600fa8ef8ee61c8245a`, procedure `0xf62c`: calls `dev_ast2500_adc_6_read`, adds 1, and applies `1.8`, `1/1024`, `0.01`.

Hopper preserves helper names as `sub_e4364`, `sub_e4448`, `sub_e46b4`, `sub_e48b8`; arithmetic semantics are an analyst interpretation of the soft-float implementation, not recovered source names. Other channel constants are independently decoded from ARM literal loads and direct calls in `voltage-native-conversion-constants.json`. Helper VAs in wolfpass: int-to-double `0xe4364`, multiply `0xe4448`, divide `0xe46b4`, integer conversion `0xe48b8` (semantics inferred). BCM equivalents are shifted by `0x854`. No runtime sampling of stock code was performed.

## Read-only live plausibility correlation

The parent supplied fresh OpenBMC ADC-pin millivolt inputs for channels 0..12. Applying the recovered divider ratios produces the following; these are calculated estimates, not actual stock firmware readings:

| ADC/name | ADC-pin mV | Estimated rail V |
|---|---:|---:|
| 0 / 3VSB | 1320 | 3.3172 |
| 1 / 5VSB | 1365 | 5.0877 |
| 2 / VCPU | 1054 | 1.4053 |
| 3 / VSOC | 731 | 0.9747 |
| 4 / VCCM | 1216 | 1.2160 |
| 5 / APU_VDDP | 921 | 0.9210 |
| 6 / PM_VDD_CLDO | 1197 | 1.1970 |
| 7 / PM_VDDCR_S5 | 1016 | 1.0160 |
| 8 / PM_VDDCR | 1005 | 1.0050 |
| 9 / BAT | 5 | 0.0150 |
| 10 / 3V | 1341 | 3.3700 |
| 11 / 5V | 1683 | 5.0490 |
| 12 / 12V | 1017 | 12.2040 |

All populated normal rails are plausible. BAT yields 0.015 V from a near-zero 5 mV pin input: the multiplier alone cannot fix this. Battery measurement enable/gating, acquisition timing, or hardware routing remains unresolved. Do not publish that as a real empty-battery voltage yet.

## Remaining limits and next steps

- The stock physical resistor values themselves are not proven; the ratios above are the exact numerical calibration embedded in the shipped binary. For example `2890/1150` should not be interpreted as an independently measured resistor pair.
- Runtime wolfpass versus BCM selection is not proven, but their shared 13-channel conversion and SDR records agree. BCM-only extras need separate hardware proof.
- Trace battery-specific GPIO/control and kernel acquisition behavior before enabling BAT alarms or trusting its reading.
- Check the actual OpenBMC ADC consumer uses the reciprocal ScaleFactor convention, confirm its calibration path, then implement these channel-specific ratios and selected thresholds. This analysis did not make those changes.
- SDR contains PSU VIN records too (sensors 24 and 25, M=2, Rexp=0, no thresholds), but they are PMBus readings, not additional AST2500 ADC rails; their presence alone does not establish an installed PMBus PSU.

## Reproduction artifacts

`voltage-parse.py` is a static SDR/ELF parser. `voltage-wolfpass-SDR.json` and `voltage-wolfpass-BCM-SDR.json` preserve every parsed full record and its exact bytes. `voltage-native-conversion-constants.json`, `voltage-native-conversion-constants-BCM.json`, and `voltage-profile-comparison.json` record instruction/literal calculations and profile equivalence. `voltage-platform-disassembly.txt` and `voltage-adc-hw-disassembly.txt` preserve trusted objdump output of stock bytes. These ordinary static outputs are supporting evidence and are not claimed to be REA Evidence.
