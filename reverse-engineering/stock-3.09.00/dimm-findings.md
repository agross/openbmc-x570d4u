# Stock firmware DIMM temperature findings

## Result

The stock image explicitly reads four DIMM temperature sensors on **BMC I2C bus 7**, register **0x05**, with a **two-byte read**. The complete slot mapping is recoverable; A2 and B2 are no longer unknown.

| Stock SDR name | Sensor number | Reader suffix | Stock 8-bit address | Linux 7-bit address | wolfpass SDR record header offset |
|---|---:|---:|---:|---:|---:|
| DDR4_A2_Temp | 0x40 / 64 | mem_temp_1_read | 0x30 | **0x18** | 0x5e2 |
| DDR4_A1_Temp | 0x41 / 65 | mem_temp_2_read | 0x34 | **0x1a** | 0x622 |
| DDR4_B2_Temp | 0x42 / 66 | mem_temp_3_read | 0x32 | **0x19** | 0x662 |
| DDR4_B1_Temp | 0x43 / 67 | mem_temp_4_read | 0x36 | **0x1b** | 0x6a2 |

This is derived from shipped data and native instructions, not from numerical address order or an assumed motherboard layout.

## Provenance

Original image: `/Users/agross/Downloads/X570D4U-2L2T_3.09.00.ima`, SHA256 `660c22ae2516fc4e1819d067ba93210f39223ffd0a2ea0af616c27d8df7e8d07`.

Full, case-sensitive Linux extraction: Docker named volume `openbmc-stock-309-analysis`, root `/analysis/rootfs`. The partial macOS extraction is incomplete and was not used to establish this result. All binary examination was static; no stock program was executed and this investigation made no live BMC changes.

| Extracted file | SHA256 |
|---|---|
| `/usr/local/lib/ipmi/wolfpass/libipmipar.so.6.31.0` | `91222fc918e8286c23faa26ac830a9b0bacc5bb93f18202c3cf3d933e60d4aad` |
| `/usr/local/lib/ipmi/wolfpass_BCM/libipmipar.so.1.0.0` | `f9004c8790fff28cc9eebd216c509e7be21495df3495d5a34411710a968fdf09` |
| `/usr/local/lib/libipmihalhw.so.6.3.0` | `adb4d9eaddc1371127afa2cc7881a417bf5296d7fb5b79c45eb7bd0301da1974` |
| `/etc/defconfig/BMC1/wolfpass/SDR.dat` | `1b6dc8f8bb3a626c164828c66c9307aade37069910ebb4d7b27c945f573aad3b` |
| `/etc/defconfig/BMC1/wolfpass_BCM/SDR.dat` | `93c6afab2f5a161def0e6a7dc85a6fdf3f85f830d810ab1192369cc264cdc5ec` |
| `/usr/local/lib/libipmipdk.so.6.28.0` | `d30370c6d127ce2249f203b3e58a161a8fd47c9fcab83335de7a6a613df96d82` |

## Exact chain establishing the mapping

1. `wolfpass/SDR.dat` contains full sensor records 24–27 at offsets above. The record bodies bind names to sensor numbers 0x40–0x43 and entity instances 1, 0, 3, 2 respectively. Names begin 43 bytes into each record body. Each is a Celsius temperature sensor with linear coefficients M=1, B=0, exponent byte=0. Upper critical is 85 C and upper noncritical 84 C. Other threshold bytes being zero does not by itself prove they are enabled.
2. Platform wrapper names bind those sensor channels to concrete readers. `dev_asrr_temp_v0dot1_64_0_32_read_device` at VA 0x10a30 calls `dev_dimm_device_mem_temp_1_read`; its public `_read` wrapper is 0x10a40. Equivalent channel 65, 66 and 67 `_read_device` wrappers are at 0x10ab8, 0x10b40 and 0x10bc8; these call readers 2, 3 and 4. Their `_read` wrappers are 0x10ac8, 0x10b50 and 0x10bd8.
3. The four `wolfpass` reader functions have VAs 0x109c8, 0x10a50, 0x10ad8 and 0x10b60, each 104 bytes. They store 0x30, 0x34, 0x32 and 0x36 respectively in context byte 45, set byte 46 to register 5, set context word at offset 12 to length 2, and call `dev_ast2500_i2c_7_readreg`.
4. `dev_ast2500_i2c_7_readreg` at VA 0xf034 writes bus number 7 into context byte 44 and calls `i2c_read_reg`.
5. `i2c_read_reg` in `libipmihalhw.so.6.3.0`, VA 0x1688 (140 bytes), loads context byte 45 at 0x16c0, then **right-shifts the address by one at 0x16d4** before handing it to the hardware layer. The addresses in the table are therefore unequivocally 7-bit Linux addresses, not a guess about address conventions.

The wrapper, SDR parsing, bus helper and HAL observations above come from ordinary static ELF/disassembly and binary parsing. They are not labeled REA Evidence.

## REA corroboration

Parent analysis obtained the following Hopper-backed REA function results for the `wolfpass` library, preserving both artifact hash and function instructions:

| Function | VA | REA Evidence ID | Local evidence file |
|---|---:|---|---|
| dev_dimm_device_mem_temp_1_read | 0x109c8 | `ev_34d411b33c82b7d809c5f690fd84190ad0432b41d7b71c46164e5d8c973d895b` | `rea-dimm-1-function.json` |
| dev_dimm_device_mem_temp_2_read | 0x10a50 | `ev_46a2057119d7d587911e9e2b43a2ab5de4d3c49b78df7c0d55963027740cb4e0` | `rea-dimm-2-function.json` |
| dev_dimm_device_mem_temp_3_read | 0x10ad8 | `ev_ce914f3e0cd2837fac22f998ac439aea0169e556ab4ca3ad0dfcea5e8f82cccd` | `rea-dimm-3-function.json` |
| dev_dimm_device_mem_temp_4_read | 0x10b60 | `ev_01cf567ed78431246aa65a8052efd33b9375d767f27a1682e246f8c0acd62309` | `rea-dimm-4-function.json` |

`PDK_PreMonitorSensor_DDR4ATEMP` at 0x9d80 is a no-op (`mov r0,#0; bx lr`), supported by REA Evidence `ev_e1c8e770b63f9485b5bc61cc9e91f9aed912ef536ec9fc9ea753f9574c266c68` in `rea-dimm-a-function.json`. Ordinary disassembly of its postmonitor at 0xc230 shows a call to `ASRR_TEventFilter`; it contains no direct temperature conversion. Event-filter internals were not analyzed here.

## Profile selection caveat resolved for DIMM purposes

The image bundles both `wolfpass` and `wolfpass_BCM`. Its literal `/usr/local/lib/ipmi/platform.conf` content is `wolfpass_BCM`. That file alone does not establish which variant the user's Intel-network X570D4U-2L2T selects at runtime; runtime selection and board-specific switching were not proved here.

However, the **DIMM result is identical in both profiles**. Ordinary static disassembly of the BCM library finds readers 1–4 at VAs 0x1121c, 0x112a4, 0x1132c and 0x113b4, with the same addresses, register, two-byte reads, bus7 calls and conversion instructions. BCM SDR records at offsets 0x552, 0x592, 0x5d2 and 0x612 bind the exact same four names to sensor numbers 64–67. Thus the unresolved runtime profile choice does not change this DIMM mapping.

## Temperature conversion and precision

Each platform reader interprets its two response bytes as:

```
output_byte0 = ((response_byte0 << 4) + (response_byte1 >> 4)) & 0xff
output_byte1 = 0
```

This discards fractional bits and high status bits as a consequence of the final 8-bit store. For ordinary positive DIMM temperatures below 128 C, it yields integer Celsius from a JC42-compatible temperature register. It also clears context word zero when the resulting reading is nonzero. There is no explicit signed-temperature conversion in these readers; behavior for negative values and all event/filter semantics is not established. OpenBMC should use its normal JC42 driver, retaining its correct sign, status handling and fractional precision, instead of reproducing the vendor's truncation.

Parent reported fresh, read-only live register-5 reads during this investigation (SMBus-word byte ordering):

| Linux address | Raw SMBus word | Byte-swapped register | Interpreted temperature | Slot from stock mapping |
|---|---:|---:|---:|---|
| 0x18 | 0x04c2 | 0xc204 | 32.25 C | A2 |
| 0x19 | 0x14c2 | 0xc214 | 33.25 C | B2 |
| 0x1a | 0x00c2 | 0xc200 | 32.00 C | A1 |
| 0x1b | 0x20c2 | 0xc220 | 34.00 C | B1 |

These contain high alert/status flags; treating the full 16-bit word as a temperature would be wrong. Masking the temperature portion gives plausible readings and corroborates device presence. This is a parent-reported live observation, separate from the stock static proof. No driver binding or configuration deployment was performed by this analysis.

## Implementation readiness and limits

The address/slot map is sufficiently grounded for a proposed OpenBMC JC42 configuration with four named DIMM sensors on bus7. Driver availability was previously reported by the parent; an implementation still needs normal runtime driver binding, verification of stable sensor naming, threshold behavior and web/IPMI exposure. This report is research only and does not claim those changes have been deployed.
