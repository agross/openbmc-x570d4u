#include <ipmid/api.hpp>
#include <systemd/sd-journal.h>

#include <cstdint>

namespace
{
constexpr ipmi::NetFn asrockNetFn = 0x3a;
constexpr ipmi::Cmd getKcsAccess = 0x81;

ipmi::RspType<uint8_t> getKcsAccessState()
{
    sd_journal_send("MESSAGE=ASRock IPMI GetKcsAccess requested", "PRIORITY=6", nullptr);
    // This platform always permits host KCS access; it does not implement
    // the stock firmware's optional access-disable control.
    return ipmi::responseSuccess(uint8_t{1});
}

void registerKcsAccess() __attribute__((constructor));

void registerKcsAccess()
{
    ipmi::registerHandler(ipmi::prioOemBase, asrockNetFn, getKcsAccess,
                          ipmi::Privilege::User, getKcsAccessState);
}
} // namespace
