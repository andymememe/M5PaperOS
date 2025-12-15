#ifndef MARQUEE_H
#define MARQUEE_H

#include <WebServer.h>
#include <DNSServer.h>
#include <vector>
#include "AppBase.h"

// 定義模式
enum MarqueeMode {
    MODE_SCROLLING,
    MODE_WEB_CONFIG
};

// 定義跑馬燈設定結構
struct MarqueeConfig {
    int lastShowLine = 0;
};

class MarqueeApp : public AppBase {
private:
    // --- 常數設定 ---
    const String MARQUEE_DIR = "/Marquee";
    const String MARQUEE_FILE = "/Marquee/data.txt";
    const String MARQUEE_CONFIG_FILE = "/Marquee/config.json";
    const char* AP_SSID = "M5Paper-Marquee";
    const char* AP_PASS = "m5pmarquee"; // 密碼 8 碼
    const int TEXT_SIZE = 4; // 字體放大倍率 (24px * 4 = 96px 高)
    
    // --- 定義固定 IP (避免手機抓不到) ---
    const IPAddress apIP = IPAddress(192, 168, 4, 1); // <--- 新增
    const IPAddress netMsk = IPAddress(255, 255, 255, 0); // <--- 新增
    
    // --- 狀態變數 ---
    MarqueeMode currentMode = MODE_SCROLLING;
    
    // --- 跑馬燈變數 ---
    MarqueeConfig mConfig;
    std::vector<String> lines;
    int scrollX = 0;
    int textWidth = 0;
    unsigned long lastScrollTime = 0;
    int scrollSpeed = 60; // 每次移動像素

    // --- Web Server ---
    WebServer* server = nullptr;
    DNSServer dnsServer;
    
    // --- 內部函式 ---
    void _loadConfig();
    void _saveConfig();
    void _loadMessages();
    void _saveMessage(String msg);
    void _drawMarquee();
    void _drawWebConfigUI();
    void _startAP();
    void _stopAP();
    
    // Web Server Handlers
    void _handleRoot();
    void _handleUpload();
    void _handleNotFound();

public:
    MarqueeApp();
    ~MarqueeApp(); // 解構子用來釋放 server
    
    void setup(M5Canvas* _canvas) override;
    void loop(lgfx::touch_point_t t, bool isPressed) override;
    void drawUI() override;
    void exit() override;
};

#endif