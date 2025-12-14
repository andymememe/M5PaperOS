#include "App/Settings.h"
#include "SystemManager.h" // 引用 SystemManager 以呼叫 API

SettingsApp::SettingsApp() : AppBase("Settings") {
    // 計算按鈕位置，讓它們垂直置中
    int startY = 100;
    btnSyncY = startY;
    btnCalibrateY = startY + BTN_HEIGHT + GAP;
    selectedIndex = 0; // 預設選中第一個
}

void SettingsApp::setup(M5Canvas* _canvas) {
    AppBase::setup(_canvas); // 初始化畫布
    drawUI();
}

void SettingsApp::drawUI() {
    canvas->fillSprite(COLOR_WHITE);

    // 標題
    canvas->setTextSize(3);
    canvas->setTextColor(COLOR_BLACK);
    canvas->setTextDatum(top_center);
    canvas->drawString("System Settings", SCREEN_WIDTH / 2, 20);
    canvas->drawFastHLine(20, 60, SCREEN_WIDTH - 40, COLOR_BLACK);

    // 繪製按鈕 (根據 selectedIndex 決定誰反白)
    _drawBtnState(0, selectedIndex == 0);
    _drawBtnState(1, selectedIndex == 1);
}

void SettingsApp::loop(lgfx::touch_point_t t, bool isPressed) {
    bool selectionChanged = false;

    // 往上撥 (選 Sync)
    if (M5.BtnA.wasPressed()) {
        if (selectedIndex != 0) {
            selectedIndex = 0;
            selectionChanged = true;
        }
    }
    
    // 往下撥 (選 Calibrate)
    if (M5.BtnC.wasPressed()) {
        if (selectedIndex != 1) {
            selectedIndex = 1;
            selectionChanged = true;
        }
    }

    // 局部重繪：如果選擇改變了，更新畫面
    if (selectionChanged) {
        _drawBtnState(0, selectedIndex == 0);
        _drawBtnState(1, selectedIndex == 1);
        canvas->pushSprite(0, TOP_BAR_HEIGHT);
    }

    // 按下撥桿 (執行)
    if (M5.BtnB.wasPressed()) {
        _executeAction(selectedIndex);
        return;
    }
}

void SettingsApp::_drawBtnState(int index, bool isSelected) {
    int x = (SCREEN_WIDTH - BTN_WIDTH) / 2;
    int y = (index == 0) ? btnSyncY : btnCalibrateY;
    String label = (index == 0) ? "Sync Time (NTP)" : "Touch Calibration";

    // 設定顏色 (選中=黑底白字, 未選中=白底黑字)
    uint16_t bgColor = isSelected ? COLOR_BLACK : COLOR_WHITE;
    uint16_t textColor = isSelected ? COLOR_WHITE : COLOR_BLACK;

    // 填色與邊框
    canvas->fillRect(x, y, BTN_WIDTH, BTN_HEIGHT, bgColor);
    canvas->drawRect(x, y, BTN_WIDTH, BTN_HEIGHT, COLOR_BLACK);

    // 文字
    canvas->setTextSize(2); // 字型稍微改小一點以免爆框
    canvas->setTextDatum(middle_center);
    canvas->setTextColor(textColor);
    canvas->drawString(label, SCREEN_WIDTH / 2, y + BTN_HEIGHT / 2);
}

void SettingsApp::_executeAction(int index) {
    if (index == 0) {
        // --- Sync Time ---
        // 畫一個 "Syncing..." 的狀態
        int x = (SCREEN_WIDTH - BTN_WIDTH) / 2;
        canvas->fillRect(x, btnSyncY, BTN_WIDTH, BTN_HEIGHT, COLOR_BLACK);
        canvas->setTextColor(COLOR_WHITE);
        canvas->drawString("Syncing...", SCREEN_WIDTH / 2, btnSyncY + BTN_HEIGHT/2);
        canvas->pushSprite(0, TOP_BAR_HEIGHT);

        sys.syncTime();
    } 
    else if (index == 1) {
        // --- Calibration ---
        int x = (SCREEN_WIDTH - BTN_WIDTH) / 2;
        canvas->fillRect(x, btnCalibrateY, BTN_WIDTH, BTN_HEIGHT, COLOR_BLACK);
        canvas->setTextColor(COLOR_WHITE);
        canvas->drawString("Starting...", SCREEN_WIDTH / 2, btnCalibrateY + BTN_HEIGHT/2);
        canvas->pushSprite(0, TOP_BAR_HEIGHT);
        
        delay(200);
        sys.calibrateTouch();
    }

    // 執行完畢後重繪 UI (恢復按鈕狀態)
    drawUI();
    canvas->pushSprite(0, TOP_BAR_HEIGHT);
}
