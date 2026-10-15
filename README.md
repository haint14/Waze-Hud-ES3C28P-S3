# WazeHUD cho ESP32-S3 2.8 inch (ES3C28P / ES3N28P)

HUD ô tô hiển thị dữ liệu dẫn đường Waze qua Bluetooth BLE (giao thức HLP/1 của Waze Mod), có **cảnh báo quá tốc độ bằng giọng nói** qua loa của board.

> [!NOTE]
> **Lời cảm ơn & Tôn trọng bản quyền tác giả (Credits & Respect):**
> Dự án này là phiên bản phát triển & tối ưu hóa mở rộng dựa trên mã nguồn gốc [WazeHUD của tác giả ShindouAris](https://github.com/ShindouAris/WazeHUD) và cộng đồng [WazeMod Vietnam](https://wazemod.io.vn). Phần giao diện, giao thức và cấu hình kế thừa từ bản CYD của [quangdo92](https://github.com/quangdo92/waze-hud). Xin trân trọng ghi nhận và cảm ơn công sức của các tác giả. Xin hãy luôn tôn trọng tác giả gốc và chia sẻ có trích nguồn. Giấy phép: GPL-3.0.

> [!IMPORTANT]
> Repo này **chỉ dành cho board ESP32-S3 2.8 inch ES3C28P / ES3N28P** (LCDWiki).
> Board **CYD ESP32-2432S028** (ESP32 thường) do [quangdo92](https://github.com/quangdo92/waze-hud) duy trì, vui lòng tải firmware CYD ở repo đó.
> **Không nạp firmware S3 lên board CYD và ngược lại.**

---

## 📥 Tải firmware

👉 **[GitHub Releases](https://github.com/haint14/Waze-Hud-ES3C28P-S3/releases/latest)**

| File | Dùng cho |
|---|---|
| `ES3C28P-BLE-<ngày>-<bản>.bin` | Dùng thật với Waze Mod qua Bluetooth BLE |
| `ES3C28P-MOCK-<ngày>-<bản>.bin` | Chạy thử giao diện + âm thanh, **không cần điện thoại** (12 kịch bản tự đổi mỗi 5 giây) |
| `SHA256.txt` | Mã kiểm tra toàn vẹn các file |

Đây là file **Factory All-in-One** (đã gồm bootloader, bảng phân vùng và firmware), nạp tại offset **`0x0`**.

## ⚡ Cách nạp

Cần Python và `esptool` (`pip install esptool`), hoặc dùng terminal của ESP-IDF.

```powershell
# Thay COM5 bằng cổng COM của board
python -m esptool --chip esp32s3 -p COM5 -b 460800 write_flash 0x0 ES3C28P-BLE-131026-b15.bin
```

- Chip: **ESP32-S3**, flash **16 MB**.
- Nếu board không tự vào chế độ nạp: giữ nút **BOOT**, nhấn **EN (RESET)** một lần, rồi thả BOOT.
- Cổng Type-C của board nối với USB nội của ESP32-S3. Windows 10 trở lên nhận cổng COM ảo, không cần driver riêng.
- Công cụ Web Flasher của [WazeMod](https://wazemod.io.vn/flash-firmware) có thể dùng được nếu hỗ trợ ESP32-S3 (chưa kiểm chứng với bản này).

---

## ✨ Tính năng

### 🔊 Cảnh báo quá tốc độ bằng giọng nói
- Phát câu **"Bạn đã quá tốc độ"** qua loa khi tốc độ vượt giới hạn + ngưỡng (chỉnh trong app) liên tục **1,5 giây**.
- Lặp lại mỗi **4 giây** (tính từ lúc đọc xong) nếu vẫn quá tốc độ. Về tốc độ cho phép thì ngừng.
- Dùng codec **ES8311** + bộ khuếch đại của board. Nếu không tìm thấy codec, HUD vẫn chạy bình thường (chỉ ghi cảnh báo vào log).
- Có thể đổi giọng nói hoặc dùng tiếng bíp (xem mục *Tùy chỉnh âm thanh*).

### 🚗 Giao diện HUD tối ưu cho taplo ô tô
- Nền đen tuyệt đối, chống chói khi đặt trên taplo.
- Hàng trên cùng: **`dd/mm | hh:mm`** (ngày/tháng màu vàng, giờ màu trắng), đồng bộ từ điện thoại.
- Tốc độ xe và biển giới hạn tốc độ cỡ lớn. Khi quá tốc độ: viền đỏ nhấp nháy 2 Hz, số tốc độ nháy đỏ.
- Hướng rẽ, khoảng cách tới lượt rẽ, vòng xuyến (kèm lối ra), tối đa 10 làn đường với mũi tên chỉ hướng (kể cả rẽ chếch 45°).
- Tên đường hiện tại (có tiền tố `HT:` khi đang dẫn đường) và **tên đường kế tiếp trượt mượt ~30 FPS** dưới biển báo tốc độ.
- ETA, quãng đường và thời gian còn lại.
- Cảnh báo: camera phạt nguội, cảnh sát, tai nạn, kẹt xe (kèm mức độ và số phút trễ), công trường, đèn đỏ, vùng cấm vượt, các biển cấm/hiệu lệnh... hiển thị tối đa 4 thẻ khi chạy tự do.
- **Màu số mét cảnh báo 4 mức**: xanh lá (> 600 m), trắng (300–600 m), cam (100–300 m), đỏ (< 100 m).
- Chạy tiếng Việt có dấu (tự chạy chữ nếu tên đường quá dài).
- Chỉ vẽ lại đúng vùng thay đổi (dirty-region), không giật khung hình.

### 🌙 Độ sáng
- **Tự động theo giờ** (mặc định): 100% ban ngày (06:00–17:30), giảm dần lúc hoàng hôn (17:30–19:00), **20%** ban đêm (19:00–05:00), tăng dần lúc bình minh (05:00–06:00).
- Bật **"Cố định độ sáng"** trong app để dùng mức độ sáng chọn tay (10–100%).

### 🎨 Đảo màu từ app
- Mục **"Đảo ngược màu"** trong app bật/tắt chế độ đảo màu của màn hình (INVON/INVOFF) mà không cần nạp lại firmware. **Mặc định bật** cho board này. Nếu màn hình bị âm bản, hãy tắt đi.

---

## ⌨️ Nút BOOT (GPIO 0)

| Thao tác | Tác dụng |
|---|---|
| Nhấn 1 lần | Xoay màn hình 180° (tùy hướng cắm cáp) |
| Nhấn đúp | Bật / tắt lật gương (Mirror HUD) để chiếu hắt kính lái |
| Nhấn giữ | Hiện màn hình trạng thái thiết bị và BLE |

Các thiết lập xoay, lật gương được lưu vào NVS, không mất khi rút nguồn.

## ⚙️ Cấu hình từ Waze Mod

Khi Waze Mod hỗ trợ `device_config` (HLP Schema Rev 7), HUD gửi lên 9 thiết lập:

| Thiết lập | Giá trị | Ý nghĩa |
|---|---|---|
| Cố định độ sáng | Bật / Tắt | Bật để dùng độ sáng chọn tay, tắt để tự động theo giờ |
| Độ sáng | 10–100%, bước 5% | Chỉ chỉnh được khi bật "Cố định độ sáng" |
| Giao diện | Tự động / Ban ngày / Ban đêm | Màu giao diện biển báo |
| Đảo ngược màu | Bật / Tắt | Đảo cực tính màn hình (INVON/INVOFF) |
| Phản chiếu HUD | Bật / Tắt | Lật ngang để chiếu hắt kính lái |
| Xoay màn hình | Bật / Tắt | Xoay 180° theo hướng lắp |
| Ngưỡng quá tốc | −10 đến +5 km/h | Bù vào giới hạn trước khi cảnh báo (áp dụng cả cho giọng nói) |
| Dịch ngang | −5 đến +5 px | Tinh chỉnh vị trí giao diện |
| Dịch dọc | −5 đến +5 px | Tinh chỉnh vị trí giao diện |

---

## 🔧 Phần cứng ES3C28P / ES3N28P

| Thành phần | Thông số / Chân kết nối |
|---|---|
| Vi điều khiển | ESP32-S3 (Xtensa LX7 lõi kép, 240 MHz), 16 MB flash, 8 MB PSRAM OPI |
| Màn hình LCD | ILI9341V 2.8 inch IPS, SPI @ 40 MHz, 240×320 (HUD dùng 320×240 landscape, xoay mềm) |
| Chân LCD | CS: GPIO 10 \| DC: GPIO 46 \| SCLK: GPIO 12 \| MOSI: GPIO 11 \| MISO: GPIO 13 |
| Reset LCD | Nối chung chân EN của ESP32-S3 (không dùng GPIO) |
| Đèn nền | GPIO 45 (LEDC PWM) |
| Loa / Âm thanh | Codec ES8311 (I2C: SDA 16, SCL 15; địa chỉ 0x18). I2S: MCLK 4, BCLK 5, LRCK 7, DOUT 8. Bật bộ khuếch đại: GPIO 1 (mức thấp) |
| Nút BOOT | GPIO 0 |
| Console / log | Cổng Type-C (USB Serial/JTAG) |

**Chưa hỗ trợ trong firmware**: đèn LED RGB (WS2812, GPIO 42), đo pin (GPIO 9), cảm ứng, thẻ nhớ microSD, micro.

---

## 🛠️ Build từ mã nguồn

Yêu cầu: **ESP-IDF 5.5.x** (đã thử với 5.5.5), Windows PowerShell. Component `espressif/es8311` và `esp_lcd_ili9341` được tải tự động.

```powershell
cd waze-hud
.\build_es3c28p.ps1                 # -> dist\waze_hud_es3c28p_factory.bin
.\build_es3c28p.ps1 -Mock           # -> dist\waze_hud_es3c28p_mock_factory.bin
.\build_es3c28p.ps1 -Port COM5      # build, gộp và nạp luôn
```

Script tự dựng file Factory gộp (nạp tại `0x0`). Tham số `-IdfProfile` đổi đường dẫn profile PowerShell của ESP-IDF nếu bạn cài ở chỗ khác (mặc định `C:\Espressif\tools\Microsoft.v5.5.5.PowerShell_profile.ps1`).

### Tùy chỉnh âm thanh
Các tùy chọn nằm trong `waze-hud/sdkconfig.es3c28p.defaults` (xóa file `sdkconfig.es3c28p` rồi build lại để áp dụng):

| Tùy chọn | Mặc định | Ý nghĩa |
|---|---|---|
| `CONFIG_WAZE_HUD_AUDIO_VOLUME` | 49 | Âm lượng (1–100) |
| `CONFIG_WAZE_HUD_AUDIO_REPEAT_SEC` | 4 | Giây giữa hai lần cảnh báo (2–60) |
| `CONFIG_WAZE_HUD_AUDIO_USE_VOICE` | bật | Bật: đọc giọng nói; tắt: tiếng bíp-bíp-bíp |
| `CONFIG_WAZE_HUD_LCD_INVERT_COLOR` | bật | Mặc định đảo màu của màn hình |

**Đổi giọng nói:** ghi âm câu bạn muốn, xuất ra **WAV 16-bit** (ví dụ bằng Audacity), rồi:

```powershell
python waze-hud\tools\wav_to_voice_clip.py "audio\file.wav" --end 1.40
```

`--start` / `--end` (giây) dùng để cắt lấy một đoạn. Công cụ tự đổi sang 16 kHz mono, cắt khoảng lặng, chuẩn hóa âm lượng và ghi vào `waze-hud/main/audio/overspeed_voice.cpp`; sau đó build lại. File gốc "Bạn đã quá tốc độ" nằm trong thư mục `audio/`.

---

## 🔄 Theo dõi cập nhật từ bản CYD

Phần dùng chung (giao thức, hiển thị) được kế thừa từ [quangdo92/waze-hud](https://github.com/quangdo92/waze-hud). Để lấy cập nhật mới:

```powershell
git remote add upstream https://github.com/quangdo92/waze-hud
git fetch upstream
git merge upstream/CYD2.8-V2USB
```

Sau khi gộp, build lại cả hai board để chắc không vỡ (`.\build_es3c28p.ps1` cho S3, và build CYD bằng `sdkconfig.defaults` để kiểm tra).
