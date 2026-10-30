#include <systemd/sd-bus.h>

#include <nlohmann/json.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <string_view>
#include <unistd.h>
#include <utility>
#include <vector>

namespace
{
constexpr const char* root = "/xyz/openbmc_project/inventory";
constexpr const char* rfInventory = "/var/lib/x570d4u-bios-uploads/rf-inventory.json";
constexpr const char* smbiosFile = "/var/lib/x570d4u-bios-uploads/Smbios.dat";
constexpr const char* service = "xyz.openbmc_project.Inventory.Ami";

enum class Kind : uint8_t
{
    cpu = 1,
    dimm = 8,
    pcie = 0x20,
    chassis = 0x30,
    bmc = 0x33,
};

struct Item
{
    std::string path;
    std::string name;
    Kind kind;
    nlohmann::json properties = nlohmann::json::object();
};

std::vector<Item> items;

int getName(sd_bus*, const char*, const char*, const char*, sd_bus_message* m,
            void* userdata, sd_bus_error*)
{
    return sd_bus_message_append(m, "s", static_cast<Item*>(userdata)->name.c_str());
}

int getPresent(sd_bus*, const char*, const char*, const char*, sd_bus_message* m,
               void*, sd_bus_error*)
{
    return sd_bus_message_append(m, "b", 1);
}

const sd_bus_vtable itemVtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_PROPERTY("PrettyName", "s", getName, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("Present", "b", getPresent, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_VTABLE_END};
int getStringProperty(sd_bus*, const char*, const char*, const char* property,
                      sd_bus_message* m, void* userdata, sd_bus_error*)
{
    const auto& data = static_cast<Item*>(userdata)->properties;
    const auto found = data.find(property);
    const std::string value = found != data.end() && found->is_string() ?
        found->get<std::string>() : std::string{};
    return sd_bus_message_append(m, "s", value.c_str());
}

uint64_t numericProperty(const Item& item, const char* property)
{
    const auto found = item.properties.find(property);
    if (found == item.properties.end() || !found->is_number_integer())
    {
        return 0;
    }
    if (found->is_number_unsigned())
    {
        return found->get<uint64_t>();
    }
    const int64_t value = found->get<int64_t>();
    return value < 0 ? 0 : static_cast<uint64_t>(value);
}

int getUint16Property(sd_bus*, const char*, const char*, const char* property,
                      sd_bus_message* m, void* userdata, sd_bus_error*)
{
    const uint64_t n = numericProperty(*static_cast<Item*>(userdata), property);
    return sd_bus_message_append(m, "q", static_cast<uint16_t>(std::min<uint64_t>(n, UINT16_MAX)));
}
int getUint32Property(sd_bus*, const char*, const char*, const char* property,
                      sd_bus_message* m, void* userdata, sd_bus_error*)
{
    const uint64_t n = numericProperty(*static_cast<Item*>(userdata), property);
    return sd_bus_message_append(m, "u", static_cast<uint32_t>(std::min<uint64_t>(n, UINT32_MAX)));
}
int getUint64Property(sd_bus*, const char*, const char*, const char* property,
                      sd_bus_message* m, void* userdata, sd_bus_error*)
{
    return sd_bus_message_append(m, "t", numericProperty(*static_cast<Item*>(userdata), property));
}

const sd_bus_vtable cpuVtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_PROPERTY("Socket", "s", getStringProperty, 0, 0),
    SD_BUS_PROPERTY("CoreCount", "q", getUint16Property, 0, 0),
    SD_BUS_PROPERTY("ThreadCount", "q", getUint16Property, 0, 0),
    SD_BUS_PROPERTY("MaxSpeedInMhz", "u", getUint32Property, 0, 0),
    SD_BUS_VTABLE_END};
const sd_bus_vtable dimmVtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_PROPERTY("MemoryDeviceLocator", "s", getStringProperty, 0, 0),
    SD_BUS_PROPERTY("MemorySizeInKB", "t", getUint64Property, 0, 0),
    SD_BUS_PROPERTY("MemoryDataWidth", "q", getUint16Property, 0, 0),
    SD_BUS_PROPERTY("MemoryTotalWidth", "q", getUint16Property, 0, 0),
    SD_BUS_PROPERTY("MaxMemorySpeedInMhz", "q", getUint16Property, 0, 0),
    SD_BUS_PROPERTY("MemoryConfiguredSpeedInMhz", "q", getUint16Property, 0, 0),
    SD_BUS_VTABLE_END};
const sd_bus_vtable assetVtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_PROPERTY("Manufacturer", "s", getStringProperty, 0, 0),
    SD_BUS_PROPERTY("Model", "s", getStringProperty, 0, 0),
    SD_BUS_PROPERTY("PartNumber", "s", getStringProperty, 0, 0),
    SD_BUS_PROPERTY("SerialNumber", "s", getStringProperty, 0, 0),
    SD_BUS_PROPERTY("SKU", "s", getStringProperty, 0, 0),
    SD_BUS_VTABLE_END};
const sd_bus_vtable markerVtable[] = {SD_BUS_VTABLE_START(0), SD_BUS_VTABLE_END};

std::string safe(std::string value)
{
    for (char& c : value)
    {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9')))
        {
            c = '_';
        }
    }
    return value.empty() ? "unnamed" : value;
}

std::string stringValue(const nlohmann::json& value)
{
    if (value.is_string())
    {
        return value.get<std::string>();
    }
    if (value.is_number_integer())
    {
        return std::to_string(value.get<int64_t>());
    }
    if (value.is_number_unsigned())
    {
        return std::to_string(value.get<uint64_t>());
    }
    return {};
}

std::string label(const nlohmann::json& object,
                  std::initializer_list<std::string_view> keys)
{
    for (std::string_view key : keys)
    {
        const auto found = object.find(key);
        if (found != object.end())
        {
            std::string value = stringValue(*found);
            if (!value.empty())
            {
                return value;
            }
        }
    }
    return {};
}

void add(std::vector<Item>& parsed, std::set<std::string>& paths, Kind kind,
         std::string_view group, std::string name,
         nlohmann::json properties = nlohmann::json::object())
{
    if (name.empty())
    {
        return;
    }
    std::string path = std::string(root) + "/system/" + std::string(group) + "/" + safe(name);
    for (size_t suffix = 2; !paths.insert(path).second; ++suffix)
    {
        path = std::string(root) + "/system/" + std::string(group) + "/" + safe(name) + "_" +
               std::to_string(suffix);
    }
    parsed.push_back({std::move(path), std::move(name), kind, std::move(properties)});
}

void collect(const nlohmann::json& value, Kind kind,
             std::vector<Item>& parsed, std::set<std::string>& paths)
{
    if (value.is_array())
    {
        for (const nlohmann::json& child : value)
        {
            collect(child, kind, parsed, paths);
        }
        return;
    }
    if (!value.is_object() || label(value, {"Id"}).empty() ||
        label(value, {"Name"}).empty())
    {
        return;
    }

    const std::string name = kind == Kind::cpu ?
        label(value, {"Socket", "Name", "Model", "Id", "ProcessorId"}) :
        kind == Kind::dimm ? label(value, {"DeviceLocator", "Name", "Id", "PartNumber"}) :
                             label(value, {"Name", "Model", "Id", "PartNumber"});
    if (!name.empty())
    {
        const char* group = kind == Kind::cpu ? "cpu" : kind == Kind::dimm ? "dimm" : "pcie";
        nlohmann::json props = nlohmann::json::object();
        for (const char* key : {"Manufacturer", "Model", "PartNumber", "SerialNumber", "Socket"})
        {
            if (value.contains(key))
            {
                props[key] = value[key];
            }
        }
        const auto copyNumber = [&](const char* from, const char* to) {
            if (value.contains(from) && value[from].is_number_integer())
            {
                props[to] = value[from];
            }
        };
        if (kind == Kind::cpu)
        {
            copyNumber("TotalCores", "CoreCount");
            copyNumber("TotalThreads", "ThreadCount");
            copyNumber("MaxSpeedMHz", "MaxSpeedInMhz");
        }
        else if (kind == Kind::dimm)
        {
            props["MemoryDeviceLocator"] = name;
            copyNumber("DataWidthBits", "MemoryDataWidth");
            copyNumber("BusWidthBits", "MemoryTotalWidth");
            copyNumber("OperatingSpeedMhz", "MemoryConfiguredSpeedInMhz");
            if (value.contains("CapacityMiB") && value["CapacityMiB"].is_number_integer())
            {
                const auto capacity = value["CapacityMiB"].get<int64_t>();
                if (capacity >= 0 && static_cast<uint64_t>(capacity) <= UINT64_MAX / 1024)
                {
                    props["MemorySizeInKB"] = static_cast<uint64_t>(capacity) * 1024;
                }
            }
        }
        add(parsed, paths, kind, group, name, std::move(props));
        return;
    }
}

void findCollections(const nlohmann::json& value, std::vector<Item>& parsed,
                     std::set<std::string>& paths)
{
    if (value.is_array())
    {
        for (const nlohmann::json& child : value)
        {
            findCollections(child, parsed, paths);
        }
        return;
    }
    if (!value.is_object())
    {
        return;
    }
    for (const auto& child : value.items())
    {
        if (child.key() == "Processors")
        {
            collect(child.value(), Kind::cpu, parsed, paths);
        }
        else if (child.key() == "Memory")
        {
            collect(child.value(), Kind::dimm, parsed, paths);
        }
        else if (child.key() == "PCIeDevices")
        {
            collect(child.value(), Kind::pcie, parsed, paths);
        }
        else
        {
            findCollections(child.value(), parsed, paths);
        }
    }
}

bool loadRfInventory(std::vector<Item>& parsed)
{
    std::ifstream input(rfInventory, std::ios::binary);
    nlohmann::json inventory = nlohmann::json::parse(input, nullptr, false);
    if (inventory.is_discarded() || !inventory.is_object() ||
        !inventory.contains("Systems") || !inventory.contains("Chassis"))
    {
        return false;
    }

    std::set<std::string> paths;
    findCollections(inventory.at("Systems"), parsed, paths);
    findCollections(inventory.at("Chassis"), parsed, paths);
    const std::string board = label(inventory.at("Chassis"), {"Model", "Name", "PartNumber", "Id"});
    if (!board.empty())
    {
        add(parsed, paths, Kind::chassis, "chassis", board);
    }
    return !parsed.empty();
}

bool load(std::vector<Item>& parsed, bool& fromRfInventory)
{
    parsed.clear();
    fromRfInventory = loadRfInventory(parsed);
    if (std::filesystem::exists(smbiosFile))
    {
        std::erase_if(parsed, [](const Item& item) {
            return item.kind == Kind::cpu || item.kind == Kind::dimm ||
                   item.kind == Kind::chassis;
        });
    }
    return !parsed.empty();
}

const char* interfaceFor(Kind kind)
{
    switch (kind)
    {
        case Kind::cpu:
            return "xyz.openbmc_project.Inventory.Item.Cpu";
        case Kind::dimm:
            return "xyz.openbmc_project.Inventory.Item.Dimm";
        case Kind::pcie:
            return "xyz.openbmc_project.Inventory.Item.PCIeDevice";
        case Kind::chassis:
            return "xyz.openbmc_project.Inventory.Item.Chassis";
        case Kind::bmc:
            return "xyz.openbmc_project.Inventory.Item.Bmc";
    }
    return "xyz.openbmc_project.Inventory.Item";
}
} // namespace

int main()
{
    bool fromRfInventory = false;
    load(items, fromRfInventory);
    items.push_back({std::string(root) + "/bmc/ASPEED_AST2500", "ASPEED AST2500", Kind::bmc,
                     {{"Manufacturer", "ASPEED Technology"}, {"PartNumber", "AST2500"},
                      {"Model", "AST2500"}}});
    std::error_code timestampError;
    std::filesystem::file_time_type rfTimestamp =
        std::filesystem::last_write_time(rfInventory, timestampError);
    const bool hadRfInventory = !timestampError;
    std::error_code smbiosError;
    const auto smbiosTimestamp = std::filesystem::last_write_time(smbiosFile, smbiosError);
    const bool hadSmbios = !smbiosError;

    sd_bus* bus = nullptr;
    if (sd_bus_open_system(&bus) < 0 ||
        sd_bus_add_object_manager(bus, nullptr, root) < 0 ||
        sd_bus_request_name(bus, service, 0) < 0)
    {
        return 1;
    }
    for (Item& item : items)
    {
        const auto* kindVtable = item.kind == Kind::cpu ? cpuVtable :
                                 item.kind == Kind::dimm ? dimmVtable : markerVtable;
        if (sd_bus_add_object_vtable(bus, nullptr, item.path.c_str(),
                                     "xyz.openbmc_project.Inventory.Item", itemVtable, &item) < 0 ||
            sd_bus_add_object_vtable(bus, nullptr, item.path.c_str(),
                                     interfaceFor(item.kind), kindVtable, &item) < 0 ||
            sd_bus_add_object_vtable(bus, nullptr, item.path.c_str(),
                                     "xyz.openbmc_project.Inventory.Decorator.Asset", assetVtable, &item) < 0)
        {
            return 1;
        }
        sd_bus_emit_object_added(bus, item.path.c_str());
    }
    for (;;)
    {
        while (sd_bus_process(bus, nullptr) > 0)
        {}
        sd_bus_wait(bus, 5ULL * 1000ULL * 1000ULL);
        std::error_code currentError;
        const std::filesystem::file_time_type current =
            std::filesystem::last_write_time(rfInventory, currentError);
        std::error_code smbiosCurrentError;
        const auto smbiosCurrent = std::filesystem::last_write_time(smbiosFile, smbiosCurrentError);
        if ((!smbiosCurrentError &&
             (!hadSmbios || smbiosCurrent != smbiosTimestamp)) ||
            (!fromRfInventory && !currentError &&
             (!hadRfInventory || current != rfTimestamp)) ||
            (fromRfInventory && !currentError && current != rfTimestamp))
        {
            return 0;
        }
    }
}
