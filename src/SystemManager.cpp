#include "SystemManager.h"

#include "App/Launcher.h"

SystemManager sys;

SystemManager::SystemManager()
    : topSprite(&M5.Display),
      bottomSprite(&M5.Display),
      appSprite(&M5.Display),
      currentApp(nullptr),
      lastStatusUpdate(0),
      lastInteractionTime(millis()),
      isSleeping(false),
      forceAwake(false) {}

void SystemManager::begin() {
  auto cfg = M5.config();
  M5.begin(cfg);

  M5.Display.setRotation(1);
  M5.Display.setEpdMode(epd_mode_t::epd_fast);
  M5.Display.clear(COLOR_WHITE);

  // 設定 Sprites (4-bit 節省記憶體)
  topSprite.setColorDepth(4);
  bottomSprite.setColorDepth(4);
  appSprite.setColorDepth(4);

  topSprite.createSprite(SCREEN_WIDTH, TOP_BAR_HEIGHT);
  bottomSprite.createSprite(SCREEN_WIDTH, BOTTOM_BAR_HEIGHT);
  appSprite.createSprite(SCREEN_WIDTH, APP_AREA_HEIGHT);

  _drawStaticUI();

  // 嘗試掛載 SD 卡
  SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);
  if (SD.begin(SD_SPI_CS_PIN, SPI, SD_SPI_FREQUENCY)) {
    config.sdMounted = true;
    showNotification("SD Card Mounted", NOTIFICATION_DEFAULT_DURATION_MS);
    _loadSystemConfig();  // 讀取 config.json
    _loadCalibration();   // 讀取校正檔案
  } else {
    showNotification("SD Card Init. Failed", NOTIFICATION_INFINITE_DURATION_MS);
    config.sdMounted = false;
  }

  // --- 開機時自動校時 ---
  // 如果讀取到 WiFi 設定，才進行校時
  if (config.wifiSSID != "") {
    syncTime();
  }

  // 初始環境數據讀取
  if (updateEnvSensor()) {
    _logEnvData();
    lastEnvLogTime = millis();
  }
}

void SystemManager::run() {
  M5.update();

  // 執行電源檢查
  _checkPowerManagement();

  // 紀錄環境數據
  if (millis() - lastEnvLogTime > LOG_ENV_DATA_INTERVAL_MS) {
    if (updateEnvSensor()) {
      _logEnvData();
      lastEnvLogTime = millis();
    }
  }

  // --- 休眠狀態下的行為 ---
  if (isSleeping) {
    // 在休眠時，我們只更新 Status Bar (例如每分鐘更新時間)
    // 但不執行 App 的 loop (讓畫面靜止)
    _updateStatusBar();

    // 稍微延遲，讓 CPU 休息更多
    delay(100);
    return;  // <--- 重要：休眠時直接 return，不執行下方 App 邏輯
  }

  // --- 檢查通知是否過期 ---
  if (isNotificationActive) {
    if (notificationEndTime >= 0 && millis() > notificationEndTime) {
      // 時間到！關閉通知狀態
      isNotificationActive = false;
      // 強制刷新一次，變回原本的時鐘介面
      _updateStatusBar(true);
    }
  } else {
    // 只有在沒有通知的時候，才去檢查每分鐘的時間更新
    _updateStatusBar();
  }

  lgfx::touch_point_t t;
  bool isPressed = false;

  if (M5.Touch.getCount() > 0) {
    t = M5.Touch.getDetail(0);
    isPressed = true;
  } else {
    t.x = -1;
    t.y = -1;
  }

  // 檢查 Bottom Bar (Home 鍵)
  if (isPressed && t.y > (SCREEN_HEIGHT - BOTTOM_BAR_HEIGHT)) {
    goHome();
    return;
  }

  // 執行 App 邏輯
  if (currentApp) {
    lgfx::touch_point_t appTouch = t;
    if (isPressed) {
      appTouch.y -= TOP_BAR_HEIGHT;
      if (appTouch.y < 0 || appTouch.y > APP_AREA_HEIGHT) {
        appTouch.x = -1;
        appTouch.y = -1;
        isPressed = false;
      }
    }
    currentApp->loop(appTouch, isPressed);
  }

  if (!isPressed) delay(20);
}

void SystemManager::setLauncher(LauncherApp* launcher) {
  mainLauncher = launcher;
}

void SystemManager::goHome() {
  if (mainLauncher != nullptr) {
    // 如果當前已經是 Launcher，就不用再切換了 (或者可以重置 Launcher 狀態)
    if (currentApp != mainLauncher) {
      launchApp(mainLauncher);
    }
  }
}

void SystemManager::launchApp(AppBase* app) {
  if (currentApp) currentApp->exit();
  currentApp = app;

  if (currentApp) {
    appSprite.fillSprite(COLOR_WHITE);
    currentApp->setup(&appSprite);

    // 切換 App 時用高品質刷新一次
    M5.Display.setEpdMode(epd_mode_t::epd_quality);
    appSprite.pushSprite(0, TOP_BAR_HEIGHT);
    M5.Display.setEpdMode(epd_mode_t::epd_fast);

    _updateStatusBar(true);
  }
}

void SystemManager::showNotification(String message, int durationMs) {
  isNotificationActive = true;
  notificationMsg = message;
  if (durationMs >= 0) {
    notificationEndTime = millis() + durationMs;
  } else {
    notificationEndTime = -1;  // 永遠不結束
  }

  // 強制刷新 Status Bar，這會觸發 _updateStatusBar 的繪圖邏輯
  _updateStatusBar(true);
}

void SystemManager::stopNotification() {
  isNotificationActive = false;
  notificationMsg = "";
  notificationEndTime = 0;

  // 強制刷新 Status Bar，恢復正常顯示
  _updateStatusBar(true);
}

void SystemManager::keepAwake(bool enable) {
  forceAwake = enable;
  if (enable) {
    lastInteractionTime = millis();  // 順便重置時間
  }
}

bool SystemManager::isInSleepMode() { return isSleeping; }

bool SystemManager::setWiFi(bool enable) {
  if (config.wifiSSID == "") {
    showNotification("No Wi-Fi Config", NOTIFICATION_DEFAULT_DURATION_MS);
    return false;
  }

  if (enable) {
    // 如果已經連線，就不需要重連
    if (isWiFiConnected()) {
      return true;
    }

    showNotification("Wi-Fi Connecting", NOTIFICATION_INFINITE_DURATION_MS);

    WiFi.mode(WIFI_STA);
    WiFi.begin(config.wifiSSID.c_str(), config.wifiPass.c_str());

    // 等待連線 (Timeout 10秒)
    int timeout = 0;
    while (WiFi.status() != WL_CONNECTED && timeout < 20) {
      delay(500);
      timeout++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      showNotification("Wi-Fi Connected", NOTIFICATION_DEFAULT_DURATION_MS);
      return true;
    } else {
      showNotification("Wi-Fi Connect Failed",
                       NOTIFICATION_DEFAULT_DURATION_MS);
      // 連線失敗，關閉以省電
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
      return false;
    }
  } else {
    // 關閉 Wi-Fi
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);

    _updateStatusBar(true);
    return false;
  }
}

void SystemManager::calibrateTouch() {
  // 準備接收校正數據的陣列 (通常需要 8 個 uint16_t)
  uint16_t calData[8];

  // 顯示提示訊息 (因為 calibrateTouch 會直接畫在螢幕上，我們先清空)
  M5.Display.fillScreen(COLOR_WHITE);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(COLOR_BLACK);
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString("Touch Calibration", SCREEN_WIDTH / 2,
                        SCREEN_HEIGHT / 2 - 40);
  M5.Display.drawString("Touch the arrows", SCREEN_WIDTH / 2,
                        SCREEN_HEIGHT / 2);

  // 稍等一下讓使用者看清楚
  delay(NOTIFICATION_DEFAULT_DURATION_MS);

  // 執行校正 (M5GFX 內建功能)
  // 參數: 數據陣列, 箭頭顏色, 背景顏色, 箭頭大小
  M5.Display.calibrateTouch(calData, COLOR_BLACK, COLOR_WHITE,
                            max(SCREEN_WIDTH, SCREEN_HEIGHT) >> 3);

  // 校正完成，將數據存入 SD 卡
  if (config.sdMounted) {
    JsonDocument doc;
    JsonArray data = doc["cal_data"].to<JsonArray>();
    for (int i = 0; i < 8; i++) {
      data.add(calData[i]);
    }

    File file = SD.open(CALIBRATION_FILE, FILE_WRITE);
    if (file) {
      serializeJson(doc, file);
      file.close();
      showNotification("Calibration Saved", NOTIFICATION_DEFAULT_DURATION_MS);
    } else {
      showNotification("Save Failed", NOTIFICATION_DEFAULT_DURATION_MS);
    }
  }

  // 恢復 UI
  M5.Display.clear(COLOR_WHITE);
  _drawStaticUI();
  if (currentApp) {
    // 如果是在 App 執行中被呼叫，要幫 App 重繪
    currentApp->setup(
        &appSprite);  // 這裡偷懶直接叫 setup 重繪，或是您可以加一個 refresh()
    appSprite.pushSprite(0, TOP_BAR_HEIGHT);
  }
}

bool SystemManager::updateEnvSensor() {
  // 發送測量命令
  if (!M5.In_I2C.start(SHT30_I2C_ADDR, false, SHT30_I2C_FREQ)) {
    M5.In_I2C.stop();
    return false;
  }

  // 寫入命令 0x2C (High Repeatability)
  if (!M5.In_I2C.write(0x2C)) {
    M5.In_I2C.stop();
    return false;
  }

  // 寫入命令 0x06
  if (!M5.In_I2C.write(0x06)) {
    M5.In_I2C.stop();
    return false;
  }

  // 發送 Stop 訊號，讓 SHT30 開始工作
  if (!M5.In_I2C.stop()) {
    return false;
  }
  delay(50);

  // 讀取數據
  if (!M5.In_I2C.start(SHT30_I2C_ADDR, true, SHT30_I2C_FREQ)) {
    M5.In_I2C.stop();
    return false;
  }

  uint8_t data[6];
  if (!M5.In_I2C.read(data, 6, true)) {
    M5.In_I2C.stop();
    return false;
  }

  // 發送 Stop 訊號，read 完後結束
  if (!M5.In_I2C.stop()) {
    showNotification("SHT30 I2C Stop Error", NOTIFICATION_DEFAULT_DURATION_MS);
  }

  // 檢查 Data
  if (data[0] == 0 && data[1] == 0 && data[3] == 0) {
    showNotification("SHT30 Data Error", NOTIFICATION_INFINITE_DURATION_MS);
    return false;
  }

  // 數值轉換
  uint16_t rawTemp = (uint16_t(data[0]) << 8) | uint16_t(data[1]);
  uint16_t rawHum = (uint16_t(data[3]) << 8) | uint16_t(data[4]);

  envTemp = -45 + 175 * ((float)rawTemp / 65535.0);
  envHum = 100 * ((float)rawHum / 65535.0);

  return true;
}

bool SystemManager::isWiFiConnected() {
  return (WiFi.status() == WL_CONNECTED);
}

String SystemManager::getWiFiIP() {
  if (!isWiFiConnected()) return "Disconnected";
  return WiFi.localIP().toString();
}

void SystemManager::syncTime() {
  if (config.wifiSSID == "") return;  // 沒設定就不跑

  showNotification("Syncing Time", NOTIFICATION_INFINITE_DURATION_MS);

  // 記錄原本的 Wi-Fi 狀態
  bool wasConnected = isWiFiConnected();

  // 如果原本沒連網，現在開啟
  if (!wasConnected) {
    // 嘗試開啟，如果失敗則直接結束
    if (!setWiFi(true)) return;
  }

  // 設定 ESP32 內部系統時間的時區
  configTime(0, 0, config.ntpServer.c_str());

  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 5000)) {
    m5::rtc_time_t rtc_time;
    rtc_time.hours = timeinfo.tm_hour;
    rtc_time.minutes = timeinfo.tm_min;
    rtc_time.seconds = timeinfo.tm_sec;

    m5::rtc_date_t rtc_date;
    rtc_date.year = timeinfo.tm_year + 1900;
    rtc_date.month = timeinfo.tm_mon + 1;
    rtc_date.date = timeinfo.tm_mday;
    rtc_date.weekDay = timeinfo.tm_wday;

    M5.Rtc.setDateTime({rtc_date, rtc_time});
    showNotification("Time Synced!", NOTIFICATION_DEFAULT_DURATION_MS);
  } else {
    showNotification("NTP Sync Failed", NOTIFICATION_DEFAULT_DURATION_MS);
  }

  long gmtOffset_sec = config.timezone * 3600;
  configTime(gmtOffset_sec, 0, config.ntpServer.c_str());

  // 如果原本是關閉的，校時完就關掉 (恢復原狀)
  // 如果原本就是開的 (例如使用者手動開啟)，則保持開啟
  if (!wasConnected) {
    setWiFi(false);
  }

  stopNotification();
}

String SystemManager::showFileSelector(String startPath, String extFilter) {
  // 1. 準備獨立 Canvas (Sprite)
    // 視窗大小設定
    int winW = 500;
    int winH = 400;
    int winX = (960 - winW) / 2;
    int winY = (540 - winH) / 2;

    M5Canvas overlay(&M5.Display);
    overlay.createSprite(winW, winH);
    
    // 設定字型 (防止被其他 App 影響)
    overlay.setFont(nullptr);
    overlay.setTextSize(2);

    // 2. 載入檔案列表
    String currentPath = startPath;
    if (!currentPath.endsWith("/")) currentPath += "/";
    std::vector<FileInfo> files = _getFileList(currentPath, extFilter);

    int selectedIndex = 0;
    int scrollOffset = 0;
    int itemsPerPage = FS_MAX_VISIBLE;
    int itemHeight = FS_ITEM_HEIGHT;
    int listStartY = 60;  // 列表起始 Y
    
    bool needsUpdate = true;
    long lastInputTime = millis();
    while (true) {
        M5.update();

        if (millis() - lastInputTime > FS_TIMEOUT_MS) {
            // 超過 30 秒沒操作，自動取消
            overlay.deleteSprite(); // 釋放記憶體
            return "";
        }
        
        // --- 輸入處理 ---
        // 滾輪控制
        if (M5.BtnA.wasPressed()) { // 上
            selectedIndex--;
            needsUpdate = true;
            lastInputTime = millis();
        }
        if (M5.BtnC.wasPressed()) { // 下
            selectedIndex++;
            needsUpdate = true;
            lastInputTime = millis();
        }

        // 邊界檢查與滾動計算
        if (selectedIndex < 0) selectedIndex = 0;
        if (selectedIndex >= files.size()) selectedIndex = files.size() - 1;

        if (selectedIndex < scrollOffset) scrollOffset = selectedIndex;
        if (selectedIndex >= scrollOffset + itemsPerPage) scrollOffset = selectedIndex - itemsPerPage + 1;

        // 觸控處理
        auto t = M5.Touch.getDetail();
        if (t.isPressed()) {
            // 轉換座標到視窗內部
            int localX = t.x - winX;
            int localY = t.y - winY;

            if (localX >= 0 && localX <= winW && localY >= 0 && localY <= winH) {
                // 檢查是否點擊 Cancel 按鈕 (底部區域)
                if (localY > winH - 60) {
                    overlay.deleteSprite(); // 釋放記憶體
                    return ""; // 取消回傳空字串
                }
                
                // 檢查是否點擊列表項目
                if (localY > listStartY && localY < listStartY + itemsPerPage * itemHeight) {
                    int clickedIndex = (localY - listStartY) / itemHeight + scrollOffset;
                    if (clickedIndex < files.size()) {
                        selectedIndex = clickedIndex;
                        needsUpdate = true;
                    }
                }

                lastInputTime = millis(); // 更新最後操作時間
            }
        }

        // 確認鍵 (BtnB 或 按下撥輪)
        if (M5.BtnB.wasPressed()) {
            if (files.empty()) continue; // 列表為空不能選

            FileInfo& target = files[selectedIndex];
            if (target.isFolder) {
                // 進入資料夾
                if (target.name == "..") {
                    // 回上一層 logic
                    // 去除最後一個 slash
                    currentPath = currentPath.substring(0, currentPath.length() - 1);
                    int lastSlash = currentPath.lastIndexOf('/');
                    currentPath = currentPath.substring(0, lastSlash + 1);
                } else {
                    // 進入資料夾
                    currentPath += target.name + "/";
                }
                files = _getFileList(currentPath, extFilter); // 重新讀取
                selectedIndex = 0;
                scrollOffset = 0;
                needsUpdate = true;
                lastInputTime = millis();
            } else {
                // 選中檔案
                String result = currentPath + target.name;
                overlay.deleteSprite();
                return result;
            }
        }

        // --- 繪圖邏輯 ---
        if (needsUpdate) {
            needsUpdate = false;

            // 清空畫布並畫邊框
            overlay.fillSprite(WHITE);
            overlay.drawRect(0, 0, winW, winH, BLACK);
            overlay.drawRect(4, 4, winW - 8, winH - 8, BLACK);

            // 畫標題
            overlay.setTextDatum(top_center);
            overlay.setTextColor(BLACK);
            overlay.drawString("Select File: " + currentPath, winW / 2, 20);
            overlay.drawFastHLine(20, 55, winW - 40, BLACK);

            // 畫列表
            int count = 0;
            for (int i = scrollOffset; i < files.size(); i++) {
                if (count >= itemsPerPage) break;
                
                int drawY = listStartY + count * itemHeight;
                _drawFileSelectorItem(&overlay, i, drawY, (i == selectedIndex), files[i]);
                count++;
            }
            
            // 若列表為空
            if (files.empty()) {
                overlay.setTextDatum(top_center);
                overlay.drawString("(Empty Folder)", winW / 2, listStartY + 20);
            }

            // 畫 Cancel 按鈕 (固定在底部)
            int btnH = 40;
            int btnY = winH - 50;
            overlay.drawFastHLine(20, btnY - 10, winW - 40, BLACK); // 分隔線
            
            // 這裡可以做一個簡單的按鈕樣式
            overlay.fillRect(150, btnY, winW - 300, btnH, BLACK); // 按鈕背景
            overlay.setTextColor(WHITE);
            overlay.setTextDatum(middle_center);
            overlay.drawString("CANCEL", winW / 2, btnY + btnH / 2);

            // 推送到螢幕
            overlay.pushSprite(winX, winY);
        }
        
        delay(10); // 避免 Watchdog 觸發
    }
}

bool SystemManager::loadAppConfig(String path, JsonDocument& doc) {
  if (!config.sdMounted) return false;
  File file = SD.open(path);
  if (!file) return false;

  DeserializationError error = deserializeJson(doc, file);
  file.close();
  return !error;
}

bool SystemManager::saveAppConfig(String path, const JsonDocument& doc) {
  if (!config.sdMounted) return false;
  File file = SD.open(path, FILE_WRITE);  // FILE_WRITE 會覆寫或新建
  if (!file) return false;

  serializeJson(doc, file);
  file.close();
  return true;
}

bool SystemManager::appendLog(String path, String message) {
  if (!config.sdMounted) return false;
  File file = SD.open(path, FILE_APPEND);  // FILE_APPEND 接續寫入
  if (!file) return false;

  file.println(message);
  file.close();
  return true;
}

String SystemManager::readTextFile(String path) {
  if (!config.sdMounted) return "SD Error";
  File file = SD.open(path);
  if (!file) return "File Error";

  String content = "";
  while (file.available()) {
    content += (char)file.read();
  }
  file.close();
  return content;
}

void SystemManager::_loadSystemConfig() {
  if (!config.sdMounted) return;

  File file = SD.open(CONFIG_FILE);
  if (!file) {
    showNotification("Failed to open config.json",
                     NOTIFICATION_INFINITE_DURATION_MS);
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);

  if (error) {
    showNotification("Failed to parse config.json",
                     NOTIFICATION_INFINITE_DURATION_MS);
  } else {
    // 將 JSON 資料存入 struct
    config.wifiSSID = doc["wifi_ssid"] | "";
    config.wifiPass = doc["wifi_pass"] | "";
    config.ntpServer = doc["ntp_server"] | "pool.ntp.org";
    config.timezone = doc["timezone"] | 8;
  }
  file.close();
}

void SystemManager::_drawStaticUI() {
  // 繪製 Bottom Bar -- Home 鍵
  bottomSprite.fillSprite(COLOR_WHITE);
  bottomSprite.drawFastHLine(0, 0, SCREEN_WIDTH, COLOR_BLACK);
  bottomSprite.setTextSize(2);
  bottomSprite.setTextColor(COLOR_BLACK);
  bottomSprite.setTextDatum(middle_center);
  bottomSprite.drawString("[ HOME ]", SCREEN_WIDTH / 2, BOTTOM_BAR_HEIGHT / 2);
  bottomSprite.pushSprite(0, SCREEN_HEIGHT - BOTTOM_BAR_HEIGHT);

  _updateStatusBar(true);
}

void SystemManager::_updateStatusBar(bool force) {
  // 如果是「自動更新」模式 (force=false)，且正在顯示通知，則不更新時間
  // 以免時間跳動覆蓋掉警告訊息
  if (!force && isNotificationActive) return;

  // 如果是「自動更新」模式，且還沒滿 60 秒，則離開
  if (!force && !isNotificationActive && millis() - lastStatusUpdate < 60000)
    return;

  // --- 開始繪圖 ---

  if (isNotificationActive) {
    // === 模式 A: 警告通知 (黑底白字，醒目) ===
    topSprite.fillSprite(COLOR_BLACK);    // 黑底
    topSprite.setTextColor(COLOR_WHITE);  // 白字
    topSprite.setTextSize(2);
    topSprite.setTextDatum(middle_center);

    // 畫出訊息
    topSprite.drawString(notificationMsg, SCREEN_WIDTH / 2, TOP_BAR_HEIGHT / 2);

  } else {
    // === 模式 B: 正常狀態 (白底黑字) ===
    topSprite.fillSprite(COLOR_WHITE);
    topSprite.drawFastHLine(0, TOP_BAR_HEIGHT - 2, SCREEN_WIDTH, COLOR_BLACK);

    // --- 顯示左側：時間與資訊 ---
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
      // 如果剛開機還沒抓到時間，顯示 00:00
      timeinfo.tm_hour = 0;
      timeinfo.tm_min = 0;
      timeinfo.tm_year = 0;
      timeinfo.tm_mon = 0;
      timeinfo.tm_mday = 0;
    }
    int bat = M5.Power.getBatteryLevel();
    int vot = M5.Power.getBatteryVoltage();
    const char* wifiStatus = isWiFiConnected() ? "ON" : "OFF";

    char buffer[64];
    snprintf(buffer, sizeof(buffer),
             "%02d:%02d  %04d-%02d-%02d   BAT: %d%% (%dmV)   WIFI: %s",
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_year + 1900,
             timeinfo.tm_mon + 1, timeinfo.tm_mday, bat, vot, wifiStatus);

    topSprite.setTextSize(2);
    topSprite.setTextDatum(middle_left);  // 靠左對齊
    topSprite.setTextColor(COLOR_BLACK);
    topSprite.drawString(buffer, 20, TOP_BAR_HEIGHT / 2);

    // --- 顯示右側：App Name ---
    topSprite.setTextDatum(middle_right);  // 靠右對齊
    if (currentApp != nullptr) {
      topSprite.drawString(currentApp->appName, SCREEN_WIDTH - 20,
                           TOP_BAR_HEIGHT / 2);
    } else {
      topSprite.drawString("No App", SCREEN_WIDTH - 20, TOP_BAR_HEIGHT / 2);
    }

    // 只有在正常模式下才更新計時器
    lastStatusUpdate = millis();
  }

  // 推送畫面
  topSprite.pushSprite(0, 0);
}

bool SystemManager::_checkInput() {
  bool hasInput = false;

  // 檢查觸控
  if (M5.Touch.getCount() > 0) {
    hasInput = true;
  }

  // 檢查按鍵 (M5Paper 側邊撥桿: 左/右/按)
  if (M5.BtnA.isPressed() || M5.BtnB.isPressed() || M5.BtnC.isPressed()) {
    hasInput = true;
  }

  return hasInput;
}

void SystemManager::_checkPowerManagement() {
  int battery = M5.Power.getBatteryVoltage();

  // 檢查是否需要 [強制關機] (保護電池)
  if (battery < BATTERY_CRITICAL_THRESHOLD) {
    // 顯示關機圖示
    showNotification("Low Battery! Shutting Down",
                     NOTIFICATION_INFINITE_DURATION_MS);
    M5.Power.powerOff();  // 斷電關機 (需按電源鍵才能重開)
    return;
  }

  // 決定休眠的超時時間
  unsigned long timeout = (battery < BATTERY_LOW_THRESHOLD)
                              ? SLEEP_TIMEOUT_LOW_BAT_MS
                              : SLEEP_TIMEOUT_NORMAL_MS;

  // 檢查是否有輸入活動
  if (_checkInput()) {
    lastInteractionTime = millis();  // 重置計時器

    // 如果原本是休眠狀態，現在被喚醒
    if (isSleeping) {
      isSleeping = false;
      setCpuFrequencyMhz(240);  // 恢復 CPU 全速

      // 喚醒時，更新一次 UI 表示活著
      showNotification("Waking Up", 1000);
    }
  }

  // 判斷是否該進入休眠
  // 條件：沒被強制喚醒 AND 超過時間 AND 目前還沒睡
  if (!forceAwake && !isSleeping &&
      (millis() - lastInteractionTime > timeout)) {
    isSleeping = true;

    showNotification("Sleep Mode", NOTIFICATION_INFINITE_DURATION_MS);
    if (!isWiFiConnected()) {
      setWiFi(false);  // 關閉 WiFi 節省電力
    }
    M5.Power.lightSleep(timeout + 1000);
  }
}

void SystemManager::_loadCalibration() {
  if (!config.sdMounted) {
    calibrateTouch();
    return;
  }

  File file = SD.open(CALIBRATION_FILE);
  if (!file) {
    // 如果檔案不存在，代表是第一次開機
    Serial.println("No calibration file found. Running calibration...");

    // 自動執行校正
    calibrateTouch();
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    Serial.println("Calibration file corrupted.");
    return;
  }

  // 讀取數據
  JsonArray data = doc["cal_data"];
  if (data.size() == 8) {
    uint16_t calData[8];
    for (int i = 0; i < 8; i++) {
      calData[i] = data[i].as<uint16_t>();
    }

    // [關鍵] 套用校正數據
    M5.Display.setTouchCalibrate(calData);
    Serial.println("Touch Calibration Loaded.");
  }
}

void SystemManager::_logEnvData() {
  if (!config.sdMounted) return;

  // 準備新的數據字串
  if (!SD.exists("/Data")) {
    SD.mkdir("/Data");
  }

  // 取得時間
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;

  char timeStr[20];
  sprintf(timeStr, "%04d-%02d-%02d %02d:%02d", timeinfo.tm_year + 1900,
          timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour,
          timeinfo.tm_min);

  String newLine = String(timeStr) + "," + String(envTemp, 1) + "," +
                   String(envHum, 1) + "\n";

  // 檢查現有檔案行數
  int currentLines = 0;
  if (SD.exists(ENV_LOG_FILE)) {
    File f = SD.open(ENV_LOG_FILE, FILE_READ);
    while (f.available()) {
      // 快速掃描換行符號來計算行數 (比 readStringUntil 快且省記憶體)
      if (f.read() == '\n') {
        currentLines++;
      }
    }
    f.close();
  }

  // 3. 判斷是否需要清理 (Rotation)
  if (currentLines < MAX_LOG_ENV_LINES) {
    // [情況 A] 還沒滿，直接 Append 到後面
    File f = SD.open(ENV_LOG_FILE, FILE_APPEND);
    if (f) {
      f.print(newLine);
      f.close();
    }
  } else {
    // [情況 B] 滿了，執行「去舊留新」
    File sourceFile = SD.open(ENV_LOG_FILE, FILE_READ);
    File tempFile = SD.open(ENV_TEMP_LOG_FILE, FILE_WRITE);

    if (sourceFile && tempFile) {
      // 計算要跳過幾行 (舊資料)
      int linesSkipped = 0;

      // 複製舊檔案 (跳過頭幾行)
      while (sourceFile.available()) {
        String line = sourceFile.readStringUntil('\n');
        // 補回被 readStringUntil 吃掉的換行符 (如果 CSV
        // 原本最後一行沒換行可能會多補，但在這裡通常沒問題)

        if (linesSkipped < LOG_ENV_CLEANUP_LINES) {
          // 這是舊資料，跳過不寫入
          linesSkipped++;
        } else {
          // 這是要保留的資料，寫入暫存檔
          tempFile.print(line + "\n");
        }
      }

      // 寫入最新的這一筆數據
      tempFile.print(newLine);

      // 關閉檔案
      sourceFile.close();
      tempFile.close();

      // 取代檔案 (刪除舊的，重新命名暫存檔)
      SD.remove(ENV_LOG_FILE);
      SD.rename(ENV_TEMP_LOG_FILE, ENV_LOG_FILE);

      Serial.println("Log rotated successfully.");
    } else {
      // 開檔失敗防護
      if (sourceFile) sourceFile.close();
      if (tempFile) tempFile.close();
    }
  }
}

std::vector<FileInfo> SystemManager::_getFileList(String path,
                                                  String extFilter) {
 std::vector<FileInfo> list;
    File root = SD.open(path);
    if (!root || !root.isDirectory()) return list;

    // 如果不是根目錄，加入 ".." (上一層)
    if (path != "/") {
        list.push_back({"..", true});
    }

    File file = root.openNextFile();
    while (file) {
        String fileName = String(file.name());
        
        // 過濾邏輯：只顯示資料夾 或 符合副檔名的檔案
        // 忽略隱藏檔 (以 . 開頭)
        if (!fileName.startsWith(".")) {
            bool isFolder = file.isDirectory();
            bool match = isFolder; // 資料夾一定顯示

            if (!isFolder && extFilter.length() > 0) {
                if (fileName.endsWith(extFilter)) match = true;
            } else if (!isFolder && extFilter.length() == 0) {
                match = true; // 沒有過濾器則全部顯示
            }

            if (match) {
                // 為了美觀，若是資料夾，去掉路徑前綴只留名稱 (視 SD 函式庫行為而定)
                // 這裡假設 file.name() 回傳的是純檔名，若回傳完整路徑需自行切割
                if (fileName.lastIndexOf("/") > -1) {
                    fileName = fileName.substring(fileName.lastIndexOf("/") + 1);
                }
                list.push_back({fileName, isFolder});
            }
        }
        file = root.openNextFile();
    }
    
    // 排序：資料夾在最上面，接著依檔名排序
    std::sort(list.begin(), list.end(), [](const FileInfo& a, const FileInfo& b) {
        if (a.isFolder != b.isFolder) return a.isFolder > b.isFolder; // True (1) > False (0)
        return a.name < b.name;
    });

    return list;
}

void SystemManager::_drawFileSelectorItem(M5Canvas* targetCanvas, int index, int yPos, bool isSelected, const FileInfo& info) {
    int w = targetCanvas->width();
    int h = 40; // 單行高度

    // 背景
    if (isSelected) {
        targetCanvas->fillRect(5, yPos, w - 10, h, BLACK);
        targetCanvas->setTextColor(WHITE);
    } else {
        targetCanvas->fillRect(5, yPos, w - 10, h, WHITE);
        targetCanvas->setTextColor(BLACK);
    }

    // icon 與 文字
    String prefix = info.isFolder ? "[DIR] " : "      ";
    if (info.name == "..") prefix = "[UP]  ";
    targetCanvas->setTextDatum(middle_left);
    targetCanvas->drawString(prefix + info.name, 10, yPos + h / 2);

    // 畫分隔線
    if (!isSelected) {
        targetCanvas->drawFastHLine(10, yPos + h - 1, w - 20, LIGHTGREY);
    }
}
