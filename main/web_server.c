#include "web_server.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "bsp_camera.h"
#include "bsp_mic.h"
#include "bsp_sdcard.h"
#include "esp_system.h"
#include "esp_heap_caps.h"

static const char *TAG = "web_server";
static httpd_handle_t s_stream_httpd = NULL;

#define PART_BOUNDARY "123456789000000000000987654321"
static const char *STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

static const char INDEX_HTML[] = 
"<!DOCTYPE html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>XIAO ESP32-S3 Sense 监控控制台</title><style>"
"body{font-family:-apple-system,BlinkMacSystemFont,sans-serif;margin:0;padding:20px;background:#121212;color:#eee}"
".card{background:#1e1e1e;border-radius:12px;padding:16px;margin-bottom:16px;box-shadow:0 4px 12px rgba(0,0,0,0.5)}"
"h1{font-size:20px;margin-top:0}img{width:100%;max-width:800px;border-radius:8px;background:#000}"
".bar-box{background:#333;border-radius:6px;height:24px;overflow:hidden;position:relative;margin:8px 0}"
".bar{background:#00e676;height:100%;width:0%;transition:width 0.1s ease}"
".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:12px}"
".stat{background:#2a2a2a;padding:12px;border-radius:8px}.stat-v{font-size:18px;font-weight:bold;color:#29b6f6}"
"</style></head><body>"
"<h1>XIAO ESP32-S3 Sense 综合测试控制台</h1>"
"<div class='card'><img id='stream' src='/stream' alt='实时视频流'/></div>"
"<div class='card'><h3>麦克风实时音频电平</h3><div class='bar-box'><div id='mic-bar' class='bar'></div></div><span id='mic-val'>0%</span></div>"
"<div class='card'><h3>系统与硬件运行状态</h3><div class='grid'>"
"<div class='stat'><div>8MB PSRAM 剩余</div><div class='stat-v' id='psram'>-</div></div>"
"<div class='stat'><div>内部 SRAM 剩余</div><div class='stat-v' id='sram'>-</div></div>"
"<div class='stat'><div>MicroSD 卡状态</div><div class='stat-v' id='sd'>-</div></div>"
"<div class='stat'><div>开机时长</div><div class='stat-v' id='uptime'>-</div></div>"
"</div></div>"
"<script>"
"setInterval(()=>{fetch('/api/status').then(r=>r.json()).then(d=>{"
"document.getElementById('mic-bar').style.width=d.mic_level+'%';"
"document.getElementById('mic-val').innerText=d.mic_level+'%';"
"document.getElementById('psram').innerText=(d.free_psram/1024/1024).toFixed(2)+' MB';"
"document.getElementById('sram').innerText=(d.free_sram/1024).toFixed(1)+' KB';"
"document.getElementById('sd').innerText=d.sd_mounted?('已挂载 ('+(d.sd_total/1024/1024).toFixed(0)+' MB)'):'未插卡';"
"document.getElementById('uptime').innerText=d.uptime+' 秒';"
"});},500);"
"</script></body></html>";

static esp_err_t index_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t status_handler(httpd_req_t *req) {
    char json[256];
    uint64_t sd_tot = 0, sd_free = 0;
    bsp_sdcard_get_info(&sd_tot, &sd_free);
    snprintf(json, sizeof(json),
             "{\"uptime\":%lu,\"free_sram\":%lu,\"free_psram\":%lu,\"mic_level\":%u,\"sd_mounted\":%s,\"sd_total\":%llu}",
             (unsigned long)(esp_timer_get_time() / 1000000),
             (unsigned long)esp_get_free_internal_heap_size(),
             (unsigned long)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
             bsp_mic_get_level_percent(),
             bsp_sdcard_is_mounted() ? "true" : "false",
             (unsigned long long)sd_tot);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t stream_handler(httpd_req_t *req) {
    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;
    char part_buf[128];

    res = httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
    if (res != ESP_OK) return res;

    while (true) {
        fb = bsp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "Camera capture failed");
            res = ESP_FAIL;
            break;
        }
        res = httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));
        if (res == ESP_OK) {
            size_t hlen = snprintf(part_buf, sizeof(part_buf), STREAM_PART, (unsigned int)fb->len);
            res = httpd_resp_send_chunk(req, part_buf, hlen);
        }
        if (res == ESP_OK) {
            res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
        }
        bsp_camera_fb_return(fb);
        if (res != ESP_OK) break;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return res;
}

static void wifi_init_softap(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    esp_err_t event_ret = esp_event_loop_create_default();
    if (event_ret != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(event_ret);
    }
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = "XIAO_ESP32S3_Sense",
            .ssid_len = strlen("XIAO_ESP32S3_Sense"),
            .channel = 1,
            .password = "seeedstudio",
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Wi-Fi SoftAP started. SSID: XIAO_ESP32S3_Sense, Pass: seeedstudio, IP: 192.168.4.1");
}

esp_err_t web_server_start(void) {
    wifi_init_softap();

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.ctrl_port = 32768;

    if (httpd_start(&s_stream_httpd, &config) == ESP_OK) {
        httpd_uri_t index_uri = { .uri = "/", .method = HTTP_GET, .handler = index_handler };
        httpd_register_uri_handler(s_stream_httpd, &index_uri);

        httpd_uri_t status_uri = { .uri = "/api/status", .method = HTTP_GET, .handler = status_handler };
        httpd_register_uri_handler(s_stream_httpd, &status_uri);

        httpd_uri_t stream_uri = { .uri = "/stream", .method = HTTP_GET, .handler = stream_handler };
        httpd_register_uri_handler(s_stream_httpd, &stream_uri);

        ESP_LOGI(TAG, "Web server started on port 80");
        return ESP_OK;
    }
    return ESP_FAIL;
}
