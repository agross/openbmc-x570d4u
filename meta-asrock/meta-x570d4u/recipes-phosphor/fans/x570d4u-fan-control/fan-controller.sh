#!/bin/sh
set -e
controller=
full_speed() {
    [ -n "$controller" ] || return 0
    # Configuration may be invalid: fail safe even when affected channels are unknown.
    for pwm in "$controller"/pwm[1-6]; do
        [ ! -w "$pwm" ] || echo 255 > "$pwm" || :
    done
}
read_temperature() {
    sensor=$1
    labels=$2
    old_ifs=$IFS; IFS=,
    for wanted in $labels; do
        IFS=$old_ifs
        for d in /sys/class/hwmon/hwmon*; do
            [ "$(cat "$d/name" 2>/dev/null)" = "$sensor" ] || continue
            for label in "$d"/temp*_label; do
                [ -r "$label" ] || continue
                [ "$(cat "$label" 2>/dev/null)" = "$wanted" ] || continue
                input=${label%_label}_input
                value=$(cat "$input" 2>/dev/null) || continue
                case "$value" in ''|*[!0-9]*) continue ;; esac
                [ "$value" -gt 0 ] && [ "$value" -lt 125000 ] || continue
                printf '%s\n' "$value"
                return 0
            done
        done
        IFS=,
    done
    IFS=$old_ifs
    return 1
}
curve_pwm() {
    awk -v t="$1" -v curve="$2" 'BEGIN {
        if (t == "unavailable") { print 255; exit }
        n = split(curve, points, ",")
        for (i = 1; i <= n; i++) {
            split(points[i], p, ":"); x = p[1] * 1000; y = p[2]
            if (t <= x) {
                if (i == 1) percent = y
                else percent = lasty + (y - lasty) * (t - lastx) / (x - lastx)
                printf "%d\n", int(percent * 255 / 100 + 0.5); exit
            }
            lastx = x; lasty = y
        }
        printf "%d\n", int(lasty * 255 / 100 + 0.5)
    }'
}
trap full_speed EXIT
trap 'exit 0' TERM INT
for d in /sys/class/hwmon/hwmon*; do
    [ "$(cat "$d/name" 2>/dev/null)" = aspeed_pwm_tacho ] || continue
    controller=$d; break
done
[ -n "$controller" ] || exit 1
policy=$(awk -f /etc/fan-policy.awk /etc/fan-policy.yaml) || exit 1
# Start all configured temperature-controlled fans at full speed.
printf '%s\n' "$policy" | while read -r pwm mode rest; do
    if [ "$mode" = temperature ]; then
        rm -f "/run/fan-status-$pwm"
        echo 255 > "$controller/$pwm"
    fi
done
while :; do
    printf '%s\n' "$policy" | while read -r pwm mode arg1 arg2 arg3; do
        if [ "$mode" = fixed ]; then
            duty=$(( (arg1 * 255 + 50) / 100 ))
        else
            temp=$(read_temperature "$arg1" "$arg2") || temp=unavailable
            duty=$(curve_pwm "$temp" "$arg3")
            status=/run/fan-status-$pwm
            previous=$(cat "$status" 2>/dev/null) || previous=
            if [ "$previous" != "$duty" ]; then
                if [ "$temp" = unavailable ]; then display=unavailable
                else display=$(awk -v t="$temp" 'BEGIN { printf "%.3f C", t / 1000 }'); fi
                echo "$pwm temperature=$display; PWM=$duty/255"
                printf '%s\n' "$duty" > "$status"
            fi
        fi
        echo "$duty" > "$controller/$pwm"
    done
    sleep 5
done
