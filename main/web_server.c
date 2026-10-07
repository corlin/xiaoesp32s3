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
#include "bsp_temp.h"
#include "esp_system.h"
#include "esp_heap_caps.h"

static const char *TAG = "web_server";
static httpd_handle_t s_camera_httpd = NULL;
static httpd_handle_t s_stream_httpd = NULL;
static volatile int s_active_streams = 0;

#define PART_BOUNDARY "123456789000000000000987654321"
static const char *STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

static const char INDEX_HTML[] = 
"<!DOCTYPE html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>XIAO ESP32-S3 Sense 综合控制台</title><style>"
"body{font-family:-apple-system,BlinkMacSystemFont,sans-serif;margin:0;padding:16px;background:#121212;color:#eee}"
".container{max-width:960px;margin:0 auto}"
".card{background:#1e1e1e;border-radius:12px;padding:16px;margin-bottom:16px;box-shadow:0 4px 12px rgba(0,0,0,0.4)}"
"h1{font-size:20px;margin:0 0 16px 0;display:flex;align-items:center;justify-content:space-between}"
"h3{font-size:15px;margin:0 0 10px 0;color:#bbb}"
".stream-box{position:relative;width:100%;max-width:800px;margin:0 auto;background:#000;border-radius:8px;overflow:hidden;min-height:240px;display:flex;align-items:center;justify-content:center}"
"img{width:100%;height:auto;display:block}"
".stream-ctrl{display:flex;justify-content:space-between;align-items:center;margin-top:10px;font-size:13px;color:#888}"
"button{background:#29b6f6;color:#121212;border:none;border-radius:6px;padding:6px 14px;font-weight:bold;cursor:pointer;transition:background 0.2s}"
"button:hover{background:#0288d1}button.paused{background:#757575;color:#fff}"
".bar-box{background:#333;border-radius:6px;height:20px;overflow:hidden;position:relative;margin:8px 0}"
".bar{background:#00e676;height:100%;width:0%;transition:width 0.1s ease}"
".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:12px}"
".stat{background:#2a2a2a;padding:12px;border-radius:8px}.stat-lbl{font-size:12px;color:#999;margin-bottom:4px}"
".stat-v{font-size:17px;font-weight:bold;color:#29b6f6}.badge{padding:2px 8px;border-radius:10px;font-size:11px;font-weight:bold;display:inline-block}"
".badge-green{background:#1b5e20;color:#69f0ae}.badge-orange{background:#e65100;color:#ffcc80}.badge-red{background:#b71c1c;color:#ff8a80}"
"</style></head><body><div class='container'>"
"<h1><span>XIAO ESP32-S3 Sense 监控终端</span><span id='status-badge' class='badge badge-green'>在线</span></h1>"
"<div class='card'>"
"<div class='stream-box'><img id='stream' src='' alt='视频流暂停中'/><div id='stream-placeholder' style='display:none;color:#777'>视频流已暂停 (节能模式)</div></div>"
"<div class='stream-ctrl'><span>当前推流: <strong id='stream-clients'>0</strong> 路</span>"
"<button id='btn-stream' onclick='toggleStream()'>暂停实时视频</button></div></div>"
"<div class='card'><h3>麦克风实时音频能量 (MSM261D PDM)</h3>"
"<div class='bar-box'><div id='mic-bar' class='bar'></div></div><div style='display:flex;justify-content:space-between;font-size:12px;color:#aaa'><span>0%</span><span id='mic-val'>0%</span><span>100%</span></div></div>"
"<div class='card'><h3>核心状态与环境感知</h3><div class='grid'>"
"<div class='stat'><div class='stat-lbl'>CPU 核心结温</div><div class='stat-v' id='temp'>--.- °C</div></div>"
"<div class='stat'><div class='stat-lbl'>Wi-Fi 接入终端</div><div class='stat-v' id='wifi-sta'>- 台</div></div>"
"<div class='stat'><div class='stat-lbl'>8MB PSRAM 剩余</div><div class='stat-v' id='psram'>- MB</div></div>"
"<div class='stat'><div class='stat-lbl'>内部 SRAM 剩余</div><div class='stat-v' id='sram'>- KB</div></div>"
"<div class='stat'><div class='stat-lbl'>MicroSD 卡</div><div class='stat-v' id='sd'>-</div></div>"
"<div class='stat'><div class='stat-lbl'>系统运行时间</div><div class='stat-v' id='uptime'>- 秒</div></div>"
"</div></div></div>"
"<script>"
"const streamImg = document.getElementById('stream');"
"const placeholder = document.getElementById('stream-placeholder');"
"const btnStream = document.getElementById('btn-stream');"
"let streamActive = true;"
"function getStreamUrl(){ return location.protocol + '//' + location.hostname + ':81/stream'; }"
"function startStream(){ streamImg.src = getStreamUrl(); streamImg.style.display='block'; placeholder.style.display='none'; btnStream.innerText='暂停实时视频'; btnStream.className=''; streamActive=true; }"
"function stopStream(){ streamImg.src = ''; streamImg.style.display='none'; placeholder.style.display='block'; btnStream.innerText='开启实时视频'; btnStream.className='paused'; streamActive=false; }"
"function toggleStream(){ if(streamActive){ stopStream(); } else { startStream(); } }"
"startStream();"
"document.addEventListener('visibilitychange', ()=>{ if(document.hidden && streamActive){ stopStream(); } else if(!document.hidden && !streamActive){ startStream(); } });"
"setInterval(()=>{ fetch('/api/status').then(r=>r.json()).then(d=>{"
"document.getElementById('mic-bar').style.width = d.mic_level + '%';"
"document.getElementById('mic-val').innerText = d.mic_level + '%';"
"const tempElem = document.getElementById('temp');"
"tempElem.innerText = d.temp.toFixed(1) + ' °C';"
"tempElem.style.color = d.temp > 70 ? '#ff5252' : (d.temp > 55 ? '#ffab40' : '#69f0ae');"
"document.getElementById('wifi-sta').innerText = d.wifi_clients + ' 台';"
"document.getElementById('stream-clients').innerText = d.streams;"
"document.getElementById('psram').innerText = (d.free_psram/1024/1024).toFixed(2) + ' MB';"
"document.getElementById('sram').innerText = (d.free_sram/1024).toFixed(1) + ' KB';"
"document.getElementById('sd').innerText = d.sd_mounted ? ('已挂载 ('+(d.sd_total/1024/1024).toFixed(0)+'M)') : '未插卡';"
"document.getElementById('uptime').innerText = d.uptime + ' 秒';"
"}).catch(e=>{ document.getElementById('status-badge').className='badge badge-red'; document.getElementById('status-badge').innerText='离线'; }); }, 500);"
"</script></body></html>";

static esp_err_t index_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t status_handler(httpd_req_t *req) {
    char json[512];
    uint64_t sd_tot = 0, sd_free = 0;
    bsp_sdcard_get_info(&sd_tot, &sd_free);

    wifi_sta_list_t sta_list;
    uint16_t sta_num = 0;
    if (esp_wifi_ap_get_sta_list(&sta_list) == ESP_OK) {
        sta_num = sta_list.num;
    }

    float temp_c = bsp_temp_get_celsius();

    snprintf(json, sizeof(json),
             "{\"uptime\":%lu,\"free_sram\":%lu,\"free_psram\":%lu,\"mic_level\":%u,"
             "\"sd_mounted\":%s,\"sd_total\":%llu,\"temp\":%.1f,\"wifi_clients\":%u,\"streams\":%d}",
             (unsigned long)(esp_timer_get_time() / 1000000),
             (unsigned long)esp_get_free_internal_heap_size(),
             (unsigned long)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
             bsp_mic_get_level_percent(),
             bsp_sdcard_is_mounted() ? "true" : "false",
             (unsigned long long)sd_tot,
             temp_c,
             (unsigned int)sta_num,
             s_active_streams);

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t stream_handler(httpd_req_t *req) {
    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;
    char part_buf[128];

    res = httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
    if (res != ESP_OK) return res;

    s_active_streams++;
    ESP_LOGI(TAG, "Stream client connected. Active streams: %d", s_active_streams);

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
        // 适当延时，为网络堆栈与其它后台任务让渡调度周期
        vTaskDelay(pdMS_TO_TICKS(15));
    }

    s_active_streams--;
    ESP_LOGI(TAG, "Stream client disconnected. Active streams: %d", s_active_streams);
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

    // 1. Control & API HTTP Server on Port 80
    httpd_config_t config_camera = HTTPD_DEFAULT_CONFIG();
    config_camera.server_port = 80;
    config_camera.ctrl_port = 32768;

    if (httpd_start(&s_camera_httpd, &config_camera) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start control HTTP server on port 80");
        return ESP_FAIL;
    }

    httpd_uri_t index_uri = {
        .uri      = "/",
        .method   = HTTP_GET,
        .handler  = index_handler,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(s_camera_httpd, &index_uri);

    httpd_uri_t status_uri = {
        .uri      = "/api/status",
        .method   = HTTP_GET,
        .handler  = status_handler,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(s_camera_httpd, &status_uri);

    // 2. Dedicated MJPEG Video Stream HTTP Server on Port 81
    httpd_config_t config_stream = HTTPD_DEFAULT_CONFIG();
    config_stream.server_port = 81;
    config_stream.ctrl_port = 32769;

    if (httpd_start(&s_stream_httpd, &config_stream) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start stream HTTP server on port 81");
        httpd_stop(s_camera_httpd);
        s_camera_httpd = NULL;
        return ESP_FAIL;
    }

    httpd_uri_t stream_uri = {
        .uri      = "/stream",
        .method   = HTTP_GET,
        .handler  = stream_handler,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(s_stream_httpd, &stream_uri);

    ESP_LOGI(TAG, "Web servers started: Control/API on port 80, Stream on port 81");
    return ESP_OK;
}
