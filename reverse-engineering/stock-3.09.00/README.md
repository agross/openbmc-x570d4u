# ASRock Rack X570D4U-2L2T stock BMC 3.09.00 analysis

Analyzed the supplied firmware to recover missing OpenBMC sensor and inventory behavior. This investigation made no firmware, service or configuration changes on the BMC or host. Stock programs were not executed. Live verification used existing SSH access for read-only sysfs and temperature-register reads.

## Artifact and method

- Input: `/Users/agross/Downloads/X570D4U-2L2T_3.09.00.ima`.
- Size: 67,109,128 bytes.
- SHA256: `660c22ae2516fc4e1819d067ba93210f39223ffd0a2ea0af616c27d8df7e8d07`.
- Two SquashFS filesystems extracted completely in a case-sensitive Linux Docker volume, `openbmc-stock-309-analysis`, at `/analysis/rootfs` and `/analysis/webfs`. The local `rootfs-partial-macos` directory is incomplete and must not be treated as a full extraction.
- Static analysis: ELF symbols/disassembly, SDR decoding, packaged web code, and REA 6.7.0 with Hopper. REA MCP was unavailable in this chat; its installed CLI worked. Firmware-region inspection was unavailable on this host; extraction and native function analysis succeeded independently.
- Detailed function evidence retains artifact digests, provider metadata, addresses and limitations. Ordinary binary parsing and live reads are separately identified and are not REA Evidence.

## DIMM temperatures: recovered

Both shipped platform variants agree on I2C bus 7, register 0x05, two-byte reads:

| Stock sensor | Linux address | Live temperature during investigation |
|---|---:|---:|
| DDR4_A2_Temp | 0x18 | 32.25 C |
| DDR4_A1_Temp | 0x1a | 32.00 C |
| DDR4_B2_Temp | 0x19 | 33.25 C |
| DDR4_B1_Temp | 0x1b | 34.00 C |

Stock reports integer Celsius; OpenBMC should use the existing JC42 kernel driver to retain fractions and proper sign/status handling. Stock SDR upper noncritical and critical threshold values are 84 C and 85 C; threshold-enable semantics still need normal implementation review. The actual map is ready to implement and validate through the web UI and IPMI.

Full proof: [DIMM findings](dimm-findings.md). Four Hopper-backed REA results are `rea-dimm-1-function.json` through `rea-dimm-4-function.json`; their full Evidence IDs appear in the detailed report.

## Voltages: calibration recovered

The following multipliers convert voltage at the ADC pin into rail voltage. An OpenBMC ADC ScaleFactor is the reciprocal of the multiplier, subject to checking the current ADC sensor implementation when making the change.

| ADC channel | Rail | Rail / ADC multiplier | OpenBMC ScaleFactor |
|---:|---|---:|---:|
| 0 | 3VSB | 2890/1150 | 1150/2890 |
| 1 | 5VSB | 4100/1100 | 1100/4100 |
| 2 | VCPU | 4000/3000 | 0.75 |
| 3 | VSOC | 4000/3000 | 0.75 |
| 4 | VCCM | 1 | 1 |
| 5 | APU_VDDP | 1 | 1 |
| 6 | PM_VDD_CLDO | 1 | 1 |
| 7 | PM_VDDCR_S5 | 1 | 1 |
| 8 | PM_VDDCR | 1 | 1 |
| 9 | VBAT | 3 | 1/3 |
| 10 | 3V | 2890/1150 | 1150/2890 |
| 11 | 5V | 3 | 1/3 |
| 12 | 12V | 12 | 1/12 |

Examples from live IIO readings: 3VSB approximately 3.32 V; 5VSB 5.09 V; VCPU 1.41 V; VSOC 0.975 V; 12V 12.20 V. These are illustrative snapshots, not precision measurements. Stock has its own ADC compensation and integer quantization; Linux IIO calibration should remain authoritative for pin voltage rather than reproducing stock raw-count arithmetic blindly.

VBAT currently reads about 5 mV at the ADC input. Multiplying produces an invalid battery reading. Divider is recovered, but battery sampling/enable behavior remains unresolved; do not expose it as a valid 0.015 V battery reading or enable misleading low-voltage alerts.

Full calibration, SDR thresholds and limitations: [Voltage findings](voltage-findings.md). Hopper-backed template evidence: `rea-voltage-1-function.json` (`ev_e102c0d982f084b4ab7b0a58cb159abbf3ec32af05eddd431d7d60fe64f57d5b`) and `rea-voltage-7-function.json` (`ev_01619e5a2f5bf7ab51c815b9468a96e652fe5aed02d0f600fa8ef8ee61c8245a`).

## BIOS version and inventory: consumer recovered, transport incomplete

The stock web UI requests `/api/asrr/smbiosdata` and decodes SMBIOS Type 0 for the BIOS version. The actual REST function `get_smbios_data_func` serves `/conf/redfish/smbios/Smbios.dat` using an X-Sendfile2 response. A separate legacy SMBIOS library uses `/conf/smbios.dmp`; these paths must not be conflated.

A separate ASRock `SetBIOSFWInfo` command copies a 21-byte request into global state and saves it. This proves a host-supplied information path exists, but does not establish that this command is the complete producer for the web UI's SMBIOS file. Its exact payload meaning and the full host-to-BMC upload protocol remain unproved.

Read-only host sysfs independently reports BIOS version **L1.79C**, vendor **American Megatrends International, LLC.**, date **05/11/2026**. OpenBMC needs a host-to-BMC inventory source or a compatible handler for the BIOS's existing protocol, followed by publication through its inventory interfaces. Merely adding a sensor definition cannot provide BIOS information. Hard-coding L1.79C would become stale after an update.

Detailed paths, command evidence and limitations: [BIOS and inventory findings](bios-inventory-findings.md). REA evidence: `rea-get-smbios-function.json` (`ev_a44403bd7e597b2decd90e2ed1de05dc0d6cfd9b6d9a2d1d6842ae1beddeac8b`) and `rea-set-bios-info-function.json` (`ev_c8f61e3eb9365e0cfbf45659be88ac865f98cc558a2bb3b4fdac89f866672b48`).

## Next implementation scope

1. Add four named JC42 DIMM temperature sensors, then verify their values, availability, web display and IPMI/exporter exposure.
2. Add the 12 usable voltage channels with recovered scaling and reviewed stock thresholds. Treat powered-off rails correctly. Keep VBAT unavailable until sampling is solved.
3. Handle BIOS/SMBIOS inventory as a separate feature: trace the host protocol or arrange a host publisher, then verify updates after reboot and BIOS upgrades.

No rebuilt firmware was produced or deployed by this analysis. `live-readonly-validation.json` contains the live observations used above.
