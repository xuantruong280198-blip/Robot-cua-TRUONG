#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

// Register Custom GPIO and JSON routes on XiaoZhi's existing port-80 server.
class OttoDeskConfigServer {
public:
    static esp_err_t RegisterRoutes(httpd_handle_t server);
};
