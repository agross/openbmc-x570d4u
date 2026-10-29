# Validate the supported YAML subset before the controller changes any PWM.
BEGIN { ok = 1 }
/^[[:space:]]*(#|$)/ { next }
{
    if ($1 !~ /^pwm[1-6]:$/ || seen[$1]++) { ok = 0; next }
    pwm = $1; sub(/:$/, "", pwm)
    if ($2 == "fixed" && NF == 3 && $3 ~ /^[0-9]+$/ && $3 <= 100) {
        lines[++count] = pwm " fixed " $3
    } else if ($2 == "temperature" && NF == 5 &&
               $3 ~ /^[A-Za-z0-9_-]+$/ &&
               $4 ~ /^[A-Za-z0-9_-]+(,[A-Za-z0-9_-]+)*$/) {
        n = split($5, points, ","); previous = -1
        if (n < 2) ok = 0
        for (i = 1; i <= n; i++) {
            if (points[i] !~ /^[0-9]+:[0-9]+$/) { ok = 0; continue }
            split(points[i], point, ":")
            if (point[1] <= previous || point[1] >= 125 || point[2] > 100) ok = 0
            previous = point[1]
        }
        lines[++count] = pwm " temperature " $3 " " $4 " " $5
    } else ok = 0
}
END {
    if (!count || !ok) { print "Invalid fan policy" > "/dev/stderr"; exit 1 }
    for (i = 1; i <= count; i++) print lines[i]
}
