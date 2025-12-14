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

bool AppBase::checkButton(int x, int y, int w, int h, String label, lgfx::touch_point_t t) {
    if (!canvas) return false;

    bool pressed = (t.x >= x && t.x <= x + w && t.y >= y && t.y <= y + h);

    if (pressed) {
        canvas->fillRect(x, y, w, h, COLOR_BLACK);
        canvas->setTextColor(COLOR_WHITE);
    } else {
        canvas->drawRect(x, y, w, h, COLOR_BLACK);
        canvas->setTextColor(COLOR_BLACK);
    }
    canvas->setTextDatum(middle_center);
    canvas->drawString(label, x + w / 2, y + h / 2);

    if (pressed) {
        // 為了按鈕即時回饋，這裡做局部刷新
        // 注意：Y軸偏移量要在這裡補上，因為 pushSprite 是對螢幕絕對座標
        canvas->pushSprite(0, TOP_BAR_HEIGHT); 
    }
    return pressed;
}