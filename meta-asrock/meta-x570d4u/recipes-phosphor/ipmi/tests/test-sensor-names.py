#!/usr/bin/env python3
"""Compile the actual patched SDR name function; pass the provider source dir."""
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

source = (Path(sys.argv[1]) / 'dbus-sdr/sensorcommands.cpp').read_text()
start = source.index('std::string parseSdrIdFromPath(')
body = source[start:source.index('\nbool getVrEventStatus(', start)]
harness = r'''
#include <array>
#include <cassert>
#include <cstdio>
#include <string>
#include <utility>
#include <boost/algorithm/string/replace.hpp>
constexpr size_t FULL_RECORD_ID_STR_MAX_LENGTH = 16;
''' + body + r'''
int main() {
    const std::string prefix = "/xyz/openbmc_project/sensors/";
    assert(parseSdrIdFromPath(prefix + "voltage/VBAT") == "BAT");
    // All other voltage names match Prometheus on 2026-10-10.
    for (const auto* name : {"3VSB", "5VSB", "VCPU", "VSOC", "VCCM",
                            "APU_VDDP", "PM_VDD_CLDO", "PM_VDDCR_S5",
                            "PM_VDDCR", "3V", "5V", "12V"})
        assert(parseSdrIdFromPath(prefix + "voltage/" + name) == name);
    for (const auto* name : {"CPU_Temp", "MB_Temp", "Card_Side_Temp",
                            "X570_Temp", "Onboard_LAN_Temp", "MB_Aux_Temp",
                            "LAN_Local_Temp"}) {
        std::string expected = name;
        boost::replace_all(expected, "_", " ");
        assert(parseSdrIdFromPath(prefix + "temperature/" + name) == expected);
    }
    assert(parseSdrIdFromPath(prefix + "temperature/DDR4_A1_Temp") == "DDR4_A1_Temp");
    assert(parseSdrIdFromPath(prefix + "fan_tach/FAN4_1") == "FAN4_1");
    assert(parseSdrIdFromPath(prefix + "temperature/VBAT") == "VBAT");
    assert(parseSdrIdFromPath(prefix + "voltage/Other_VBAT") == "Other_VBAT");
    assert(parseSdrIdFromPath("/other/VBAT") == "VBAT");
    assert(parseSdrIdFromPath(prefix + "voltage/Long_Sensor_Name_123") == "Long_Sensor_Name");
    puts("PASS: historical voltage/temperature names, exact-path scope, unchanged fan/DIMM names and truncation");
}
'''
with tempfile.TemporaryDirectory(prefix='ipmi-sensor-names-') as temp:
    path = Path(temp)
    (path / 'test.cpp').write_text(harness)
    subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++20', '-Wall',
                    '-Wextra', '-Werror', *shlex.split(os.environ.get('CPPFLAGS', '')),
                    str(path / 'test.cpp'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
