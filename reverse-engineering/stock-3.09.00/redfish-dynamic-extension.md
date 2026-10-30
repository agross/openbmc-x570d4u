# AMI Dynamic Redfish Extension: public evidence

Research date: 2026-10-30.

## Findings

- Advantech's [AST2600 AMI BMC manual](https://advdownload.advantech.com/productfile/Downloadfile2/1-2LQBGA8/BMC_AST2600_AMI_Code_User_Manual_Ed1_FINAL.pdf) describes `/redfish/v1/DynamicExtension` as the AMI Dynamic Extension root, and `/redfish/v1/DynamicExtension/GamiExtensions` as a collection of dynamic-extension instances. This is evidence of the AMI feature on that platform, not the exact behavior of X570D4U firmware.
- A [reported AMI service root](https://github.com/DMTF/Redfish-Service-Validator/issues/345) exposes that root under `Oem.Dre` with type `#AMIDynamicExtension.v1_0_0.AMIDynamicExtension`. This is a reported implementation example, not a standard Redfish resource definition.
- [Eclypsium's primary disclosure](https://eclypsium.com/research/bmcc-lights-out-forever/) describes an AMI dynamic-extension feature that accepts posted code which executes as root in the Redfish service, and its code-injection finding CVE-2023-34330. It says the tested configuration allowed the feature on the host interface. The researchers intentionally omitted exact URL and payload details. The report does not establish that this particular X570D4U firmware is vulnerable, nor that each BIOS POST contains executable code.
- [AMI advisory AMI-SA-2023006](https://9443417.fs1.hubspotusercontent-na1.net/hubfs/9443417/Security%20Advisories/AMI-SA-2023006.pdf) lists CVE-2023-34330 and SPx 12.4 / 13.2 fix versions. Those generic SPx version numbers do not establish the board firmware's patch level.
- DMTF also defines a vocabulary named `RedfishExtensions`; its use as an alias for `@Redfish` annotations is described in the [Redfish Data Model Specification](https://redfish.dmtf.org/schemas/v1/DSP0268_2023.3.html). The similar name does not by itself mean the AMI dynamic-extension endpoint implements that vocabulary.

## Firmware evidence

Stock BMC 3.09.00's extracted `usr/local/redfish/db_init/bios_dre.rcmd` (SHA-256 `a51c6ccaf464eaaa3b170a6207de5e3bdc6ceb5f025cf03daed7ace84ac604be`) assigns five UUIDs to directory names and records MD5 checksums for BIOS versions `170`, `180`, and `18a`. The repository BIOS image [`X572L2T1.79C`](../X572L2T1.79C) (SHA-256 `7b23a12f9d28dcd917ecc16740682a1d80a29a41a2685644afc73840ee9a62e5`) contains matching GUID-named UEFI freeform raw sections. Each inspected section is a gzip-compressed tar archive containing Lua source and an `install.sh` that moves its `app` directory into the named extension directory. Inspection read archive members without executing or installing them.

| UUID | Directory | What the BIOS 1.79C archive contains | Archive MD5 |
| --- | --- | --- | --- |
| `34E46539-1213-4208-9AB6-2D1C21A35523` | `ami_inventory` | Inventory CRC handler; GET returns `GroupCrcList`, POST stores values, route `/oem/ami/inventory/crc`. | `24e5614de3ead58517b9a1f001f272a8` |
| `24C5E8D6-7D92-4E54-916E-FEE44013F13F` | `FwConfig_LoadDef_dynextn` | BIOS default values handler, route `/bios/defaultSD`, storing `DefaultData`. | `f33b77b217dd5fc1deae86d64c5b1e28` |
| `7F2CA6E3-55D5-4F37-A98C-CDD39B3CEAC4` | `ami` | Adds `Oem.Ami.AutomationLog` fields for BIOS update activity, progress and status to Systems; handles their PATCH. | `cbfd3cd01959fde62819b7b1e46d9f0a` |
| `18D8AFB5-4779-4091-9E3B-0A8899045341` | `ami_bios` | Memory and PCIe function change-state actions and PCIe mapping-out data. | `fa4e1ebc6022aeb0441c1ab52f6237a1` |
| `4CAF7FF8-50E7-4703-BE1C-7A6C4BBB5134` | `ami_util` | Mapping-out device-list handler at `/oem/MappingOut`. | `15088bf7cac23985ee946ecb95422c31` |

Four archive MD5 values match all three stock defaults. The `ami_bios` archive differs from all three recorded values (`b5639ab09f7dcb1c4c5836287401ce2d` for `18a`/`180`; `9a7019283ba7556c4911fc7575a1034c` for `170`). This establishes a byte difference, not its cause. The checksums are matching identifiers, not security guarantees.

The BIOS 1.79C archives and stock database entries establish the extension identities and their packaged functionality. The previously observed requests were made by BIOS 1.79C to OpenBMC; no recorded POST body ties those requests byte-for-byte to the archives, and this inspection does not prove how stock installs them or what stock returns. The archived CRC route is `/oem/ami/inventory/crc`; other BIOS paths mentioned elsewhere should not be treated as aliases without further evidence.

## Limits

Searches for the complete `/redfish/v1/DynamicExtension/RedfishExtensions` path and observed extension UUIDs did not locate a public protocol or payload schema. The captured extension POST bodies, stock response status/body, exact stock installation behavior, and this firmware's vulnerability status remain unverified.
