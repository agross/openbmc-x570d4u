#!/bin/bash
set -euo pipefail

get_running_version() {
  ssh -o BatchMode=yes -o ConnectTimeout=3 "$host" bash -s <<'REMOTE_VERSION'
set -euo pipefail

object=$(busctl get-property xyz.openbmc_project.Software.Manager \
  /xyz/openbmc_project/software/bmc \
  xyz.openbmc_project.Association.Definitions Associations |
  sed -n 's/.*"functional" "software_version" "\([^"]*\)".*/\1/p')
[[ -n $object ]]
busctl get-property xyz.openbmc_project.Software.Manager "$object" \
  xyz.openbmc_project.Software.Version Version |
  sed -n 's/^s "\(.*\)"$/\1/p'
REMOTE_VERSION
}

if [[ $# -ne 2 ]]; then
  echo 'Usage: ./deploy.sh ROOT_SSH_HOST UPDATE_ARCHIVE' >&2
  exit 2
fi

host=$1
archive=$2

[[ -f $archive && -s $archive ]] || {
  echo 'Update archive not found or empty' >&2
  exit 1
}

manifest=$(tar -xOf "$archive" MANIFEST)
version=$(printf '%s\n' "$manifest" | sed -n 's/^version=//p')
machine=$(printf '%s\n' "$manifest" | sed -n 's/^MachineName=//p')
purpose=$(printf '%s\n' "$manifest" | sed -n 's/^purpose=//p')
[[ -n $version &&
  $machine == x570d4u &&
  $purpose == xyz.openbmc_project.Software.Version.VersionPurpose.BMC ]] || {
  echo 'Archive is not an X570D4U BMC update' >&2
  exit 1
}

members=$(tar -tf "$archive")
for image in image-u-boot image-kernel image-rofs image-rwfs; do
  grep -Fx "$image" <<<"$members" >/dev/null || {
    echo "Archive is missing $image" >&2
    exit 1
  }
done

ssh "$host" 'test "$(id -u)" = 0' || {
  echo 'Root SSH access is required' >&2
  exit 1
}

old_boot=$(ssh "$host" 'cat /proc/sys/kernel/random/boot_id')
current=$(get_running_version)

[[ $current != "$version" ]] || {
  echo 'Image version matches the running version; update cannot be verified' >&2
  exit 1
}

[[ -n $current ]] || {
  echo 'Could not determine the running firmware version' >&2
  exit 1
}

remote=$(ssh "$host" 'mktemp /tmp/openbmc-update.XXXXXXXX')
[[ "$remote" =~ ^/tmp/openbmc-update\.[[:alnum:]]+$ ]] || {
  echo 'Invalid remote temporary path' >&2
  exit 1
}

trap 'ssh "$host" "rm -f $remote" >/dev/null 2>&1 || :' EXIT
ssh "$host" "cat > $remote" <"$archive"

local_hash=$(shasum -a 256 "$archive" | awk '{print $1}')
remote_hash=$(ssh "$host" "sha256sum $remote" | awk '{print $1}')
[[ $local_hash == "$remote_hash" ]] || {
  echo 'Upload checksum mismatch' >&2
  exit 1
}

result=$(
  ssh "$host" bash -s -- "$remote" <<'REMOTE'
set -euo pipefail
archive=$1
whitelist=/run/initramfs/whitelist
[[ -f $whitelist && -w $whitelist ]] || {
  echo 'Settings preservation list unavailable' >&2
  exit 1
}

# The running initramfs owns preservation during this update.
paths=(
  /etc/passwd
  /etc/group
  /etc/shadow
  /etc/gshadow
  /etc/hostname
  /etc/fan-policy.yaml
  /etc/dropbear
  /etc/ssl/certs/https
  /etc/systemd/network
  /etc/ipmi_pass
  /home
  /var/lib/obmc
  /var/lib/phosphor-user-manager
  /var/lib/phosphor-settings-manager
  /var/lib/phosphor-state-manager
  /var/lib/phosphor-inventory-manager
)
for path in "${paths[@]}"; do
  grep -Fxq "$path" "$whitelist" || printf '%s\n' "$path" >> "$whitelist"
done
object=$(busctl get-property xyz.openbmc_project.Software.Manager \
  /xyz/openbmc_project/software/bmc \
  xyz.openbmc_project.Association.Definitions Associations |
  sed -n 's/.*"functional" "software_version" "\([^"]*\)".*/\1/p')
[[ -n $object ]] || {
  echo 'Cannot find active BMC software object' >&2
  exit 1
}

exec 3<"$archive"
busctl call xyz.openbmc_project.Software.Manager "$object" \
  xyz.openbmc_project.Software.Update StartUpdate hs 3 \
  xyz.openbmc_project.Software.ApplyTime.RequestedApplyTimes.OnReset
REMOTE
)

object=$(printf '%s\n' "$result" | sed -n 's/^o "\([^"]*\)"$/\1/p')
[[ "$object" =~ ^/xyz/openbmc_project/software/[A-Za-z0-9_]+$ ]] || {
  echo "Unexpected updater response: $result" >&2
  exit 1
}

prepared=false
for ((attempt = 0; attempt < 60; attempt++)); do
  state=$(ssh "$host" \
    "busctl get-property xyz.openbmc_project.Software.Manager $object xyz.openbmc_project.Software.Activation Activation" \
    2>/dev/null) || state=
  case "$state" in
    *Activations.Active*)
      prepared=true
      break
      ;;
    *Activations.Failed* | *Activations.Invalid*)
      echo "Update failed: $state" >&2
      exit 1
      ;;
  esac
  sleep 2
done

[[ $prepared == true ]] || {
  echo 'Update did not reach Active; BMC was not rebooted. The update may still apply at the next reset.' >&2
  exit 1
}

# Static-flash updater copies the images to initramfs before reporting Active.
for image in image-u-boot image-kernel image-rofs image-rwfs; do
  local_hash=$(tar -xOf "$archive" "$image" | shasum -a 256 | awk '{print $1}')
  remote_hash=$(ssh "$host" "sha256sum /run/initramfs/$image" | awk '{print $1}')
  [[ $local_hash == "$remote_hash" ]] || {
    echo "Prepared $image checksum mismatch; BMC was not rebooted" >&2
    exit 1
  }
done

echo "Update prepared at $object; requesting BMC reboot"
ssh "$host" 'systemctl reboot' || true

for ((attempt = 0; attempt < 120; attempt++)); do
  sleep 5
  boot=$(ssh -o BatchMode=yes -o ConnectTimeout=3 "$host" \
    'cat /proc/sys/kernel/random/boot_id' 2>/dev/null) || continue
  [[ $boot != "$old_boot" ]] || continue
  installed=$(get_running_version 2>/dev/null) || continue
  [[ $installed == "$version" ]] || {
    echo "BMC restarted with unexpected firmware: $installed" >&2
    exit 1
  }
  echo "BMC restarted with firmware $installed"
  exit 0
done

echo 'Timed out waiting for the BMC and firmware verification' >&2
exit 1
