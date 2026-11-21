#include "ui.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace portalis {
namespace {
uint16_t BG = 0x0843, PANEL = 0x10E6, ACCENT = 0x6F99;
constexpr uint16_t FG = 0xF7BE, MUTED = 0x9CF3, GOLD = 0xFD6A;
constexpr int W = 240, H = 135;
// Large fallback snapshots must not live on the 8 KB Arduino loop stack.
// drawUi reserves its frame before selecting a screen, including the main menu.
template<class T> const T& emptyView() {
    static const T value{};
    return value;
}
void text(M5Canvas& c, const std::string& value, int x, int y, uint16_t color = FG, float size = 2) {
    c.setTextColor(color); c.setTextSize(size); c.setTextDatum(top_left); c.drawString(value.c_str(), x, y);
}
std::string shorten(const std::string& s, size_t width) { return s.size() <= width ? s : s.substr(0, width - 2) + ".."; }
void ticker(M5Canvas& c, const std::string& value, int x, int y, int width, uint32_t time, uint16_t color = FG, float size = 2) {
    c.setTextSize(size);
    const int overflow = c.textWidth(value.c_str()) - width;
    int offset = 0;
    if (overflow > 0) {
        const uint32_t travel = overflow * 30;
        const uint32_t phase = time % (travel + 2400);
        offset = phase < 1200 ? 0 : (phase < 1200 + travel ? (phase - 1200) / 30 : overflow);
    }
    c.setClipRect(x, y, width, static_cast<int>(8 * size));
    text(c, value, x - offset, y, color, size);
    c.clearClipRect();
}
void footer(M5Canvas& c, const std::string& value) {
    c.fillRect(0, 116, W, 19, PANEL); text(c, value, 6, 120, MUTED, 1.5f);
}
void title(M5Canvas& c, const char* label) { text(c, label, 6, 4, ACCENT); c.drawFastHLine(6, 23, 228, PANEL); }
void selectedRow(M5Canvas& c, const std::string& value, int row, bool active, uint32_t time) {
    const int y = 28 + row * 21;
    if (active) c.fillRoundRect(3, y - 2, 234, 20, 3, ACCENT);
    text(c, active ? ">" : " ", 6, y, active ? BG : MUTED);
    if (active) ticker(c, value, 22, y, 210, time, BG);
    else text(c, shorten(value, 17), 22, y, FG);
}
void wrapped(M5Canvas& c, const std::string& value, int y, int maxRows, uint16_t color = FG) {
    size_t offset = 0;
    for (int row = 0; row < maxRows && offset < value.size(); ++row) {
        while (offset < value.size() && value[offset] == ' ') ++offset;
        size_t length = std::min(size_t(19), value.size() - offset);
        if (offset + length < value.size()) {
            const size_t space = value.rfind(' ', offset + length);
            if (space != std::string::npos && space > offset) length = space - offset;
        }
        text(c, value.substr(offset, length), 6, y + row * 18, color);
        offset += length;
    }
}
void orbit(M5Canvas& c, int x, int y, uint32_t now, bool active) {
    c.drawCircle(x, y, 9, active ? ACCENT : MUTED);
    c.drawCircle(x, y, 4, active ? ACCENT : MUTED);
    if (!active) return;
    const float angle = now * 0.002f;
    c.fillCircle(x + static_cast<int>(9 * std::cos(angle)), y + static_cast<int>(9 * std::sin(angle)), 2, FG);
}
uint16_t blendColor(uint16_t base, uint16_t accent, float amount) {
    const auto channel = [amount](int a, int b) { return static_cast<uint16_t>(a + (b - a) * amount); };
    return (channel((base >> 11) & 31, (accent >> 11) & 31) << 11) |
           (channel((base >> 5) & 63, (accent >> 5) & 63) << 5) |
           channel(base & 31, accent & 31);
}
void counter(M5Canvas& c, int x, int width, const char* label, const std::string& value,
             uint16_t numberColor, uint16_t accent, float pulse, bool strong) {
    pulse = std::max(0.0f, std::min(1.0f, pulse));
    const uint16_t fill = blendColor(PANEL, accent, pulse * (strong ? 0.32f : 0.20f));
    c.fillRoundRect(x, 46, width, 46, 5, blendColor(PANEL, accent, pulse * 0.85f));
    const int inset = pulse > 0.5f ? 2 : 1;
    c.fillRoundRect(x + inset, 46 + inset, width - 2 * inset, 46 - 2 * inset, 4, fill);
    text(c, label, x + 6, 50, blendColor(MUTED, FG, pulse), 1.5f);
    text(c, value, x + 6, 66 - static_cast<int>(pulse * 2),
         blendColor(numberColor, FG, pulse), 3 + pulse * (strong ? 0.25f : 0.15f));
}
template<size_t N> void graph(M5Canvas& c, const SampleHistory<N>& data, int low, int high, int y, int height) {
    c.fillRoundRect(6,y,228,height,3,PANEL);
    if (data.count<1 || high<=low) return;
    auto px=[](size_t i) { return 8+static_cast<int>(i*223/(N-1)); };
    auto py=[&](int value) { return y+height-3-(std::max(low,std::min(high,value))-low)*(height-6)/(high-low); };
    for (size_t i=0;i<data.count;++i) {
        if (data.values[i]==kMissingSample) { c.fillRect(px(i),y+height-3,2,2,GOLD); continue; }
        if (i && data.values[i-1]!=kMissingSample) c.drawLine(px(i-1),py(data.values[i-1]),px(i),py(data.values[i]),ACCENT);
        else c.fillCircle(px(i),py(data.values[i]),1,ACCENT);
    }
}
const char* checkName(CheckState state) {
    switch (state) { case CheckState::Pass:return "OK"; case CheckState::NoReply:return "No reply"; case CheckState::Error:return "Error"; default:return "Checking"; }
}
void battery(M5Canvas& c, int percent) {
    const bool known = percent >= 0;
    percent = std::max(0, std::min(100, percent));
    const uint16_t color = !known ? MUTED : percent <= 20 ? GOLD : ACCENT;
    c.fillRoundRect(172, 6, 18, 11, 2, color);
    c.fillRect(190, 9, 2, 5, color);
    c.fillRect(174, 8, 14, 7, BG);
    if (known) c.fillRect(174, 8, percent * 14 / 100, 7, color);
    text(c, known ? std::to_string(percent) + "%" : "--%", 196, 5, color, 1.5f);
}
std::string countLabel(uint32_t value) {
    if (value < 10000) return std::to_string(value);
    char out[16];
    if (value < 100000) std::snprintf(out, sizeof(out), "%.1fk", value / 1000.0);
    else if (value < 1000000) std::snprintf(out, sizeof(out), "%.0fk", value / 1000.0);
    else if (value < 10000000) std::snprintf(out, sizeof(out), "%.1fM", value / 1000000.0);
    else if (value < 1000000000) std::snprintf(out, sizeof(out), "%.0fM", value / 1000000.0);
    else std::snprintf(out, sizeof(out), "%.1fB", value / 1000000000.0);
    return out;
}
}
void drawUi(M5Canvas& c, const ViewModel& v) {
    UiSettings settings = v.settings; settings.validate();
    BG = kBackgrounds[settings.background].background; PANEL = kBackgrounds[settings.background].panel;
    ACCENT = kAccents[settings.accent].color;
    c.clearClipRect(); c.fillSprite(BG); c.setFont(&fonts::Font0); c.setTextWrap(false); c.setTextDatum(top_left);
    const uint32_t elapsed = v.now - v.screenSince;
    switch (v.screen) {
        case Screen::Splash: {
            const float reveal = std::min(1.0f, elapsed / 350.0f);
            orbit(c, 120, 28, v.now, true);
            const uint16_t logo = blendColor(BG, ACCENT, reveal);
            text(c, "PORTALIS", 24, 54, logo, 4);
            // A clipped highlight moves across the existing lettering.
            const int sweep = -48 + static_cast<int>(std::min(elapsed, kSplashDuration) * 312 / kSplashDuration);
            const int left = std::max(24, sweep), right = std::min(216, sweep + 48);
            if (right > left) {
                c.setClipRect(left, 54, right - left, 32);
                text(c, "PORTALIS", 24, 54, blendColor(logo, FG, reveal), 4);
                c.clearClipRect();
            }
            text(c, "LOCAL CONNECTION PORTAL", 21, 98, blendColor(BG, MUTED, reveal), 1.5f);
            c.fillRect(24, 120, 192, 2, PANEL);
            c.fillRect(24, 120, std::min(elapsed, kSplashDuration) * 192 / kSplashDuration, 2, ACCENT);
            break;
        }
        case Screen::Home: {
            title(c, "PORTALIS");
            const char* labels[] = {"Portal", "Wi-Fi radar", "KARMA", "Channels", "Signal tracker", "Diagnostics", "Ping monitor", "Network changes", "IP range scan", "Host port scan", "SSH terminal", "Settings"};
            c.setClipRect(0, 26, 240, 66);
            for (int icon = 0; icon < CarouselMotion::count; ++icon) {
                float delta = icon - v.carouselPosition;
                while (delta > CarouselMotion::count / 2.0f) delta -= CarouselMotion::count;
                while (delta < -CarouselMotion::count / 2.0f) delta += CarouselMotion::count;
                if (std::abs(delta)>2.0f) continue;
                const float focus = std::max(0.0f, 1.0f - std::abs(delta));
                const int cx = 120 + static_cast<int>(delta * 91);
                const int cy = 58 + static_cast<int>((1.0f - focus) * 4);
                const int width = 54 + static_cast<int>(focus * 14);
                const float breathe = 0.5f + 0.5f * std::sin((v.now % 3200) * (6.28318530718f / 3200.0f));
                const uint16_t fill = blendColor(PANEL, ACCENT, focus * (0.14f + breathe * 0.06f));
                const uint16_t ink = blendColor(MUTED, ACCENT, focus);
                c.fillRoundRect(cx - width / 2, cy - 28, width, 56, 9, fill);
                if (icon == 0) {
                    // A light band travels through the doorway and fades at its edges.
                    const float phase = (v.now % 2600) / 2600.0f;
                    c.fillRoundRect(cx - 15, cy - 20, 30, 40, 8, ink);
                    c.fillRoundRect(cx - 11, cy - 16, 22, 36, 5, fill);
                    for (int trail = 0; trail < 5; ++trail) {
                        const int y = -24 + static_cast<int>(phase * 52) - trail;
                        if (y >= -12 && y <= 16) {
                            const float fade = std::min(1.0f, std::min((y + 13) / 6.0f, (17 - y) / 6.0f));
                            c.drawLine(cx - 8, cy + y, cx + 8, cy + y,
                                       blendColor(fill, ink, fade * (1.0f - trail / 5.0f)));
                        }
                    }
                    c.fillCircle(cx + 6, cy + 2, 2, blendColor(ink, FG, breathe));
                    c.drawLine(cx - 20, cy + 20, cx + 20, cy + 20, ink);
                } else if (icon == 1) {
                    constexpr float tau = 6.28318530718f;
                    const float angle = (v.now % 4000) * (tau / 4000.0f);
                    c.drawCircle(cx, cy, 19, ink);
                    c.drawCircle(cx, cy, 11, blendColor(fill, ink, 0.45f));
                    for (int trail = 5; trail >= 0; --trail) {
                        const float sweep = angle - trail * 0.11f;
                        c.drawLine(cx, cy, cx + static_cast<int>(18 * std::cos(sweep)),
                                   cy + static_cast<int>(18 * std::sin(sweep)),
                                   blendColor(fill, ink, 1.0f - trail / 6.0f));
                    }
                    for (int blip = 0; blip < 3; ++blip) {
                        const float bearing = 0.8f + blip * 2.1f;
                        const float age = std::fmod(angle - bearing + tau, tau) / tau;
                        const float glow = std::max(0.0f, 1.0f - age * 3.0f);
                        const int radius = blip == 1 ? 9 : 14;
                        c.fillCircle(cx + static_cast<int>(radius * std::cos(bearing)),
                                     cy + static_cast<int>(radius * std::sin(bearing)), 1 + (glow > 0.6f),
                                     blendColor(ink, FG, glow));
                    }
                } else if (icon == 2) {
                    // Two staggered rings expand from the antenna and fade away.
                    for (int ring = 0; ring < 2; ++ring) {
                        const float phase = ((v.now % 2400 + ring * 1200) % 2400) / 2400.0f;
                        c.drawCircle(cx, cy - 3, 4 + static_cast<int>(phase * 18),
                                     blendColor(fill, ink, 1.0f - phase));
                    }
                    c.drawLine(cx, cy + 1, cx, cy + 20, ink);
                    c.drawLine(cx - 12, cy + 20, cx + 12, cy + 20, ink);
                    c.fillCircle(cx, cy - 3, 3, blendColor(ink, FG, breathe));
                } else if (icon == RangeScanHome) {
                    c.drawLine(cx-16,cy,cx+16,cy,ink);
                    c.drawLine(cx,cy-16,cx,cy+16,ink);
                    const int highlight=(v.now/450)%4;
                    const int xs[]={-16,16,0,0}, ys[]={0,0,-16,16};
                    for (int node=0;node<4;++node) {
                        c.fillRoundRect(cx+xs[node]-5,cy+ys[node]-5,10,10,2,node==highlight ? FG : ink);
                    }
                } else if (icon == PortScanHome) {
                    // An Ethernet socket with a moving probe, distinct from the event list.
                    c.fillRoundRect(cx-23,cy-18,32,30,3,ink);
                    c.fillRect(cx-20,cy-15,26,19,fill);
                    c.fillRect(cx-14,cy+3,14,6,fill);
                    for(int pin=0;pin<4;++pin) c.fillRect(cx-17+pin*5,cy-14,2,8,FG);
                    const int sweep=static_cast<int>((v.now%1800)*22/1800);
                    c.drawLine(cx-18+sweep,cy-4,cx-18+sweep,cy+1,ACCENT);
                    c.fillCircle(cx+10,cy+7,9,fill);c.drawCircle(cx+10,cy+7,9,FG);
                    c.drawLine(cx+16,cy+14,cx+23,cy+21,FG);
                    c.drawLine(cx+17,cy+13,cx+24,cy+20,FG);
                } else if (icon == SshHome) {
                    c.fillRoundRect(cx-23,cy-18,46,36,4,ink);
                    c.fillRoundRect(cx-21,cy-16,42,32,3,fill);
                    c.drawLine(cx-14,cy-6,cx-6,cy,FG);c.drawLine(cx-6,cy,cx-14,cy+6,FG);
                    if((v.now/500)%2==0) c.drawLine(cx,cy+8,cx+13,cy+8,ink);
                } else if (icon == SettingsHome) {
                    const float rotation=(v.now%12000)*(6.28318530718f/12000.0f);
                    auto gear=[&](int x,int y,int root,int tip,int teeth,float angle) {
                        int firstX=0,firstY=0,oldX=0,oldY=0;
                        for(int point=0;point<=teeth*4;++point) {
                            const int phase=point%4;
                            const float a=angle+point*(6.28318530718f/(teeth*4));
                            const int radius=(phase==1 || phase==2) ? tip : root;
                            const int px=x+static_cast<int>(radius*std::cos(a));
                            const int py=y+static_cast<int>(radius*std::sin(a));
                            if(point) c.drawLine(oldX,oldY,px,py,ink);
                            else { firstX=px;firstY=py; }
                            oldX=px;oldY=py;
                        }
                        c.drawLine(oldX,oldY,firstX,firstY,ink);
                        c.drawCircle(x,y,root/2,ink);
                    };
                    gear(cx-8,cy+6,11,15,8,rotation);
                    gear(cx+10,cy-8,7,11,6,-rotation*8.0f/6.0f+0.52f);
                } else if (icon == ChannelsHome) {
                    for (int bar=0;bar<5;++bar) {
                        const int h=8+static_cast<int>(24*(0.5f+0.5f*std::sin(v.now%4000*0.00157f+bar)));
                        c.fillRoundRect(cx-21+bar*9,cy+18-h,6,h,2,ink);
                    }
                    c.drawLine(cx-23,cy+20,cx+23,cy+20,ink);
                } else if (icon == SignalHome || icon == PingHome) {
                    c.drawLine(cx-22,cy+19,cx+22,cy+19,ink);
                    int oldY=cy;
                    for (int x=-21;x<=21;++x) {
                        const float phase=x*0.22f+(v.now%4000)*0.00157f;
                        const int y=cy-static_cast<int>((icon==SignalHome ? std::sin(phase) : std::pow(std::max(0.0f,std::cos(phase)),8))*14);
                        if (x>-21) c.drawLine(cx+x-1,oldY,cx+x,y,ink);
                        oldY=y;
                    }
                    if (icon==PingHome) c.fillCircle(cx+18,cy-17,2,FG);
                } else if (icon == DiagnosticsHome) {
                    c.drawCircle(cx-12,cy,7,ink); c.drawCircle(cx+12,cy,7,ink);
                    c.drawLine(cx-5,cy,cx+5,cy,ink);
                    c.fillCircle(cx-12+static_cast<int>((v.now%2000)*24/2000),cy,2,FG);
                    c.drawLine(cx-5,cy+14,cx,cy+19,ink); c.drawLine(cx,cy+19,cx+12,cy+8,ink);
                } else {
                    for (int row=0;row<3;++row) {
                        const int y=cy-14+row*14;
                        c.fillCircle(cx-17,y,2,blendColor(fill,ink,(v.now/500)%3==static_cast<unsigned>(row)?1.0f:0.4f));
                        c.drawLine(cx-9,y,cx+18,y,ink);
                    }
                }
            }
            c.clearClipRect();
            const int selected = std::max(0, std::min(CarouselMotion::count-1, v.selected));
            const std::string label = labels[selected];
            text(c, label, (240 - label.size() * 12) / 2, 95);
            for (int dot = 0; dot < CarouselMotion::count; ++dot) c.fillCircle(120 - (CarouselMotion::count-1)*9/2 + dot * 9, 113, 1, dot == selected ? ACCENT : MUTED);
            footer(c, ",/ Move  Enter Open");
            break;
        }
        case Screen::Channels: {
            title(c,"CHANNELS");
            const auto& scan=v.radar ? *v.radar : emptyView<RadarSnapshot>();
            const int chosen=std::max(1,std::min(14,v.selected+1));
            text(c,scan.failed ? "Scan failed - R retry" : scan.scanning ? "Scanning..." : v.channelOverlapView ? "Overlap estimate" : "AP counts / 2.4 GHz",6,28,MUTED,1.5f);
            uint32_t peak=1;
            for (int ch=1;ch<=14;++ch) peak=std::max(peak,v.channelOverlapView ? scan.channelOverlap[ch] : scan.channelCounts[ch]);
            for (int ch=1;ch<=14;++ch) {
                const int x=10+(ch-1)*16;
                const int h=(v.channelOverlapView ? scan.channelOverlap[ch] : scan.channelCounts[ch])*40/peak;
                c.fillRect(x,84-h,12,std::max(1,h),ch==chosen ? ACCENT : MUTED);
                text(c,std::to_string(ch),x,88,ch==chosen ? ACCENT : MUTED,1);
            }
            text(c,"CH"+std::to_string(chosen)+" "+std::to_string(scan.channelCounts[chosen])+" APs",6,100,FG,1.5f);
            text(c,scan.channelCounts[chosen] ? std::to_string(scan.channelStrongest[chosen])+"dBm" : "--dBm",162,100,ACCENT,1.5f);
            footer(c,"ENT Mode ;/. CH R Scan"); break;
        }
        case Screen::ToolNetworks: {
            title(c,"WI-FI PICKER");
            const auto& scan=v.radar ? *v.radar : emptyView<RadarSnapshot>();
            const int first=std::max(0,v.selected/4)*4;
            for (int row=0;row<4 && first+row<static_cast<int>(scan.count);++row) {
                const int index=first+row;
                selectedRow(c,std::string(scan.points[index].ssid)+(v.rememberedNetworks[index] ? " *" : ""),row,index==v.selected,elapsed);
            }
            if (!scan.count) text(c,scan.failed ? "Scan failed" : scan.scanning ? "Scanning..." : "No networks found",6,45,MUTED,1.5f);
            footer(c,v.networkPickerCanJoin ? "ENT Join F Forget R Scan" : "ENT Track R Scan Esc"); break;
        }
        case Screen::Signal: {
            title(c,"SIGNAL");
            if (v.signal) {
                const auto& signal=*v.signal;
                ticker(c,signal.target.ssid,6,28,228,elapsed,MUTED,1.5f);
                const int current=signal.samples.latest(), average=signal.average();
                text(c,current==kMissingSample ? "Not seen" : std::to_string(current)+"dBm",6,46,ACCENT,2);
                text(c,average==kMissingSample ? "Avg --" : "Avg "+std::to_string(average),145,49,MUTED,1.5f);
                graph(c,signal.samples,-100,-20,68,39);
            }
            footer(c,"ENT Choose AP R Scan Esc"); break;
        }
        case Screen::Changes: {
            title(c,"CHANGES");
            if (v.changes && v.changes->count) {
                const int index=std::max(0,std::min(v.selected,static_cast<int>(v.changes->count)-1));
                const auto& event=v.changes->events[index];
                text(c,std::to_string(index+1)+"/"+std::to_string(v.changes->count)+" at "+elapsedLabel(event.at/1000),6,29,MUTED,1.5f);
                wrapped(c,event.text,49,3);
            } else text(c,v.changes && v.changes->hasBaseline() ? "No changes yet" : "Taking baseline...",6,48,FG,1.5f);
            footer(c,v.radar && v.radar->failed ? "Scan failed R Retry Esc" : v.changes && v.changes->limited ? ";/. Log Top32 C Clear" : ";/. Log C Clear R Scan"); break;
        }
        case Screen::NetworkPassword:
            title(c,"JOIN WI-FI"); ticker(c,v.networkName,6,29,228,elapsed,MUTED,1.5f);
            text(c,"Password:",6,49,FG,1.5f);
            text(c,std::string(std::min(size_t(18),v.passwordLength),'*'),6,68,ACCENT,2);
            text(c,std::to_string(v.passwordLength)+" chars | saved on join",6,96,MUTED,1);
            footer(c,"Enter Join Esc Cancel"); break;
        case Screen::PingTarget:
            title(c,"PING TARGET"); text(c,"IPv4 address:",6,33,MUTED,1.5f);
            text(c,v.draft,6,56,ACCENT,2);
            footer(c,"Enter Apply Esc Cancel"); break;
        case Screen::SshSetup: {
            title(c,"SSH TERMINAL");
            const auto& net=v.network ? *v.network : emptyView<NetworkStatus>();
            const auto& config=v.sshConfig ? *v.sshConfig : emptyView<SshConfig>();
            const auto phase=v.sshStatus ? v.sshStatus->phase : SshPhase::Idle;
            if(net.phase==NetworkPhase::Connecting) {
                ticker(c,net.ssid,6,32,228,elapsed,FG,1.5f);text(c,"Joining Wi-Fi...",6,60,MUTED,1.5f);
            } else if(net.phase!=NetworkPhase::Ready) wrapped(c,*net.message ? net.message : "Connect to Wi-Fi first",32,3);
            else if(phase==SshPhase::Stopping) text(c,"Closing SSH session...",6,50,MUTED,1.5f);
            else {
                const std::array<std::string,7> labels{{"Host: "+config.host,"Port: "+std::to_string(config.port),
                    "User: "+config.user,config.auth==SshAuth::Password ? "Auth: password" : "Auth: private key",
                    std::string(config.auth==SshAuth::Password ? "Password: " : "Passphrase: ")+(config.secret.empty()?"not set":"********"),
                    "Key: "+config.keyPath,"Connect"}};
                const int first=std::max(0,v.selected-3);
                for(int row=0;row<4 && first+row<7;++row) selectedRow(c,labels[first+row],row,first+row==v.selected,elapsed);
            }
            footer(c,";/. Move ENT Select Esc");break;
        }
        case Screen::SshInput:
            title(c,"SSH SETTING");text(c,v.sshInputLabel,6,30,MUTED,1.5f);
            ticker(c,v.sshInputSecret ? std::string(std::strlen(v.draft),'*') : v.draft,6,56,228,elapsed,ACCENT,1.5f);
            footer(c,"Enter Apply Esc Cancel");break;
        case Screen::SshSession: {
            const auto& state=v.sshStatus ? *v.sshStatus : emptyView<SshStatus>();
            if(state.phase==SshPhase::Active && v.terminal) {
                const auto& terminal=*v.terminal;
                text(c,v.terminalScroll ? "SSH / SCROLLBACK" : "SSH / CONNECTED",2,0,ACCENT,1);
                c.drawFastHLine(0,10,240,PANEL);
                for(int row=0;row<TerminalBuffer::rows;++row)
                    text(c,terminal.line(row,v.terminalScroll),0,12+row*8,FG,1);
                if(!v.terminalScroll && (v.now/500)%2==0)
                    c.drawFastHLine(terminal.column()*6,12+terminal.row()*8+7,6,ACCENT);
                text(c,"Fn+Del Exit Ctrl+arrows Fn+up/dn Scroll",0,126,MUTED,1);
            } else if(state.phase==SshPhase::VerifyHost) {
                title(c,"SSH HOST KEY");
                text(c,"SHA256 (hex):",6,28,MUTED,1.5f);
                for(int row=0;row<4;++row) text(c,std::string(state.fingerprint).substr(row*16,16),6,45+row*16,FG,1.5f);
                footer(c,"ENT Trust once Esc Reject");
            } else {
                title(c,"SSH SESSION");
                const char* label=state.phase==SshPhase::Connecting ? "Connecting..." : state.phase==SshPhase::Authenticating ? "Authenticating..." : state.message;
                wrapped(c,label,35,4);
                footer(c,"Esc Back / Cancel");
            }
            break;
        }
        case Screen::ScanInput:
            title(c,"SCAN TARGET"); text(c,v.scanInputLabel,6,32,MUTED,1.5f);
            text(c,v.draft,6,58,ACCENT,2);
            footer(c,"Enter Apply Esc Cancel"); break;
        case Screen::RangeSetup:
        case Screen::PortSetup: {
            const bool range=v.screen==Screen::RangeSetup;
            title(c,range ? "IP RANGE SCAN" : "HOST PORT SCAN");
            const auto& net=v.network ? *v.network : emptyView<NetworkStatus>();
            if (net.phase==NetworkPhase::Connecting) {
                ticker(c,net.ssid,6,32,228,elapsed,FG,1.5f);
                text(c,"Joining / waiting DHCP",6,57,MUTED,1.5f);
                footer(c,"Esc Cancel");
            } else if (net.phase!=NetworkPhase::Ready) {
                wrapped(c,*net.message ? net.message : "Connect to Wi-Fi first",32,3);
                footer(c,"Esc Back");
            } else {
                const std::array<std::string,3> labels{{
                    std::string(range ? "From: " : "Host: ")+(range ? v.rangeStart : v.scanHost),
                    std::string(range ? "To: " : "Ports: ")+(range ? v.rangeEnd : v.scanPortRange),
                    v.scanBusy ? "Stopping scan..." : "Start scan"}};
                for (int row=0;row<3;++row) selectedRow(c,labels[row],row,row==v.selected,elapsed);
                text(c,range ? "Ping discovery / max 256" : "TCP connect / 250ms each",6,103,MUTED,1.5f);
                footer(c,";/. Move ENT Select Esc");
            }
            break;
        }
        case Screen::RangeResults:
        case Screen::PortResults: {
            const bool range=v.screen==Screen::RangeResults;
            title(c,range ? "FOUND HOSTS" : "OPEN TCP PORTS");
            const auto& hosts=v.rangeScan ? *v.rangeScan : emptyView<RangeScanStatus>();
            const auto& ports=v.portScan ? *v.portScan : emptyView<PortScanStatus>();
            const ScanProgress& progress=range ? static_cast<const ScanProgress&>(hosts) : static_cast<const ScanProgress&>(ports);
            const char* phase=progress.phase==ScanPhase::Running ? "Scan" : progress.phase==ScanPhase::Complete ? "Done" : progress.phase==ScanPhase::Cancelled ? "Stopped" : progress.phase==ScanPhase::Failed ? "Failed" : "Ready";
            ticker(c,std::string(phase)+" "+std::to_string(progress.checked)+"/"+std::to_string(progress.total)+" Found "+std::to_string(progress.found),6,28,228,elapsed,MUTED,1.5f);
            const size_t count=range ? hosts.count : ports.count;
            if (!count) {
                text(c,range ? "No ping replies yet" : "No open ports found",6,58,FG,1.5f);
                text(c,range ? "Some hosts block ping" : "Closed or no response",6,82,MUTED,1.5f);
            } else {
                const int first=std::max(0,v.selected)/3*3;
                for (int row=0;row<3 && first+row<static_cast<int>(count);++row) {
                    const int index=first+row, y=48+row*21;
                    if (index==v.selected) c.fillRoundRect(3,y-2,234,20,4,PANEL);
                    const std::string label=range ? scanIpText(hosts.hosts[index].ip)+" "+std::to_string(hosts.hosts[index].ms)+"ms" : "TCP "+std::to_string(ports.ports[index])+" open";
                    ticker(c,label,6,y,228,elapsed,index==v.selected ? ACCENT : FG,1.5f);
                }
            }
            if (progress.found>count) text(c,"First 256 results shown",6,110,MUTED,1);
            footer(c,range ? ";/. Host ENT Ports S Stop" : ";/. Ports S Stop Esc Back"); break;
        }
        case Screen::NetworkStopping:
            title(c,"DISCONNECT"); text(c,"Finishing checks...",6,43,FG,1.5f);
            text(c,"DNS may take 15 sec",6,66,MUTED,1.5f);
            footer(c,"Returning to main menu"); break;
        case Screen::Diagnostics:
        case Screen::Ping: {
            title(c,v.screen==Screen::Ping ? "PING MONITOR" : "DIAGNOSTICS");
            const auto& net=v.network ? *v.network : emptyView<NetworkStatus>();
            if (net.phase==NetworkPhase::Connecting) {
                ticker(c,net.ssid,6,32,228,elapsed,FG,1.5f);
                text(c,"Joining / waiting DHCP",6,57,MUTED,1.5f);
                text(c,"30 second timeout",6,80,MUTED,1.5f);
            } else if (net.phase==NetworkPhase::Failed) {
                wrapped(c,net.message,32,3);
                text(c,"Esc: choose Wi-Fi again",6,96,MUTED,1.5f);
            } else if (v.screen==Screen::Diagnostics) {
                text(c,std::string("IP ")+net.address,6,29,ACCENT,1.5f);
                text(c,"Gateway: "+std::string(checkName(net.gatewayCheck)),6,48,FG,1.5f);
                text(c,"DNS: "+std::string(checkName(net.dns)),6,67,FG,1.5f);
                text(c,"TCP 443: "+std::string(checkName(net.internet)),6,86,FG,1.5f);
                text(c,"No reply may mean filtered",6,105,MUTED,1);
            } else {
                text(c,std::string(net.target)+(net.paused ? " paused" : ""),6,28,MUTED,1.5f);
                text(c,"Avg "+std::to_string(net.ping.average())+"ms",6,45,ACCENT,1.5f);
                text(c,"Loss "+std::to_string(net.ping.lossPercent())+"%",141,45,GOLD,1.5f);
                graph(c,net.ping.samples,0,std::max(50,static_cast<int>(net.ping.maximum)),63,32);
                text(c,"Min "+std::to_string(net.ping.minimum)+" Max "+std::to_string(net.ping.maximum)+"ms",6,100,MUTED,1.5f);
                if (*net.message) ticker(c,net.message,6,28,228,elapsed,GOLD,1.5f);
            }
            footer(c,v.screen==Screen::Ping ? "T Target P Pause R Reset" : "R Recheck Esc Disconnect"); break;
        }
        case Screen::Setup: {
            title(c, "PORTALIS"); text(c, v.sdReady ? "SD" : "NO SD", 115, 7, MUTED, 1.5f);
            const std::array<std::string, 4> labels{{"AP: " + std::string(v.ssid), "Page: " + std::string(v.pageName),
                "Start portal", "Submissions"}};
            const int first = std::max(0, v.selected - 3);
            for (int row = 0; row < 4 && first + row < 4; ++row)
                selectedRow(c, labels[first + row], row, first + row == v.selected, elapsed);
            footer(c, ";/. Move ENT Select H Log"); break;
        }
        case Screen::Radar:
        case Screen::ScanList: {
            title(c, v.screen == Screen::Radar ? "WI-FI RADAR" : "WI-FI LIST");
            const auto& scan = v.radar ? *v.radar : emptyView<RadarSnapshot>();
            const size_t chosen = scan.count ? std::min(static_cast<size_t>(std::max(0, v.selected)), scan.count - 1) : 0;
            if (v.screen == Screen::Radar) {
                constexpr int cx = 53, cy = 69, radius = 39;
                for (int r : {13, 26, radius}) c.drawCircle(cx, cy, r, blendColor(BG, ACCENT, 0.45f));
                c.drawLine(cx - radius, cy, cx + radius, cy, PANEL);
                c.drawLine(cx, cy - radius, cx, cy + radius, PANEL);
                const float sweep = (v.now % 4000) * (6.28318530718f / 4000.0f);
                for (int trail = 4; trail >= 0; --trail) {
                    const float angle = sweep - trail * 0.12f;
                    c.drawLine(cx, cy, cx + static_cast<int>(radius * std::cos(angle)),
                               cy + static_cast<int>(radius * std::sin(angle)), blendColor(BG, ACCENT, 0.9f - trail * 0.16f));
                }
                for (size_t i = 0; i < scan.count; ++i) {
                    const auto& ap = scan.points[i];
                    const float angle = radarAngle(ap.id);
                    const int r = radarRadius(ap.rssi);
                    const int x = cx + static_cast<int>(r * std::cos(angle));
                    const int y = cy + static_cast<int>(r * std::sin(angle));
                    const float age = std::fmod(sweep - angle + 6.28318530718f, 6.28318530718f);
                    const uint16_t color = i == chosen ? FG : blendColor(MUTED, ACCENT, 1.0f - age / 6.28318530718f);
                    c.fillCircle(x, y, i == chosen ? 3 : 2, color);
                    if (i == chosen) c.drawCircle(x, y, 5, ACCENT);
                }
                c.fillCircle(cx, cy, 2, ACCENT);
                const std::string count = std::to_string(scan.count) + (scan.total > scan.count ? "/" + std::to_string(scan.total) : "") + " APs";
                text(c, count, 106, 29, ACCENT, 1.5f);
                if (scan.count) {
                    const auto& ap = scan.points[chosen];
                    ticker(c, ap.ssid, 106, 48, 128, elapsed);
                    text(c, std::to_string(ap.rssi) + " dBm", 106, 68);
                    text(c, "CH" + std::to_string(ap.channel) + " " + ap.security, 106, 87, MUTED, 1.5f);
                } else {
                    text(c, scan.failed ? "Scan failed" : scan.scanning ? "Scanning..." : "No APs yet", 106, 50, MUTED, 1.5f);
                    text(c, "2.4 GHz", 106, 74, MUTED, 1.5f);
                }
                text(c, "Signal only", 106, 102, MUTED, 1.5f);
            } else {
                if (!scan.count) {
                    text(c, scan.failed ? "Scan failed" : scan.scanning ? "Scanning..." : "No access points", 6, 47);
                    text(c, "R retries; Esc exits", 6, 75, MUTED, 1.5f);
                }
                const size_t first = chosen / 3 * 3;
                for (size_t row = 0; row < 3 && first + row < scan.count; ++row) {
                    const auto& ap = scan.points[first + row];
                    const bool active = first + row == chosen;
                    const int y = 27 + row * 29;
                    if (active) c.fillRoundRect(3, y - 1, 234, 28, 3, PANEL);
                    ticker(c, ap.ssid, 6, y, 228, elapsed, active ? ACCENT : FG);
                    char details[40];
                    std::snprintf(details, sizeof(details), "%d dBm CH%u %s %04X", ap.rssi, ap.channel, ap.security,
                                  static_cast<unsigned>(ap.id & 0xffff));
                    ticker(c, details, 6, y + 16, 228, elapsed, MUTED, 1.5f);
                }
            }
            footer(c, scan.scanning ? ";/. AP ENT View Scanning" : ";/. AP ENT View R Scan");
            break;
        }
        case Screen::Probe: {
            title(c, "KARMA");
            if (!v.probeRunning) {
                const std::string path = v.probeConfig ? v.probeConfig->noticePath : "";
                const std::string pageName = path.empty() ? "Select HTML file" : path.substr(path.find_last_of('/') + 1);
                const std::array<std::string, 4> labels{{
                    "Portal: " + pageName,
                    "Timeout: " + std::to_string(v.probeConfig ? v.probeConfig->timeoutSeconds : 120) + "s",
                    "Start KARMA", "Submissions"}};
                for (int row = 0; row < 4; ++row)
                    selectedRow(c, labels[row], row, row == v.selected, elapsed);
                footer(c, v.selected == 1 ? ",/ Adjust ENT +30s" : ";/. Move ENT Select R SD");
                break;
            }
            const auto& state = v.probeStatus ? *v.probeStatus : emptyView<ProbeStatus>();
            const bool ap = state.phase == ProbePhase::Waiting || state.phase == ProbePhase::Notice;
            if (ap) {
                ticker(c, state.ssid, 6, 29, 228, elapsed);
                text(c, std::string(state.phase == ProbePhase::Notice ? "AP joined " : "Waiting ") + std::to_string(state.remaining) + "s", 6, 51, ACCENT);
            } else {
                text(c, v.probeRunning ? "Listening CH" + std::to_string(state.channel) : state.phase == ProbePhase::Failed ? "Radio error" : "Ready to test", 6, 29, ACCENT);
                // Previous label: text(c, std::to_string(approved) + " approved SSIDs", 6, 51, MUTED, 1.5f);
                ticker(c, v.probeConfig && !v.probeConfig->noticePath.empty() ? v.probeConfig->noticePath : "Select HTML file", 6, 51, 228, elapsed, MUTED, 1.5f);
            }
            ticker(c, "Tests " + std::to_string(state.attempts) + "  Joined " + std::to_string(state.joins), 6, 74, 228, elapsed, FG, 1.5f);
            // Previous hints: "ENT Scope  H Log" / "S Start R Reload Esc Exit"
            text(c, "ENT Scope H Log V Forms", 6, 96, MUTED, 1.5f);
            footer(c, v.probeRunning ? "S Stop  Esc Exit" : "S Start T HTML R Reload");
            break;
        }

        case Screen::ProbeHistory: {
            title(c, "TEST LOG");
            const size_t count = v.probeStatus ? v.probeStatus->eventCount : 0;
            if (!count) text(c, "No tests yet", 6, 42);
            const int first = std::max(0, v.selected) / 3 * 3;
            for (int row = 0; row < 3 && first + row < static_cast<int>(count); ++row) {
                const auto& event = v.probeStatus->events[first + row];
                const int y = 27 + row * 29;
                ticker(c, event.ssid, 6, y, 228, elapsed, first + row == v.selected ? ACCENT : FG);
                ticker(c, "+" + elapsedLabel(event.seconds) + (event.connected ? " AP joined" : " No join seen"), 6, y + 16, 228, elapsed, MUTED, 1.5f);
            }
            footer(c, ";/. Scroll  Esc Back"); break;
        }
        case Screen::Settings: {
            title(c, "SETTINGS");
            const std::array<std::string, kSettingsCount> labels{{
                "Accent: " + std::string(kAccents[settings.accent].name),
                "Background: " + std::string(kBackgrounds[settings.background].name),
                "LED: " + std::to_string(settings.ledBrightness) + "%",
                std::string("Menu sounds: ") + (settings.sounds ? "On" : "Off"),
                "Volume: " + std::to_string(settings.volume) + "%",
                std::string("Event sounds: ") + (settings.eventSounds ? "On" : "Off"),
                "Breathing: " + std::string(kLedColors[settings.ledColors[0]].name),
                "Connect: " + std::string(kLedColors[settings.ledColors[1]].name),
                "Confirm: " + std::string(kLedColors[settings.ledColors[2]].name),
                "Disconnect: " + std::string(kLedColors[settings.ledColors[3]].name),
                "LED mode: " + std::string(kLedAnimations[settings.ledAnimation])
            }};
            const int first = std::max(0, v.selected - 3);
            for (int row = 0; row < 4 && first + row < kSettingsCount; ++row)
                selectedRow(c, labels[first + row], row, first + row == v.selected, elapsed);
            footer(c, ";/. Move ,/ Set ESC Save"); break;
        }
        case Screen::Names: {
            title(c, "AP NAMES");
            const int count = v.config ? static_cast<int>(v.config->count) : 0;
            const int first = (v.selected / 4) * 4;
            for (int row = 0; row < 4 && first + row <= count; ++row) {
                const int index = first + row;
                const std::string label = index == 0 ? "Type a name..." : v.config->apNames[index - 1];
                selectedRow(c, label, row, v.selected == index, elapsed);
            }
            if (!count) {
                text(c, "Presets: /portalis.json", 6, 59, MUTED, 1.5f);
                text(c, "Add ap_names on SD", 6, 79, MUTED, 1.5f);
            }
            footer(c, "ENT Select R Reload Esc"); break;
        }
        case Screen::Name:
            title(c, "AP NAME");
            text(c, "Up to 32 characters", 6, 29, MUTED, 1.5f);
            c.fillRoundRect(3, 48, 234, 47, 4, PANEL);
            text(c, std::string(v.draft).substr(0, 19), 6, 53);
            text(c, std::string(v.draft).substr(std::min(size_t(19), strlen(v.draft))), 6, 73);
            if ((v.now / 500) % 2 == 0) {
                const size_t len = strlen(v.draft);
                c.fillRect(6 + (len % 19) * 12, len < 19 ? 68 : 88, 10, 2, ACCENT);
            }
            footer(c, "Enter Save  Esc Cancel"); break;
        case Screen::Templates: {
            title(c, "SD TEMPLATES");
            if (!v.browser) break;
            const auto& b = *v.browser;
            // Display three entries, leaving one full readable row for the path.
            ticker(c, b.directory, 6, 28, 228, elapsed, MUTED, 1.5f);
            const int first = (v.selected / 3) * 3;
            for (int row = 0; row < 3; ++row) {
                const int index = first + row;
                if (index >= static_cast<int>(b.count)) break;
                const auto& e = b.entries[index];
                const int y = 48 + row * 21;
                const bool chosen = index == v.selected;
                if (chosen) c.fillRoundRect(3, y - 2, 234, 20, 3, ACCENT);
                text(c, e.directory ? ">" : "*", 6, y, chosen ? BG : MUTED);
                if (chosen) ticker(c, e.label, 22, y, 210, elapsed, BG);
                else text(c, shorten(e.label, 17), 22, y);
            }
            if (!b.count) text(c, "No HTML templates", 6, 58, MUTED);
            footer(c, "ENT Open DEL Up R Scan"); break;
        }
        case Screen::Live:
            text(c, "PORTALIS", 6, 3, ACCENT); text(c, v.running ? "LIVE" : "STOPPED", 105, 5, v.running ? ACCENT : GOLD, 1.5f);
            ticker(c, v.ssid, 6, 25, 228, elapsed);
            counter(c, 4, 112, "CLIENTS", std::to_string(v.clients), FG,
                    v.clientsDecreased ? GOLD : ACCENT, v.clientsPulse, false);
            counter(c, 123, 113, "CONFIRMED", countLabel(v.count), ACCENT, ACCENT, v.confirmedPulse, true);
            text(c, "172.0.0.1", 6, 99, MUTED, 1.5f);
            ticker(c, elapsedLabel(v.seconds), 135, 99, 98, elapsed, FG, 1.5f);
            footer(c, v.running ? "S Forms C Config H Log X" : "S Forms C Config ENT Setup"); break;
        case Screen::Stop:
            title(c, "STOP PORTAL?");
            text(c, "Disconnect clients", 6, 33); text(c, "and turn off Wi-Fi.", 6, 53);
            text(c, "History stays saved.", 6, 83, MUTED, 1.5f);
            footer(c, "Enter Stop  Esc Back"); break;
        case Screen::History: {
            title(c, "HISTORY");
            text(c, !v.sdReady ? "RAM" : v.logHealthy ? "SD" : "ERR", 115, 7, v.logHealthy ? ACCENT : GOLD, 1.5f);
            if (!v.history || !v.history->size()) { text(c, "No confirmations", 6, 45); text(c, "Tap the page button.", 6, 71, MUTED, 1.5f); }
            else for (size_t row = 0; row < 4 && v.historyOffset + row < v.history->size(); ++row) {
                const auto event = v.history->newest(v.historyOffset + row);
                const std::string label = "S" + std::to_string(event.session) + " +" + elapsedLabel(event.seconds);
                ticker(c, label, 6, 29 + row * 21, 228, elapsed);
            }
            footer(c, ";/. Scroll  Esc Back"); break;
        }
        case Screen::Submissions: {
            title(c, "SUBMISSIONS");
            const size_t count = v.submissions ? v.submissions->size() : 0;
            if (!count) text(c, "No saved forms", 6, 45);
            const size_t first = (v.selected / 4) * 4;
            for (size_t row = 0; row < 4 && first + row < count; ++row) {
                const auto& entry = (*v.submissions)[count - 1 - first - row];
                const std::string label = "S" + std::to_string(entry.event.session) + " +" + elapsedLabel(entry.event.seconds) +
                                          " (" + std::to_string(entry.fields) + ")";
                selectedRow(c, label, row, first + row == static_cast<size_t>(v.selected), elapsed);
            }
            footer(c, "ENT Open R Refresh Esc"); break;
        }
        case Screen::Submission: {
            title(c, "SUBMISSION");
            if (v.submission) {
                ticker(c, "S" + std::to_string(v.submission->event.session) + " +" + elapsedLabel(v.submission->event.seconds), 6, 28, 228, elapsed, MUTED, 1.5f);
                for (size_t row = 0; row < 4 && v.submissionOffset + row < v.submission->lines.size(); ++row)
                    text(c, v.submission->lines[v.submissionOffset + row], 6, 45 + row * 18);
            }
            footer(c, ";/. Scroll  Esc Back"); break;
        }
        case Screen::Error:
            title(c, "NOTICE"); wrapped(c, v.error, 29, 4, GOLD); footer(c, "Enter / Esc Back"); break;
    }
    if (v.screen != Screen::Splash && !(v.screen==Screen::SshSession && v.sshStatus && v.sshStatus->phase==SshPhase::Active)) battery(c, v.batteryPercent);
}
} // namespace portalis
