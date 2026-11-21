#include <unity.h>
#include "../../src/packet_capture.cpp"
using namespace portalis;
void setUp() {
    SD.reset();WiFi={};fakeWriteLimit=static_cast<size_t>(-1);fakeMicros=0;
    fakeTaskFail=false;fakeRadioFail=false;fakeTaskTick=nullptr;fakeRandom=0;
}
void tearDown() {}
uint32_t u32(const std::string& data,size_t at) {
    uint32_t out=0;for(int i=0;i<4;++i) out|=uint32_t(uint8_t(data.at(at+i)))<<(8*i);return out;
}
void frame(size_t size=30,wifi_promiscuous_pkt_type_t type=WIFI_PKT_MGMT,uint8_t error=0) {
    wifi_promiscuous_pkt_t packet;packet.rx_ctrl.sig_len=size;packet.rx_ctrl.channel=6;packet.rx_ctrl.rssi=-42;packet.rx_ctrl.rx_state=error;
    packet.payload[0]=0x80; // beacon fixture, opaque body sufficient for serialization tests
    fakePromiscuousCallback(&packet,type);
}
void finish(PacketCapture& capture) {capture.stop();runFakeTask();capture.poll();}
void test_pcap_radiotap_lengths_timestamp_and_frame() {
    PacketCapture capture;TEST_ASSERT_TRUE(capture.start(6));fakeMicros=1234567;frame();finish(capture);
    const auto& status=capture.status();TEST_ASSERT_EQUAL(CapturePhase::Saved,status.phase);
    auto data=SD.nodes.at(status.path)->content;
    TEST_ASSERT_EQUAL_HEX32(0xa1b2c3d4,u32(data,0));TEST_ASSERT_EQUAL(127,u32(data,20));
    TEST_ASSERT_EQUAL(pcap::snapLength,u32(data,16));TEST_ASSERT_EQUAL(24+16+15+30,data.size());
    TEST_ASSERT_EQUAL(1,u32(data,24));TEST_ASSERT_EQUAL(234567,u32(data,28));
    TEST_ASSERT_EQUAL(45,u32(data,32));TEST_ASSERT_EQUAL(45,u32(data,36));
    TEST_ASSERT_EQUAL(0x2a,u32(data,44));TEST_ASSERT_EQUAL(0x10,uint8_t(data[48]));
    TEST_ASSERT_EQUAL(2437,uint8_t(data[50])|(uint16_t(uint8_t(data[51]))<<8));
    TEST_ASSERT_EQUAL(-42,int8_t(data[54]));TEST_ASSERT_EQUAL_HEX8(0x80,data[55]);
    TEST_ASSERT_EQUAL(1,status.packets);TEST_ASSERT_EQUAL(WIFI_OFF,WiFi.getMode());TEST_ASSERT_FALSE(fakePromiscuous);TEST_ASSERT_NULL(fakePromiscuousCallback);
}
void test_queue_overflow_truncation_and_stop_drain() {
    PacketCapture capture;TEST_ASSERT_TRUE(capture.start(6));
    frame(30,WIFI_PKT_MISC);frame(30,WIFI_PKT_DATA,1); // metadata-only and invalid frames excluded
    for(int i=0;i<10;++i) frame(3000,WIFI_PKT_DATA);
    finish(capture);TEST_ASSERT_EQUAL(8,capture.status().packets);TEST_ASSERT_EQUAL(2,capture.status().dropped);
    auto data=SD.nodes.at(capture.status().path)->content;
    TEST_ASSERT_EQUAL(2515,u32(data,32));TEST_ASSERT_EQUAL(3015,u32(data,36));
    TEST_ASSERT_EQUAL(24+8*(16+2515),data.size());
}
void test_radio_sd_allocation_failures_and_no_overwrite() {
    PacketCapture capture;
    WiFi.currentMode=WIFI_STA;TEST_ASSERT_FALSE(capture.start(1));TEST_ASSERT_EQUAL(WIFI_STA,WiFi.currentMode);WiFi.currentMode=WIFI_OFF;
    SD.present=false;TEST_ASSERT_FALSE(capture.start(1));SD.present=true;
    fakeTaskFail=true;TEST_ASSERT_FALSE(capture.start(1));TEST_ASSERT_FALSE(capture.busy());TEST_ASSERT_EQUAL(WIFI_OFF,WiFi.getMode());fakeTaskFail=false;
    fakeRadioFail=true;TEST_ASSERT_FALSE(capture.start(1));TEST_ASSERT_FALSE(fakePromiscuous);fakeRadioFail=false;
    SD.seed("/captures/cap-00000001.pcap","keep");fakeRandom=0;
    TEST_ASSERT_TRUE(capture.start(1));finish(capture);
    TEST_ASSERT_EQUAL_STRING("keep",SD.nodes.at("/captures/cap-00000001.pcap")->content.c_str());
    TEST_ASSERT_EQUAL_STRING("/captures/cap-00000002.pcap",capture.status().path);
    TEST_ASSERT_EQUAL(24,SD.nodes.at(capture.status().path)->content.size());
}
void test_sd_short_write_stops_capture() {
    PacketCapture capture;TEST_ASSERT_TRUE(capture.start(1));frame();fakeWriteLimit=3;finish(capture);
    TEST_ASSERT_EQUAL(CapturePhase::Failed,capture.status().phase);TEST_ASSERT_EQUAL(0,capture.status().packets);
    TEST_ASSERT_EQUAL(1,capture.status().dropped);TEST_ASSERT_FALSE(capture.busy());TEST_ASSERT_EQUAL(WIFI_OFF,WiFi.getMode());
}
void test_file_limit_closes_at_record_boundary() {
    PacketCapture capture;TEST_ASSERT_TRUE(capture.start(6));frame(2500);
    fakeTaskTick=[] { if(fakePromiscuousCallback) frame(2500);fakeMicros+=1000; };
    runFakeTask();capture.poll();fakeTaskTick=nullptr;
    TEST_ASSERT_EQUAL(CapturePhase::Saved,capture.status().phase);
    TEST_ASSERT_EQUAL((pcap::fileLimit-24)/(16+pcap::snapLength),capture.status().packets);
    TEST_ASSERT_LESS_OR_EQUAL(pcap::fileLimit,capture.status().bytes);
    TEST_ASSERT_EQUAL(24+capture.status().packets*(16+pcap::snapLength),SD.nodes.at(capture.status().path)->content.size());
    TEST_ASSERT_NOT_NULL(std::strstr(capture.status().message,"limit"));TEST_ASSERT_FALSE(capture.busy());
}
int main() {UNITY_BEGIN();RUN_TEST(test_file_limit_closes_at_record_boundary);RUN_TEST(test_pcap_radiotap_lengths_timestamp_and_frame);RUN_TEST(test_queue_overflow_truncation_and_stop_drain);RUN_TEST(test_radio_sd_allocation_failures_and_no_overwrite);RUN_TEST(test_sd_short_write_stops_capture);return UNITY_END();}
