#include "App/Invoice.h"

#include "SystemManager.h"  // 引用 SystemManager 以呼叫 API

InvoiceApp::InvoiceApp() : AppBase("Invoice") {}

void InvoiceApp::setup(M5Canvas* _canvas) {
  AppBase::setup(_canvas);
  carriage = sys.readTextFile(INVOICE_DATA_PATH);
  drawUI();
}

void InvoiceApp::drawUI() {
  M5.Display.setEpdMode(epd_mode_t::epd_quality);
  _drawCode39(carriage, 200);
  M5.Display.setEpdMode(epd_mode_t::epd_fast);
}

void InvoiceApp::loop(lgfx::touch_point_t t, bool isPressed) {}

void InvoiceApp::exit() {}

// 簡單的 Code 39 繪製函數
void InvoiceApp::_drawCode39(String content, int height) {
  // Code 39 字元表 (部分範例，涵蓋數字與大寫英文)
  const char* chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ-. *$/+%";
  const int codes[] = {
      0x34,  0x121, 0x61,  0x160, 0x31,
      0x130, 0x70,  0x25,  0x124, 0x64,  // 0-9
      0x109, 0x49,  0x148, 0x19,  0x118,
      0x58,  0xd,   0x10c, 0x4c,  0x1c,  // A-J
      0x103, 0x43,  0x142, 0x13,  0x112,
      0x52,  0x7,   0x106, 0x46,  0x16,  // K-T
      0x181, 0xc1,  0x1c0, 0x91,  0x190,
      0xd0,  0x85,  0x184, 0xc4,  0x94,  // U-Z, -, ., space, *
      0xa8,  0xa2,  0x8a,  0x2a          // $, /, +, %
  };

  content.toUpperCase();
  if (!content.startsWith("*")) content = "*" + content;
  if (!content.endsWith("*")) content += "*";

  int currentX = canvas->getPivotX() - 300;
  int narrowW = 4;            // 窄條寬度 (像素)
  int wideW = narrowW * 2.5;  // 寬條寬度 (通常是窄條的 2-3 倍)

  canvas->fillRect(
      currentX, canvas->getPivotY() - (height / 2),
      (content.length() * (6 * narrowW + 3 * wideW + narrowW)) + 20, height,
      WHITE);  // 清空背景

  for (int i = 0; i < content.length(); i++) {
    int idx = -1;
    for (int j = 0; j < 43; j++) {
      if (chars[j] == content[i]) {
        idx = j;
        break;
      }
    }
    if (idx == -1) continue;

    int code = codes[idx];
    for (int bit = 8; bit >= 0; bit--) {
      int w = (code & (1 << bit)) ? wideW : narrowW;
      int isBlack = (bit % 2 == 0);
      if (isBlack) {
        canvas->fillRect(currentX, canvas->getPivotY() - (height / 2), w,
                         height, BLACK);
      }
      currentX += w;
    }
    currentX += narrowW;  // 字符間隔
  }
}