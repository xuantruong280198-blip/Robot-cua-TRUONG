#pragma once

#include "esp_err.h"

// Temporary configuration API available only while the board is in its
// Wi-Fi provisioning AP mode. It accepts schema-validated JSON; peripherals
// remain disabled by default and require explicit routing acknowledgement.
class OttoDeskConfigServer {
public:
    static esp_err_t Start();
    static void Stop();
};
