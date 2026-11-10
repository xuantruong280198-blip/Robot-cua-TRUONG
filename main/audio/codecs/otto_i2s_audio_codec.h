#pragma once

#include "audio_codec.h"

#include <driver/gpio.h>

// Independent I2S RX/TX codec for INMP441 + MAX98357A.
// The caller must supply a validated runtime pin assignment.
class OttoI2sAudioCodec final : public AudioCodec {
public:
    OttoI2sAudioCodec(int input_sample_rate, int output_sample_rate,
                      gpio_num_t mic_ws, gpio_num_t mic_sck, gpio_num_t mic_sd,
                      gpio_num_t amp_bclk, gpio_num_t amp_lrclk, gpio_num_t amp_din,
                      bool board_pinout_validated = false);
    ~OttoI2sAudioCodec() override;

    void EnableInput(bool enable) override;
    void EnableOutput(bool enable) override;
    void SetOutputVolume(int volume) override;
    bool IsReady() const { return rx_handle_ != nullptr && tx_handle_ != nullptr; }

private:
    int Read(int16_t* dest, int samples) override;
    int Write(const int16_t* data, int samples) override;

    bool input_channel_ready_ = false;
    bool output_channel_ready_ = false;
};
