#include "ui.h"
#include <iostream>
using namespace portalis;
int main() {
    M5Canvas canvas; ViewModel view;
    view.sdReady = true; view.batteryPercent = 84; view.now = 850;
    auto save = [&](const char* name, Screen screen) {
        std::cout << "[\"frame\",\"" << name << "\"]\n";
        view.screen = screen; drawUi(canvas, view);
    };
    save("home", Screen::Home);
    view.running = true; view.clients = 3; view.count = 12; view.seconds = 183;
    save("dashboard", Screen::Live);
    RadarSnapshot radar;
    const char* names[] = {"Demo Lab", "Guest Wi-Fi", "Workshop", "Office"};
    for (int i=0; i<4; ++i) {
        RadarAccessPoint ap; ap.id=100+i; ap.rssi=-42-i*12; ap.channel=1+i*3;
        std::strcpy(ap.ssid,names[i]); std::strcpy(ap.security,"WPA2"); radar.add(ap); radar.tally(ap);
    }
    radar.total=4; view.radar=&radar;
    save("radar", Screen::Radar);
    ProbeConfig probe; probe.noticePath="/feedback.html"; probe.timeoutSeconds=120;
    view.probeConfig=&probe; view.selected=1;
    save("karma", Screen::Probe);
    view.selected=2;
    save("settings", Screen::Settings);
    SubmissionView submission; submission.event={1,42};
    submission.lines={"rating:","5", "feedback:","Easy to connect!"};
    view.submission=&submission; view.selected=0;
    save("submission", Screen::Submission);
    view.selected=0;
    save("channels", Screen::Channels);
    view.selected=0; view.rememberedNetworks[0]=true;
    save("wifi-picker", Screen::ToolNetworks);
    view.networkName="Demo Lab"; view.passwordLength=12;
    save("wifi-join", Screen::NetworkPassword);
    SignalHistory signal;signal.select(radar.points[0],0);
    for (int i=0;i<60;++i) signal.samples.add(i==25 ? kMissingSample : -64+static_cast<int>(std::sin(i*0.19f)*13));
    view.signal=&signal;
    save("signal", Screen::Signal);
    NetworkStatus net;net.phase=NetworkPhase::Ready;net.dhcp=net.gatewayCheck=net.dns=net.internet=CheckState::Pass;
    std::strcpy(net.ssid,"Demo Lab");std::strcpy(net.address,"192.168.1.42");std::strcpy(net.target,"1.1.1.1");
    view.network=&net;
    save("diagnostics", Screen::Diagnostics);
    for (int i=0;i<60;++i) net.ping.add(i%19!=0,22+static_cast<int>(std::abs(std::sin(i*0.3f))*42));
    net.pingMode=true;
    save("ping", Screen::Ping);
    NetworkChanges changes;radar.generation=1;changes.update(radar);
    radar.generation=2;radar.updatedAt=123000;radar.points[0].channel=6;changes.update(radar);
    view.changes=&changes;
    save("changes", Screen::Changes);
    CarouselMotion motion; motion.reset(0,0);
    for (int frame=0; frame<(CarouselMotion::count+1)*30; ++frame) {
        view.now=frame*80;
        if (frame && frame%30==0) motion.move(1,view.now);
        view.selected=(frame/30)%CarouselMotion::count; view.carouselPosition=motion.position(view.now);
        save("animation", Screen::Home);
    }
}
