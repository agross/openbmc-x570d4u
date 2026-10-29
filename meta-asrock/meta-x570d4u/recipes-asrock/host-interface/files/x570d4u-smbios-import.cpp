#include <systemd/sd-bus.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string_view>
#include <vector>

namespace
{
constexpr const char* inputPath = "/var/lib/x570d4u-bios-version/host-uploads/Smbios.dat";
constexpr const char* outputPath = "/var/lib/smbios/smbios2";

bool convert(const std::vector<uint8_t>& source, std::vector<uint8_t>& output)
{
    if (source.size() < 32 || std::string_view(reinterpret_cast<const char*>(source.data()), 4) != "_SM_" ||
        source[5] < 31 || source[5] > 32 || source[6] != 3 ||
        (source[7] != 0 && (source[7] < 2 || source[7] > 8)) ||
        std::string_view(reinterpret_cast<const char*>(source.data() + 16), 5) != "_DMI_")
    {
        return false;
    }
    uint8_t checksum = 0;
    for (size_t i = 0; i < source[5]; ++i) { checksum += source[i]; }
    if (checksum != 0) { return false; }
    checksum = 0;
    for (size_t i = 16; i < 31; ++i) { checksum += source[i]; }
    const size_t length = source[22] | (size_t(source[23]) << 8);
    if (checksum != 0 || length == 0 || length > source.size() - 32 || length + 24 > 32768)
    {
        return false;
    }
    size_t at = 32;
    const size_t end = at + length;
    bool endMarker = false;
    while (at + 4 <= end)
    {
        const size_t formatted = source[at + 1];
        if (formatted < 4 || formatted > end - at) { return false; }
        size_t next = at + formatted;
        while (next + 1 < end && (source[next] != 0 || source[next + 1] != 0)) { ++next; }
        if (next + 1 >= end) { return false; }
        if (source[at] == 127) { endMarker = true; break; }
        at = next + 2;
    }
    if (!endMarker) { return false; }
    const uint32_t dataSize = static_cast<uint32_t>(length + 24);
    output.assign(10 + dataSize, 0);
    output[0] = 1;
    output[1] = 2;
    for (unsigned i = 0; i < 4; ++i) { output[6 + i] = static_cast<uint8_t>(dataSize >> (8 * i)); }
    constexpr std::array<uint8_t, 5> anchor{'_', 'S', 'M', '3', '_'};
    for (size_t i = 0; i < anchor.size(); ++i) { output[10 + i] = anchor[i]; }
    output[16] = 24;
    output[17] = source[6];
    output[18] = source[7];
    output[20] = 1;
    for (unsigned i = 0; i < 4; ++i) { output[22 + i] = static_cast<uint8_t>(length >> (8 * i)); }
    output[26] = 24;
    checksum = 0;
    for (size_t i = 10; i < 34; ++i) { checksum += output[i]; }
    output[15] = static_cast<uint8_t>(0 - checksum);
    std::copy(source.begin() + 32, source.begin() + end, output.begin() + 34);
    return true;
}
}

int main()
{
    std::ifstream input(inputPath, std::ios::binary);
    if (!input)
    {
        return std::filesystem::exists(inputPath) ? 1 : 0;
    }
    const std::vector<uint8_t> source((std::istreambuf_iterator<char>(input)), {});
    std::vector<uint8_t> converted;
    if (!convert(source, converted))
    {
        std::cerr << "Invalid or unsupported SMBIOS upload\n";
        return 1;
    }
    std::error_code ec;
    std::filesystem::create_directories("/var/lib/smbios", ec);
    if (ec) { return 1; }
    const std::string temporary = std::string(outputPath) + ".new";
    {
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        if (!file.write(reinterpret_cast<const char*>(converted.data()), converted.size()) ||
            !file.flush()) { return 1; }
    }
    std::filesystem::rename(temporary, outputPath, ec);
    if (ec) { return 1; }
    sd_bus* bus = nullptr;
    sd_bus_message* reply = nullptr;
    sd_bus_error error = SD_BUS_ERROR_NULL;
    int result = sd_bus_open_system(&bus);
    if (result >= 0)
    {
        result = sd_bus_call_method(bus, "xyz.openbmc_project.Smbios.MDR_V2",
            "/xyz/openbmc_project/Smbios/MDR_V2", "xyz.openbmc_project.Smbios.MDR_V2",
            "AgentSynchronizeData", &error, &reply, "");
    }
    int imported = 0;
    if (result >= 0) { result = sd_bus_message_read(reply, "b", &imported); }
    if (result < 0 || !imported)
    {
        std::cerr << "SMBIOS import failed: " << (error.message ? error.message : "import rejected") << '\n';
    }
    sd_bus_error_free(&error);
    sd_bus_message_unref(reply);
    sd_bus_unref(bus);
    return result < 0 || !imported ? 1 : 0;
}
