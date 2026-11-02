#!/bin/sh
# Systemd invokes this before swampd starts and after every exit, including crashes.
# Leave every available channel safe even if the policy cannot be loaded.
set -eu
for controller in /sys/class/hwmon/hwmon*; do
    [ "$(cat "$controller/name" 2>/dev/null)" = aspeed_pwm_tacho ] || continue
    result=0
    for pwm in "$controller"/pwm[1-6]; do
        [ -e "$pwm" ] || continue
        echo 255 > "$pwm" || result=1
    done
    exit "$result"
done
echo 'ASPEED PWM controller unavailable' >&2
exit 1
