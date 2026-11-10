#include "wifi_board.h"
#include "audio/audio_codec.h"
#include "audio/codecs/dummy_audio_codec.h"
#include "audio/codecs/otto_i2s_audio_codec.h"
#include "otto_desk_display.h"
#include "otto_desk_config_server.h"
#include "otto_desk_config_store.h"

#include <driver/spi_master.h>
#include <esp_err.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>

#include <memory>
#include <string>

namespace {
constexpr char kTag[] = "OttoDeskBoard";
constexpr spi_host_device_t kDisplaySpiHost = SPI3_HOST;
constexpr int kDisplaySpiMode = 3;
constexpr uint32_t kDisplayPixelClockHz = 40 * 1000 * 1000;
constexpr int kAudioInputSampleRate = 16000;
constexpr int kAudioOutputSampleRate = 24000;
}

class OttoDeskBoard : public WifiBoard {
private:
    std::unique_ptr<AudioCodec> audio_codec_;
    std::unique_ptr<OttoDeskDisplay> display_;
    std::unique_ptr<PwmBacklight> backlight_;
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;

    bool LoadRuntimeConfig(OttoDeskRuntimeConfig& config) {
        std::string json;
        const esp_err_t load_err = OttoDeskConfigStore::Load(json);
        if (load_err != ESP_OK) {
            ESP_LOGI(kTag, "No saved Otto hardware config (%s); peripherals stay inactive",
                     esp_err_to_name(load_err));
            return false;
        }
        const esp_err_t decode_err = OttoDeskConfigStore::Decode(json, config);
        if (decode_err != ESP_OK) {
            ESP_LOGE(kTag, "Saved Otto hardware config rejected (%s)", esp_err_to_name(decode_err));
            return false;
        }
        return true;
    }

    void InitializeConfiguredDisplay(const OttoDeskRuntimeConfig& config) {
        if (!config.hardware_activation_permitted || !config.display_enabled ||
            !config.display_sclk || !config.display_mosi || !config.display_dc ||
            !config.display_reset || !config.display_backlight) {
            return;
        }

        spi_bus_config_t bus_config = {};
        bus_config.mosi_io_num = static_cast<gpio_num_t>(*config.display_mosi);
        bus_config.miso_io_num = GPIO_NUM_NC;
        bus_config.sclk_io_num = static_cast<gpio_num_t>(*config.display_sclk);
        bus_config.quadwp_io_num = GPIO_NUM_NC;
        bus_config.quadhd_io_num = GPIO_NUM_NC;
        bus_config.max_transfer_sz =
            config.display_width * config.display_height * static_cast<int>(sizeof(uint16_t));

        esp_err_t err = spi_bus_initialize(kDisplaySpiHost, &bus_config, SPI_DMA_CH_AUTO);
        if (err != ESP_OK) {
            ESP_LOGE(kTag, "ST7789 SPI bus initialization failed: %s", esp_err_to_name(err));
            return;
        }

        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = config.display_cs
            ? static_cast<gpio_num_t>(*config.display_cs) : GPIO_NUM_NC;
        io_config.dc_gpio_num = static_cast<gpio_num_t>(*config.display_dc);
        io_config.spi_mode = kDisplaySpiMode;
        io_config.pclk_hz = kDisplayPixelClockHz;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;

        err = esp_lcd_new_panel_io_spi(kDisplaySpiHost, &io_config, &panel_io_);
        if (err != ESP_OK) {
            ESP_LOGE(kTag, "ST7789 panel IO creation failed: %s", esp_err_to_name(err));
            spi_bus_free(kDisplaySpiHost);
            return;
        }

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = static_cast<gpio_num_t>(*config.display_reset);
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;

        err = esp_lcd_new_panel_st7789(panel_io_, &panel_config, &panel_);
        if (err == ESP_OK) err = esp_lcd_panel_reset(panel_);
        if (err == ESP_OK) err = esp_lcd_panel_init(panel_);
        if (err == ESP_OK) err = esp_lcd_panel_invert_color(panel_, true);
        if (err != ESP_OK) {
            ESP_LOGE(kTag, "ST7789 panel initialization failed: %s", esp_err_to_name(err));
            if (panel_ != nullptr) {
                esp_lcd_panel_del(panel_);
                panel_ = nullptr;
            }
            esp_lcd_panel_io_del(panel_io_);
            panel_io_ = nullptr;
            spi_bus_free(kDisplaySpiHost);
            return;
        }

        display_ = std::make_unique<OttoDeskDisplay>(
            panel_io_, panel_, config.display_width, config.display_height);
        backlight_ = std::make_unique<PwmBacklight>(
            static_cast<gpio_num_t>(*config.display_backlight));
        backlight_->RestoreBrightness();
        ESP_LOGI(kTag, "Otto ST7789 display initialized from validated runtime config");
    }

    void InitializeConfiguredAudio(const OttoDeskRuntimeConfig& config) {
        if (!config.hardware_activation_permitted ||
            !config.microphone_enabled || !config.amplifier_enabled ||
            !config.microphone_ws || !config.microphone_sck || !config.microphone_sd ||
            !config.amplifier_bclk || !config.amplifier_lrclk || !config.amplifier_din) {
            return;
        }

        auto codec = std::make_unique<OttoI2sAudioCodec>(
            kAudioInputSampleRate, kAudioOutputSampleRate,
            static_cast<gpio_num_t>(*config.microphone_ws),
            static_cast<gpio_num_t>(*config.microphone_sck),
            static_cast<gpio_num_t>(*config.microphone_sd),
            static_cast<gpio_num_t>(*config.amplifier_bclk),
            static_cast<gpio_num_t>(*config.amplifier_lrclk),
            static_cast<gpio_num_t>(*config.amplifier_din),
            true);
        if (!codec->IsReady()) {
            ESP_LOGE(kTag, "Configured I2S codec did not become ready; using dummy codec");
            return;
        }
        audio_codec_ = std::move(codec);
        ESP_LOGI(kTag, "INMP441 + MAX98357A initialized from validated runtime config");
    }

public:
    OttoDeskBoard() {
        OttoDeskRuntimeConfig config;
        if (!LoadRuntimeConfig(config)) return;

        // Decode currently hard-locks this permission to false. Draft GPIOs
        // alone cannot open the carrier-routing gate or start physical drivers.
        if (!config.hardware_activation_permitted) {
            ESP_LOGI(kTag, "Peripheral activation interlock is closed; no GPIO drivers started");
            return;
        }
        InitializeConfiguredDisplay(config);
        InitializeConfiguredAudio(config);
    }

    AudioCodec* GetAudioCodec() override {
        if (audio_codec_) return audio_codec_.get();
        static DummyAudioCodec fallback_codec(16000, 24000);
        return &fallback_codec;
    }

    Display* GetDisplay() override {
        return display_ ? display_.get() : Board::GetDisplay();
    }

    Backlight* GetBacklight() override {
        return backlight_.get();
    }

protected:
    void OnNetworkEvent(NetworkEvent event, const std::string& data = "") override {
        WifiBoard::OnNetworkEvent(event, data);
        if (event == NetworkEvent::WifiConfigModeEnter) {
            const esp_err_t err = OttoDeskConfigServer::Start();
            if (err != ESP_OK) {
                ESP_LOGE(kTag, "Config API unavailable: %s", esp_err_to_name(err));
            }
        } else if (event == NetworkEvent::Connected ||
                   event == NetworkEvent::WifiConfigModeExit) {
            OttoDeskConfigServer::Stop();
        }
    }
};

DECLARE_BOARD(OttoDeskBoard);
