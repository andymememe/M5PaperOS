#include "App/Test.h"

TestApp::TestApp() : AppBase("Test") {}

void TestApp::setup(M5Canvas* _canvas) {
    AppBase::setup(_canvas);
    drawUI();
}

void TestApp::drawUI() {
    canvas->fillSprite(COLOR_WHITE);
    
    // 標題
    canvas->setTextSize(3);
    canvas->setTextColor(COLOR_BLACK);
    canvas->setTextDatum(top_center);
    canvas->drawString("Hardware Input Test", SCREEN_WIDTH / 2, 20);
    
    // 說明文字
    canvas->setTextSize(2);
    canvas->drawString("Press Buttons (Wheel) or Touch Screen", SCREEN_WIDTH / 2, 60);

    // 繪製初始按鈕狀態 (空心)
    _drawButtonState(200, 200, "L (Up)", false);
    _drawButtonState(480, 200, "Push", false);
    _drawButtonState(760, 200, "R (Down)", false);
}

void TestApp::_drawButtonState(int x, int y, String label, bool pressed) {
    int r = 50; // 半徑
    
    if (pressed) {
        canvas->fillCircle(x, y, r, COLOR_BLACK);
        canvas->setTextColor(COLOR_WHITE);
    } else {
        canvas->fillCircle(x, y, r, COLOR_WHITE);
        canvas->drawCircle(x, y, r, COLOR_BLACK);
        canvas->setTextColor(COLOR_BLACK);
    }
    
    canvas->setTextDatum(middle_center);
    canvas->setTextSize(2);
    canvas->drawString(label, x, y);
}

void TestApp::loop(lgfx::touch_point_t t, bool isPressed) {
    bool needUpdate = false;

    // --- 檢測按鍵狀態 ---
    // 使用 isPressed() 來獲得即時狀態 (按住就為真)
    bool curBtnL = M5.BtnA.isPressed();
    bool curBtnR = M5.BtnC.isPressed();
    bool curBtnP = M5.BtnB.isPressed();

    // 只有狀態改變時才重繪，節省資源
    if (curBtnL != lastBtnL) {
        _drawButtonState(200, 200, "L (Up)", curBtnL);
        lastBtnL = curBtnL;
        needUpdate = true;
    }
    if (curBtnP != lastBtnP) {
        _drawButtonState(480, 200, "Push", curBtnP);
        lastBtnP = curBtnP;
        needUpdate = true;
    }
    if (curBtnR != lastBtnR) {
        _drawButtonState(760, 200, "R (Down)", curBtnR);
        lastBtnR = curBtnR;
        needUpdate = true;
    }

    // --- 檢測觸控 ---
    if (isPressed) {
        // 畫出觸控軌跡
        canvas->fillCircle(t.x, t.y, 10, COLOR_BLACK);
        
        // 顯示座標數值
        // 先用白色方塊蓋掉舊數值
        canvas->fillRect(0, 400, SCREEN_WIDTH, 50, COLOR_WHITE);
        canvas->setTextDatum(middle_center);
        canvas->setTextColor(COLOR_BLACK);
        canvas->setTextSize(3);
        String coord = "X: " + String(t.x) + "  Y: " + String(t.y);
        canvas->drawString(coord, SCREEN_WIDTH/2, 425);
        
        needUpdate = true;
    }

    // --- 統一推送畫面 ---
    if (needUpdate) {
        // 使用 DU4 快速刷新模式，確保測試反應靈敏
        canvas->pushSprite(0, TOP_BAR_HEIGHT);
    }
}