#include "otto_i2s_audio_codec.h"
#include "otto_i2s_audio_math.h"
#include "boards/otto-desk/otto_desk_gpio_safety.h"

#include <driver/i2s_std.h>
#include <esp_log.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>

namespace {
constexpr char kTag[] = "OttoI2SAudioCodec";
constexpr int kMicI2sPort = 0;
constexpr int kAmpI2sPort = 1;
}

OttoI2sAudioCodec::OttoI2sAudioCodec(
    int input_sample_rate, int output_sample_rate,
    gpio_num_t mic_ws, gpio_num_t mic_sck, gpio_num_t mic_sd,
    gpio_num_t amp_bclk, gpio_num_t amp_lrclk, gpio_num_t amp_din,
    bool board_pinout_validated) {
    if (!board_pinout_validated) {
        ESP_LOGE(kTag, "Refusing I2S init: carrier-board pinout is not verified");
        return;
    }
    // Fail closed before allocating I2S resources. This is an extra driver
    // guard; it does not replace verification of the carrier-board pin map.
    const gpio_num_t pins[] = {mic_ws, mic_sck, mic_sd, amp_bclk, amp_lrclk, amp_din};
    for (size_t i = 0; i < sizeof(pins) / sizeof(pins[0]); ++i) {
        if (!otto_desk::IsSafeEsp32S3Gpio(static_cast<int>(pins[i]))) {
            ESP_LOGE(kTag, "Refusing I2S init: unsafe GPIO %d", static_cast<int>(pins[i]));
            return;
        }
        for (size_t j = 0; j < i; ++j) {
            if (pins[i] == pins[j]) {
                ESP_LOGE(kTag, "Refusing I2S init: GPIO %d is duplicated", static_cast<int>(pins[i]));
                return;
            }
        }
    }
    if (input_sample_rate <= 0 || output_sample_rate <= 0) {
        ESP_LOGE(kTag, "Refusing I2S init: sample rates must be positive");
        return;
    }

    input_sample_rate_ = input_sample_rate;
    output_sample_rate_ = output_sample_rate;
    input_channels_ = 1;
    output_channels_ = 1;
    duplex_ = true;
    input_reference_ = false;

    i2s_chan_config_t rx_channel_config =
        I2S_CHANNEL_DEFAULT_CONFIG(XIAOZHI_I2S_PORT(kMicI2sPort), I2S_ROLE_MASTER);
    rx_channel_config.dma_desc_num = AUDIO_CODEC_DMA_DESC_NUM;
    rx_channel_config.dma_frame_num = AUDIO_CODEC_DMA_FRAME_NUM;
    rx_channel_config.auto_clear = true;

    i2s_chan_config_t tx_channel_config =
        I2S_CHANNEL_DEFAULT_CONFIG(XIAOZHI_I2S_PORT(kAmpI2sPort), I2S_ROLE_MASTER);
    tx_channel_config.dma_desc_num = AUDIO_CODEC_DMA_DESC_NUM;
    tx_channel_config.dma_frame_num = AUDIO_CODEC_DMA_FRAME_NUM;
    tx_channel_config.auto_clear = true;

    ESP_ERROR_CHECK(i2s_new_channel(&rx_channel_config, nullptr, &rx_handle_));
    ESP_ERROR_CHECK(i2s_new_channel(&tx_channel_config, &tx_handle_, nullptr));

    // INMP441 emits 24-bit samples in a 32-bit I2S slot. Convert to int16
    // in Read(); the amplifier receives the framework's 16-bit PCM stream.
    i2s_std_config_t mic_config = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(static_cast<uint32_t>(input_sample_rate_)),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = mic_sck,
            .ws = mic_ws,
            .dout = I2S_GPIO_UNUSED,
            .din = mic_sd,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle_, &mic_config));

    i2s_std_config_t amp_config = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(static_cast<uint32_t>(output_sample_rate_)),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = amp_bclk,
            .ws = amp_lrclk,
            .dout = amp_din,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle_, &amp_config));

    ESP_LOGI(kTag, "I2S input/output channels initialized (mic=%dHz, amp=%dHz)",
             input_sample_rate_, output_sample_rate_);
}

OttoI2sAudioCodec::~OttoI2sAudioCodec() {
    if (input_enabled_) EnableInput(false);
    if (output_enabled_) EnableOutput(false);
    if (rx_handle_ != nullptr) {
        i2s_del_channel(rx_handle_);
        rx_handle_ = nullptr;
    }
    if (tx_handle_ != nullptr) {
        i2s_del_channel(tx_handle_);
        tx_handle_ = nullptr;
    }
}

void OttoI2sAudioCodec::EnableInput(bool enable) {
    if (enable == input_enabled_) return;
    if (rx_handle_ == nullptr) {
        ESP_LOGE(kTag, "Cannot enable microphone: I2S RX is not initialized");
        return;
    }
    if (enable) {
        ESP_ERROR_CHECK(i2s_channel_enable(rx_handle_));
        input_channel_ready_ = true;
    } else if (input_channel_ready_) {
        ESP_ERROR_CHECK(i2s_channel_disable(rx_handle_));
        input_channel_ready_ = false;
    }
    AudioCodec::EnableInput(enable);
}

void OttoI2sAudioCodec::EnableOutput(bool enable) {
    if (enable == output_enabled_) return;
    if (tx_handle_ == nullptr) {
        ESP_LOGE(kTag, "Cannot enable amplifier: I2S TX is not initialized");
        return;
    }
    if (enable) {
        ESP_ERROR_CHECK(i2s_channel_enable(tx_handle_));
        output_channel_ready_ = true;
    } else if (output_channel_ready_) {
        ESP_ERROR_CHECK(i2s_channel_disable(tx_handle_));
        output_channel_ready_ = false;
    }
    AudioCodec::EnableOutput(enable);
}

void OttoI2sAudioCodec::SetOutputVolume(int volume) {
    AudioCodec::SetOutputVolume(std::clamp(volume, 0, 100));
}

int OttoI2sAudioCodec::Read(int16_t* dest, int samples) {
    if (!input_enabled_ || rx_handle_ == nullptr || dest == nullptr || samples <= 0) return 0;

    std::array<int32_t, AUDIO_CODEC_DMA_FRAME_NUM> raw{};
    int remaining = samples;
    int written = 0;
    while (remaining > 0) {
        const int count = std::min(remaining, static_cast<int>(raw.size()));
        size_t bytes_read = 0;
        const esp_err_t err = i2s_channel_read(
            rx_handle_, raw.data(), static_cast<size_t>(count) * sizeof(int32_t),
            &bytes_read, portMAX_DELAY);
        if (err != ESP_OK || bytes_read == 0 || bytes_read % sizeof(int32_t) != 0) {
            ESP_LOGE(kTag, "I2S microphone read failed or returned a partial sample: %s",
                     esp_err_to_name(err));
            return written;
        }
        const int frames = static_cast<int>(bytes_read / sizeof(int32_t));
        if (frames <= 0 || frames > count) {
            ESP_LOGE(kTag, "I2S microphone returned an invalid frame count: %d", frames);
            return written;
        }
        for (int i = 0; i < frames; ++i) {
            // INMP441's signed 24-bit payload is left-aligned in the 32-bit slot.
            dest[written + i] = otto_desk::Inmp441SlotToPcm16(raw[i]);
        }
        written += frames;
        remaining -= frames;
    }
    return written;
}

int OttoI2sAudioCodec::Write(const int16_t* data, int samples) {
    if (!output_enabled_ || tx_handle_ == nullptr || data == nullptr || samples <= 0) return 0;

    std::array<int16_t, AUDIO_CODEC_DMA_FRAME_NUM> scaled{};
    int written = 0;
    while (written < samples) {
        const int count = std::min(
            samples - written, static_cast<int>(scaled.size()));
        for (int i = 0; i < count; ++i) {
            scaled[i] = otto_desk::ScalePcm16(data[written + i], output_volume_);
        }
        size_t bytes_written = 0;
        const esp_err_t err = i2s_channel_write(
            tx_handle_, scaled.data(), static_cast<size_t>(count) * sizeof(int16_t),
            &bytes_written, portMAX_DELAY);
        if (err != ESP_OK || bytes_written == 0 || bytes_written % sizeof(int16_t) != 0) {
            ESP_LOGE(kTag, "I2S amplifier write failed or returned a partial sample: %s",
                     esp_err_to_name(err));
            return written;
        }
        const int frames = static_cast<int>(bytes_written / sizeof(int16_t));
        if (frames <= 0 || frames > count) {
            ESP_LOGE(kTag, "I2S amplifier returned an invalid frame count: %d", frames);
            return written;
        }
        written += frames;
    }
    return written;
}
