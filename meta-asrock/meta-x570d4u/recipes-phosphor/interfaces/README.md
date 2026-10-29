# BIOS Redfish compatibility

The bmcweb append applies the host-interface TLS and inventory-upload patches.
The USB host interface uses BMC address `169.254.0.17`; requests from the BIOS
at `169.254.0.18` can submit AMI inventory data and boot-complete notifications
at the OEM Redfish endpoint. This unauthenticated compatibility path is scoped
to that host-interface connection and source address. The TLS patch chooses a
separate context for that local address; external management connections use
the regular context. Both contexts obtain their certificate through bmcweb's
certificate handling.

The host firmware may suppress inventory groups when its CRC checks match.
See [host interface](../../recipes-asrock/README.md).
