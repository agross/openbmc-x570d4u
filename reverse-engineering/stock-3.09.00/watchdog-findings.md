# Stock IPMI watchdog behavior

The ASRock Rack X570D4U-2L2T 3.09.00 firmware runs the standard IPMI watchdog
countdown in BMC software. On expiration it dispatches the selected chassis
action. The separate ASRock watchdog code programs BMC hardware registers.
The standard IPMI reset path does not require copying that register code into
OpenBMC.

The analysis used the root filesystem at offset `0x4c0000` of
`X570D4U-2L2T_3.09.00.ima` (SHA-256
`660c22ae2516fc4e1819d067ba93210f39223ffd0a2ea0af616c27d8df7e8d07`).
The SquashFS was extracted in a case-sensitive Linux volume. REA used Hopper
on the extracted ARM ELFs. No stock code was executed.

## Standard IPMI timer

- `SetWDT` (`0x35c64`), `GetWDT` (`0x35680`) and `ResetWDT`
  (`0x35368`) in `libipmimsghndlr.so.6.205.0` configure, report and refresh
  the timer. `ResetWDT` returns `0x80` before configuration.
- `WDTTimerTask` (`0x4583c`) waits on a semaphore, reads Linux jiffies and
  tracks the countdown. At `0x45e2c` and `0x45ef0`, it passes the three
  low bits of the action byte to the dispatcher at `0x45298`.
- ARM disassembly resolves that dispatcher's table at `0x45388`.
  Action 1 calls `Platform_HostColdReset` at `0x45750`, action 2 calls
  `Platform_HostPowerOff` at `0x45724`, and action 3 calls
  `Platform_HostPowerCycle` at `0x456f8`. All three record restart
  cause 4 before the action. This is the built-in path after the
  dispatcher checks optional platform hooks.
- `Platform_HostColdReset` (`0x7b88`, `libipmistack.so.6.47.0`) posts
  to `/var/ChassisCtrlQ`. This establishes the handoff to the stock chassis
  controller; it does not identify the physical GPIO from stock code.

The separate `ASRR_WatchDogSetting` (`0x11e4`,
`libasrrglobal.so.1.0.0`) enables, refreshes or disables a hardware timer
using memory-mapped registers at `0x1e785024` through `0x1e78502c`. The
standard IPMI timeout's observed reset dispatch goes through the chassis
controller instead.

## Evidence and limits

- `watchdog-ipmi-evidence.json`: `WDTTimerTask`
  `ev_5edd6b72f156e5150ade05df2d0c70b4703d0b1b4245d23a0ba4dcc1c53866db`,
  `ResetWDT`
  `ev_536b93804a6d4262cb36ca5327fa01dda44a6a625a46371d83e347046e09348a`,
  and Set/Get dossiers with binary identity and limitations.
- `watchdog-platform-evidence.json`: `Platform_HostColdReset`
  `ev_229c7e97cf761d64354740b9b8c04e364ed26a053c7b88e01c5132d4ebe7a34c`.
- `watchdog-asrr-evidence.json`: `ASRR_WatchDogSetting`
  `ev_f76e218b6dc5c009dabfd7dd0fabc8e2104efae930fa0ac043f06d9f65ee0c47`.
- `watchdog-action-disassembly.txt`: supplementary LLVM ARM disassembly.
  Hopper did not return a dossier for the unnamed dispatcher. The symbol
  printed by LLVM is the preceding exported symbol; the dispatcher has a
  separate function entry at `0x45298`.

These static findings establish the stock software path, not electrical
behavior or the BIOS's timer policy on a live system. OpenBMC can route expiry
to its existing chassis controller. Its reset output and host shutdown
behavior still need an end-to-end test.
