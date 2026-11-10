#pragma once

#include "display/lcd_display.h"

#include <functional>

// Full-screen Otto face for the stationary 240x240 Desk display.
// The caller must initialize the ST7789 panel with a verified runtime pin map.
class OttoDeskDisplay : public SpiLcdDisplay {
public:
    OttoDeskDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                    int width = 240, int height = 240, int offset_x = 0, int offset_y = 0,
                    bool mirror_x = false, bool mirror_y = false, bool swap_xy = false,
                    std::function<void(const char*)> emotion_callback = {});

    void SetupUI() override;
    void SetEmotion(const char* emotion) override;
    void SetStatus(const char* status) override;
    void SetPreviewImage(std::unique_ptr<LvglImage> image) override;

private:
    void FitEmotionToScreen();
    std::function<void(const char*)> emotion_callback_;
};
