#include "App/Marquee.h"

#include <WiFi.h>

#include "SystemManager.h"

MarqueeApp::MarqueeApp() : AppBase("Marquee") {}

MarqueeApp::~MarqueeApp() {
  if (server) {
    delete server;
    server = nullptr;
  }
}

void MarqueeApp::setup(M5Canvas* _canvas) {
  AppBase::setup(_canvas);

  sys.keepAwake(true);

  // 確保資料夾存在
  if (!SD.exists(MARQUEE_DIR)) {
    SD.mkdir(MARQUEE_DIR);
  }

  _loadMessages();
  _loadConfig();

  currentMode = MODE_SCROLLING;
  scrollX = SCREEN_WIDTH;  // 從最右邊開始

  drawUI();
}

void MarqueeApp::exit() {
  _saveConfig();
  sys.keepAwake(false);
  _stopAP();  // 離開 App 時確保關閉 AP
}

// ---------------------------------------------------------
// Config 讀寫
// ---------------------------------------------------------
void MarqueeApp::_loadConfig() {
  JsonDocument doc;
  if (sys.loadAppConfig(MARQUEE_CONFIG_FILE, doc)) {
    mConfig.lastShowLine = doc["line"] | 0;
  }
}

void MarqueeApp::_saveConfig() {
  JsonDocument doc;
  doc["line"] = mConfig.lastShowLine;
  sys.saveAppConfig(MARQUEE_CONFIG_FILE, doc);
}

// ---------------------------------------------------------
// 核心邏輯
// ---------------------------------------------------------

void MarqueeApp::drawUI() {
  if (currentMode == MODE_SCROLLING) {
    // 清空畫面
    canvas->fillSprite(COLOR_WHITE);

    // 顯示操作說明
    canvas->setFont(nullptr);
    canvas->setTextSize(2);
    canvas->setTextColor(COLOR_BLACK);
    canvas->setTextDatum(top_center);
    canvas->drawString("Wheel: Wi-Fi Setup", SCREEN_WIDTH / 2, 10);
    canvas->drawFastHLine(0, 40, SCREEN_WIDTH, COLOR_BLACK);

    // 畫一次初始文字
    _drawMarquee();
  } else {
    _drawWebConfigUI();
  }
  canvas->pushSprite(0, TOP_BAR_HEIGHT);
}

void MarqueeApp::loop(lgfx::touch_point_t t, bool isPressed) {
  // --- 模式 1: 跑馬燈 ---
  if (currentMode == MODE_SCROLLING) {
    // 1. 處理滾輪 (切換行數)
    if (M5.BtnA.wasPressed()) {  // 上一行
      mConfig.lastShowLine--;
      if (mConfig.lastShowLine < 0) mConfig.lastShowLine = lines.size() - 1;
      _saveConfig();
      scrollX = SCREEN_WIDTH;  // 重置位置
    }
    if (M5.BtnC.wasPressed()) {  // 下一行
      mConfig.lastShowLine++;
      if (mConfig.lastShowLine >= lines.size()) mConfig.lastShowLine = 0;
      _saveConfig();
      scrollX = SCREEN_WIDTH;  // 重置位置
    }

    // 處理中鍵 (暫停/繼續)
    if (M5.BtnB.wasPressed()) {
      currentMode = MODE_WEB_CONFIG;
      _startAP();
      _drawWebConfigUI();
      canvas->pushSprite(0, TOP_BAR_HEIGHT);
      return;
    }

    // 動畫邏輯
    if (!lines.empty()) {
      // 使用快速刷新模式
      M5.Display.setEpdMode(epd_mode_t::epd_fast);
      _drawMarquee();
      M5.Display.setEpdMode(epd_mode_t::epd_quality);  // 恢復

      // 移動座標
      scrollX -= scrollSpeed;
      // 如果整串字都跑出左邊界了，重置回右邊
      if (scrollX < -textWidth) {
        scrollX = SCREEN_WIDTH;
      }
    }
  }
  // --- 模式 2: Web 設定 ---
  else {
    // 處理 DNS 請求 (這是 Captive Portal 的關鍵)
    dnsServer.processNextRequest();

    // 處理 Web Client
    if (server) server->handleClient();

    if (M5.BtnB.wasPressed()) {
      _stopAP();
      _loadMessages();
      currentMode = MODE_SCROLLING;
      scrollX = SCREEN_WIDTH;
      drawUI();
    }
  }
}

// ---------------------------------------------------------
// 繪圖功能
// ---------------------------------------------------------

void MarqueeApp::_drawMarquee() {
  if (lines.empty()) return;

  if (mConfig.lastShowLine >= lines.size()) {
    mConfig.lastShowLine = lines.size() - 1;
    _saveConfig();
  }
  String text = lines[mConfig.lastShowLine];
  int y = (APP_AREA_HEIGHT - (24 * TEXT_SIZE)) / 2;  // 垂直置中

  // 清除中間的跑馬燈區域 (保留上方的說明列)
  canvas->fillRect(0, 50, SCREEN_WIDTH, APP_AREA_HEIGHT - 50, COLOR_WHITE);

  // 設定字型與大小
  canvas->setFont(&efontTW_24);  // 使用中文字型
  canvas->setTextSize(TEXT_SIZE);
  canvas->setTextColor(COLOR_BLACK);
  canvas->setTextDatum(top_left);

  // 計算寬度 (如果還沒算過)
  // 注意: 在 loop 中頻繁 textWidth 比較耗效能，這裡簡化處理
  textWidth = canvas->textWidth(text);

  // 繪製文字
  canvas->drawString(text, scrollX, y);

  // 局部推送
  canvas->pushSprite(0, TOP_BAR_HEIGHT);
}

void MarqueeApp::_drawWebConfigUI() {
  canvas->fillSprite(COLOR_WHITE);
  canvas->setFont(nullptr);

  // 標題
  canvas->setTextSize(3);
  canvas->setTextColor(COLOR_BLACK);
  canvas->setTextDatum(top_center);
  canvas->drawString("Wi-Fi Setup Mode", SCREEN_WIDTH / 2, 20);

  // 畫分隔線
  canvas->drawFastHLine(20, 60, SCREEN_WIDTH - 40, COLOR_BLACK);

  int qrY = 100;
  int qrSize = 250;

  // --- 左邊: Wi-Fi QR Code ---
  // 格式: WIFI:S:SSID;T:WPA;P:PASSWORD;;
  String wifiQR =
      "WIFI:S:" + String(AP_SSID) + ";T:WPA;P:" + String(AP_PASS) + ";;";
  canvas->qrcode(wifiQR, 100, qrY + TOP_BAR_HEIGHT, qrSize, 5);

  canvas->setTextSize(2);
  canvas->setTextDatum(top_center);
  canvas->drawString("1. Connect Wi-Fi", 100 + qrSize / 2, qrY + qrSize + 10);
  canvas->drawString("SSID: " + String(AP_SSID), 100 + qrSize / 2,
                     qrY + qrSize + 40);
  canvas->drawString("Pass: " + String(AP_PASS), 100 + qrSize / 2,
                     qrY + qrSize + 70);

  // --- 右邊: URL QR Code ---
  String urlQR = "http://" + apIP.toString();
  canvas->qrcode(urlQR, 550, qrY + TOP_BAR_HEIGHT, qrSize, 5);

  canvas->drawString("2. Scan to Upload", 550 + qrSize / 2, qrY + qrSize + 10);
  canvas->drawString("http://" + apIP.toString(), 550 + qrSize / 2,
                     qrY + qrSize + 40);
}

// ---------------------------------------------------------
// 檔案處理
// ---------------------------------------------------------

void MarqueeApp::_loadMessages() {
  lines.clear();

  if (sys.config.sdMounted && SD.exists(MARQUEE_FILE)) {
    File f = SD.open(MARQUEE_FILE);
    while (f.available()) {
      String line = f.readStringUntil('\n');
      line.trim();
      if (line.length() > 0) {
        lines.push_back(line);
      }
    }
    f.close();
  }

  // 如果是空的，加入預設文字
  if (lines.empty()) {
    lines.push_back("請使用手機上傳文字");
  }
}

void MarqueeApp::_saveMessage(String msg) {
  if (!sys.config.sdMounted) return;

  // 這裡我們選擇「覆蓋」還是「新增」？
  // 根據需求是「上傳句子」，我們設計為 Append 模式，或者只存一行
  // 這裡實作 Append 模式，讓上下鍵可以切換

  File f = SD.open(MARQUEE_FILE, FILE_APPEND);
  if (f) {
    f.println(msg);
    f.close();
  }
}

// ---------------------------------------------------------
// Wi-Fi & Web Server
// ---------------------------------------------------------

void MarqueeApp::_startAP() {
  sys.showNotification("Starting AP", NOTIFICATION_DEFAULT_DURATION_MS);

  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP);

  // 強制設定 IP 位址，這能解決大部分連不上的問題
  WiFi.softAPConfig(apIP, apIP, netMsk);
  WiFi.softAP(AP_SSID, AP_PASS);

  // 啟動 DNS Server，攔截所有域名請求 ("*") 指向 apIP
  // 這會觸發手機的 Captive Portal (登入網路頁面)
  dnsServer.start(53, "*", apIP);

  if (server) delete server;
  server = new WebServer(80);

  server->on("/", [this]() { _handleRoot(); });
  server->on("/upload", HTTP_POST, [this]() { _handleUpload(); });
  // 處理 404 Not Found，將其重導向回首頁 (針對 Android/iOS 探測連線)
  server->onNotFound([this]() { _handleNotFound(); });

  server->begin();
}

void MarqueeApp::_stopAP() {
  dnsServer.stop();  // 停止 DNS
  if (server) {
    server->stop();
    delete server;
    server = nullptr;
  }
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
}

void MarqueeApp::_handleNotFound() {
  // 當手機嘗試連線 google.com 或 apple.com 進行連線測試時
  // 我們將其重導向到我們的 Captive Portal IP
  server->sendHeader("Location", String("http://") + apIP.toString(), true);
  server->send(302, "text/plain", "Redirect to Captive Portal");
}

void MarqueeApp::_handleRoot() {
  String html = R"(
        <!DOCTYPE html>
        <html>
        <head>
            <meta charset="UTF-8">
            <meta name="viewport" content="width=device-width, initial-scale=1.0">
            <title>M5Paper Marquee</title>
            <style>
                body { font-family: sans-serif; text-align: center; padding: 20px; background-color: #f0f0f0; }
                h2 { color: #333; }
                form { background: #fff; padding: 20px; border-radius: 10px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }
                input { width: 90%; padding: 12px; font-size: 18px; margin: 10px 0; border: 1px solid #ccc; border-radius: 5px; }
                button { width: 100%; padding: 12px; font-size: 18px; background: #000; color: #fff; border: none; border-radius: 5px; cursor: pointer; }
                button:active { background: #333; }
            </style>
        </head>
        <body>
            <h2>Upload Marquee Text</h2>
            <form action="/upload" method="POST">
                <input type="text" name="msg" maxlength="42" placeholder="輸入文字 (限14中文字)..." required>
                <br>
                <button type="submit">Upload & Display</button>
            </form>
        </body>
        </html>
    )";
  server->send(200, "text/html", html);
}

void MarqueeApp::_handleUpload() {
  if (server->hasArg("msg")) {
    String msg = server->arg("msg");

    // 簡單的長度檢查 (UTF-8 中文通常 3 bytes, 14字約 42 bytes)
    // 但為了寬容度，這裡不做嚴格截斷，只存入
    msg.trim();
    if (msg.length() > 0) {
      _saveMessage(msg);

      String html = "<h1>Saved!</h1><p>" + msg + "</p><a href='/'>Back</a>";
      server->send(200, "text/html", html);
      return;
    }
  }
  server->send(400, "text/plain", "Invalid Data");
}