#include <gpiod.h>
#include <systemd/sd-bus.h>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <unistd.h>

static constexpr const char* groupInterface = "xyz.openbmc_project.Led.Group";
static constexpr const char* identifyPath = "/xyz/openbmc_project/led/groups/enclosure_identify";
static constexpr const char* bootPath = "/xyz/openbmc_project/led/groups/bmc_booted";
static volatile sig_atomic_t stopping = 0;
static void stop(int) { stopping = 1; }

struct Controller
{
    gpiod_chip* chip = nullptr;
    gpiod_line* sense = nullptr;
    gpiod_line* button = nullptr;
    sd_bus* bus = nullptr;
    bool senseRequested = false;
    bool buttonRequested = false;
    bool cached = false;
    bool booted = false;

    ~Controller()
    {
        if (buttonRequested)
        {
            // Release the active-low button, then restore its input direction.
            gpiod_line_set_value(button, 1);
            gpiod_line_release(button);
            if (gpiod_line_request_input(button, "uid-led-release") == 0)
                gpiod_line_release(button);
        }
        if (senseRequested) gpiod_line_release(sense);
        if (chip) gpiod_chip_close(chip);
        sd_bus_unref(bus);
    }

    int actual() const
    {
        int raw = gpiod_line_get_value(sense);
        return raw < 0 ? -1 : !raw;
    }

    void publish(bool value)
    {
        if (cached != value)
        {
            cached = value;
            sd_bus_emit_properties_changed(bus, identifyPath, groupInterface,
                                           "Asserted", nullptr);
        }
    }
};

static int getIdentify(sd_bus*, const char*, const char*, const char*,
                       sd_bus_message* reply, void* data, sd_bus_error* error)
{
    auto& c = *static_cast<Controller*>(data);
    int value = c.actual();
    if (value < 0)
        return sd_bus_error_setf(error, SD_BUS_ERROR_IO_ERROR,
                                 "Cannot read UID LED feedback");
    return sd_bus_message_append(reply, "b", value);
}

static int setIdentify(sd_bus*, const char*, const char*, const char*,
                       sd_bus_message* message, void* data, sd_bus_error* error)
{
    auto& c = *static_cast<Controller*>(data);
    int requested = 0;
    int r = sd_bus_message_read(message, "b", &requested);
    if (r < 0) return r;
    int before = c.actual();
    if (before < 0)
        return sd_bus_error_setf(error, SD_BUS_ERROR_IO_ERROR,
                                 "Cannot read UID LED feedback; refusing to toggle");
    if (before == requested) return 0;

    // This board has a latched UID circuit: pulse the button only on mismatch.
    if (gpiod_line_set_value(c.button, 0) < 0)
        return sd_bus_error_setf(error, SD_BUS_ERROR_IO_ERROR,
                                 "Cannot assert UID button");
    usleep(200000);
    if (gpiod_line_set_value(c.button, 1) < 0)
        return sd_bus_error_setf(error, SD_BUS_ERROR_IO_ERROR,
                                 "Cannot release UID button");
    for (int i = 0; i < 50; ++i)
    {
        int value = c.actual();
        if (value == requested)
        {
            c.publish(value);
            std::fprintf(stderr, "UID LED %s confirmed by GPIO A0\n",
                         value ? "on" : "off");
            return 1;
        }
        usleep(10000);
    }
    int value = c.actual();
    if (value >= 0) c.publish(value);
    return sd_bus_error_setf(error, SD_BUS_ERROR_IO_ERROR,
                             "UID LED did not reach requested state");
}

// The existing boot target asserts this logical group. The heartbeat LED
// remains driven by its kernel timer; this group does not touch the UID LED.
static int getBoot(sd_bus*, const char*, const char*, const char*,
                   sd_bus_message* reply, void* data, sd_bus_error*)
{
    return sd_bus_message_append(reply, "b",
                                 static_cast<Controller*>(data)->booted);
}
static int setBoot(sd_bus*, const char*, const char*, const char*,
                   sd_bus_message* message, void* data, sd_bus_error*)
{
    int value = 0;
    int r = sd_bus_message_read(message, "b", &value);
    if (r < 0) return r;
    auto& c = *static_cast<Controller*>(data);
    if (c.booted == bool(value)) return 0;
    c.booted = value;
    sd_bus_emit_properties_changed(c.bus, bootPath, groupInterface,
                                   "Asserted", nullptr);
    return 1;
}
static const sd_bus_vtable identifyVtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_WRITABLE_PROPERTY("Asserted", "b", getIdentify, setIdentify, 0,
                             SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_VTABLE_END
};
static const sd_bus_vtable bootVtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_WRITABLE_PROPERTY("Asserted", "b", getBoot, setBoot, 0,
                             SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_VTABLE_END
};

int main()
{
    Controller c;
    c.chip = gpiod_chip_open_by_name("gpiochip0");
    if (!c.chip) { std::perror("gpiochip0"); return 1; }
    c.sense = gpiod_chip_get_line(c.chip, 0);
    c.button = gpiod_chip_get_line(c.chip, 22);
    if (!c.sense || !c.button || !gpiod_line_name(c.sense) ||
        !gpiod_line_name(c.button) ||
        std::strcmp(gpiod_line_name(c.sense), "input-locatorled-n") ||
        std::strcmp(gpiod_line_name(c.button), "control-locatorbutton-n"))
    {
        std::fprintf(stderr, "Unexpected UID GPIO mapping; refusing GPIO writes\n");
        return 1;
    }
    if (gpiod_line_request_input(c.sense, "x570d4u-uid-state") < 0)
    { std::perror("UID state request"); return 1; }
    c.senseRequested = true;
    if (gpiod_line_request_output_flags(c.button, "x570d4u-uid-button",
             GPIOD_LINE_REQUEST_FLAG_OPEN_DRAIN, 1) < 0)
    { std::perror("UID button request"); return 1; }
    c.buttonRequested = true;
    int value = c.actual();
    if (value < 0) { std::perror("UID state read"); return 1; }
    c.cached = value;

    int r = sd_bus_open_system(&c.bus);
    if (r >= 0) r = sd_bus_add_object_manager(c.bus, nullptr,
                                             "/xyz/openbmc_project/led/groups");
    if (r >= 0) r = sd_bus_add_object_vtable(c.bus, nullptr, identifyPath,
                            groupInterface, identifyVtable, &c);
    if (r >= 0) r = sd_bus_add_object_vtable(c.bus, nullptr, bootPath,
                            groupInterface, bootVtable, &c);
    if (r >= 0) r = sd_bus_request_name(c.bus,
                              "xyz.openbmc_project.LED.GroupManager", 0);
    if (r < 0)
    { std::fprintf(stderr, "D-Bus initialization: %s\n", std::strerror(-r)); return 1; }
    sd_bus_emit_object_added(c.bus, identifyPath);
    sd_bus_emit_object_added(c.bus, bootPath);
    std::signal(SIGTERM, stop);
    std::signal(SIGINT, stop);
    std::fprintf(stderr, "UID controller ready; initial LED %s\n",
                 c.cached ? "on" : "off");
    while (!stopping)
    {
        while ((r = sd_bus_process(c.bus, nullptr)) > 0) {}
        if (r < 0) return 1;
        value = c.actual();
        if (value < 0)
        { std::perror("UID feedback read"); return 1; }
        c.publish(value);
        r = sd_bus_wait(c.bus, 250000);
        if (r < 0 && r != -EINTR) return 1;
    }
    return 0;
}
