#include "App/Launcher.h"
#include "SystemManager.h" // 為了呼叫 sys.launchApp

#define ITEM_HEIGHT 80    // 每個選項的高度
#define ITEM_MARGIN 10    // 邊距

LauncherApp::LauncherApp() : AppBase("Launcher") {}

void LauncherApp::registerApp(AppBase* app) {
    AppItem item;
    item.name = app->appName;
    item.app = app;
    appList.push_back(item);
}

void LauncherApp::setup(M5Canvas* _canvas) {
    AppBase::setup(_canvas);
    // 每次進入 Launcher 重新計算，確保選中的項目在視野內
    if (selectedIndex < scrollOffset) {
        scrollOffset = selectedIndex;
    } else if (selectedIndex >= scrollOffset + MAX_VISIBLE_ITEMS) {
        scrollOffset = selectedIndex - MAX_VISIBLE_ITEMS + 1;
    }
    drawUI();
}

void LauncherApp::drawUI() {
    canvas->setFont(nullptr); // 使用預設字型
    canvas->fillSprite(COLOR_WHITE);

    // 繪製標題
    canvas->setTextSize(3);
    canvas->setTextColor(COLOR_BLACK);
    canvas->setTextDatum(top_center);
    canvas->drawString("Main Menu", SCREEN_WIDTH / 2, 20);
    
    // 繪製分隔線
    canvas->drawFastHLine(20, 60, SCREEN_WIDTH - 40, COLOR_BLACK);
    
    // 繪製列表
    _drawList();
}

// 繪製單個列表項目 (包含選中狀態的判斷)
void LauncherApp::_drawItem(int index) {
    // 計算該項目在「螢幕上」的相對位置
    // 如果該項目不在目前的可見視窗內，就不畫
    if (index < scrollOffset || index >= scrollOffset + MAX_VISIBLE_ITEMS) return;

    int relativeIndex = index - scrollOffset; // 在畫面上的第幾排 (0 ~ 4)
    int y = LIST_START_Y + (relativeIndex * ITEM_HEIGHT);
    
    bool isSelected = (index == selectedIndex);

    // 設定顏色
    uint16_t bgColor = isSelected ? COLOR_BLACK : COLOR_WHITE;
    uint16_t textColor = isSelected ? COLOR_WHITE : COLOR_BLACK;

    // 畫背景框 (稍微內縮一點)
    canvas->fillRect(40, y + 5, SCREEN_WIDTH - 80, ITEM_HEIGHT - 10, bgColor);
    canvas->drawRect(40, y + 5, SCREEN_WIDTH - 80, ITEM_HEIGHT - 10, COLOR_BLACK);

    // 畫文字
    canvas->setTextColor(textColor);
    canvas->setTextSize(3);
    canvas->setTextDatum(middle_center);
    canvas->drawString(appList[index].name, SCREEN_WIDTH / 2, y + ITEM_HEIGHT / 2);
}

void LauncherApp::_drawList() {
    // 只繪製「可見範圍」內的項目
    // 範圍從 scrollOffset 到 scrollOffset + MAX_VISIBLE_ITEMS
    // 或是直到列表結束
    for (int i = 0; i < MAX_VISIBLE_ITEMS; i++) {
        int actualIndex = scrollOffset + i;
        if (actualIndex >= appList.size()) break; // 沒東西了

        _drawItem(actualIndex);
    }
    
    // 繪製捲軸指示器 (Scrollbar) - 選用功能，讓使用者知道還有下面
    if (appList.size() > MAX_VISIBLE_ITEMS) {
        int barHeight = APP_AREA_HEIGHT * MAX_VISIBLE_ITEMS / appList.size();
        int barY = LIST_START_Y + (scrollOffset * (APP_AREA_HEIGHT - LIST_START_Y) / appList.size());
        // 簡單畫一條線在右邊
        canvas->fillRect(SCREEN_WIDTH - 10, LIST_START_Y, 5, APP_AREA_HEIGHT - LIST_START_Y, COLOR_WHITE); // 清除舊的
        canvas->fillRect(SCREEN_WIDTH - 10, barY, 5, barHeight, COLOR_BLACK);
    }
}

void LauncherApp::loop(lgfx::touch_point_t t, bool isPressed) {
    // --- 處理實體按鍵 (撥桿) ---
    // M5Paper 撥桿: 左(上)BtnA, 右(下)BtnC, 按下BtnB
    
    if (M5.BtnA.wasPressed()) { // 往上 (Previous)
        if (selectedIndex > 0) {
            int oldIndex = selectedIndex;
            selectedIndex--;

            // 檢查是否需要捲動
            if (selectedIndex < scrollOffset) {
                // 游標跑出上面了 -> 捲動視窗
                scrollOffset = selectedIndex;
                drawUI(); // 重繪整個 UI (包含列表移動)
                canvas->pushSprite(0, TOP_BAR_HEIGHT);
            } else {
                // 沒有捲動，只是游標移動 -> 局部刷新兩行
                _drawItem(oldIndex);
                _drawItem(selectedIndex);
                canvas->pushSprite(0, TOP_BAR_HEIGHT);
            }
        }
    }

    if (M5.BtnC.wasPressed()) { // 往下 (Next)
        if (selectedIndex < appList.size() - 1) {
            int oldIndex = selectedIndex;
            selectedIndex++;

            // 檢查是否需要捲動
            if (selectedIndex >= scrollOffset + MAX_VISIBLE_ITEMS) {
                // 游標跑出下面了 -> 捲動視窗
                // 新的 offset = 目前選的 index - (一頁能顯示的數量 - 1)
                scrollOffset = selectedIndex - MAX_VISIBLE_ITEMS + 1;
                drawUI(); // 重繪整個 UI
                canvas->pushSprite(0, TOP_BAR_HEIGHT);
            } else {
                // 沒有捲動 -> 局部刷新
                _drawItem(oldIndex);
                _drawItem(selectedIndex);
                canvas->pushSprite(0, TOP_BAR_HEIGHT);
            }
        }
    }

    if (M5.BtnB.wasPressed()) { // 確認
        sys.launchApp(appList[selectedIndex].app);
        return;
    }

    // --- 2. 處理觸控 (選取與執行) ---
    // 使用者沒有要求觸控捲動，只要求觸控點擊選取
    if (isPressed) {
        int relativeY = t.y - LIST_START_Y;
        
        // 確保點擊在列表區域內
        if (relativeY >= 0) {
            // 計算點擊的是「畫面上的第幾個格子」
            int visualIndex = relativeY / ITEM_HEIGHT;
            
            // 換算成「真實的 App Index」
            int actualIndex = scrollOffset + visualIndex;

            // 邊界檢查
            if (visualIndex < MAX_VISIBLE_ITEMS && actualIndex < appList.size()) {
                
                // 如果點擊的跟原本選的不一樣，先視覺切換過去
                if (selectedIndex != actualIndex) {
                    selectedIndex = actualIndex;
                    drawUI(); // 重繪確保高亮正確 (因為觸控可能跨越好幾格)
                    canvas->pushSprite(0, TOP_BAR_HEIGHT);
                    delay(100); 
                }
                
                // 執行 App
                sys.launchApp(appList[selectedIndex].app);
            }
        }
    }
}