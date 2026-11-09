#pragma once

#include "display/lcd_display.h"

// Full-screen Otto face for the stationary 240x240 Desk display.
// The caller must initialize the ST7789 panel with a verified runtime pin map.
class OttoDeskDisplay : public SpiLcdDisplay {
public:
    OttoDeskDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                    int width = 240, int height = 240, int offset_x = 0, int offset_y = 0,
                    bool mirror_x = false, bool mirror_y = false, bool swap_xy = false);

    void SetupUI() override;
    void SetPreviewImage(std::unique_ptr<LvglImage> image) override;
};
