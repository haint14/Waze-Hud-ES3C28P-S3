#!/usr/bin/env bash
set -e

# Đặt tên bản build S3 theo quy ước phát hành và tính SHA256, đầu ra vào releases/.
# Chạy waze-hud/build_es3c28p.ps1 trước để có waze-hud/dist/waze_hud_es3c28p_factory.bin.
# Sử dụng: ./tools/pack_release.sh <bản>
# Ví dụ:   ./tools/pack_release.sh 16-r2   -> releases/ES3C28P-BLE-<ddmmyy>-b16-r2.bin

if [ -z "$1" ]; then
    echo "Cách dùng: $0 <bản>   (ví dụ: 16-r2)"
    exit 1
fi

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SOURCE_BIN="${PROJECT_ROOT}/waze-hud/dist/waze_hud_es3c28p_factory.bin"
RELEASE_DIR="${PROJECT_ROOT}/releases"
TIMESTAMP="$(TZ='Asia/Ho_Chi_Minh' date +'%d%m%y')"
OUTPUT_NAME="ES3C28P-BLE-${TIMESTAMP}-b$1.bin"

if [ ! -f "${SOURCE_BIN}" ]; then
    echo "Lỗi: không tìm thấy ${SOURCE_BIN}. Chạy waze-hud/build_es3c28p.ps1 trước." >&2
    exit 1
fi

mkdir -p "${RELEASE_DIR}"
cp "${SOURCE_BIN}" "${RELEASE_DIR}/${OUTPUT_NAME}"
(cd "${RELEASE_DIR}" && sha256sum "${OUTPUT_NAME}" > SHA256.txt)

echo "Đã tạo: ${RELEASE_DIR}/${OUTPUT_NAME} (nạp tại 0x0, --chip esp32s3)"
cat "${RELEASE_DIR}/SHA256.txt"
