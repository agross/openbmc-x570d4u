#!/bin/sh
set -e
quiet=false
bus=/sys/bus/i2c/devices/i2c-1
device=/sys/bus/i2c/devices/1-002d
log() {
    [ "$quiet" = true ] || echo "nct6796: $*" >&2
}
has_cpu_temperature() {
    for d in "$device"/hwmon/hwmon*; do
        [ -d "$d" ] || continue
        for label in "$d"/temp*_label; do
            [ -r "$label" ] || continue
            [ "$(cat "$label" 2>/dev/null)" = TSI0_TEMP ] && return 0
        done
    done
    return 1
}
ensure_sensor() {
    recreated=false
    [ -w "$bus/new_device" ] || return 1
    if [ -d "$device" ]; then
        name=$(cat "$device/name" 2>/dev/null) || name=
        [ "$name" = nct6796 ] || return 1
    else
        recreated=true
        log "creating device at $device"
        echo 'nct6796 0x2d' > "$bus/new_device"
        [ -d "$device" ] || return 1
        log "created device at $device"
    fi
    has_cpu_temperature && return 0
    # Host reboot can leave the hwmon directory while removing its attributes.
    log "no readable temperature labels; removing stale device"
    echo 0x2d > "$bus/delete_device"
    return 1
}
monitor_sensor() {
    state=
    while :; do
        quiet=true
        if ensure_sensor; then
            next=available
        else
            next=reattaching
        fi
        quiet=false
        if [ "$next" != "$state" ]; then
            case "$next" in
                available) log "temperature sensor available" ;;
                reattaching) log "temperature sensor unavailable; retrying" ;;
            esac
            state=$next
        elif [ "$recreated" = true ]; then
            log "temperature sensor unavailable; recreating device"
            log "temperature sensor available"
        fi
        sleep 5
    done
}
case "${1-}" in
    --monitor) monitor_sensor ;;
    --quiet) quiet=true; ensure_sensor ;;
    '') ensure_sensor ;;
    *) exit 2 ;;
esac
