#include "wifi_board.h"
#include "audio/codecs/dummy_audio_codec.h"

// Safe bring-up scaffold only. Hardware drivers are intentionally not started
// until the N16R8 header pin map and runtime GPIO configuration are verified.
class OttoDeskBoard : public WifiBoard {
public:
    AudioCodec* GetAudioCodec() override {
        // Keep the application audio object non-null without touching any GPIO.
        static DummyAudioCodec audio_codec(16000, 24000);
        return &audio_codec;
    }
};

DECLARE_BOARD(OttoDeskBoard);
