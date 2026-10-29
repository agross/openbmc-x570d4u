# ASRock Rack X570D4U OpenBMC customizations

This repository overlays the upstream OpenBMC X570D4U machine layer at commit
`5c21ebb175489fad79b64fea2628d073dec5043e`. It is not a complete OpenBMC checkout.
Stock firmware was investigated using the [REA reverse-engineering skill](https://github.com/morluto/rea).

## Improvements over upstream

- Board monitoring: NCT6796 CPU and board temperatures, four DIMM temperatures,
  twelve voltage rails, and all six fan-header sensor objects. IPMI keeps the
  historical temperature names. An unpopulated fan can report unavailable;
  battery voltage is omitted until its acquisition is understood.
- Corrected FAN4–FAN6 tachometer channels and PWM names matching the physical
  headers. A configurable fan controller supports temperature curves, fixed
  duties, and full-speed fallback if temperature readings fail.
- UID LED control through the standard OpenBMC identify interface, including
  changes made using the physical button.
- AMI BIOS compatibility: host-provided BIOS version and inventory, SMBIOS
  import, and the USB network interface used by the BIOS to upload inventory.
- A distinct firmware version identifies builds with this overlay.

The included fan policy reflects one system's fan population and cooling needs.
Review it for your hardware before use. See the recipe documentation for
[fan control](meta-asrock/meta-x570d4u/recipes-phosphor/fans/README.md),
[sensors](meta-asrock/meta-x570d4u/recipes-phosphor/configuration/README.md),
[UID LED](meta-asrock/meta-x570d4u/recipes-phosphor/leds/README.md),
[kernel changes](meta-asrock/meta-x570d4u/recipes-kernel/linux/README.md),
[host interface](meta-asrock/meta-x570d4u/recipes-asrock/README.md), and
[bmcweb](meta-asrock/meta-x570d4u/recipes-phosphor/interfaces/README.md).

## Build with Docker

Use Docker and a non-root Unix user. The build downloads upstream sources and
needs substantial free disk space, memory, and time. Run these commands from the
root of this repository. Docker builds for its native platform.

Build the container and firmware, then copy the resulting artifacts to `output/`:

```sh
./build.sh
```

Each build creates a fresh source tree from the pinned revision in
`openbmc-revision`, applies the overlay, and removes the source tree afterwards.
The upstream Git repository, downloads, shared state, and build output remain
in a Docker volume for reuse.

## Deploy to the BMC

Provide a root SSH destination and the update archive from `output/`. The
local machine needs Bash, SSH, `tar`, and `shasum`; the BMC needs Bash and
`busctl`. Use an archive containing the four partition images (`image-u-boot`,
`image-kernel`, `image-rofs`, and `image-rwfs`):

```sh
./deploy.sh root@bmc.example output/UPDATE_ARCHIVE.tar
```

The script uploads and verifies the archive, prepares it for the next reset,
verifies the prepared images, and reboots the BMC. It waits for the BMC to
return and checks the running firmware version. If preparation fails after the
update is accepted, it may still apply at the next BMC reset. The host is not
rebooted. The new firmware version must differ from the running version so the
script can verify the result. Set a new version in
[os-release.bbappend](meta-asrock/meta-x570d4u/recipes-core/os-release/os-release.bbappend)
before rebuilding when necessary.

Validate the image and your board's recovery procedure before flashing. Upstream
updates require reviewing the overlay and patches against the new revision; the
build script rejects an unapproved revision.
