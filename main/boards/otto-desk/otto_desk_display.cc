#include "otto_desk_display.h"

#include <esp_err.h>
#include <esp_log.h>

#include <algorithm>

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

    // Remove the legacy XiaoZhi chrome: the face owns the whole 240x240 panel.
    {
        DisplayLockGuard lock(this);
        lv_obj_t* chrome[] = {top_bar_, status_bar_, content_, side_bar_, bottom_bar_,
                              status_label_, notification_label_, network_label_, mute_label_,
                              battery_label_, chat_message_label_};
        for (auto* object : chrome) {
            if (object != nullptr) {
                lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
            }
        }
        if (emoji_box_ != nullptr) {
            lv_obj_set_size(emoji_box_, width_, height_);
            lv_obj_set_style_bg_opa(emoji_box_, LV_OPA_TRANSP, 0);
            lv_obj_set_style_pad_all(emoji_box_, 0, 0);
            lv_obj_set_style_border_width(emoji_box_, 0, 0);
            lv_obj_center(emoji_box_);
        }
        if (emoji_label_ != nullptr) {
            lv_obj_center(emoji_label_);
        }
    }

    // The Otto GIF collection is selected for the otto-desk board in main/CMakeLists.txt.
    SetEmotion("staticstate");
}

void OttoDeskDisplay::SetEmotion(const char* emotion) {
    LcdDisplay::SetEmotion(emotion);
    FitEmotionToScreen();
}

void OttoDeskDisplay::FitEmotionToScreen() {
    DisplayLockGuard lock(this);
    if (emoji_image_ == nullptr) {
        return;
    }

    const auto* source = static_cast<const lv_image_dsc_t*>(lv_image_get_src(emoji_image_));
    if (source == nullptr || source->header.w == 0 || source->header.h == 0) {
        return;
    }

    // Cover the complete panel while preserving the GIF aspect ratio.
    const uint32_t scale_x = (256U * static_cast<uint32_t>(width_) + source->header.w - 1U) /
                             source->header.w;
    const uint32_t scale_y = (256U * static_cast<uint32_t>(height_) + source->header.h - 1U) /
                             source->header.h;
    const uint32_t scale = std::max(scale_x, scale_y);
    lv_image_set_scale(emoji_image_, static_cast<uint16_t>(std::min(scale, 65535U)));
    lv_obj_center(emoji_image_);
}

void OttoDeskDisplay::SetPreviewImage(std::unique_ptr<LvglImage> image) {
    DisplayLockGuard lock(this);
    if (preview_image_ == nullptr) {
        ESP_LOGE(TAG, "Preview image requested before UI initialization");
        return;
    }

    if (image == nullptr) {
        esp_timer_stop(preview_timer_);
        lv_obj_add_flag(preview_image_, LV_OBJ_FLAG_HIDDEN);
        preview_image_cached_.reset();
        if (emoji_box_ != nullptr) {
            lv_obj_remove_flag(emoji_box_, LV_OBJ_FLAG_HIDDEN);
        } else if (gif_controller_ != nullptr) {
            lv_obj_remove_flag(emoji_image_, LV_OBJ_FLAG_HIDDEN);
        } else if (emoji_label_ != nullptr) {
            lv_obj_remove_flag(emoji_label_, LV_OBJ_FLAG_HIDDEN);
        }
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
    if (emoji_box_ != nullptr) {
        lv_obj_add_flag(emoji_box_, LV_OBJ_FLAG_HIDDEN);
    } else {
        if (emoji_image_ != nullptr) {
            lv_obj_add_flag(emoji_image_, LV_OBJ_FLAG_HIDDEN);
        }
        if (emoji_label_ != nullptr) {
            lv_obj_add_flag(emoji_label_, LV_OBJ_FLAG_HIDDEN);
        }
    }
    lv_obj_remove_flag(preview_image_, LV_OBJ_FLAG_HIDDEN);
    esp_timer_stop(preview_timer_);
    ESP_ERROR_CHECK(esp_timer_start_once(preview_timer_, PREVIEW_IMAGE_DURATION_MS * 1000));
}
