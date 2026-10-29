# Host interface and inventory

`host-interface.bb` installs the AMI IPMI provider, SMBIOS importer, inventory
publisher, and USB network setup. The provider accepts the BIOS OEM inventory
notification over KCS and publishes the host-provided BIOS version. The path
unit watches for SMBIOS input, and the inventory publisher exposes received
inventory through OpenBMC interfaces. The package group enables these services.

The BIOS-compatible RNDIS gadget uses a separate virtual USB hub port, leaving
the existing keyboard and mouse gadget in place. systemd-networkd configures
`169.254.0.17/16` on the gadget; the BIOS uses `169.254.0.18`. The
[bmcweb patches](../recipes-phosphor/interfaces/README.md) handle the BIOS
inventory upload. BIOS inventory receipt depends on the host firmware and boot sequence.
