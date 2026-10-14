#include "ota/wifi_ota_manager.h"

#include "bluetooth/ble_transport.h"
#include "esp_app_format.h"
#include "esp_check.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace waze_hud {
namespace {

constexpr char kTag[] = "WIFI_OTA";

const char kIndexHtml[] = R"rawliteral(<!DOCTYPE html>
<html lang="vi">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>WazeHUD - Cập Nhật Firmware</title>
<style>
body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; background: #121212; color: #fff; text-align: center; padding: 20px; margin: 0; }
.card { background: #1e1e1e; border-radius: 16px; padding: 24px; max-width: 400px; margin: 20px auto; box-shadow: 0 4px 20px rgba(0,0,0,0.5); }
h1 { font-size: 20px; margin-bottom: 8px; color: #3399ff; }
p { font-size: 13px; color: #aaa; margin-bottom: 20px; }
.file-box { border: 2px dashed #444; border-radius: 12px; padding: 20px; margin-bottom: 20px; cursor: pointer; transition: 0.2s; }
.file-box:active { background: #252525; }
.btn { background: #007bff; color: #fff; border: none; border-radius: 10px; padding: 12px 24px; font-size: 16px; font-weight: bold; width: 100%; cursor: pointer; }
.btn:disabled { background: #444; color: #888; cursor: not-allowed; }
.prog-wrap { display: none; margin-top: 20px; background: #333; border-radius: 8px; height: 16px; overflow: hidden; }
.prog-bar { background: #28a745; width: 0%; height: 100%; transition: width 0.2s; }
.status { margin-top: 15px; font-size: 14px; font-weight: bold; }
.hint { margin-top: 15px; font-size: 12px; color: #888; }
</style>
</head>
<body>
<div class="card">
  <h1>WazeHUD WiFi OTA</h1>
  <p>Nạp trực tiếp firmware mới cho thiết bị qua WiFi</p>
  <div class="file-box" onclick="document.getElementById('f').click()">
    <input type="file" id="f" accept=".bin" style="display:none" onchange="fileSelected(this)">
    <span id="file-name">📂 Chạm để chọn file .bin</span>
  </div>
  <button id="btn" class="btn" disabled onclick="upload()">Bắt đầu Cập nhật</button>
  <div class="prog-wrap" id="prog-wrap">
    <div class="prog-bar" id="bar"></div>
  </div>
  <div class="status" id="status"></div>
  <div class="hint">⚠️ Vui lòng giữ điện thoại gần HUD và không tắt nguồn trong khi đang nạp.</div>
</div>
<script>
function fileSelected(input) {
  if (input.files.length > 0) {
    document.getElementById('file-name').innerText = '📄 ' + input.files[0].name;
    document.getElementById('btn').disabled = false;
  }
}
function upload() {
  var file = document.getElementById('f').files[0];
  if (!file) return;
  var btn = document.getElementById('btn');
  btn.disabled = true;
  document.getElementById('prog-wrap').style.display = 'block';
  var status = document.getElementById('status');
  status.innerText = 'Đang tải lên...';
  status.style.color = '#3399ff';
  var xhr = new XMLHttpRequest();
  xhr.upload.onprogress = function(e) {
    if (e.lengthComputable) {
      var percent = Math.round((e.loaded / e.total) * 100);
      document.getElementById('bar').style.width = percent + '%';
      status.innerText = 'Đang nạp: ' + percent + '%';
    }
  };
  xhr.onload = function() {
    if (xhr.status == 200) {
      document.getElementById('bar').style.width = '100%';
      status.innerText = 'Cập nhật thành công! HUD đang khởi động lại...';
      status.style.color = '#28a745';
    } else {
      status.innerText = 'Lỗi (' + xhr.status + '): ' + xhr.responseText;
      status.style.color = '#dc3545';
      btn.disabled = false;
    }
  };
  xhr.onerror = function() {
    status.innerText = 'Mất kết nối với HUD!';
    status.style.color = '#dc3545';
    btn.disabled = false;
  };
  xhr.open('POST', '/update', true);
  xhr.send(file);
}
</script>
</body>
</html>)rawliteral";

esp_err_t getIndexHandler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, kIndexHtml, HTTPD_RESP_USE_STRLEN);
}

void restartTask(void *) {
    vTaskDelay(pdMS_TO_TICKS(1500));
    ESP_LOGI(kTag, "OTA complete; restarting MCU now...");
    esp_restart();
}

esp_err_t postUpdateHandler(httpd_req_t *req) {
    const size_t totalLen = req->content_len;
    if (totalLen == 0 || totalLen > 0x1d0000) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Kích thước file không hợp lệ");
        WifiOtaManager::instance().setStatus(OtaStatus::Error, "Kich thuoc file sai");
        return ESP_FAIL;
    }

    const esp_partition_t *updatePartition = esp_ota_get_next_update_partition(nullptr);
    if (!updatePartition) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Không tìm thấy phân vùng OTA");
        WifiOtaManager::instance().setStatus(OtaStatus::Error, "Loi phan vung OTA");
        return ESP_FAIL;
    }

    ESP_LOGI(kTag, "Starting OTA to partition %s at 0x%lx (size %u)",
             updatePartition->label, (unsigned long)updatePartition->address, (unsigned)totalLen);

    WifiOtaManager::instance().setStatus(OtaStatus::Writing, "Dang ghi flash...");

    esp_ota_handle_t otaHandle = 0;
    esp_err_t err = esp_ota_begin(updatePartition, totalLen, &otaHandle);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "esp_ota_begin failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Khởi tạo ghi OTA thất bại");
        WifiOtaManager::instance().setStatus(OtaStatus::Error, "Init OTA loi");
        return ESP_FAIL;
    }

    char buf[1024];
    size_t received = 0;
    bool checkedHeader = false;

    while (received < totalLen) {
        const int readBytes = httpd_req_recv(req, buf, std::min(sizeof(buf), totalLen - received));
        if (readBytes <= 0) {
            if (readBytes == HTTPD_SOCK_ERR_TIMEOUT) continue;
            ESP_LOGE(kTag, "Socket error during OTA receive: %d", readBytes);
            esp_ota_abort(otaHandle);
            WifiOtaManager::instance().setStatus(OtaStatus::Error, "Loi ket noi giua chung");
            return ESP_FAIL;
        }

        if (!checkedHeader) {
            if (static_cast<size_t>(readBytes) < sizeof(esp_image_header_t)) {
                esp_ota_abort(otaHandle);
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Dữ liệu header quá ngắn");
                WifiOtaManager::instance().setStatus(OtaStatus::Error, "Header qua ngan");
                return ESP_FAIL;
            }
            const auto *header = reinterpret_cast<const esp_image_header_t *>(buf);
            if (header->magic != ESP_IMAGE_HEADER_MAGIC) {
                esp_ota_abort(otaHandle);
                ESP_LOGE(kTag, "Invalid magic byte: 0x%02x (expected 0xE9)", header->magic);
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                                    "File không phải firmware ESP32 hợp lệ (Magic != 0xE9)");
                WifiOtaManager::instance().setStatus(OtaStatus::Error, "File khong hop le");
                return ESP_FAIL;
            }
            checkedHeader = true;
        }

        err = esp_ota_write(otaHandle, buf, readBytes);
        if (err != ESP_OK) {
            ESP_LOGE(kTag, "esp_ota_write failed: %s", esp_err_to_name(err));
            esp_ota_abort(otaHandle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Ghi flash thất bại");
            WifiOtaManager::instance().setStatus(OtaStatus::Error, "Ghi flash loi");
            return ESP_FAIL;
        }

        received += readBytes;
        const uint8_t percent = static_cast<uint8_t>((received * 100ULL) / totalLen);
        WifiOtaManager::instance().updateProgress(percent);
    }

    err = esp_ota_end(otaHandle);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "esp_ota_end failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Kết thúc OTA thất bại");
        WifiOtaManager::instance().setStatus(OtaStatus::Error, "Ket thuc OTA loi");
        return ESP_FAIL;
    }

    err = esp_ota_set_boot_partition(updatePartition);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Chọn phân vùng khởi động thất bại");
        WifiOtaManager::instance().setStatus(OtaStatus::Error, "Set boot partition loi");
        return ESP_FAIL;
    }

    WifiOtaManager::instance().setStatus(OtaStatus::Success, "Cap nhat thanh cong!");
    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);

    // Lên lịch reboot an toàn sau 1.5 giây
    xTaskCreate(restartTask, "ota_restart", 2048, nullptr, 5, nullptr);
    return ESP_OK;
}

void otaTimerTask(void *) {
    while (WifiOtaManager::instance().isActive()) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        WifiOtaManager::instance().tickSecond();
    }
    vTaskDelete(nullptr);
}

}  // namespace

WifiOtaManager &WifiOtaManager::instance() {
    static WifiOtaManager sManager;
    return sManager;
}

esp_err_t WifiOtaManager::start() {
    if (active_) return ESP_OK;
    active_ = true;
    status_ = OtaStatus::WaitingConnection;
    progress_ = 0;
    timeout_ = 180;
    std::snprintf(message_, sizeof(message_), "Cho ket noi...");

    ESP_LOGI(kTag, "Stopping BLE and starting WiFi SoftAP & Web OTA mode...");
#if !CONFIG_WAZE_HUD_MOCK_MODE
    BleTransport::instance().stop();
#endif
    startAp();
    startHttpServer();

    xTaskCreate(otaTimerTask, "ota_timer", 2048, nullptr, 2, nullptr);
    return ESP_OK;
}

void WifiOtaManager::stop() {
    if (!active_) return;
    active_ = false;
    stopHttpServer();
    stopAp();
}

bool WifiOtaManager::isActive() const {
    return active_;
}

WifiOtaSnapshot WifiOtaManager::snapshot() const {
    WifiOtaSnapshot snap{};
    snap.active = active_;
    snap.status = status_;
    snap.progress = progress_;
    snap.timeoutRemaining = timeout_;
    std::snprintf(snap.message, sizeof(snap.message), "%s", message_);
    return snap;
}

void WifiOtaManager::updateProgress(uint8_t percent) {
    progress_ = percent;
    status_ = OtaStatus::Writing;
    std::snprintf(message_, sizeof(message_), "Dang nap: %u%%", percent);
}

void WifiOtaManager::setStatus(OtaStatus status, const char *msg) {
    status_ = status;
    if (msg) {
        std::snprintf(message_, sizeof(message_), "%s", msg);
    }
}

void WifiOtaManager::tickSecond() {
    if (status_ == OtaStatus::WaitingConnection) {
        if (timeout_ > 0) {
            --timeout_;
            if (timeout_ == 0) {
                ESP_LOGW(kTag, "OTA timeout expired with no upload; restarting back to normal HUD...");
                esp_restart();
            }
        }
    }
}

void WifiOtaManager::startAp() {
    if (!apNetif_) {
        apNetif_ = esp_netif_create_default_wifi_ap();
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t err = esp_wifi_init(&cfg);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(kTag, "esp_wifi_init failed: %s", esp_err_to_name(err));
        return;
    }

    wifi_config_t wifiConfig{};
    constexpr char kSsid[] = "WazeHUD-OTA";
    constexpr char kPass[] = "12345678";
    std::memcpy(wifiConfig.ap.ssid, kSsid, sizeof(kSsid));
    wifiConfig.ap.ssid_len = sizeof(kSsid) - 1;
    std::memcpy(wifiConfig.ap.password, kPass, sizeof(kPass));
    wifiConfig.ap.max_connection = 4;
    wifiConfig.ap.authmode = WIFI_AUTH_WPA2_PSK;

    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &wifiConfig);
    err = esp_wifi_start();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "esp_wifi_start failed: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(kTag, "WiFi AP started. SSID: %s, Pass: %s", kSsid, kPass);
}

void WifiOtaManager::stopAp() {
    esp_wifi_stop();
    esp_wifi_deinit();
}

void WifiOtaManager::startHttpServer() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 4096;
    config.max_uri_handlers = 8;

    httpd_handle_t server = nullptr;
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t getUri{
            .uri = "/",
            .method = HTTP_GET,
            .handler = getIndexHandler,
            .user_ctx = nullptr
        };
        httpd_register_uri_handler(server, &getUri);

        httpd_uri_t postUri{
            .uri = "/update",
            .method = HTTP_POST,
            .handler = postUpdateHandler,
            .user_ctx = nullptr
        };
        httpd_register_uri_handler(server, &postUri);

        httpd_ = server;
        ESP_LOGI(kTag, "Web OTA server started on port %d", config.server_port);
    }
}

void WifiOtaManager::stopHttpServer() {
    if (httpd_) {
        httpd_stop(reinterpret_cast<httpd_handle_t>(httpd_));
        httpd_ = nullptr;
    }
}

}  // namespace waze_hud
