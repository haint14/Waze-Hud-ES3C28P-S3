# Nhật Ký Thay Đổi (Changelog) - WazeHUD CYD 2.8"

Dự án WazeHUD tối ưu cho màn hình ESP32-2432S028 (CYD 2.8 inch). Toàn bộ firmware được phát hành dưới định dạng **Factory All-in-One** (đã bao gồm Bootloader, Partition Table và Firmware), nạp duy nhất 1 file tại offset **`0x0`**.

---

## [Build 15 (v2.8.15-b15)] - 2026-10-13

> Bản phát hành B15 tối ưu hóa toàn bộ hệ sinh thái firmware về **3 file BIN tinh gọn**, nâng cấp chuẩn cấu hình **HLP Device Config Schema Rev 7** (bổ sung Cố định độ sáng và Đảo ngược màu màn hình tích hợp 2USB & 1USB), thang màu số mét biển báo 4 cấp độ, giảm độ sáng đêm về 20%, tên đường kế tiếp trượt dưới biển báo tốc độ và sửa triệt để lỗi bootloop trên bản 3.5".

### 📦 Tinh gọn & Thống nhất còn đúng 3 Firmware Factory duy nhất (Offset 0x0)
- **`CYD2.8-BLE-131026-b15.bin`**: Bản Bluetooth BLE cho màn 2.8" (Tích hợp cả bản 2-USB và 1-USB làm 1 firmware duy nhất nhờ tính năng Đảo ngược màu).
- **`CYD2.8-Serial-131026-b15.bin`**: Bản cắm cáp USB Serial 115200 cho màn 2.8" (Tích hợp cả bản 2-USB và 1-USB).
- **`CYD3.5-BLE-131026-b15.bin`**: Bản Bluetooth BLE cho module ESP32-S3 3.5" ST77922 QSPI 480×320 (đã sửa dứt điểm lỗi bootloop).

### ⚙️ Nâng cấp Chuẩn Cài đặt HLP Device Config Schema Rev 7 (9 mục)
- **Mục 1 (`fixed_brightness` - Cố định độ sáng, toggle):**
  - Khi **ON**: Mở khóa Mục 2 (`brightness`) cho phép chỉnh độ sáng tùy ý từ App, độ sáng này sẽ giữ cố định và ưu tiên cao hơn chế độ tự động theo giờ.
  - Khi **OFF**: Khóa Mục 2 và tự động hoàn trả độ sáng theo khung giờ thực tế (100% ban ngày / 20% ban đêm).
- **Mục 4 (`invert_color` - Đảo ngược màu, toggle):**
  - Đảo cực tính màn hình LCD phần cứng (`INVOFF` / `INVON`), cho phép người dùng dùng mạch CYD 1-USB hay 2-USB đều hiển thị chuẩn màu trên cùng một file firmware.

### 🌙 Giảm độ sáng ban đêm về 20%
- Trong khung giờ từ **19:00 đến 05:00 sáng**, HUD tự động hạ độ sáng về mức **20%** (thay vì 30% trước đây) giúp chống chói mắt tối đa khi lái xe ban đêm.

### 🎨 Thang màu khoảng cách biển báo 4 cấp độ trực quan
- **`> 600m`**: Màu xanh lá cây (`Green`) - Khoảng cách còn xa, trạng thái an toàn.
- **`300m – 600m`**: Màu trắng (`White`) - Đang tiến lại gần cảnh báo.
- **`100m – 300m`**: Màu cam vàng (`Amber`) - Cảnh giác, sẵn sàng giảm tốc độ.
- **`< 100m`**: Màu đỏ tươi (`Red`) - Rất gần vị trí cảnh báo / điểm bắn tốc độ.

### 🚗 Thiết kế Tên đường tiếp theo trượt lọt dưới biển báo tốc độ
- Độ dài tên đường kế tiếp mở rộng từ 75px lên **105px**, phần đuôi tên đường vươn sang góc màn hình và trượt mượt mà bên dưới viền biển báo tốc độ tròn với lớp mặt nạ che bo tròn sắc nét.

### 🛠️ Sửa lỗi Bootloop trên phiên bản 3.5" (ESP32-S3)
- Bật cờ `CONFIG_SPIRAM_IGNORE_NOTFOUND=y` chống crash `abort()` khi khởi động trên board dùng Quad PSRAM hoặc không có PSRAM.
- Cung cấp cơ chế fallback an toàn cho bộ đệm vẽ DMA, chống tràn RAM DMA khi NimBLE chạy đồng thời.

---

## [Build 14 (v2.8.14-b14)] - 2026-10-08

> Bản phát hành B14 nâng cấp toàn diện hệ thống hiển thị mũi tên làn đường chếch 45° (Slight Left / Right) với đường cong uốn lượn mềm mại, căn giữa chuẩn xác, cơ chế render phân lớp Z-Order và khắc phục triệt để lỗi bắt tay giao thức/cài đặt trên ứng dụng điện thoại.

### ⚡ Khắc phục lỗi Giao thức & Cài đặt App (Hotfix Protocol & App Settings - ĐẶT LÊN ĐẦU)
- **Khắc phục triệt để lỗi App không chỉnh được cài đặt & Làn đường không nhảy:**
  - **Xóa bỏ vòng lặp vô tận Handshake `dev` ↔ `hi`:** Loại bỏ hoàn toàn khối kiểm tra sai trường trong bản tin `hi`, chấm dứt tình trạng HUD và app liên tục gửi lại khai báo khiến dữ liệu dẫn đường (`lan` và cảnh báo) bị nghẽn và xóa trắng liên tục.
  - **Làn đường nhảy tức thì:** Khôi phục luồng nhận dữ liệu điều hướng mượt mà, màn hình chuyển sang cụm mũi tên chỉ dẫn làn ngay khi có thông tin từ Waze, không còn bị đơ ở ETA.
  - **Cài đặt trên App phản hồi tức thì:** Xóa bỏ độ trễ thừa trong lệnh gửi JSON cấu hình và tối ưu hóa hàng đợi BLE giúp app Waze Mod gửi/lưu cài đặt (độ sáng, giao diện Ngày/Đêm, xoay màn hình...) phản hồi ngay lập tức, không còn báo lỗi transaction timeout.
  - **Đồng bộ trên trọn bộ 5 biến thể firmware** (CYD 2.8" 2-USB BLE, 1-USB BLE, 2-USB Serial, 1-USB Serial và CYD 3.5").

### 🏹 Cải tiến Đồ họa Mũi tên Làn đường (Lane Graphics Redesign)
- **Thân mũi tên rẽ chếch 45° uốn cong mềm mại (Smooth Curved Arrows):** Thay thế các đường gấp khúc thô cứng bằng đường cong uốn lượn nhẹ tự nhiên (Quad Curve) theo phong cách Waze; thân đi thẳng giữ trục đứng cân đối.
- **Đầu mũi tên vát Chevron thanh thoát:** Thiết kế cánh mũi tên góc $\pm 48^\circ$ kèm đuôi vát khí động học (Chevron notch), ôm khít đỉnh cung cong.
- **Căn giữa chuẩn xác:** Làn đơn rẽ chếch trái (`0x02`) và chếch phải (`0x10`) được căn chỉnh đối trọng trục gốc, nằm cân đối tuyệt đối ngay tâm làn.
- **Tách biệt tổ hợp Rẽ chếch 45° + Vuông góc 90° (`0x30`, `0x06`):** Nhánh 45° vươn lên đỉnh cao nhất, nhánh rẽ 90° tách ra từ bụng dưới, cách nhau 30px, không còn bị chen chúc.
- **Cơ chế render Two-Pass phân lớp Z-Order:** Vẽ toàn bộ các nhánh không chọn (màu xám) xuống dưới nền trước, sau đó vẽ đè nhánh được chọn (màu trắng) lên trên. Loại bỏ hoàn toàn việc màu xám đè lên hoặc cắn khuyết nhánh màu trắng tại các điểm giao cắt.
- **Hỗ trợ đầy đủ các tổ hợp làn phức tạp:** Làn Thẳng + Chếch (`0x03`, `0x11`), Chạc đôi chữ Y (`0x12`), Làn 3 hướng (`0x13`, `0x25`).

---

## [Beta 8 (v2.8.8)] - 2026-10-06

> Bản cập nhật lớn toàn diện tối ưu hóa trải nghiệm lái xe thực tế, nâng cấp cụm tốc độ taplo và chế độ chạy tự do so với bản Beta 7 (v2.8.2 phát hành ngày 05/10/2026).

### 🛑 Thiết kế lại cụm tốc độ Taplo hiện đại (Redesigned Speed Cluster)
- **Biển báo giới hạn tốc độ cỡ đại:** Phóng to vòng tròn biển báo tốc độ lên mức tối đa với bán kính **$R = 60\text{ px}$** (đường kính **$120\text{ px}$**), viền đỏ 100% đỏ cờ dày $10\text{ px}$, số bên trong to đậm nét giúp tài xế quan sát cực kỳ rõ ràng từ khoảng cách xa trên taplo.
- **Bỏ chữ "km/h" rườm rà:** Loại bỏ chữ km/h dưới số tốc độ giúp giao diện thoáng đãng, hiện đại, loại bỏ chi tiết thừa gây phân tâm khi lái xe.
- **Huy hiệu tốc độ xe thông minh ($38 \times 28\text{ px}$):** Tốc độ thực tế của xe được thu nhỏ số lại cho tinh tế và đặt gọn gàng trong huy hiệu bo góc tại góc dưới bên phải biển giới hạn ($x=100, y=100$).
- **Khắc phục lỗi tràn viền khi chạy tốc độ cao (> 100 km/h):** Khi xe chạy trên cao tốc ở tốc độ 3 chữ số (105, 115, 120 km/h), chữ số tự động chuyển sang cỡ vừa vặn nằm hoàn toàn bên trong huy hiệu, tuyệt đối không bao giờ lấn sang cột cảnh báo bên phải hay che mất đồng hồ.

### 🧭 Chế độ Chạy Tự Do hiển thị tối đa cảnh báo (Smart Free Drive Mode)
- **Tận dụng 100% dải đáy màn hình ($320 \times 82\text{ px}$):** Khi không cài lộ trình dẫn đường (chế độ chạy tự do), dải băng phía dưới tự động hiển thị đồng thời lên đến **4 thẻ cảnh báo độc lập** (Camera phạt nguội, Camera đèn đỏ, Điểm bắn tốc độ, Cảnh báo giao thông...).
- **Cảnh báo Quá Tốc Độ kích hoạt ở mọi chế độ lái:** Gỡ bỏ giới hạn chỉ cảnh báo khi có lộ trình dẫn đường. Giờ đây khi xe vượt tốc độ giới hạn (cả khi dẫn đường và khi chạy tự do), viền đỏ nhấp nháy toàn màn hình (2 Hz) và huy hiệu tốc độ xe chuyển sang nền đỏ rực cảnh báo sẽ luôn hoạt động chuẩn xác.

### 🎨 Tinh chỉnh độ nét đồ họa (Pixel-Perfect Graphics)
- **Khắc phục lỗi mờ/vỡ nét mũi tên phân làn:** Làm mượt các góc cua 90 độ và góc cua chữ U của mũi tên dẫn đường bằng thuật toán điền đầy hình tròn (fillCircle) tại các khớp nối. Các nét vẽ giờ đây sắc nét, mạch lạc và không còn hiện tượng răng cưa trên màn hình 320x240.
- **Biểu tượng Maneuver sắc nét:** Đảm bảo toàn bộ hệ thống biểu tượng chỉ đường (rẽ trái, phải, vòng xuyến) render chính xác từng pixel, mang lại trải nghiệm thị giác cao cấp.

### 📡 Ổn định kết nối & Hiệu năng hệ thống
- **Tương thích hoàn hảo WazeMod Android:** Cập nhật logic xử lý kết nối, đảm bảo kết nối Bluetooth BLE và USB Serial luôn duy trì liên tục và ổn định. Khắc phục triệt để hiện tượng thiết bị tự ngắt kết nối sau khi ghép nối thành công.

### 📦 Phát hành đồng bộ 4 biến thể phần cứng CYD 2.8" (Factory Offset 0x0)
1. **`waze_hud_cyd_28_factory.bin`**: Dành cho CYD 2.8" (2 Cổng USB) kết nối Bluetooth BLE.
2. **`waze_hud_cyd_28_1usb_factory.bin`**: Dành cho CYD 2.8" (1 Cổng Micro USB) kết nối Bluetooth BLE (đã cấu hình đảo màu & xoay chuẩn mạch 1 cổng).
3. **`waze_hud_cyd_28_usb_factory.bin`**: Dành cho CYD 2.8" (2 Cổng USB) cắm cáp USB Serial CH340.
4. **`waze_hud_cyd_28_1usb_serial_factory.bin`**: Dành cho CYD 2.8" (1 Cổng Micro USB) cắm cáp USB Serial UART.

---

## [v2.8.2] - 2026-10-05

- **Cảnh báo Đèn giao thông mới (Traffic Light Alert - Mã 75):** Bổ sung `AlertKind::TrafficLight = 75` đồng bộ với WazeMod APK, thiết kế icon pill-shape 3 bóng Đỏ - Vàng - Xanh.
- **Lưu giữ làn đường thông minh theo vận tốc (Speed-Aware Lane Retention):** Tạm dừng đếm ngược 15s khi tốc độ < 10 km/h để giữ nguyên chỉ dẫn làn khi dừng đèn đỏ hoặc kẹt xe.

