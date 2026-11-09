#include "otto_desk_display.h"

#include <esp_err.h>
#include <esp_log.h>

#include "display/lvgl_display/lvgl_image.h"
#include "display/lvgl_display/lvgl_theme.h"

#define TAG "OttoDeskDisplay"

OttoDeskDisplay::OttoDeskDisplay(esp_lcd_panel_io_handle_t panel_io,
                                 esp_lcd_panel_handle_t panel, int width, int height,
                                 int offset_x, int offset_y, bool mirror_x, bool mirror_y,
                                 bool swap_xy)
    : SpiLcdDisplay(panel_io, panel, width, height, offset_x, offset_y, mirror_x, mirror_y,
                    swap_xy) {}

void OttoDeskDisplay::SetupUI() {
    if (setup_ui_called_) {
        ESP_LOGW(TAG, "SetupUI called more than once; ignoring duplicate call");
        return;
    }

    SpiLcdDisplay::SetupUI();

    auto* dark_theme = LvglThemeManager::GetInstance().GetTheme("dark");
    if (dark_theme != nullptr) {
        SetTheme(dark_theme);
    }

    {
        DisplayLockGuard lock(this);
        if (preview_image_ != nullptr) {
            lv_obj_set_size(preview_image_, width_, height_);
        }
    }

    // The Otto GIF collection is selected for the otto-desk board in main/CMakeLists.txt.
    SetEmotion("staticstate");
}

void OttoDeskDisplay::SetPreviewImage(std::unique_ptr<LvglImage> image) {
    DisplayLockGuard lock(this);
    if (preview_image_ == nullptr) {
        ESP_LOGE(TAG, "Preview image requested before UI initialization");
        return;
    }

    if (image == nullptr) {
        esp_timer_stop(preview_timer_);
        lv_obj_remove_flag(emoji_box_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(preview_image_, LV_OBJ_FLAG_HIDDEN);
        preview_image_cached_.reset();
        if (gif_controller_ != nullptr) {
            gif_controller_->Start();
        }
        return;
    }

    preview_image_cached_ = std::move(image);
    const auto* image_desc = preview_image_cached_->image_dsc();
    lv_image_set_src(preview_image_, image_desc);
    lv_image_set_rotation(preview_image_, 0);
    if (image_desc->header.w > 0 && image_desc->header.h > 0) {
        lv_image_set_scale(preview_image_, 256 * width_ / image_desc->header.w);
    }

    if (gif_controller_ != nullptr) {
        gif_controller_->Stop();
    }
    lv_obj_add_flag(emoji_box_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(preview_image_, LV_OBJ_FLAG_HIDDEN);
    esp_timer_stop(preview_timer_);
    ESP_ERROR_CHECK(esp_timer_start_once(preview_timer_, PREVIEW_IMAGE_DURATION_MS * 1000));
}
