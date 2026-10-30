# BIOS version and host inventory in stock 3.09.00

## Result

The stock BMC contains a host inventory pipeline. Its SMBIOS screen obtains a cached SMBIOS binary from the BMC and decodes BIOS version, CPU and DIMM inventory in the browser. This is distinct from reading temperature or voltage registers. The shipped firmware also contains a host-interface SMBIOS upload handler and a separate OEM BIOS-info IPMI command. The exact transport actually used by this board's BIOS at boot is not established by the image alone.

No stock binaries or Lua bytecode were executed; no live BMC setting or firmware was changed during this analysis. Lua bytecode constants below are observations, not decompiled control-flow proof.

## Artifact identity and method

Input: `/Users/agross/Downloads/X570D4U-2L2T_3.09.00.ima`, SHA256 `660c22ae2516fc4e1819d067ba93210f39223ffd0a2ea0af616c27d8df7e8d07`.

Complete extracted trees were inspected in Docker volume `openbmc-stock-309-analysis` at `/analysis/rootfs` and `/analysis/webfs`. All firmware paths below are relative to the extracted root filesystem unless explicitly called webfs. The partial Mac extraction was not used as evidence of absence.

Evidence files in this directory:

- `bios-webui-source.js`: gzip-decoded webfs `source.min.js`, 11,436,803 bytes; SHA256 `21fa46c89e20ca290ed3d2cd41a3b119caeae0203c9bf79e4d5cbd6dee7c1449`.
- `bios-smbios-ui-module.js`: exact `views/SmbiosInfoView` AMD module extracted from that source; SHA256 `ba347cc93bb7759ddd2183581601038578ca30baafd7cfa2f0b8549e9d8b6a6b`. It starts at source line 190790.
- `bios-static-evidence.txt`: file hashes and Lua bytecode string offsets.
- Parent-collected `rea-get-smbios-function.json`, REA evidence `ev_a44403bd7e597b2decd90e2ed1de05dc0d6cfd9b6d9a2d1d6842ae1beddeac8b`.
- Parent-collected `rea-set-bios-info-function.json`, REA evidence `ev_c8f61e3eb9365e0cfbf45659be88ac865f98cc558a2bb3b4fdac89f866672b48`.

The ordinary static inspections have no REA Evidence ID; only the last two artifacts do.

## Proven consumer: stock SMBIOS screen

In `bios-smbios-ui-module.js`:

| Lines | Actual behavior |
| --- | --- |
| 29–38 | `afterRender` makes a binary `GET /api/asrr/smbiosdata`, then reads its `arrayBuffer`. |
| 91–98 | Reads SMBIOS entry-point major/minor from bytes 6 and 7; advances `data.slice(32)` to the structures. |
| 107–109 | Type 0 calls `displayBIOSInfo`. |
| 1119–1120 | `BIOS Vendor` uses SMBIOS string index `data[4]`; `BIOS Version` uses string index `data[5]`. |
| 1261, 1340, 1386 | Type 4 renderer displays processor version and part number from the same blob. |
| 2072, 2202, 2205 | Type 17 renderer displays DIMM manufacturer and part number from the same blob. |
| 2860–2868 | Download action requests the same API and names the download `smbios.dat`. |

This proves what the SMBIOS screen displays and where it obtains that data. It does not prove that the latest host boot has populated the cache, nor that all other stock pages use it.

A separate stock firmware-update screen calls `/api/maintenance/hpm/getbiosversions` and displays `existing_bios_version` and `uploaded_bios_version`. This is a different API and purpose. The home overview also contains a `bios_ver` display inside a commented-out block; that block is not evidence of an active consumer.

## Proven REST backing file

`/usr/local/lib/libasrrspx_restservice.so.1.0.0` (SHA256 `cf17d28fba677868c9aae746773435a16c97723da7b1ce45b7b0e2d63f36168d`) exports:

- `get_smbios_data_func`, virtual address `0x27990`, size 1192 bytes.
- `smbios_data`, virtual address `0x27e38`, size 28 bytes.

REA provides a string reference **inside** `get_smbios_data_func`:

- instruction/source `0x27bf4` references `/conf/redfish/smbios/Smbios.dat` at `0x30b80`.
- instruction/source `0x27bf8` references `X-Sendfile2: %s 0-\r\n\r\n` at `0x2c8a8`.

The route constant `/asrr/smbiosdata` is at file offset `0x30ba0`. Together with the JavaScript caller, these establish that the SMBIOS API serves the cached file through an HTTP sendfile response. The decompiler omits arguments on some imported calls; the resolved instruction-level string references, rather than the incomplete pseudocode rendering, establish the file path.

## Host-interface upload path: strong static evidence, not runtime confirmation

`/usr/local/redfish/oem/asrr_hi/route.lua` is LuaJIT bytecode. Its string constants include:

- offset `0x7b`: `/Systems/([^/]+)/Smbios$`
- offset `0xab`: `SERVICE_PREFIX`
- offset `0xbc`: `oem.asrr_hi.smbios-hi`

`/usr/local/redfish/oem/asrr_hi/smbios-hi.lua` is also LuaJIT bytecode. Its constants include:

- offset `0x2e5`: `hi_post`; offset `0x2da`: `hi_delete`.
- offset `0x2f1`: `multipart/form%-data;`.
- offsets `0x10f`–`0x129`: `write`, `wb`, `open`, `filename`, `content-disposition`, `file`.
- offset `0x336`: `Smbios.dat`; offset `0x341`: `/conf/redfish/smbios/`.
- offset `0x18c`: `HI_IGNORE_POST` and storage-limit/missing-resource error constants.

**Inference:** this ASRock host-interface endpoint accepts a multipart SMBIOS upload and stores the exact cache served by the stock SMBIOS UI. `/redfish/v1/Systems/<system>/Smbios` is the likely fully expanded route if `SERVICE_PREFIX` has the conventional Redfish value. The literal prefix value, authentication requirements, exact multipart field and processing branches have not been decoded; do not implement a client from these strings alone.

The image additionally contains generic host-interface inventory support. Bytecode constants in `/usr/local/redfish/oem/ami_hi/hi-inventory-data.lua` include `/Oem/Ami/InventoryData`, `hi_post`, multipart upload, and queued inventory-file processing. `/usr/local/sync-agent/extensions/host-interface/hi-inventory-helper.lua` includes `BiosVersion`, processor/memory data, and `/conf/redfish/bios/inventory.json.gz`. `/usr/local/redfish/redfish/system.lua` contains Redis property `:BiosVersion`. This is a second inventory ingestion/Redfish representation mechanism; its actual activation on this board is unknown.

## BIOS requests observed over the USB host interface

A later BIOS boot trace captured requests to our OpenBMC receiver, not to stock firmware. The recorded OpenBMC response statuses do not establish stock response statuses or bodies. Stock firmware's exact replies remain unknown.

| Request | Purpose or evidence |
| --- | --- |
| `POST /redfish/v1/DynamicExtension/RedfishExtensions` | Five extension posts were attempted. Their bodies were not captured in the available record. Matching BIOS 1.79C archives identify five extension packages; see [dynamic extension research](redfish-dynamic-extension.md). |
| `GET /redfish/v1/DynamicExtension/RedfishExtensions/<UUID>` | The BIOS queried extension IDs including `34E46539-1213-4208-9AB6-2D1C21A35523`, `24C5E8D6-7D92-4E54-916E-FEE44013F13F`, and `7F2CA6E3-55D5-4F37-A98C-CDD39B3CEAC4`. Stock defaults and BIOS archives identify the packages; see [dynamic extension research](redfish-dynamic-extension.md). |
| `POST /redfish/v1/Systems/Self/Smbios` | The BIOS uploaded `Smbios.dat` to OpenBMC. Stock firmware contains the ASRock host-interface handler and cache described above. |
| `POST /redfish/v1/Registries/BiosAttributeRegistry1AWQE.1.79.0.json` and `/redfish/v1/BiosStaticFiles` | The BIOS uploaded its attribute registry and BIOS setup assets, including `SetupData.xml`, `bios.json`, HTML, JavaScript and images. |
| `POST /redfish/v1/Oem/Ami/InventoryData` | The BIOS inventory mechanism sends CPU, DIMM and PCIe data and a separate `BootComplete` notification. Stock firmware contains an AMI inventory handler. The final OpenBMC boot trace did not verify a real PCIe inventory upload. |

BIOS static analysis also identified an AMI inventory CRC lookup for `CPU`, `DIMM` and `PCIE` groups. Matching CRCs can suppress the corresponding group upload. Neither the actual stock response body nor its status was established. Matching BIOS 1.79C archives have since been identified; verifying the actual posted bytes and stock responses requires further analysis or a capture from stock firmware.

## Separate ASRock OEM BIOS-info command

`/usr/local/lib/libasrrcmds.so.1.0.0` (SHA256 `fbf1e05b4d6cb9d8bdf1d632df1391d2da41da36398e447098fe50c992a0ffeb`) exports `SetBIOSFWInfo` at virtual address `0x3e10`, size 84 bytes.

REA decompilation shows it obtains `AsrrGlobal()`, copies five 32-bit request words into global offsets `0x120` through `0x130`, copies request byte `0x14` into global byte `0x134`, calls `SaveGlobalInfo()`, and returns a one-byte successful completion code. It is a persisted **host-supplied** 21-byte value; the function does not read BIOS SPI flash or an SMBIOS table itself.

The handler relocation at `0x8b2c` points to this function. The command table entry begins at `0x8b28` with bytes `b2 04 00 00 00 00 00 00 ff 00 aa aa ff ff 00 00`. This is evidence for candidate command byte `0xb2`; the net function and field definitions have not been proved. The copied fields have not been decoded. In particular, it is not yet established that these 20+1 bytes encode the human-readable BIOS version displayed by any particular screen.

Do not deploy an OpenBMC handler with a guessed netfn, payload or field meaning.

## Legacy AMI inventory/SMBIOS paths

These are present but must not be conflated with the ASRock UI cache:

- `/usr/local/lib/libsmbios_v3.so.6.3.2` contains `/conf/smbios.dmp` at file offset `0xe788`; exports `GetSMBiosFile` at `0x5eb4`, `Fill_SMBIOSInfo` at `0x6048`, and `decodeSMBIOS2C` at `0x6bb8`.
- `/usr/local/lib/libipmimsghndlr.so.6.205.0` also contains `/conf/smbios.dmp` at file offset `0x88b40`. It contains generic file-transfer handlers, but the precise writer and protocol were not established in this pass.
- `/usr/local/sync-agent/extensions/ipmi/libipmi/SMBIOS.lua` declares FFI `Fill_EntryPoint` and `Fill_SMBIOSInfo`, and types 0, 4, 17 and others. It loads `libsmbios_v3`.
- `/usr/local/lib/libinventory.so.6.1.2` exports `InventorySetBiosInfo` (`0x2414`), `InventoryGetBiosInfo` (`0x253c`), `InventorySetSysInfo` (`0x3f18`), and `InventoryGetSysInfo` (`0x4f58`). Its strings include `BIOS`, `BiosVersion`, and `BiosVersionValid`.
- `/usr/local/lib/libipmiamioeminventory.so.6.0.1` contains `/conf/inventory.ini`, `/conf/inv.txt`, and a command-handler table whose relocations map candidate command `0x5a` to `AMISetInvenoryInfo` (`0x8cc`) and `0x5b` to `AMIGetInvenoryInfo` (`0xaf8`). Netfn and wire-format remain unproved.

The two SMBIOS file paths are different. There is no evidence here that one is copied into the other.

## OpenBMC integration implications

1. BIOS version/CPU/DIMM inventory should be populated through a host-to-BMC inventory mechanism. The evidence does not justify treating them as hardware-monitor channels or reading arbitrary BIOS flash locations.
2. The most direct bounded implementation would publish host-read SMBIOS data into OpenBMC inventory and Redfish. The parent independently verified host sysfs currently reports BIOS vendor `American Megatrends International, LLC.`, version `L1.79C`, date `05/11/2026`; this read-only live observation is separate from stock firmware analysis.
3. For automatic compatibility with the board BIOS, identify which supported transfer it actually sends at boot: ASRock host-interface SMBIOS upload, AMI inventory commands, legacy SMBIOS file transfer, or more than one. A BIOS source review or a controlled boot trace would resolve this. The presence of handlers alone does not prove use.
4. OpenBMC SMBIOS/MDR ingestion is a candidate consumer once the payload/transport are confirmed. This report does not establish wire compatibility with OpenBMC's existing SMBIOS-MDR implementation.
5. Keep SMBIOS inventory (DIMM sizes/models/part numbers and BIOS version) separate from live DIMM temperature acquisition. Both may appear together in a UI but have different producers and freshness.

## Remaining focused reverse-engineering targets

If firmware-only protocol recovery is required before a controlled trace:

- Decode `oem/asrr_hi/route.lua` and `smbios-hi.lua` as LuaJIT bytecode without executing the firmware, to establish route prefix, request field, and exact file write.
- In `libipmimsghndlr.so.6.205.0`, find the code reference to `/conf/smbios.dmp` (`0x88b40`) to identify the legacy upload producer. Candidate generic handlers: `ProcessUploadOpCodeReqData` (`0x32610`), `ProcessUploadCompleteFileData` (`0x32d58`), `AMIFirmwareCommand` (`0x33dc0`). They are leads, not proved callers of that string.
- Decode the ASRock command registration/select table around `g_asrrcmds_CmdSelect` (`0x1a178`) and metainfo (`0x1a238`), then field consumers of AsrrGlobal offsets `0x120`–`0x134` to establish `SetBIOSFWInfo` protocol and meaning.
- Decompile `InventorySetSysInfo` / `AMISetInvenoryInfo` only if runtime evidence selects that inventory protocol.
