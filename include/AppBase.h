#ifndef APP_BASE_H
#define APP_BASE_H

#include "Config.h"

class AppBase {
public:
    String appName;
    M5Canvas* canvas; // App 專屬畫布指標

    AppBase(String name);
    virtual ~AppBase();

    // 必須實作的虛擬函式
    virtual void setup(M5Canvas* _canvas);
    virtual void loop(lgfx::touch_point_t touch, bool isTouchPressed);
    virtual void drawUI();
    virtual void exit();
};

#endif