#!/bin/bash
set -euo pipefail

if [[ ${1-} == --container ]]; then
  mkdir -p /work
  chown builder /work
  exec {lock_fd}>/work/.build.lock
  if ! flock --exclusive --nonblock "$lock_fd"; then
    echo 'Another build is using the OpenBMC volume; wait for it to finish.' >&2
    exit 1
  fi

  exec gosu builder /bin/bash /adjustments/build.sh --build
fi

if [[ ${1-} == --build ]]; then
  revision=$(cat /adjustments/openbmc-revision)

  [[ "$revision" =~ ^[0-9a-f]{40}$ ]] || {
    echo 'Invalid OpenBMC revision' >&2
    exit 1
  }

  # Keep the pristine upstream repository separate from generated sources.
  if [[ ! -d /work/upstream.git ]]; then
    upstream=$(mktemp -d /work/upstream.XXXXXXXX)
    trap 'rm -rf "$upstream"' EXIT
    git clone --bare https://github.com/openbmc/openbmc.git "$upstream"
    mv "$upstream" /work/upstream.git
    trap - EXIT
  fi

  if ! git --git-dir=/work/upstream.git cat-file -e "$revision^{commit}"; then
    git --git-dir=/work/upstream.git fetch origin
  fi

  # Recreate the source tree at a stable path for BitBake caches.
  rm -rf /work/source

  source_dir=$(mktemp -d /work/source.XXXXXXXX)
  trap 'rm -rf "$source_dir"' EXIT
  git clone --shared --no-checkout /work/upstream.git "$source_dir"
  git -C "$source_dir" checkout --detach "$revision"
  mv "$source_dir" /work/source
  source_dir=/work/source

  cp --recursive /adjustments/meta-asrock/. "$source_dir/meta-asrock/"
  cd "$source_dir"
  build_dir="/work/build-clean-$(uname -m)"

  set +u
  source ./setup x570d4u "$build_dir"
  set -u

  # Reuse the earlier build's persistent downloads and shared state.
  if ! grep -Fq '# openbmc-build-cache' conf/local.conf; then
    cat >>conf/local.conf <<'CACHE'
# openbmc-build-cache
DL_DIR = "/work/build/downloads"
SSTATE_DIR = "/work/build/sstate-cache"
CACHE
  fi
  if ! grep -Eq '^BB_HASHSERVE_DB_DIR[[:space:]]*[?:+]?=' conf/local.conf; then
    printf '%s\n' 'BB_HASHSERVE_DB_DIR = "${SSTATE_DIR}"' >>conf/local.conf
  fi

  # Give each image a distinct UTC version while retaining the upstream revision.
  build_timestamp=$(date --utc +%Y-%m-%d-%H-%M-%S)
  upstream_revision=$(git --git-dir=/work/upstream.git describe --tags --long "$revision")

  # Avoid exhausting memory when several recipes compile at once.
  cat >conf/openbmc-build.conf <<CONFIG
BB_NUMBER_THREADS = "4"
PARALLEL_MAKE = "-j4"
X570D4U_UPSTREAM_REVISION = "$upstream_revision"
X570D4U_BUILD_TIMESTAMP = "$build_timestamp"
CONFIG
  if ! grep -Fxq 'require conf/openbmc-build.conf' conf/local.conf; then
    printf '%s\n' 'require conf/openbmc-build.conf' >>conf/local.conf
  fi

  bitbake obmc-phosphor-image
  cp --recursive "$build_dir/tmp/deploy/images/x570d4u/." /output/
  exit
fi

[[ $# -eq 0 ]] || {
  echo 'Usage: ./build.sh' >&2
  exit 2
}

root=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$root/output"
docker build \
  --tag openbmc-x570d4u \
  --build-arg BUILD_UID="$(id -u)" \
  --build-arg BUILD_GID="$(id -g)" \
  "$root"

docker run \
  --rm \
  --cap-add SYS_ADMIN \
  --cap-add SYS_NICE \
  --volume openbmc-x570d4u:/work \
  --volume "$root:/adjustments:ro" \
  --volume "$root/output:/output" \
  openbmc-x570d4u
