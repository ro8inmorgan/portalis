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
    view.selected=0;view.rangeStart="192.168.1.1";view.rangeEnd="192.168.1.254";
    view.scanHost="192.168.1.10";view.scanPortRange="1-1024";
    RangeScanStatus hosts;hosts.phase=ScanPhase::Complete;hosts.checked=hosts.total=254;hosts.found=hosts.count=4;
    hosts.hosts[0]={0xc0a80101,3};hosts.hosts[1]={0xc0a8010a,7};hosts.hosts[2]={0xc0a8012a,2};hosts.hosts[3]={0xc0a80164,14};
    PortScanStatus ports;ports.phase=ScanPhase::Complete;ports.host=0xc0a8010a;ports.checked=ports.total=1024;ports.found=ports.count=3;
    ports.ports[0]=22;ports.ports[1]=80;ports.ports[2]=443;
    view.rangeScan=&hosts;view.portScan=&ports;
    save("ip-range-setup",Screen::RangeSetup);view.selected=1;save("ip-range-results",Screen::RangeResults);
    view.selected=2;save("port-scan-setup",Screen::PortSetup);view.selected=0;save("port-scan-results",Screen::PortResults);
    SshConfig ssh;ssh.host="192.168.1.10";ssh.user="robin";ssh.auth=SshAuth::PrivateKey;
    SshStatus sshStatus;TerminalBuffer terminal;
    view.sshConfig=&ssh;view.sshStatus=&sshStatus;view.terminal=&terminal;view.selected=3;
    save("ssh-setup",Screen::SshSetup);
    sshStatus.phase=SshPhase::VerifyHost;
    std::strcpy(sshStatus.fingerprint,"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
    save("ssh-host-key",Screen::SshSession);
    const std::string shell="Welcome to demo-server\r\n\r\nrobin@demo-server:~$ uname -s\r\nLinux\r\nrobin@demo-server:~$ uptime\r\n 14:32:10 up 3 days, load average: 0.12\r\nrobin@demo-server:~$ ls\r\nprojects  notes  backups\r\nrobin@demo-server:~$ ";
    terminal.feed(shell.data(),shell.size());sshStatus.phase=SshPhase::Active;save("ssh-terminal",Screen::SshSession);
    CaptureStatus capture;view.capture=&capture;view.captureChannel=6;view.selected=1;
    save("capture-setup",Screen::CaptureSetup);
    capture.phase=CapturePhase::Capturing;capture.channel=6;capture.packets=12345;capture.dropped=17;capture.bytes=2413500;capture.seconds=83;
    std::strcpy(capture.path,"/captures/cap-01234567.pcap");save("capture-live",Screen::CaptureLive);
    ScriptCatalog scripts;scripts.names[0]="counter.py";scripts.names[1]="greeting.py";scripts.names[2]="hello.py";scripts.count=3;
    view.scripts=&scripts;view.selected=2;save("python-scripts",Screen::PythonScripts);
    PythonStatus python;python.phase=PythonPhase::Done;std::strcpy(python.name,"hello.py");
    TerminalBuffer pythonTerminal;const std::string pythonOutput="Hello from MicroPython!\r\nSquares: [0, 1, 4, 9, 16, 25, 36, 49, 64, 81]\r\nSquare root of 81: 9.0\r\nPython heap free: 48720 bytes\r\n";
    pythonTerminal.feed(pythonOutput.data(),pythonOutput.size());view.python=&python;view.pythonTerminal=&pythonTerminal;
    save("python-output",Screen::PythonOutput);
    for(const auto& icon: {std::pair<int,const char*>{PortScanHome,"port-scan-icon"},{SettingsHome,"settings-icon"},{SshHome,"ssh-icon"},{CaptureHome,"capture-icon"},{PythonHome,"python-icon"}}) {
        view.selected=icon.first;view.carouselPosition=icon.first;save(icon.second,Screen::Home);
    }
    CarouselMotion motion; motion.reset(0,0);
    for (int frame=0; frame<(CarouselMotion::count+1)*30; ++frame) {
        view.now=frame*80;
        if (frame && frame%30==0) motion.move(1,view.now);
        view.selected=(frame/30)%CarouselMotion::count; view.carouselPosition=motion.position(view.now);
        save("animation", Screen::Home);
    }
}
