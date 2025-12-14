#ifndef SETTINGS_H
#define SETTINGS_H

#include "AppBase.h"

class SettingsApp : public AppBase {
private:
    // 定義按鈕區域，方便管理 layout
    const int BTN_HEIGHT = 80;
    const int BTN_WIDTH = 400;
    const int GAP = 40;
    
    // 按鈕 Y 座標 (自動計算)
    int btnSyncY;
    int btnCalibrateY;
    int selectedIndex = 0; 

    // 輔助函式：繪製指定按鈕的狀態 (反白或正常)
    void _drawBtnState(int index, bool isSelected);

    // 輔助函式：執行該選項的功能
    void _executeAction(int index);

public:
    SettingsApp();
    void setup(M5Canvas* _canvas) override;
    void loop(lgfx::touch_point_t t, bool isPressed) override;
    void drawUI() override;
};

#endif