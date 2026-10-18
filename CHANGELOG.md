# Nhật Ký Thay Đổi (Changelog) - WazeHUD ESP32-S3 2.8" (ES3C28P / ES3N28P)

Firmware được phát hành dưới định dạng **Factory All-in-One** (đã bao gồm Bootloader, Partition Table và Firmware), nạp duy nhất 1 file tại offset **`0x0`**. Các mục `S3` bên dưới là thay đổi riêng của bản ESP32-S3; các mục `Build`/`Beta` còn lại là lịch sử kế thừa từ bản CYD của quangdo92.

> [!NOTE]
> **Lời cảm ơn & tuyên bố dự án:** bản ESP32-S3 này được phát triển với sự hỗ trợ của **AI (Claude Code, Anthropic)**, trên nền mã nguồn bản CYD của [quangdo92](https://github.com/quangdo92/waze-hud) (B15/B16, theo changelog của họ được phát triển với Google Gemini), vốn dựa trên [WazeHUD của ShindouAris](https://github.com/ShindouAris/WazeHUD) và cộng đồng [WazeMod Vietnam](https://wazemod.io.vn). Xin trân trọng cảm ơn các tác giả. Đây là bản cá nhân hóa phục vụ nghiên cứu, độ chế và sử dụng thực tế trên xe, chia sẻ mã nguồn mở (GPL-3.0) cho cộng đồng cùng sở thích.

---

## [v2.8.16-s3-r4] - 2026-10-18 (ESP32-S3)

Vẫn nền B16 (không gộp B17 của quangdo92), firmware báo số phiên bản nội bộ `2.8.15`.

- **Lane đường ở ô trái dải đáy:** khi dẫn đường có dữ liệu làn, ô trái dải đáy hiện lane, ô phải vẫn là thẻ tóm tắt (ETA, quãng đường và thời gian còn lại), nên không còn mất dải đáy. Từ 1 đến 5 lane dùng lane lớn (có thanh xanh dưới lane được chọn), từ 6 đến 12 lane dùng lane gọn. Lane được ưu tiên hơn thẻ cảnh báo thứ ba. Cách hiển thị lane là một tuỳ chọn build (lane chiếm dải đáy, lane thu nhỏ ở góc, lane ở ô trái dải đáy).
- **Thẻ hành trình tự hiện khi dẫn đường:** 10 giây mỗi 5 phút, thay cho thẻ tóm tắt ETA (thẻ 4 cột khi dải đáy rảnh, bản gọn ở ô phải khi ô trái đã có lane hoặc cảnh báo).
- **Ngày khi chạy tự do:** `dd/mm` cỡ lớn màu vàng, năm xuống dòng dưới màu xanh lá, căn giữa cột trái.

## [v2.8.16-s3-r3] - 2026-10-18 (ESP32-S3)

Vẫn nền B16 (không gộp B17 của quangdo92), firmware báo số phiên bản nội bộ `2.8.15`.

- **Cột cảnh báo bên phải gọn hơn** (thiết kế lấy từ bản T-Display): 1 biển chính (chỉ hiện khi cách dưới 800 m) và 1 hàng biển phụ (icon bên trái, khoảng cách bên phải, dưới 1,5 km). Vùng cấm vượt đang đi trong đó và cảnh báo không có khoảng cách luôn hiện. Không có biển nào trong tầm thì cột hiện thẻ so sánh tốc độ với giới hạn.
- **Dải đáy cố định hai ô** khi không có làn: ô trái là tối đa 1 thẻ cảnh báo kế tiếp (dưới 2 km), ô phải là thẻ tóm tắt (ETA, quãng đường và thời gian còn lại; hoặc quãng đường đã đi và tốc độ cao nhất khi chạy tự do). Tối đa 3 cảnh báo cùng lúc, bố cục không còn nhảy khi một biển xuất hiện hoặc đi qua.
- **Ô tốc độ xe lớn hơn:** số 36 px vẽ ở 85% trong khung co giãn theo con số (trước đây số 27 px, số 3 chữ số chỉ 13 px).
- **Rút gọn chỉ dẫn:** "Tiếp tục đi thẳng ..." thành "Đi thẳng ..." trong tên đường hiện tại và kế tiếp (ý tưởng từ B17).
- **Nhãn kẹt xe chỉ còn mức độ** (NHẸ, VỪA, NẶNG, ĐỨNG IM, ĐƯỜNG ĐÓNG), bỏ số phút (ý tưởng từ B17).
- Màn chờ: "CHỜ BLUETOOTH" thành "CHỜ KẾT NỐI".
- Thêm `tools/host_preview` để xem thử giao diện trên PC (ảnh trong `docs/`).

## [v2.8.16-s3-r2] - 2026-10-14 (ESP32-S3)

Nền B16, bỏ WiFi OTA và cảm ứng của upstream. Firmware vẫn báo số phiên bản nội bộ `2.8.15` (upstream chưa tăng số trong mã B16).

- **Cột phải khi không có cảnh báo**: hiện so sánh tốc độ với giới hạn (`DƯỚI` xanh lá / `BẰNG` vàng / `VƯỢT` đỏ, kèm số km/h chênh lệch). Tính theo giới hạn thật, không cộng ngưỡng quá tốc trong app.
- **Thẻ "HÀNH TRÌNH" có màu**: quãng đường xanh lơ, thời gian trắng, tốc độ cao nhất cam, tốc độ trung bình xanh lá; đơn vị `min`, `km`, `km/h` căn thẳng hàng với chữ số.
- **Mặc định xoay màn hình 180° tắt** cho bản S3 (USB bên trái).
- **WiFi OTA và cảm ứng** thay bằng file stub rỗng trên mọi board: cảm ứng dùng chân trùng PSRAM trên S3, OTA dùng SoftAP mật khẩu cố định và tải firmware không xác thực.
- Nạp file gộp tại `0x0` đặt lại cài đặt về mặc định.

## [v2.8.15-s3] - 2026-10-13 (ESP32-S3)

Bản đầu tiên cho board LCDWiki ESP32-S3 2.8 inch, nền B15 (HLP Schema Rev 7).

- **Cảnh báo quá tốc độ bằng giọng nói** qua loa (codec ES8311): quá tốc độ liên tục 1,5 giây thì đọc "Bạn đã quá tốc độ", lặp mỗi 4 giây. Thiếu codec thì HUD vẫn chạy.
- **Ngày/tháng** cạnh giờ ở hàng trên cùng: `dd/mm | hh:mm`.
- Đảo màu màn hình chỉnh từ app, mặc định bật cho board này.
- Chưa hỗ trợ: đèn LED RGB (WS2812), đo pin, cảm ứng, thẻ nhớ.

---

## [Build 16 (v2.8.16-b16)] - 2026-10-14

> [!NOTE]
> **Lời Cảm Ơn Tác Giả Gốc & Tuyên Bố Dự Án:**
> - Bản phát triển WazeHUD trên repository này là phiên bản cá nhân hóa, được xây dựng và tối ưu mở rộng hoàn toàn dựa trên sự hỗ trợ của **AI (Google Gemini / Antigravity)** với mục đích chính là phục vụ nhu cầu sử dụng thực tế trên xe của cá nhân mình và chia sẻ mã nguồn mở cho cộng đồng cùng tham khảo.
> - Xin trân trọng gửi lời cảm ơn chân thành và sâu sắc nhất tới **Tác giả gốc [ShindouAris (WazeHUD)](https://github.com/ShindouAris/WazeHUD)** cùng cộng đồng **[WazeMod Vietnam](https://wazemod.io.vn)** đã tạo nên nền tảng phần mềm tuyệt vời này!

> Bản phát hành B16 đánh dấu bước tiến lớn với tính năng **Cập nhật Không dây WiFi Web OTA qua màn hình cảm ứng**, làm mới toàn diện **Giao diện Chế độ Chạy Tự Do (Free Drive)** với bộ Lịch vạn niên & Đồng hồ to 30px, **Biển giới hạn tốc độ thích ứng thông minh** (to khi chạy tự do, gọn gàng khi dẫn đường), **Dải tên đường tiếp theo nổi bật trên nền đen** mở rộng gấp đôi chiều rộng hiển thị (215px), và chuẩn hóa toàn bộ file firmware vào thư mục `releases/release_b16/` với quy chuẩn đặt tên timestamp ngày_giờ.

### 📦 Danh mục Firmware Phát hành B16
Toàn bộ firmware bản B16 được lưu trữ tập trung tại thư mục [`releases/release_b16/`](file:///workspaces/waze-hud/releases/release_b16):
- **`CYD2.8-BLE-141026-b16_full.bin`**: Bản Bluetooth BLE nạp Full Flash từ offset `0x0` qua cáp USB trên máy tính (hợp nhất 1-USB và 2-USB).
- **`CYD2.8-BLE-141026-b16_app.bin`**: Bản Bluetooth BLE dùng để nạp trực tiếp không dây từ điện thoại qua Web OTA.
- **`CYD2.8-USB-141026-b16.bin`**: Bản cắm cáp USB Serial 115200 cho màn 2.8" (hợp nhất 1-USB và 2-USB, nạp Full Flash offset `0x0`).

### 🚀 Cập nhật Không Dây WiFi Web OTA qua Màn Hình Cảm Ứng
- **Thao tác 2 bước an toàn (Anti-accidental trigger):** Chạm giữ màn hình (hoặc nút BOOT trên mạch) 3 giây $\rightarrow$ hiển thị modal xác nhận đếm ngược 6 giây kèm thanh tiến trình $\rightarrow$ chạm giữ tiếp 3 giây để xác nhận vào chế độ WiFi OTA.
- **Trạm phát sóng độc lập (SoftAP Hotspot):** HUD tự động phát WiFi `WazeHUD-OTA` (mật khẩu: `12345678`), mở Web Server nạp file tại địa chỉ `http://192.168.4.1`.
- **Giao diện Dark Mode cao cấp trên trình duyệt:** Cho phép người dùng chọn trực tiếp file `.bin` (`_app.bin`) từ điện thoại (iOS / Android) và nhấn "Bắt đầu Cập nhật" ngay trên xe mà không cần tháo HUD hay cắm máy tính.
- **Đồng bộ tiến trình nạp (Real-time Progress):** Thanh tiến trình và tỷ lệ % nạp flash hiển thị trực tiếp đồng thời trên cả trình duyệt điện thoại lẫn màn hình HUD.
- **An toàn tuyệt đối:** Tự động giải phóng tài nguyên BLE khi nạp, đếm ngược 180s tự thoát về HUD nếu không có thao tác, tự động khởi động lại sau khi nạp thành công.

### 🕒 Làm mới Giao diện Chế độ Chạy Tự Do (Free Drive Mode)
- **Cột bên trái ($84\text{ px}$):** Thay thế cụm biểu tượng la bàn & chữ "CHẠY TỰ DO" cũ bằng bộ Lịch & Đồng hồ tiện ích Việt hóa đầy đủ:
  - **Dòng 1:** Thứ trong tuần (font `kTextSmall`, màu Cyan: `CHỦ NHẬT`, `THỨ HAI`, `THỨ BA`, `THỨ TƯ`, `THỨ NĂM`, `THỨ SÁU`, `THỨ BẢY`).
  - **Dòng 2:** Giờ : Phút (font `kTextLarge` số to **30px**, màu Trắng sắc nét, cập nhật theo từng phút).
  - **Dòng 3:** Đường phân cách mờ tinh tế.
  - **Dòng 4:** Ngày/Tháng/Năm (font `kTextSmall`, màu Trắng: `DD/MM/YYYY`).
  - **Dòng 5:** Nhãn trạng thái `TỰ DO` (font `kTextSmall`, màu Xanh lá).
- **Tự động ẩn đồng hồ nhỏ góc trên bên phải:** Khi chạy tự do, đồng hồ phụ ở góc cụm cảnh báo tự động ẩn để tránh hiển thị trùng lặp 2 đồng hồ trên một màn hình. Khi bắt đầu lộ trình dẫn đường Waze, đồng hồ góc phải tự động hiển thị lại.

### 🛑 Biển Giới Hạn Tốc Độ Thích Ứng Thông Minh (Adaptive Speed Sign)
- **Khi Chạy Tự do:** Không có dải tên đường tiếp theo ở trên $\rightarrow$ Giữ nguyên biển báo tốc độ **To Cực Đại** ($R = 60\text{ px}$, $r = 50\text{ px}$, tâm $y = 62$).
- **Khi Dẫn đường Waze:** Tự động chuyển sang kích thước gọn gàng ($R = 48\text{ px}$, $r = 40\text{ px}$, tâm $y = 72$) nhưng **giữ nguyên kích thước chữ số to đậm** (`kNumberSpeedLimit` size 48 cho 2 số, size 36 cho 3 số). Nhường dải đen $y = 0..26$ phía trên cho tên đường tiếp theo mà không làm lẹm hay cắt xén biển báo.
- **Chế độ Ban đêm (Night Mode):** Nền biển đen (`colors::Panel`), chữ số trắng (`colors::White`), viền đỏ (`colors::Red`) tương thích hoàn hảo và liền lạc tuyệt đối với giao diện tổng thể.

### 🏷️ Tên Đường Tiếp Theo Nổi Bật Trên Dải Nền Đen (Next Street Overlay Banner)
- Dải banner trên cùng ($y = 0..26$) phủ nền đen phẳng, chữ trắng (`colors::White`) sắc nét.
- **Mở rộng chiều rộng hiển thị khả dụng từ 105px $\rightarrow$ 215px (gấp đôi):** Hầu hết các tên đường tiếng Việt phổ biến hiển thị tĩnh trọn vẹn tức thì, không cần cuộn chữ. Chỉ các tên đường siêu dài (> 215px) mới cuộn marquee êm ái với tốc độ 30 FPS.

---

## [Build 15 (v2.8.15-b15)] - 2026-10-13

> Bản phát hành B15 tối ưu hóa toàn bộ hệ sinh thái firmware về **3 file BIN tinh gọn**, nâng cấp chuẩn cấu hình **HLP Device Config Schema Rev 7** (bổ sung Cố định độ sáng và Đảo ngược màu màn hình tích hợp 2USB & 1USB), thang màu số mét biển báo 4 cấp độ, giảm độ sáng đêm về 20%, tên đường kế tiếp trượt dưới biển báo tốc độ và sửa triệt để lỗi bootloop trên bản 3.5".

### 📦 Tinh gọn & Thống nhất còn đúng 3 Firmware Factory duy nhất (Offset 0x0)
- **`CYD2.8-BLE-131026-b15.bin`**: Bản Bluetooth BLE cho màn 2.8" (Tích hợp cả bản 2-USB và 1-USB làm 1 firmware duy nhất nhờ tính năng Đảo ngược màu).
- **`CYD2.8-USB-131026-b15.bin`**: Bản cắm cáp USB Serial 115200 cho màn 2.8" (Tích hợp cả bản 2-USB và 1-USB).
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

### 🚗 Tên đường tiếp theo trượt mượt mà 30 FPS dưới biển báo tốc độ (Smooth Marquee)
- **Hợp nhất Canvas Nguyên tử (`MainCluster` $225 \times 130\text{ px}$):** Ghép cụm Điều hướng (Maneuver) và Cụm tốc độ (SpeedCluster) thành một vùng vẽ liền mạch duy nhất. Loại bỏ hoàn toàn đường cắt vạch dọc cũ ($x = 85$ và $x = 122$) và hiện tượng xé hình/giật khựng khi chữ đi qua điểm nối.
- **Hiệu ứng trượt lọt tự nhiên:** Tên đường kế tiếp vươn dài từ $x = 5$ chạy thẳng một mạch và lọt êm ái dưới đường viền tròn của biển báo tốc độ ($R = 60$).
- **Nâng tốc độ animation lên ~30 FPS:** Giảm chu kỳ tick UI từ 80ms xuống **35ms**, giảm thời gian dịch từ 45ms/px xuống **28ms/pixel**, rút ngắn thời gian dừng đọc đầu/cuối còn **1.0s / 0.5s** giúp trải nghiệm trượt chữ cực kỳ thanh thoát và êm ái.

### 🛠️ Sửa lỗi Bootloop trên phiên bản 3.5" (ESP32-S3)
- Bật cờ `CONFIG_SPIRAM_IGNORE_NOTFOUND=y` chống crash `abort()` khi khởi động trên board dùng Quad PSRAM hoặc không có PSRAM.
- Cung cấp cơ chế fallback an toàn cho bộ đệm vẽ DMA, chống tràn RAM DMA khi NimBLE chạy đồng thời.

### 🔌 Khắc phục Triệt để Lỗi Phân làn & Cài đặt App trên bản USB Serial (`CYD2.8-USB`)
- **Bổ sung bản tin HLP `dev` opt-in phân làn:** Khắc phục lỗi Waze Mod không gửi dữ liệu làn đường (`lan`) và chỉ gửi 1 cảnh báo (`alr`) do thiếu bản tin khai báo thiết bị `dev` trên kết nối Serial. Bổ sung cơ chế phát `dev` an toàn khi khởi động và theo phiên `sess` mới, chặn đứng nguy cơ lặp vô tận `dev` ↔ `hi`.
- **Nâng bộ đệm TX UART lên 2048 byte:** Đảm bảo nạp và phát trọn bộ 11 gói schema cài đặt HLP Device Config (~1.2 KB) tức thì, loại bỏ hoàn toàn hiện tượng nghẽn luồng CPU khi Waze Mod kết nối hoặc mở giao diện cài đặt trên App.

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

