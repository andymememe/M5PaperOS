#include "AppBase.h"

AppBase::AppBase(String name) : appName(name), canvas(nullptr) {}

AppBase::~AppBase() {}

void AppBase::setup(M5Canvas* _canvas) {
    canvas = _canvas;
    canvas->fillSprite(COLOR_WHITE);
    drawUI();
}

void AppBase::loop(lgfx::touch_point_t touch, bool isTouchPressed) {
    // 預設不做事
}

void AppBase::drawUI() {
    if (canvas) {
        canvas->setTextSize(2);
        canvas->setTextColor(COLOR_BLACK);
        canvas->drawString(appName, 10, 10);
    }
}

void AppBase::exit() {
    // 預設不做事
}