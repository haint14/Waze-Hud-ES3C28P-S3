# Tổng quan nhanh dự án WazeHUD (bản ESP32-S3 ES3C28P)

> Cập nhật: 2026-10-15 · Nhánh: `ES3C28P-S3` · ESP-IDF 5.5.5

WazeHUD là firmware ESP32-S3 nhận dữ liệu dẫn đường Waze qua BLE/HLP/1 và hiển thị HUD ô tô 320×240 trên màn ILI9341V của board LCDWiki ES3C28P/ES3N28P, kèm cảnh báo quá tốc độ bằng giọng nói qua loa. Mã nguồn firmware nằm trong `waze-hud/main/`. Asset gốc (PNG/font) chỉ có trên máy tác giả, không nằm trong git; chúng được chuyển thành dữ liệu nhúng (`main/assets/generated_assets.cpp`) để ESP32 không phải giải mã khi chạy.

Bản CYD (ESP32-2432S028) do [quangdo92](https://github.com/quangdo92/waze-hud) duy trì. Repo này chỉ phát triển bản S3 và gộp lại mã dùng chung từ upstream.

## Phần cứng

| Thành phần | Thông số |
|---|---|
| Vi điều khiển | ESP32-S3-WROOM N16R8: 16 MB flash, 8 MB PSRAM octal, 240 MHz |
| Màn hình | ILI9341V SPI 40 MHz, BGR, mặc định INVON; native 240×320, HUD vẽ 320×240 và xoay mềm theo từng dải 20 dòng |
| Chân LCD | CS 10, DC 46, SCLK 12, MOSI 11, MISO 13; reset nối chung EN; đèn nền GPIO 45 (LEDC) |
| Âm thanh | Codec ES8311 (I2C SDA 16, SCL 15), I2S MCLK 4, BCLK 5, LRCK 7, DOUT 8; bộ khuếch đại GPIO 1 (mức thấp là bật) |
| Nút BOOT | GPIO 0: nhấn 1 lần xoay 180°, nhấn đúp lật gương, nhấn giữ hiện trạng thái |
| Console | Type-C (USB Serial/JTAG) |

GPIO 33–37 là bus PSRAM octal, không được cấu hình. Chưa dùng: LED RGB WS2812 (GPIO 42), đo pin (GPIO 9, chưa hiệu chỉnh), cảm ứng FT6336G, thẻ nhớ, micro.

## Kiến trúc

```text
Điện thoại / Waze Mod
        │ BLE GATT
        ▼
NimBLE transport ── bounded queue ──► HLP protocol task
                                            │
                                  framing + JSON decode
                                            ▼
                                  HudStateStore snapshot
                                            │
                                            ▼
                         dirty-region renderer + embedded assets
                                            │
                                            ▼
                                  display driver RGB565
```

| Đường dẫn | Trách nhiệm |
| --- | --- |
| `main/bluetooth/` | BLE GATT, advertising, write/notify và reconnect |
| `main/protocol/` | HLP framing, handshake, heartbeat và decode state |
| `main/state/` | Snapshot HUD cố định dung lượng, thống kê hành trình |
| `main/display/` | Layout, renderer, font và driver LCD |
| `main/config/` | Cấu hình động (9 mục, schema rev 7), validation, lưu NVS |
| `main/audio/` | Cảnh báo quá tốc độ qua ES8311 |
| `main/system/` | Trạng thái BLE/pin |
| `main/assets/` | RGB565, alpha mask và font đã generate |
| `main/touch/`, `main/ota/` | File stub rỗng; file gốc của upstream giữ nguyên để merge không xung đột |

BLE callback chỉ sao chép dữ liệu vào queue. JSON, cập nhật state và truyền LCD chạy ngoài callback. Renderer chỉ vẽ lại vùng bị thay đổi.

## Build

```powershell
cd waze-hud
.\build_es3c28p.ps1              # -> dist\waze_hud_es3c28p_factory.bin (nạp tại 0x0)
.\build_es3c28p.ps1 -Port COM5   # build, gộp và nạp luôn
.\build_es3c28p.ps1 -Mock        # bản mock, không cần điện thoại
```

Đặt tên file phát hành và tính SHA256: `./tools/pack_release.sh <bản>` (ví dụ `16-r2`).

## Tài liệu liên quan

- `README.md`: giới thiệu, cách nạp, tính năng, cấu hình.
- `CLAUDE.md`: quy tắc làm việc và các ràng buộc giao thức.
- `CHANGELOG.md`: lịch sử thay đổi (mục `S3` là riêng bản này).
- `waze-hud/DISPLAY_CYD_28_PORTING.md`: ghi chú cổng LCD ILI9341 (viết cho CYD, cùng panel).
- `waze-hud/FLASH_FIRMWARE_VI.md`: hướng dẫn nạp bằng tiếng Việt (viết cho bản CYD của upstream).
- `waze-hud-link-sdk-ai-bundle.md`: đặc tả HLP/1.
- `waze-hud/main/Kconfig.projbuild`: tùy chọn build.
