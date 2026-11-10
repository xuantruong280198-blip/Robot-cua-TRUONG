#include "wifi_board.h"
#include "audio/codecs/dummy_audio_codec.h"
#include "otto_desk_config_server.h"

#include <esp_err.h>
#include <esp_log.h>

// Safe bring-up scaffold only. Hardware drivers are intentionally not started
// until peripheral signal-to-GPIO wiring and runtime configuration are audited.
class OttoDeskBoard : public WifiBoard {
public:
    AudioCodec* GetAudioCodec() override {
        // Keep the application audio object non-null without touching any GPIO.
        static DummyAudioCodec audio_codec(16000, 24000);
        return &audio_codec;
    }

protected:
    void OnNetworkEvent(NetworkEvent event, const std::string& data = "") override {
        WifiBoard::OnNetworkEvent(event, data);
        if (event == NetworkEvent::WifiConfigModeEnter) {
            // The provisioning portal uses port 80; Otto's bounded JSON editor
            // runs separately on 8080 and only while the setup AP is active.
            const esp_err_t err = OttoDeskConfigServer::Start();
            if (err != ESP_OK) {
                ESP_LOGE("OttoDeskBoard", "Config API unavailable: %s", esp_err_to_name(err));
            }
        } else if (event == NetworkEvent::Connected ||
                   event == NetworkEvent::WifiConfigModeExit) {
            OttoDeskConfigServer::Stop();
        }
    }
};

DECLARE_BOARD(OttoDeskBoard);
