#!/usr/bin/env bash
set -e

# Script đóng gói firmware Waze HUD với timestamp (ngày_giờ) vào thư mục releases/
# Sử dụng: ./tools/pack_release.sh [TARGET_NAME]
# Ví dụ: ./tools/pack_release.sh CYD2.8-BLE-OTA

TARGET_NAME="${1:-CYD2.8-BLE-OTA}"
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/waze-hud/build"
RELEASE_DIR="${PROJECT_ROOT}/releases"
TIMESTAMP="$(TZ='Asia/Ho_Chi_Minh' date +'%d%m%y_%H%M')"

mkdir -p "${RELEASE_DIR}"

if [ ! -f "${BUILD_DIR}/waze_hud_cyd_28.bin" ]; then
    echo "❌ Lỗi: Không tìm thấy ${BUILD_DIR}/waze_hud_cyd_28.bin. Vui lòng build trước!"
    exit 1
fi

echo "📦 Đang đóng gói firmware cho mục tiêu: ${TARGET_NAME} (Thời gian: ${TIMESTAMP})..."

# Kích hoạt ESP-IDF nếu chưa có esptool trong PATH
if ! command -v esptool.py &> /dev/null && [ -f /workspaces/esp-idf/export.sh ]; then
    . /workspaces/esp-idf/export.sh >/dev/null 2>&1
fi

FULL_BIN="${RELEASE_DIR}/${TARGET_NAME}-${TIMESTAMP}_full.bin"
APP_BIN="${RELEASE_DIR}/${TARGET_NAME}-${TIMESTAMP}_app.bin"

# 1. Ghép toàn bộ thành file nạp 0x0 (Full Flash)
python3 -m esptool --chip esp32 merge_bin \
  -o "${FULL_BIN}" \
  --flash_mode dio --flash_size 4MB --flash_freq 40m \
  0x1000 "${BUILD_DIR}/bootloader/bootloader.bin" \
  0x8000 "${BUILD_DIR}/partition_table/partition-table.bin" \
  0xf000 "${BUILD_DIR}/ota_data_initial.bin" \
  0x20000 "${BUILD_DIR}/waze_hud_cyd_28.bin"

# 2. Copy riêng partition app để nạp qua Web OTA
cp "${BUILD_DIR}/waze_hud_cyd_28.bin" "${APP_BIN}"

echo ""
echo "✅ Đóng gói hoàn tất vào thư mục: ${RELEASE_DIR}"
echo "  1️⃣ Full Flash (Nạp qua USB ở offset 0x0): ${FULL_BIN}"
echo "  2️⃣ App OTA    (Nạp qua WiFi Web OTA):      ${APP_BIN}"
echo ""
ls -lh "${FULL_BIN}" "${APP_BIN}"

