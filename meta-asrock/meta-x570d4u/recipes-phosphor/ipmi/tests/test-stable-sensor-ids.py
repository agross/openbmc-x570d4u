#!/usr/bin/env python3
"""Compile actual provider functions with fake D-Bus/SDR construction boundaries.

Usage: python3 test-stable-sensor-ids.py /path/to/phosphor-ipmi-host/source
Run against the recipe's patched source directory after bitbake --cmd=patch.
"""
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def function(source, start, end):
    return source[source.index(start):source.index(end, source.index(start))]


source = Path(sys.argv[1])
utils = (source / 'dbus-sdr/sdrutils.cpp').read_text()
commands = (source / 'dbus-sdr/sensorcommands.cpp').read_text()
number_map = function(utils, 'bool getSensorNumMap(', '} // namespace details')
info = function(commands, 'static ipmi::RspType<uint8_t, // respcount', '/* end sensor commands */')
sdr = function(commands, 'static int getSensorDataRecord(', '/** @brief implements the get SDR Info')
harness = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>
constexpr uint16_t maxSensorsPerLUN = 255, maxIPMISensors = 765;
constexpr uint16_t lun1Sensor0 = 0x100, lun3Sensor0 = 0x300;
[[maybe_unused]] constexpr uint16_t lun0MaxSensorNum = 254, lun1MaxSensorNum = 510;
constexpr uint16_t lun3MaxSensorNum = 1022, lastRecordIndex = 0xffff;
constexpr int GENERAL_ERROR = -1;
[[maybe_unused]] constexpr uint8_t lun0 = 0, lun1 = 1, lun3 = 3;
struct SensorNumMap {
    using value_type = std::pair<int, std::string>;
    std::map<int, std::string> left;
    std::map<std::string, int> right;
    void insert(value_type v) { left.insert(v); right.emplace(v.second,v.first); }
    bool empty() const { return left.empty(); }
};
using SensorSubTree = std::map<std::string, int>;
auto tree = std::make_shared<SensorSubTree>();
uint16_t generation = 1;
namespace details {
uint16_t getSensorSubtree(std::shared_ptr<SensorSubTree>& out) {
    out = tree; return generation;
}
''' + number_map + r'''
}
namespace lg2 { template<class... T> void error(T...) {} }
namespace get_sdr {
struct SensorDataRecordHeader { uint16_t recordId; uint8_t recordLength; };
struct SensorDataFullRecord {
    SensorDataRecordHeader header;
    uint16_t sensorNum;
    uint8_t key;
};
using SensorDataEventRecord = SensorDataFullRecord;
}
struct SensorValue { static constexpr const char* interface = "value"; };
uint16_t getSensorNumberFromPath(const std::string& path) {
    std::shared_ptr<SensorNumMap> m; details::getSensorNumMap(m);
    return m->right.at(path);
}
size_t otherCount = 2;
bool getSensorSubtree(SensorSubTree& out) { out=*tree; return true; }
constexpr uint32_t sdrLastAdd = 1;
namespace ipmi {
struct InfoResponse { int status; uint8_t count, flags; uint32_t timestamp; };
template<class... T> using RspType = InfoResponse;
InfoResponse responseResponseError() { return {1,0,0,0}; }
InfoResponse responseInvalidFieldRequest() { return {2,0,0,0}; }
InfoResponse responseSuccess(uint8_t count,uint8_t flags,uint32_t timestamp) {
    return {0,count,flags,timestamp};
}
SensorSubTree& getSensorTree() { return *tree; }
struct Context { using ptr = std::shared_ptr<Context>; uint8_t lun = 0; };
uint16_t getNumberOfSensors() { return tree->size(); }
namespace sensor {
constexpr const char* vrInterface = "vr";
size_t getOtherSensorsCount(Context::ptr) { return otherCount; }
int getOtherSensorsDataRecord(Context::ptr, uint16_t id, std::vector<uint8_t>& out) {
    assert(id >= tree->size() && id < tree->size() + otherCount);
    get_sdr::SensorDataRecordHeader h{id, 0};
    out.assign(reinterpret_cast<uint8_t*>(&h), reinterpret_cast<uint8_t*>(&h)+sizeof(h));
    return 0;
}
}
int getSensorConnection(Context::ptr ctx, uint8_t n, std::string& connection,
                        std::string& path, std::vector<std::string>* interfaces) {
    std::shared_ptr<SensorNumMap> m; details::getSensorNumMap(m);
    auto it=m->left.find((ctx->lun<<8)|n);
    if(it==m->left.end()) return 1;
    path=it->second; connection="test"; *interfaces={SensorValue::interface}; return 0;
}
void constructSensorSdrHeaderKey(uint16_t num,uint16_t id,get_sdr::SensorDataFullRecord& r) {
    r.header={id, sizeof(r)-sizeof(r.header)}; r.sensorNum=num;
}
bool constructSensorSdr(Context::ptr, const std::unordered_set<std::string>&,
 uint16_t num,uint16_t id,const std::string&,const std::string&,get_sdr::SensorDataFullRecord& r) {
    constructSensorSdrHeaderKey(num,id,r); return true;
}
void constructEventSdrHeaderKey(uint16_t n,uint16_t id,get_sdr::SensorDataEventRecord& r) {
    constructSensorSdrHeaderKey(n,id,r);
}
bool constructVrSdr(Context::ptr ctx,const std::unordered_set<std::string>& p,
 uint16_t n,uint16_t id,const std::string& s,const std::string& path,get_sdr::SensorDataEventRecord& r) {
    return constructSensorSdr(ctx,p,n,id,s,path,r);
}
''' + sdr + info + r'''
}
const std::string prefix = "/xyz/openbmc_project/sensors/";
void populate(bool pwm, bool missing) {
    tree=std::make_shared<SensorSubTree>(); ++generation;
    for (auto name : {"FAN1","FAN2","FAN3","FAN4_1","FAN5_1","FAN6_1"})
        (*tree)[prefix+"fan_tach/"+name]=0;
    (*tree)[prefix+"temperature/CPU_Temp"]=0;
    (*tree)[prefix+"voltage/12V"]=0;
    if(pwm) for(int n=1;n<=6;++n) (*tree)[prefix+"fan_pwm/Pwm_"+std::to_string(n)]=0;
    if(missing) {
        tree->erase(prefix+"fan_tach/FAN1");
        tree->erase(prefix+"fan_tach/FAN3");
        tree->erase(prefix+"temperature/CPU_Temp");
    }
}
std::vector<int> traverse() {
    auto ctx=std::make_shared<ipmi::Context>();
    std::vector<int> ids; uint16_t id=0;
    do {
        std::vector<uint8_t> data;
        int next=ipmi::getSensorDataRecord(ctx,{},data,id);
        assert(next>=0 && data.size()>=sizeof(get_sdr::SensorDataRecordHeader));
        auto* h=reinterpret_cast<get_sdr::SensorDataRecordHeader*>(data.data());
        ids.push_back(h->recordId);
        if(h->recordId<0x400) {
            auto* r=reinterpret_cast<get_sdr::SensorDataFullRecord*>(data.data());
            assert(r->sensorNum==h->recordId);
        }
        assert(ids.size()<=tree->size()+otherCount);
        id=next;
    } while(id!=lastRecordIndex);
    assert(ids.size()==tree->size()+otherCount);
    assert(std::is_sorted(ids.begin(),ids.end()));
    assert(std::adjacent_find(ids.begin(),ids.end())==ids.end());
    std::vector<uint8_t> last;
    assert(ipmi::getSensorDataRecord(ctx,{},last,lastRecordIndex)==lastRecordIndex);
    assert(reinterpret_cast<get_sdr::SensorDataRecordHeader*>(last.data())->recordId==ids.back());
    return ids;
}
int main() {
    // Real numbering function, repeated tree rebuilds and new process equivalent.
    for(int repeat=0;repeat<3;++repeat) for(bool pwm:{true,false}) for(bool missing:{false,true}) {
        populate(pwm,missing);
        assert(getSensorNumberFromPath(prefix+"fan_tach/FAN6_1")==5);
        assert(getSensorNumberFromPath(prefix+"voltage/12V")==104);
        if(!missing) assert(getSensorNumberFromPath(prefix+"temperature/CPU_Temp")==200);
        if(pwm) assert(getSensorNumberFromPath(prefix+"fan_pwm/Pwm_6")==55);
        auto ids=traverse();
        auto ctx=std::make_shared<ipmi::Context>();
        auto info=ipmi::ipmiSensorGetDeviceSdrInfo(ctx,0);
        assert(info.status==0 && info.count==tree->size() && info.flags==0x81);
        if(missing) assert(ids.front()==1);
        if(pwm) assert(std::find(ids.begin(),ids.end(),55)!=ids.end());
        assert(ids[ids.size()-2]==0x400 && ids.back()==0x401);
    }
    auto ctx=std::make_shared<ipmi::Context>(); std::vector<uint8_t> data;
    assert(ipmi::getSensorDataRecord(ctx,{},data,150)<0);
    assert(ipmi::getSensorDataRecord(ctx,{},data,0x402)<0);
    // Unlisted sensors use another LUN; counts and flags must follow IDs.
    (*tree)[prefix+"current/Extra"]=0; ++generation;
    assert(getSensorNumberFromPath(prefix+"current/Extra")==300);
    auto ids=traverse(); assert(std::find(ids.begin(),ids.end(),300)!=ids.end());
    ctx->lun=1;
    auto info=ipmi::ipmiSensorGetDeviceSdrInfo(ctx,0);
    assert(info.status==0 && info.count==1 && info.flags==0x83);
    ctx->lun=2; info=ipmi::ipmiSensorGetDeviceSdrInfo(ctx,0);
    assert(info.count==0 && info.flags==0x83);
    otherCount=0; traverse();
    tree=std::make_shared<SensorSubTree>(); ++generation;
    assert(ipmi::getSensorDataRecord(ctx,{},data,0)<0);
    otherCount=2; assert(traverse()==std::vector<int>({0x400,0x401}));
    otherCount=std::numeric_limits<size_t>::max();
    assert(ipmi::getSensorDataRecord(ctx,{},data,0)<0);
    puts("PASS: category ranges, fixed IDs, PWM additions, missing sensors, reappearance, sparse SDR traversal, first/last records, non-sensor records, invalid IDs, LUN counts");
}
'''
with tempfile.TemporaryDirectory(prefix='stable-ipmi-ids-') as temp:
    path = Path(temp)
    (path / 'test.cpp').write_text(harness)
    subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++20', '-Wall', '-Wextra', '-Werror', str(path / 'test.cpp'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
