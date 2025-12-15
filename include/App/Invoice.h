#ifndef INVOICE_H
#define INVOICE_H

#include "AppBase.h"

class InvoiceApp : public AppBase {
private:
    const String INVOICE_DATA_PATH = "/Invoice/invoice.txt";

    String carriage = "";

    void _drawCode39(String content, int height);

public:
    InvoiceApp();
    void setup(M5Canvas* _canvas) override;
    void loop(lgfx::touch_point_t t, bool isPressed) override;
    void drawUI() override;
    void exit() override;
};

#endif