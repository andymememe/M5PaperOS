#ifndef TEST_H
#define TEST_H

#include "AppBase.h"

class TestApp : public AppBase {
private:
    //用來記錄上一次的按鍵狀態，避免重複繪圖
    bool lastBtnL = false;
    bool lastBtnR = false;
    bool lastBtnP = false;

    // 輔助繪圖函式：畫按鈕狀態
    void _drawButtonState(int x, int y, String label, bool pressed);

public:
    TestApp();
    void setup(M5Canvas* _canvas) override;
    void loop(lgfx::touch_point_t t, bool isPressed) override;
    void drawUI() override;
};

#endif