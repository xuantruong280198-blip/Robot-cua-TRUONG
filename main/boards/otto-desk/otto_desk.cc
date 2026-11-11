#include "wifi_board.h"
#include "application.h"
#include "audio/audio_codec.h"
#include "audio/codecs/dummy_audio_codec.h"
#include "audio/codecs/otto_i2s_audio_codec.h"
#include "otto_desk_display.h"
#include "otto_desk_config_server.h"
#include "otto_desk_config_store.h"
#include "otto_desk_servo.h"
#include "otto_desk_pir.h"
#include "otto_desk_head_motion.h"
#include "otto_desk_display_rotation.h"

#include <driver/spi_master.h>
#include <driver/gpio.h>
#include <esp_err.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>

#include <memory>
#include <string>
#include <mutex>
#include <cmath>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

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
    std::unique_ptr<OttoDeskServoAxis> pan_servo_;
    std::unique_ptr<OttoDeskServoAxis> tilt_servo_;
    std::unique_ptr<OttoDeskPirSensor> pir_sensor_;
    std::unique_ptr<otto_desk::HeadMotionSequence> head_motion_;
    std::mutex head_motion_mutex_;
    std::string head_emotion_ = "neutral";
    bool speaking_nod_phase_ = false;
    uint32_t speaking_nod_elapsed_ms_ = 0;
    TaskHandle_t head_motion_task_ = nullptr;
    TaskHandle_t pir_task_ = nullptr;
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

        const auto rotation = otto_desk::GetDisplayRotation(config.display_rotation.value_or(0));
        display_ = std::make_unique<OttoDeskDisplay>(
            panel_io_, panel_, config.display_width, config.display_height,
            0, 0, rotation.mirror_x, rotation.mirror_y, rotation.swap_xy,
            [this](const char* emotion) { SetHeadEmotion(emotion); });
        backlight_ = std::make_unique<PwmBacklight>(
            static_cast<gpio_num_t>(*config.display_backlight));
        backlight_->RestoreBrightness();
        ESP_LOGI(kTag, "Otto ST7789 display initialized from validated runtime config");
    }

    void InitializeConfiguredServos(const OttoDeskRuntimeConfig& config) {
        if (!config.hardware_activation_permitted || !config.servo_external_power_verified) {
            return;
        }

        if (config.pan.enabled && config.pan.gpio) {
            pan_servo_ = std::make_unique<OttoDeskServoAxis>(
                static_cast<gpio_num_t>(*config.pan.gpio), LEDC_CHANNEL_2,
                config.hardware_activation_permitted, config.servo_external_power_verified,
                config.pan.min_deg, config.pan.max_deg);
            esp_err_t err = pan_servo_->Initialize();
            if (err == ESP_OK) err = pan_servo_->SetAngle(config.pan.center_deg);
            if (err != ESP_OK) {
                ESP_LOGE(kTag, "PAN servo initialization failed: %s", esp_err_to_name(err));
                pan_servo_.reset();
            }
        }

        if (config.tilt.enabled && config.tilt.gpio) {
            tilt_servo_ = std::make_unique<OttoDeskServoAxis>(
                static_cast<gpio_num_t>(*config.tilt.gpio), LEDC_CHANNEL_3,
                config.hardware_activation_permitted, config.servo_external_power_verified,
                config.tilt.min_deg, config.tilt.max_deg);
            esp_err_t err = tilt_servo_->Initialize();
            if (err == ESP_OK) err = tilt_servo_->SetAngle(config.tilt.center_deg);
            if (err != ESP_OK) {
                ESP_LOGE(kTag, "TILT servo initialization failed: %s", esp_err_to_name(err));
                tilt_servo_.reset();
            }
        }
        StartHeadMotion(config);
    }

    void SetHeadEmotion(const char* emotion) {
        std::lock_guard<std::mutex> lock(head_motion_mutex_);
        head_emotion_ = emotion != nullptr ? emotion : "neutral";
        speaking_nod_phase_ = false;
        speaking_nod_elapsed_ms_ = 0;
        if (head_motion_ != nullptr) head_motion_->SetEmotion(head_emotion_.c_str());
    }

    void AdvanceHeadMotion(uint32_t elapsed_ms) {
        otto_desk::HeadMotionFrame frame;
        {
            std::lock_guard<std::mutex> lock(head_motion_mutex_);
            if (head_motion_ == nullptr) return;
            if (head_emotion_ == "speaking") {
                speaking_nod_elapsed_ms_ += elapsed_ms;
                if (speaking_nod_elapsed_ms_ >= 700) {
                    speaking_nod_elapsed_ms_ = 0;
                    speaking_nod_phase_ = !speaking_nod_phase_;
                    head_motion_->SetEmotion(
                        otto_desk::SpeakingNodEmotion(speaking_nod_phase_));
                }
            }
            frame = head_motion_->Advance(elapsed_ms);
        }
        if (pan_servo_ != nullptr && pan_servo_->IsReady()) {
            pan_servo_->SetAngle(static_cast<int>(std::lround(frame.pan_deg)));
        }
        if (tilt_servo_ != nullptr && tilt_servo_->IsReady()) {
            tilt_servo_->SetAngle(static_cast<int>(std::lround(frame.tilt_deg)));
        }
    }

    static void HeadMotionTask(void* context) {
        auto* self = static_cast<OttoDeskBoard*>(context);
        constexpr uint32_t kFrameMs = 20;
        while (true) {
            self->AdvanceHeadMotion(kFrameMs);
            vTaskDelay(pdMS_TO_TICKS(kFrameMs));
        }
    }

    void StartHeadMotion(const OttoDeskRuntimeConfig& config) {
        if ((pan_servo_ == nullptr || !pan_servo_->IsReady()) &&
            (tilt_servo_ == nullptr || !tilt_servo_->IsReady())) return;

        const otto_desk::HeadAxisLimits pan_limits = config.pan.enabled
            ? otto_desk::HeadAxisLimits{config.pan.min_deg, config.pan.center_deg,
                                        config.pan.max_deg, config.pan.speed_deg_per_sec}
            : otto_desk::HeadAxisLimits{0, 90, 180, 0.0F};
        const otto_desk::HeadAxisLimits tilt_limits = config.tilt.enabled
            ? otto_desk::HeadAxisLimits{config.tilt.min_deg, config.tilt.center_deg,
                                        config.tilt.max_deg, config.tilt.speed_deg_per_sec}
            : otto_desk::HeadAxisLimits{0, 90, 180, 0.0F};
        head_motion_ = std::make_unique<otto_desk::HeadMotionSequence>(
            pan_limits, tilt_limits);
        if (!head_motion_->IsValid()) {
            head_motion_.reset();
            ESP_LOGE(kTag, "Head motion limits rejected");
            return;
        }
        const BaseType_t result = xTaskCreate(
            HeadMotionTask, "otto_head", 4096, this, 4, &head_motion_task_);
        if (result != pdPASS) {
            head_motion_task_ = nullptr;
            head_motion_.reset();
            ESP_LOGE(kTag, "Could not start head motion task");
        }
    }

    static void PirMonitorTask(void* context) {
        auto* self = static_cast<OttoDeskBoard*>(context);
        // HC-SR501 needs a stabilization interval after power-up.
        vTaskDelay(pdMS_TO_TICKS(30000));
        bool previous_motion = self->pir_sensor_ != nullptr &&
                               self->pir_sensor_->IsMotionDetected();
        TickType_t last_trigger = 0;
        constexpr TickType_t kTriggerCooldown = pdMS_TO_TICKS(15000);
        while (true) {
            const bool motion = self->pir_sensor_ != nullptr &&
                                self->pir_sensor_->IsMotionDetected();
            if (motion && !previous_motion) {
                const TickType_t now = xTaskGetTickCount();
                const auto state = Application::GetInstance().GetDeviceState();
                if (state == kDeviceStateIdle &&
                    (last_trigger == 0 || now - last_trigger >= kTriggerCooldown)) {
                    ESP_LOGI(kTag, "PIR motion detected; starting listening");
                    Application::GetInstance().StartListening();
                    last_trigger = now;
                }
            }
            previous_motion = motion;
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    void InitializeConfiguredPir(const OttoDeskRuntimeConfig& config) {
        if (!config.hardware_activation_permitted || !config.pir_enabled || !config.pir_gpio) {
            return;
        }
        pir_sensor_ = std::make_unique<OttoDeskPirSensor>(
            static_cast<gpio_num_t>(*config.pir_gpio), config.hardware_activation_permitted);
        const esp_err_t err = pir_sensor_->Initialize();
        if (err != ESP_OK) {
            ESP_LOGE(kTag, "PIR initialization failed: %s", esp_err_to_name(err));
            pir_sensor_.reset();
            return;
        }
        ESP_LOGI(kTag, "HC-SR501 motion input initialized");
        const BaseType_t result = xTaskCreate(
            PirMonitorTask, "otto_pir", 3072, this, 3, &pir_task_);
        if (result != pdPASS) {
            pir_task_ = nullptr;
            pir_sensor_.reset();
            ESP_LOGE(kTag, "Could not start PIR monitor task");
        }
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
    // GPIO40 is reserved as a temporary, active-low Wi-Fi provisioning trigger.
    // Hold GPIO40 to GND while powering on; GPIO40 is not an ESP32-S3 strapping pin.
    void StartNetwork() override {
        constexpr gpio_num_t kProvisioningTriggerGpio = GPIO_NUM_40;
        gpio_config_t trigger_config = {};
        trigger_config.pin_bit_mask = 1ULL << kProvisioningTriggerGpio;
        trigger_config.mode = GPIO_MODE_INPUT;
        trigger_config.pull_up_en = GPIO_PULLUP_ENABLE;
        trigger_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
        trigger_config.intr_type = GPIO_INTR_DISABLE;

        bool force_config_mode = false;
        if (gpio_config(&trigger_config) == ESP_OK) {
            // Require a stable low level for 300 ms to reject noise.
            force_config_mode = true;
            for (int sample = 0; sample < 30; ++sample) {
                if (gpio_get_level(kProvisioningTriggerGpio) != 0) {
                    force_config_mode = false;
                    break;
                }
                vTaskDelay(pdMS_TO_TICKS(10));
            }
        }

        WifiBoard::StartNetwork();
        if (force_config_mode) {
            ESP_LOGW(kTag, "GPIO40 held low at startup; forcing Wi-Fi provisioning AP");
            EnterWifiConfigMode();
        }
    }

    OttoDeskBoard() {
        OttoDeskRuntimeConfig config;
        if (!LoadRuntimeConfig(config)) return;

        // JSON remains inactive by default. The operator must explicitly set
        // board.peripheral_routing_verified=true after wiring/pinout review.
        if (!config.hardware_activation_permitted) {
            ESP_LOGI(kTag, "Peripheral activation interlock is closed; no GPIO drivers started");
            return;
        }
        InitializeConfiguredDisplay(config);
        InitializeConfiguredAudio(config);
        InitializeConfiguredServos(config);
        InitializeConfiguredPir(config);
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
