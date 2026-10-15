# CLAUDE.md – WazeHUD cho ESP32-S3 2.8" (ES3C28P / ES3N28P)

Hướng dẫn làm việc cho Claude Code trong repo này. Trả lời người dùng bằng **tiếng Việt**.

## 1. Dự án là gì

Firmware ESP32-S3 (ESP-IDF 5.5.x, C++17) nhận dữ liệu dẫn đường Waze qua **BLE / HLP/1** từ app WazeMod và vẽ HUD ô tô 320×240 trên màn ILI9341V. Có **cảnh báo quá tốc độ bằng giọng nói** qua loa (codec ES8311).

- Repo: `haint14/Waze-Hud-ES3C28P-S3` (công khai, GPL-3.0; tên cũ `haint14/waze-hud` vẫn chuyển hướng tới đây). Nhánh duy nhất và mặc định: **`ES3C28P-S3`**.
- Đây là fork của `quangdo92/waze-hud` (bản CYD). Người dùng **chỉ phát triển bản S3**; bản CYD do quangdo92 duy trì. Remote `upstream` trỏ tới quangdo92.
- Mã nguồn firmware: `waze-hud/main/`. Asset gốc (`assets/Waze`, `alerts`, `fonts`, `speedLimit`, `App`) chỉ có trên máy tác giả, **không nằm trong git** (`/assets/` bị gitignore, chỉ `assets/demo` được theo dõi), nên không tạo lại `generated_assets.cpp` được từ bản clone mới. Giọng nói gốc: `audio/`.

## 2. Quy trình làm việc

1. **Thay đổi giao thức, cấu hình hoặc bố cục nhiều file**: nêu ngắn gọn phạm vi file bị ảnh hưởng và rủi ro (protocol, task timing, bộ nhớ DMA, giao diện) **trước khi sửa**. Sửa nhỏ, rõ ràng thì làm thẳng.
2. **Mặc định chỉ build một bản: S3 BLE cho WazeMod** (`waze-hud\build_es3c28p.ps1`). **Không** build mock trừ khi người dùng yêu cầu. Bản CYD 2.8" **chỉ build khi người dùng yêu cầu** (cách build ở mục 3). Kiểm tra CYD sau khi gộp `upstream` chỉ khi người dùng muốn.
3. **Người dùng nạp thử trên board thật rồi mới xác nhận.** Sau mỗi thay đổi: build, đưa đường dẫn file `.bin`, rồi hỏi xác nhận. **Không commit/push/phát hành khi chưa được yêu cầu.** Khi người dùng nói "commit và git push", mới làm.
4. Chỉ push lên `origin` (`haint14`). **Không bao giờ push lên `quangdo92`.** Trước khi push nhánh/xóa nhánh/ghi đè, nói rõ giả định về repo đích.
5. Không commit file `.bin` (đã có trên GitHub Releases). Thư mục `waze-hud/dist/` chỉ chứa bản build cục bộ.
6. Không dùng `git push --force`, không viết lại lịch sử đã push khi chưa được yêu cầu.

## 3. Build, nạp, phát hành

```powershell
cd waze-hud
.\build_es3c28p.ps1              # -> dist\waze_hud_es3c28p_factory.bin (nạp tại 0x0)
.\build_es3c28p.ps1 -Port COM5   # build, gộp và nạp luôn
```

- ESP-IDF 5.5.5: profile `C:\Espressif\tools\Microsoft.v5.5.5.PowerShell_profile.ps1`, mã nguồn IDF ở `D:\esp\v5.5.5\esp-idf`.
- `dependencies.lock` chỉ lưu **một** target. Bản S3 dùng `dependencies.es3c28p.lock`; script tự hoán đổi rồi khôi phục. Đừng commit `dependencies.lock` đã bị đổi sang `esp32s3`.
- Cấu hình S3: `sdkconfig.es3c28p.defaults`. File `sdkconfig.es3c28p` sinh ra từ đó (bị gitignore); **xóa nó rồi build lại** để áp dụng giá trị mới trong `.defaults`.
- Chip `esp32s3`, flash 16 MB, PSRAM octal 8 MB. Cổng Type-C là USB Serial/JTAG (console/log ra đây).
- **Build bản CYD 2.8" (khi được yêu cầu)**: `idf.py -B build-cyd -D SDKCONFIG=sdkconfig.cyd -D SDKCONFIG_DEFAULTS=sdkconfig.defaults build`, rồi trong thư mục `build-cyd` gộp bằng `python -m esptool --chip esp32 merge_bin -o <ra.bin> --flash_mode dio --flash_size 4MB --flash_freq 40m "@flash_args"` (nạp tại `0x0`, `--chip esp32`). Đặt file ra ở `waze-hud\dist\release_cyd\` (đã gitignore). Bản CYD có LED RGB nháy khi quá tốc độ, không có âm thanh, mặc định đảo màu tắt và xoay bật; `dependencies.lock` giữ target `esp32`.
- Phát hành bằng `gh release create` (đã cài, đăng nhập `haint14`). Tag dạng `v2.8.15-s3`, file đặt tên `ES3C28P-BLE-<ddmmyy>-b<bản>.bin`, kèm `SHA256.txt`. Tiêu đề có dấu tiếng Việt: tạo bằng tiêu đề ASCII rồi cập nhật qua `gh api` với tệp JSON UTF-8 (PowerShell 5.1 làm hỏng dấu nháy kép/ký tự có dấu trong tham số).

## 4. Phần cứng ES3C28P (đã kiểm chứng trên board)

| Thành phần | Chân |
|---|---|
| LCD ILI9341V (SPI, 40 MHz) | CS 10, DC 46, SCLK 12, MOSI 11, MISO 13; reset nối chung EN |
| Đèn nền | GPIO 45 (LEDC) |
| Codec ES8311 (I2C 0x18/0x19) | SDA 16, SCL 15 (chung bus với cảm ứng FT6336G) |
| I2S | MCLK 4, BCLK 5, LRCK 7, DOUT 8 |
| Bộ khuếch đại loa | GPIO 1, **mức thấp là bật** |
| Nút BOOT | GPIO 0 |

- Đảo màu LCD là **cài đặt chạy lúc runtime** (`invert_color`), mặc định **bật** cho board này (`CONFIG_WAZE_HUD_LCD_INVERT_COLOR` → `kDefaultInvertColor`).
- **GPIO 33–37 là bus PSRAM octal: không cấu hình các chân này.**
- Chưa dùng: LED RGB WS2812 (GPIO 42), đo pin (GPIO 9, chưa biết tỉ lệ chia áp), cảm ứng, thẻ nhớ, micro.

## 5. Không dùng cảm ứng CYD và WiFi OTA của upstream (cả S3 lẫn CYD)

B16 của quangdo92 thêm module cảm ứng XPT2046 (GPIO 33/36, trùng chân PSRAM trên S3) và WiFi Web OTA (SoftAP mật khẩu cố định, trang tải firmware không xác thực). Module cảm ứng chỉ phục vụ kích hoạt OTA. **Cả hai không dùng trên mọi board của dự án này, kể cả CYD.**

- Cả hai board dùng file rỗng `touch/touch_sensor_stub.cpp` và `ota/wifi_ota_manager_stub.cpp`, đặt cố định trong `main/CMakeLists.txt` (không phụ thuộc `IDF_TARGET`). Bỏ OTA làm bản CYD nhỏ đi: còn khoảng 25% trống trong slot 1,8 MB thay vì khoảng 3%.
- **Giữ nguyên file gốc của upstream** (`touch_sensor.cpp`, `wifi_ota_manager.cpp`) để các lần `git merge upstream/CYD2.8-V2USB` sau không xung đột lại. Chỉ sửa stub khi giao diện lớp thay đổi.
- Khi gộp upstream: giải xung đột, giữ README bản S3 (`git checkout --ours README.md`), build bản S3, rồi báo người dùng nạp thử.
- `GEMINI.md` của upstream đã bị xóa khỏi repo này (thay bằng `CLAUDE.md`). Nếu gộp upstream báo xung đột kiểu "deleted by us" với `GEMINI.md`, giữ trạng thái xóa: `git rm GEMINI.md`.

## 6. Giao thức HLP và cài đặt (`HLP Device Config`)

Mã nằm ở `main/protocol/` (`hlp_protocol.cpp`, `hlp_decoder.cpp`, `hlp_core.c`) và `main/config/device_config.*`. HUD **sở hữu** danh mục cài đặt và khai báo lên app.

- Khi nhận `hi` có `"caps": ["device_config"]`, `DeviceConfig::handleMessage` gọi `publishSchema`: gửi `cfg_begin` (`rev = kSchemaRevision` = **7**, `count = kItemCount` = **9**), 9 gói `cfg_item`, rồi `cfg_end`.
- 9 mục: `fixed_brightness`, `brightness`, `theme`, `invert_color`, `mirror_hud`, `rotate_display`, `overspeed_offset`, `offset_x`, `offset_y`.
- App gửi giao dịch nguyên tử `cfg_set_begin` → `cfg_set` × 9 → `cfg_set_commit`. HUD kiểm tra đủ mặt nạ bit, lưu NVS (`hud_cfg`), tăng `revision`, gọi `HudStateStore::refresh()` rồi gửi `cfg_ack`.
- Khi **thêm/bớt/đổi** một mục cài đặt, phải cập nhật đồng bộ trong `device_config.cpp/.h`: `DeviceSettings`, `kSchemaRevision`, `kItemCount`, `saveSettings`, phần đọc NVS trong `DeviceConfig::init`, `publishSchema`, nhánh `cfg_set` (bit trong `pending.mask`), và `hasSettingsChanged` trong `hud_renderer.cpp`. Đổi schema có thể làm app WazeMod từ chối giao dịch: kiểm tra với app trước khi phát hành.

**Bất khả xâm phạm:**

- **Không gửi lại `dev` khi nhận `hi`.** `dev` chỉ gửi khi BLE bật notify (`BleEventKind::NotificationsEnabled` → `sendDeviceDeclaration`). Gửi `dev` trong nhánh xử lý `hi` gây vòng lặp `dev` ↔ `hi` vô tận, reset session, xóa `HudState` (mất phân làn) và hủy giao dịch cài đặt.
- **Không chèn `vTaskDelay` vào `sendJson`/đường gửi `cfg_*`.** 11 bản tin `cfg_*` chạy trên task `hlp_protocol`; trễ ở đây làm nghẽn hàng đợi nhận. Việc chia gói BLE theo MTU (`mtu - 3` byte, nghỉ 3 ms giữa các gói) thuộc `BleTransport::sendLine`.
- Callback BLE chỉ **copy dữ liệu vào queue**; JSON, cập nhật state và vẽ chạy ngoài callback.
- Frame HLP tối đa 512 byte, kiểm tra UTF-8, bỏ frame lỗi an toàn.

## 7. Ưu tiên biển báo cảnh báo

Hai nơi sắp xếp, **cần giữ nhất quán**:

1. **`HlpDecoder` (`hlp_decoder.cpp`, `alertPriorityScore`)** chuẩn hóa danh sách `alrs`: hạ tốc độ `SpeedDrop` (100) > biển cấm/hiệu lệnh `isProhibitionAlert` (90) > cấm đường/tai nạn/nguy hiểm (70) > đèn đỏ & camera khi ≤ 250 m (60) > ùn tắc (50) > cảnh sát và các loại khác (30) > đèn đỏ xa (20) > camera xa (10). Bằng điểm: gần hơn đứng trước; nhiều `SpeedDrop` cùng vị trí: giới hạn cao hơn trước.
2. **`HudRenderer` (`hud_renderer.cpp`, `collectSortedAlerts`)** gom `noPassingZone`, `nearestAlert`, `upcomingAlerts` (bỏ trùng) và xếp 3 hạng: (1) biển cấm & hạ tốc độ, (2) camera/đèn đỏ/CSGT, (3) còn lại; cùng hạng thì gần hơn trước.

Hiển thị: biển ưu tiên số 1 (`allAlerts[0]`) vẽ **LỚN** (`dominant = true`, asset `large`), biển phụ **NHỎ**.
- Không có làn (`laneCount == 0`): cột phải chỉ có 1 biển lớn nhất; các biển tiếp theo đẩy xuống thanh đáy thành 1–4 thẻ, **không lặp lại `allAlerts[0]`**.
- Có làn (`laneCount > 0`): thanh đáy dùng cho mũi tên làn; cột phải chia 2 tầng (trên lớn, dưới nhỏ).
- Màu số mét cảnh báo 4 mức (`alertDistanceColor`): xanh lá > 600 m, trắng 300–600 m, cam 100–300 m, đỏ < 100 m.

Lấy kích thước/vị trí cụ thể từ mã (`hud_renderer.cpp`), không hardcode lại trong tài liệu.

## 8. Giao diện đặc thù của bản S3

- Hàng trên cùng cột phải **khi đang dẫn đường**: `dd/mm | hh:mm` (ngày/tháng vàng, giờ trắng, cùng font Medium, khoảng cách chữ co 1 px, vạch phân cách 1 px). Cột chỉ rộng 95 px, không đủ cho năm.
- Chạy tự do: cột trái hiện thứ/giờ/ngày (từ B16 của upstream).
- Không dùng chân GPIO của CYD (LED RGB 4/16/17) trên S3. Khối LED bị tắt bằng `#if !CONFIG_WAZE_HUD_BOARD_ES3C28P`.

## 9. Âm thanh cảnh báo (`main/audio/`)

- Điều kiện: quá tốc độ liên tục **1,5 giây** (cùng điều kiện với LED/viền đỏ: `connected && hasProducerState && !signalStale && speedLimitKmh > 0 && speedKmh > giới hạn + overspeedOffsetKmh`), lặp mỗi `CONFIG_WAZE_HUD_AUDIO_REPEAT_SEC` giây **kể từ lúc đọc xong**.
- Mặc định: giọng nói "Bạn đã quá tốc độ" (`CONFIG_WAZE_HUD_AUDIO_USE_VOICE`), âm lượng `CONFIG_WAZE_HUD_AUDIO_VOLUME` = 49, lặp 4 giây. Tắt giọng nói thì phát bíp-bíp-bíp.
- Đổi giọng nói: `python waze-hud\tools\wav_to_voice_clip.py <file.wav> [--start S] [--end S]` rồi build lại. Chỉ nhận WAV 16-bit; tự đổi sang 16 kHz mono, cắt khoảng lặng, chuẩn hóa. Tối đa 6 giây.
- Thiếu codec thì HUD vẫn chạy (chỉ ghi cảnh báo log). Bật bộ khuếch đại chỉ trong lúc phát để tránh ù.
- Console dùng `ESP_CONSOLE_USB_SERIAL_JTAG` (Type-C nối USB nội của S3, không phải UART0).

## 10. Ghi nhận tác giả và tuyên bố AI (mọi bản phát hành, README, CHANGELOG)

Trong **tất cả** bản phát hành (GitHub Releases, `CHANGELOG.md`, `README.md`, bài giới thiệu):

1. Luôn cảm ơn và dẫn link: tác giả gốc **[ShindouAris (WazeHUD)](https://github.com/ShindouAris/WazeHUD)**, cộng đồng **[WazeMod Vietnam](https://wazemod.io.vn)**, và nền tảng CYD của **[quangdo92](https://github.com/quangdo92/waze-hud)**. Giữ giấy phép **GPL-3.0** và ghi rõ nguồn.
2. Nêu minh bạch: bản S3 này là phiên bản cá nhân hóa, được phát triển với sự hỗ trợ của **AI (Claude Code, Anthropic)**, trên nền mã nguồn CYD của quangdo92 (bản CYD đó, theo CHANGELOG của họ, được phát triển với Google Gemini). Mục đích: nghiên cứu, độ chế và sử dụng thực tế trên xe của cá nhân tác giả, chia sẻ mã nguồn mở cho cộng đồng cùng sở thích.

## 11. Kiểm thử

Không có unit test. Cách kiểm chứng là **build thành công bản S3 + nạp lên board thật**. Bản mock (`-Mock`) chỉ dùng khi người dùng cần xem nhanh giao diện/âm thanh mà không cần điện thoại. Không tuyên bố "đã chạy" khi mới chỉ build: nói rõ chỉ biên dịch được hay đã kiểm chứng trên board.
