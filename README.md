# WazeHUD cho màn hình CYD 2.8 inch (Bản tối ưu Taplo Ô tô)

> [!NOTE]
> **Lời cảm ơn & Tôn trọng bản quyền tác giả (Credits & Respect):**
> Dự án này là phiên bản phát triển & tối ưu hóa mở rộng dựa trên mã nguồn gốc [WazeHUD của tác giả ShindouAris](https://github.com/ShindouAris/WazeHUD) và cộng đồng [WazeMod Vietnam](https://wazemod.io.vn). Xin trân trọng ghi nhận và cảm ơn công sức to lớn của tác giả gốc đã tạo nên nền tảng ban đầu tuyệt vời cho cộng đồng người dùng Waze! Xin hãy luôn tôn trọng tác giả gốc và chia sẻ có trích nguồn.

---

## 🚀 Cập Nhật Firmware Nhanh Bằng Điện Thoại (Từ Build 16 Trở Đi)

Từ phiên bản **Build 16 (B16)** trở đi, WazeHUD hỗ trợ tính năng **Cập nhật Không dây WiFi Web OTA qua màn hình cảm ứng**. Bạn có thể nâng cấp firmware trực tiếp từ điện thoại (iPhone, Android) ngay trên xe chỉ trong 1 phút mà **không cần tháo thiết bị hay cắm cáp vào máy tính**!

> [!IMPORTANT]
> **LƯU Ý ĐẶC BIỆT KHI LÊN BẢN B16 LẦN ĐẦU TIÊN:**
> - Nếu thiết bị của bạn đang ở các phiên bản cũ (**Build 15 trở về trước**), thiết bị chưa có phân vùng OTA và chưa có tính năng phát WiFi.
> - **Lần đầu tiên nâng cấp lên Build 16 bắt buộc phải nạp bằng cáp USB qua máy tính** (dùng file `CYD2.8-BLE-xxx-b16_full.bin` nạp tại offset `0x0` qua Web Flasher hoặc esptool theo hướng dẫn bên dưới).
> - Kể từ khi thiết bị đã chạy Build 16, tất cả các bản cập nhật trong tương lai (B17, B18...) bạn chỉ cần tải file `..._app.bin` và nạp không dây qua điện thoại!

### 📱 Các Bước Nâng Cấp Qua Điện Thoại (WiFi Web OTA)
1. **Tải firmware OTA về điện thoại:**
   - Truy cập **[GitHub Releases Mới Nhất](https://github.com/quangdo92/waze-hud/releases/latest)** bằng trình duyệt trên điện thoại.
   - Tải file có đuôi **`_app.bin`** (ví dụ: `CYD2.8-BLE-141026-b16_app.bin`).
2. **Kích hoạt chế độ OTA trên HUD:**
   - Dùng ngón tay **chạm giữ màn hình HUD trong 3 giây** (hoặc nhấn giữ nút **BOOT** sau lưng 3 giây).
   - Màn hình sẽ hiện hộp thoại xác nhận đếm ngược 6 giây kèm thanh tiến trình.
   - **Chạm giữ tiếp màn hình trong 3 giây** để xác nhận vào chế độ WiFi OTA.
3. **Kết nối WiFi từ điện thoại:**
   - Mở cài đặt WiFi trên điện thoại, tìm và kết nối vào mạng: **`WazeHUD-OTA`**
   - Mật khẩu WiFi: **`12345678`**
4. **Nạp Firmware:**
   - Mở trình duyệt web (Safari, Chrome...), truy cập địa chỉ: **`http://192.168.4.1`**
   - Bấm **Choose File** (Chọn tệp) và chọn file **`..._app.bin`** vừa tải ở Bước 1.
   - Bấm **Bắt đầu Cập nhật (Upload & Flash)**.
   - Tiến trình nạp % hiển thị đồng thời trên cả điện thoại và HUD. Khi đạt 100%, thiết bị sẽ tự khởi động lại vào phiên bản mới.

---

## 📥 Tải Firmware & Nạp Qua Cáp Máy Tính (Dành cho Lần đầu lên B16 hoặc Cứu máy)

👉 **Truy cập trang phát hành chính thức:** **[GitHub Releases - WazeHUD](https://github.com/quangdo92/waze-hud/releases/latest)**

Tại trang Releases, chọn tải file `.bin` phù hợp với nhu cầu:
- **`CYD2.8-BLE-xxx_app.bin`**: Dành cho nạp không dây qua WiFi Web OTA trên điện thoại (như hướng dẫn ở trên).
- **`CYD2.8-BLE-xxx_full.bin`**: Bản Bluetooth BLE nạp Full Flash (offset `0x0`) qua cáp USB trên máy tính (dành cho lần đầu lên B16 hoặc khi cần nạp mới hoàn toàn).
- **`CYD2.8-USB-xxx.bin`**: Bản cắm cáp USB Serial 115200 (offset `0x0`) cho mạch CYD 2.8".

### 🌐 Cách 1: Nạp trực tiếp qua Web Flasher (Khuyên dùng khi nạp cáp máy tính)
Bạn có thể sử dụng công cụ Web Flasher của tác giả WazeMod trực tiếp trên trình duyệt web (Chrome, Edge, Cốc Cốc trên máy tính hoặc điện thoại Android hỗ trợ OTG) mà không cần cài đặt Python hay bất kỳ phần mềm nào:

1. Tải file `..._full.bin` hoặc file USB Serial phù hợp về máy.
2. Cắm cáp kết nối mạch CYD với máy tính qua cổng USB.
3. Truy cập công cụ web flasher của tác giả: 👉 **[https://wazemod.io.vn/flash-firmware](https://wazemod.io.vn/flash-firmware)**
4. Bấm **Kết nối**, chọn đúng cổng COM của mạch ESP32 CYD.
5. Chọn file `.bin` đã tải, đảm bảo địa chỉ nạp là **`0x0`** và bấm **Flash** để hoàn tất.

*(Mẹo: Nếu mạch không vào được chế độ flash, hãy nhấn giữ nút **BOOT**, bấm nhả nút **RESET**, sau đó thả nút **BOOT** rồi kết nối lại).*

### 💻 Cách 2: Nạp qua dòng lệnh esptool (Dành cho Developer / Terminal)

```bash
# Nạp file Factory tương ứng tại offset 0x0
python -m esptool --chip esp32 -b 460800 write_flash 0x0 <ten_file_factory.bin>
```

---

## Tính năng chính & Cải tiến nổi bật

### 🚗 Tối ưu chuyên biệt cho Taplo Ô tô & Lái xe an toàn
- **Nền đen tuyệt đối (OLED-style True Black):** Toàn bộ hình nền và các panel giao diện sử dụng màu đen thuần túy `0x0000`, tăng tối đa độ tương phản cho màn LCD, loại bỏ hoàn toàn ánh sáng viền xám mờ và chống chói lóa mắt khi đặt trên taplo xe.
- **Tự động điều chỉnh độ sáng Ngày / Đêm theo thời gian thực:**
  - **06:00 – 17:30 (Ban ngày):** Đèn nền tự động sáng tối đa 100% để hiển thị sắc nét ngay cả dưới ánh nắng gắt.
  - **17:30 – 19:00 (Hoàng hôn):** Tự động giảm dần độ sáng từ 100% xuống 30% một cách êm ái.
  - **19:00 – 05:00 (Ban đêm):** Duy trì độ sáng 30% dịu mắt, chống lóa khoang lái trong đêm tối.
  - **05:00 – 06:00 (Bình minh):** Tăng dần độ sáng từ 30% trở lại 100%.
- **Biển báo cảnh báo & khoảng cách cỡ lớn:**
  - Icon biển báo cảnh báo phía trước được phóng to lên kích thước **60×60 px / 56×56 px** (so với 44px ban đầu).
  - Chữ hiển thị số mét khoảng cách cảnh báo được nâng cấp lên font cỡ vừa **`kTextMedium` (22 px)** to rõ, quan sát cực kỳ dễ dàng khi xe chạy ở tốc độ cao.
- **Tối ưu khu vực điều hướng rẽ:**
  - Mũi tên chỉ hướng rẽ được nâng cao lên sát mép trên (`y=16`), giải phóng không gian phía dưới.
  - Chữ số mét khoảng cách tới khúc rẽ tiếp theo được phóng to thành font lớn **`kTextMedium` ở vị trí `y=88`**.
- **Viền biển báo Đỏ tươi bão hòa 100% (`0xF800`):**
  - Chuyển toàn bộ màu viền của tất cả biển báo giới hạn tốc độ (10–120 km/h) và biển báo cấm (cấm vượt, cấm ô tô, cấm rẽ, camera đèn đỏ...) từ màu hồng phấn/cam san hô cũ sang màu đỏ cờ rực rỡ chuẩn biển báo giao thông đường bộ, khử răng cưa mượt mà.
- **Tối ưu đèn LED RGB lưng:**
  - Khi xe di chuyển ở tốc độ cho phép, đèn LED lưng tự động **TẮT HOÀN TOÀN** để không gây hắt sáng khó chịu lên kính lái ban đêm.
  - Chỉ nhấp nháy đỏ cảnh báo với tần số 2 Hz khi phát hiện xe chạy quá tốc độ giới hạn.
- **Giao diện HUD tối giản, không phân tâm:**
  - Loại bỏ biểu tượng Bluetooth và vạch sóng ở góc trên màn hình lái xe để giao diện sạch sẽ, tập trung hoàn toàn vào cảnh báo và điều hướng.
  - Loại bỏ hoàn toàn mảng bitmap Boot Logo để **tiết kiệm gần 28 KB bộ nhớ Flash ROM**, đồng thời căn giữa màn hình chờ kết nối chữ WazeHUD đẹp mắt.

### 📡 Tính năng cốt lõi kế thừa
- Kết nối không dây BLE tức thì với Waze Mod (tên thiết bị `WazeHUD`), tốc độ làm tươi 4 Hz.
- Hiển thị tốc độ xe thực tế, biển báo tốc độ giới hạn, hướng rẽ và khoảng cách tới lượt rẽ.
- Cảnh báo chướng ngại vật: Tai nạn, kẹt xe (kèm mức độ & số phút trễ), công trường, xe dừng, ngập nước, đường đóng, camera phạt nguội, kiểm tra khoảng cách, v.v.
- Hiển thị làn đường (Lane Guidance) tối đa 10 làn với mũi tên chỉ hướng được khuyến nghị.
- Hiển thị ETA (giờ đến), tên đường tiếng Việt có dấu (tự động chạy chữ nếu quá dài) và đồng hồ.
- Hai chế độ hiển thị: Ưu tiên tốc độ xe hoặc Ưu tiên biển báo giới hạn tốc độ.
- Hỗ trợ lật gương (Mirror HUD) để chiếu hắt kính lái và xoay màn hình 180° linh hoạt qua nút bấm BOOT.
- Cơ chế Dirty-region rendering: Chỉ vẽ lại đúng vùng có dữ liệu thay đổi, đảm bảo tốc độ đáp ứng tối đa và không rung giật khung hình.

## Trạng thái đèn LED RGB phía sau

| Trạng thái xe & kết nối | Màu LED RGB | Ý nghĩa |
|---|---|---|
| Chưa kết nối điện thoại | Đổi màu RGB liên tục | Đang phát sóng BLE chờ kết nối |
| Đã kết nối, chờ dữ liệu dẫn đường | Xanh dương | Đã kết nối BLE thành công |
| Tốc độ bình thường (đang dẫn đường) | **TẮT** | Giữ không gian tối êm dịu, không gây chói mắt |
| Chạy quá tốc độ giới hạn | **Nháy đỏ 2 Hz** | Cảnh báo xe đang chạy vượt tốc độ cho phép |

---

## Thao tác với nút cứng (BOOT / KEY)

| Thao tác nút BOOT | Tác dụng |
|---|---|
| Nhấn 1 lần | Xoay ngược màn hình 180° (phù hợp cắm cáp từ cạnh trên hoặc dưới) |
| Nhấn đúp (2 lần) | Bật / Tắt chế độ lật gương (Mirror HUD) chiếu hắt kính lái |
| Nhấn giữ | Hiển thị màn hình chuẩn đoán trạng thái thiết bị và BLE |

*Các thiết lập xoay và lật gương được tự động lưu vào bộ nhớ NVS và không bị mất khi rút nguồn.*

---

## Cấu hình từ Waze Mod

Khi Waze Mod hỗ trợ `device_config`, HUD gửi lên các thiết lập:

| Thiết lập | Giá trị | Ý nghĩa |
|---|---|---|
| Độ sáng | 10–100%, bước 5% | Điều chỉnh đèn nền màn hình thủ công |
| Giao diện | Tự động / Ban ngày / Ban đêm | Chọn màu giao diện |
| Hiển thị tốc độ | Tốc độ hiện tại / Biển giới hạn | Chọn thành phần tốc độ chính |
| Hiện tên đường | Bật / Tắt | Ẩn hoặc hiện tên đường |
| Phản chiếu HUD | Bật / Tắt | Lật ngang để phản chiếu kính lái |
| Xoay màn hình | Bật / Tắt | Xoay 180° theo hướng lắp mạch |
| Ngưỡng quá tốc | −10 đến +5 km/h | Bù vào giới hạn trước khi cảnh báo |
| Dịch ngang | −5 đến +5 px | Tinh chỉnh vị trí giao diện |
| Dịch dọc | −5 đến +5 px | Tinh chỉnh vị trí giao diện |

---

## Thông số phần cứng ESP32-2432S028 (CYD 2.8")

| Thành phần | Thông số / Chân kết nối |
|---|---|
| Vi điều khiển | ESP32-WROOM-32 (Dual Core 240 MHz, 4MB Flash, No PSRAM) |
| Màn hình LCD | ILI9341 2.8 inch, SPI2 @ 40 MHz, BGR |
| Độ phân giải | 320×240 Landscape (sử dụng dirty stripe xoay mềm 240×320) |
| Chân SPI LCD | MOSI: GPIO 13 \| MISO: GPIO 12 \| SCLK: GPIO 14 \| CS: GPIO 15 \| DC: GPIO 2 |
| Đèn nền (Backlight) | GPIO 21 (LEDC PWM active-high) |
| LED RGB sau lưng | Đỏ: GPIO 4 \| Xanh lá: GPIO 16 \| Xanh dương: GPIO 17 (active-low) |
| Nút bấm BOOT | GPIO 0 (active-low) |
