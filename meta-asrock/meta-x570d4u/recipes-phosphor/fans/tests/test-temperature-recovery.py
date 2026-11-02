#!/usr/bin/env python3
"""Exercise the recovery monitor with device transitions and service failures."""

from pathlib import Path
import subprocess
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / "x570d4u-fan-control/enable-temperature-sensors.sh"


class TemperatureRecoveryTest(unittest.TestCase):
    def monitor(self, states, fail_first_restart=False):
        # Load the actual monitor functions without entering the command dispatcher.
        functions = SCRIPT.read_text().split('case "${1-}" in', 1)[0]
        scenario = "\n".join(
            f"{index}) recreated={str(recreated).lower()}; return {0 if available else 1} ;;"
            for index, (available, recreated) in enumerate(states, 1)
        )
        harness = f"""
iteration=0
restarts=0
ensure_sensor() {{
    iteration=$((iteration + 1))
    case "$iteration" in
        {scenario}
    esac
    exit 99
}}
systemctl() {{
    [ "$*" = 'try-restart xyz.openbmc_project.hwmontempsensor.service' ] || exit 98
    restarts=$((restarts + 1))
    echo restart
    if [ {str(fail_first_restart).lower()} = true ] && [ "$restarts" -eq 1 ]; then
        return 1
    fi
    return 0
}}
sleep() {{
    [ "$iteration" -lt {len(states)} ] || exit 0
}}
monitor_sensor
"""
        result = subprocess.run(["sh"], input=functions + harness, text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        return result.stdout.splitlines().count("restart")

    def test_healthy_device_does_not_restart_service(self):
        self.assertEqual(self.monitor([(True, False)] * 3), 0)

    def test_recreated_device_restarts_once(self):
        self.assertEqual(self.monitor([(True, True), (True, False), (True, False)]), 1)

    def test_waits_for_labels_after_recreation(self):
        self.assertEqual(self.monitor([(False, True), (False, True)]), 0)
        self.assertEqual(self.monitor([(False, True), (False, True), (True, False), (True, False)]), 1)

    def test_recovery_between_polls_restarts_service(self):
        self.assertEqual(self.monitor([(True, False), (True, True), (True, False)]), 1)

    def test_each_outage_refreshes_handles(self):
        self.assertEqual(self.monitor([(True, False), (False, False), (True, True), (False, False), (True, True)]), 2)

    def test_failed_restart_is_retried(self):
        self.assertEqual(self.monitor([(True, True), (True, False), (True, False)], True), 2)


if __name__ == "__main__":
    unittest.main()
