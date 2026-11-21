#pragma once
#include <array>
#include <cstdint>
#include <cstddef>
namespace portalis {
namespace pcap {
constexpr size_t payloadLimit=2500, radioSize=15, snapLength=payloadLimit+radioSize;
constexpr uint32_t fileLimit=32*1024*1024;
inline void le16(uint8_t* out,uint16_t value) { out[0]=value;out[1]=value>>8; }
inline void le32(uint8_t* out,uint32_t value) { for(int i=0;i<4;++i) out[i]=value>>(8*i); }
inline std::array<uint8_t,24> header() {
    std::array<uint8_t,24> out{};le32(out.data(),0xa1b2c3d4);le16(out.data()+4,2);le16(out.data()+6,4);
    le32(out.data()+16,snapLength);le32(out.data()+20,127);return out; // LINKTYPE_IEEE802_11_RADIOTAP
}
inline void recordHeader(uint8_t* out,uint64_t micros,uint16_t captured,uint16_t original,uint8_t channel,int8_t rssi) {
    le32(out,micros/1000000);le32(out+4,micros%1000000);le32(out+8,captured+radioSize);le32(out+12,original+radioSize);
    auto* radio=out+16;
    for(size_t i=0;i<radioSize;++i) radio[i]=0;
    le16(radio+2,radioSize);le32(radio+4,0x2a); // flags, channel, dBm antenna signal
    radio[8]=0x10; // ESP-IDF includes the FCS in sig_len/payload.
    le16(radio+10,channel==14 ? 2484 : 2407+channel*5);le16(radio+12,0x0080); // 2 GHz
    radio[14]=static_cast<uint8_t>(rssi);
}
}
enum class CapturePhase { Ready, Capturing, Saving, Saved, Failed };
struct CaptureStatus {
    CapturePhase phase=CapturePhase::Ready;
    uint8_t channel=1;
    uint32_t packets=0,dropped=0,bytes=0,seconds=0;
    char path[48]={}, message[80]={};
};
} // namespace portalis
