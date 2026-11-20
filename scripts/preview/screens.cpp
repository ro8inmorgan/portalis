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
        std::strcpy(ap.ssid,names[i]); std::strcpy(ap.security,"WPA2"); radar.add(ap);
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
    CarouselMotion motion; motion.reset(0,0);
    for (int frame=0; frame<120; ++frame) {
        view.now=frame*80;
        if (frame && frame%30==0) motion.move(1,view.now);
        view.selected=frame/30; view.carouselPosition=motion.position(view.now);
        save("animation", Screen::Home);
    }
}
