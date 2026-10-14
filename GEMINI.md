# Quy tắc Bắt buộc Dự án Waze HUD (ESP32 CYD)

## 1. Quy trình Làm việc & Rà soát Lỗi Trước Khi Sửa Code (`@rules`)

1. **Lên kịch bản (Kế hoạch) trước khi chỉnh sửa bất kỳ file nào**:
   - Tuyệt đối không tự ý sửa code ngay khi nhận yêu cầu.
   - Phải phân tích nguyên nhân, liệt kê các file bị ảnh hưởng, giải pháp dự kiến và trình bày kịch bản/kế hoạch rõ ràng để người dùng duyệt trước khi thực thi.
2. **Kiểm tra, rà soát lỗi và xung đột trước khi hoàn chỉnh kế hoạch**:
   - Trước khi trình bày kế hoạch cho người dùng, bắt buộc phải rà soát chéo toàn bộ luồng dữ liệu có liên quan (protocol, task timing, bộ nhớ, giao diện, tương thích giữa các nhánh BLE và Serial) để phát hiện sớm xung đột hoặc tác dụng phụ (regression).
   - Không đề xuất các thay đổi tạo ra giả định sai về giao thức hoặc gây nghẽn (blocking) luồng xử lý chính.
3. **Thử nghiệm trên 1 nhánh mục tiêu trước khi nhân rộng ra 5 nhánh**:
   - Mọi tính năng mới hoặc thay đổi giao diện/protocol chỉ được triển khai và kiểm chứng trên 1 nhánh chính trước (mặc định là `CYD2.8-V2USB` tại `/workspaces/waze-hud`).
   - Chỉ khi người dùng xác nhận đạt yêu cầu hoặc yêu cầu phát hành đồng loạt thì mới đồng bộ sang 4 nhánh còn lại (`CYD2.8-V1USB`, `2.8-in-CYD-USB`, `CYD2.8-V1USB-Serial`, `CYD3.5`), và phải kiểm tra kỹ sự khác biệt phần cứng/transport của từng nhánh khi merge.

---

## 2. Cơ chế Khai báo & Tương tác Cài đặt giữa HUD và App (`HLP Device Config`)

HUD là bên **sở hữu và định nghĩa danh mục cài đặt**, khai báo lên App điện thoại để App dựng giao diện điều khiển và gửi thay đổi ngược về HUD:

1. **Chiều từ HUD lên App (Khai báo Schema & Giá trị hiện tại)**:
   - Khi kết nối vật lý (BLE/Serial), HUD gửi bản tin `dev` khai báo các trường dữ liệu cần nhận (`want.fields`, bao gồm `"lan"`, `"alrs"`, v.v.).
   - Khi App phản hồi bản tin bắt tay `hi` có chứa `"caps": ["device_config"]`:
     - HUD gọi `DeviceConfigManager::handleProducerHello(root)`.
     - HUD tự động phát chuỗi khai báo cấu hình gồm:
       1. `cfg_begin`: chứa phiên bản schema (`rev = kSchemaRevision`, hiện tại là `7`) và tổng số mục cài đặt (`count = kItemCount`, hiện tại là `9`).
       2. `9` gói `cfg_item`: khai báo từng mục cài đặt (`id`, `kind`, `label`, `min`/`max`/`step` hoặc `options`, và giá trị hiện tại `value` đang lưu trong NVS của HUD):
          - `1`: `fixed_brightness` (Cố định độ sáng - `toggle`: Khi ON cho phép chỉnh mục 2 và ưu tiên cao hơn sáng tự động theo giờ; khi OFF khóa mục 2 và quay về độ sáng theo giờ `100%` ngày / `20%` đêm)
          - `2`: `brightness` (Độ sáng màn hình `10–100%`, bước `5%`)
          - `3`: `theme` (Giao diện: Tự động / Ban ngày / Ban đêm)
          - `4`: `invert_color` (Đảo ngược màu - `toggle`: Đảo cực tính màu LCD `INVON`/`INVOFF`, tích hợp 2 bản màn hình 2USB và 1USB làm 1)
          - `5`: `mirror_hud` (Phản chiếu HUD)
          - `6`: `rotate_display` (Xoay màn hình 180°)
          - `7`: `overspeed_offset` (Ngưỡng cảnh báo tốc độ `-10..+5 km/h`)
          - `8`: `offset_x` (Dịch ngang X `-5..+5 px`)
          - `9`: `offset_y` (Dịch dọc Y `-5..+5 px`)
       3. `cfg_end`: kết thúc khai báo với `rev` tương ứng.
2. **Chiều từ App ngược về HUD (Tương tác thay đổi cài đặt)**:
   - Khi người dùng chỉnh cài đặt trên App, App gửi một transaction nguyên tử (atomic transaction) về HUD:
     1. `cfg_set_begin`: mở phiên cập nhật với mã giao dịch `tx` và `count = 9`.
     2. `cfg_set`: gửi từng cặp `id` và giá trị mới `val` (HUD lưu tạm vào `pending_.draft` và đánh dấu bit vào `receivedMask`).
     3. `cfg_set_commit`: yêu cầu áp dụng giao dịch `tx`.
   - Khi nhận `cfg_set_commit`, HUD kiểm tra đủ mặt nạ bit (`receivedMask == (1 << kItemCount) - 1`), lưu vào NVS (`hud_cfg`), tăng `revision`, cập nhật `HudStateStore` để `HudRenderer` áp dụng ngay (độ sáng, lật gương, xoay màn hình, giao diện...), và gửi `cfg_ack` (`ok: true`, `tx`, `rev`) về App.
3. **Các nguyên tắc bất khả xâm phạm của giao thức HLP**:
   - **Không bao giờ gửi lại `dev` khi nhận `hi`**: Bản tin `hi` từ App không chứa trường `fields`. Việc gọi `sendDeviceDeclaration()` trong nhánh xử lý `hi` sẽ gây vòng lặp `dev` $\leftrightarrow$ `hi` vô tận, làm reset session liên tục, xóa trắng `HudState` (mất phân làn) và hủy giao dịch cài đặt.
   - **Không chèn `vTaskDelay` lớn trong `DeviceConfigManager::sendJson`**: Việc phát 11 bản tin `cfg_*` nằm trên cùng `hlp_task`; chèn delay tại đây sẽ làm nghẽn hàng đợi nhận `cfg_set_*` và `state`. Việc chia nhỏ gói BLE (20 byte/chunk, nghỉ 3ms giữa các chunk) do tầng `BleTransport::sendLine` đảm nhiệm.
   - Khi thêm/bớt bất kỳ mục cài đặt nào trên HUD, phải cập nhật đồng bộ: `kSchemaRevision`, `kItemCount`, enum `ItemId`, `sendItem()`, `applyItemValue()`, và `saveToNvs()` / `loadFromNvs()`.

---

## 3. Quy tắc Ưu tiên & Hiển thị Lớn cho Biển báo Cảnh báo (`Priority Alert Signs`)

Hệ thống biển báo cảnh báo trên HUD phải tuân thủ nghiêm ngặt thứ tự ưu tiên và quy tắc kích thước hiển thị (biển ưu tiên hiển thị **LỚN / Dominant**, biển phụ hiển thị **NHỎ / Secondary**):

### 3.1. Phân hạng ưu tiên biển báo
1. **Tại bộ giải mã (`HlpDecoder` - chuẩn hóa danh sách `alrs`)**:
   - **Mức 1 (Điểm 100 - Cao nhất)**: Biển hạ tốc độ phía trước (`AlertKind::SpeedDrop`). Nếu có nhiều biển hạ tốc độ ở cùng một khoảng cách, ưu tiên hiển thị biển có **giới hạn tốc độ cao hơn trước** (`speedDropTieComesBefore`).
   - **Mức 2 (Điểm 90)**: Toàn bộ nhóm **Biển cấm & Hiệu lệnh làn đường** (`isProhibitionAlert`: Cấm vượt `NoPassing`, Cấm ô tô/xe máy, Cấm rẽ trái/phải/quay đầu/đi thẳng và các tổ hợp cấm, Đường cấm, Đường 1 chiều, Làn bắt buộc, Hết lệnh cấm...).
   - **Mức 3 (Điểm 70)**: Cấm đường (`RoadClosed`), Tai nạn (`Accident`), Nguy hiểm (`Hazard`).
   - **Mức 4 (Điểm 60 - Khi ở gần $\le 250\text{m}$)**: Đèn đỏ (`TrafficLight`) và các loại Camera phạt nguội (`isCameraAlert`: Cam tốc độ, Cam đèn đỏ, Cam điện thoại, Cam dây an toàn, Cam khoảng cách, Cam làn bus...) khi khoảng cách $\le 250\text{m}$.
   - **Mức 5 (Điểm 50)**: Ùn tắc giao thông (`TrafficJam`).
   - **Mức 6 (Điểm 30)**: Cảnh sát (`Police`) và các biển cảnh báo giao thông/thời tiết/chướng ngại vật khác.
   - **Mức 7 (Điểm 20 / 10 - Khi còn xa $> 250\text{m}$)**: Đèn đỏ ở xa $> 250\text{m}$ (điểm 20) và Camera ở xa $> 250\text{m}$ (điểm 10).
   - *Quy tắc phụ khi bằng điểm*: Biển nào có **khoảng cách gần hơn (`distanceM` nhỏ hơn)** sẽ được xếp trước.

2. **Tại bộ kết xuất (`HudRenderer::collectSortedAlerts`)**:
   - Gom cảnh báo khu vực cấm vượt đang hiệu lực (`noPassingZone`), `nearestAlert` và `upcomingAlerts` (loại bỏ trùng lặp), sau đó sắp xếp theo 3 hạng lớn để chọn biển VIP hiển thị to nhất:
     - **Hạng 1 (Ưu tiên số 1)**: Tất cả các **Biển cấm & Hạ tốc độ** (`NoPassing`, `SpeedDrop`, `NoCar`, `NoMotorcycle`, `NoLeftTurn`, `NoRightTurn`, `NoUTurn`, `NoStraight`, `ProhibitedRoad`, và toàn bộ tổ hợp biển cấm rẽ/quay đầu).
     - **Hạng 2 (Ưu tiên số 2)**: Nhóm **Camera giám sát, Đèn đỏ, CSGT** (`SpeedCamera`, `RedLightCamera`, `TrafficLight`, `Police`, `PhoneCamera`, `SeatbeltCamera`, `DistanceCamera`, `BusLaneCamera`, `NoiseCamera`, `StopSignCamera`, `DummyCamera`).
     - **Hạng 3 (Ưu tiên số 3)**: Các cảnh báo chướng ngại vật, ùn tắc, nguy hiểm, thời tiết còn lại.
   - Trong cùng một hạng, biển nào **gần xe hơn (`distanceM` nhỏ hơn)** được ưu tiên đứng trước.

### 3.2. Quy tắc Kích thước Hiển thị (Biển Ưu tiên Lớn vs Biển Phụ Nhỏ)
- **Biển báo ưu tiên số 1 (`allAlerts[0]` - Vị trí VIP tại cụm bên phải `layout::Alerts`)**:
  - Luôn được vẽ với cờ **`dominant = true`** (sử dụng bộ asset độ phân giải cao `entry.large` / `SpeedSignContext::AlertLarge` và scale theo `iconSize = radius * 2`).
  - **Khi không có phân làn (`state.laneCount == 0`)**:
    - Cụm bên phải (`layout::Alerts`) dành toàn bộ không gian hiển thị **DUY NHẤT 1 biển ưu tiên cao nhất (`allAlerts[0]`)** với kích thước **CỰC LỚN** (`iconRadius = 20` trên CYD 2.8" / `22` trên CYD 3.5", font khoảng cách `kTextMedium`).
    - Các cảnh báo tiếp theo (`allAlerts[1..4]`) được đẩy xuống **Thanh đáy mở rộng (`layout::Guidance`)** dưới dạng các thẻ (card) từ 1 đến 4 ô và **tuyệt đối không lặp lại `allAlerts[0]`**:
      - Nếu thanh đáy chỉ có 1 cảnh báo phụ (`alertCount == 1`): hiển thị thẻ rộng toàn màn hình với icon lớn (`radius = 20`, `dominant = true`) và chữ lớn.
      - Nếu thanh đáy có 2–4 cảnh báo phụ: chia thành 2, 3 hoặc 4 thẻ nhỏ ngang nhau; trong đó thẻ đầu tiên (`i == 0`) vẫn được ưu tiên `dominant = true` với bán kính thu gọn (`16`, `14`, `13`) để giữ độ sắc nét.
  - **Khi đang có phân làn (`state.laneCount > 0`)**:
    - Thanh đáy (`layout::Guidance`) bận hiển thị mũi tên phân làn.
    - Cụm bên phải (`layout::Alerts`) tự động chuyển sang bố cục **2 tầng (Trên To - Dưới Nhỏ)**:
      - **Tầng trên (Biển ưu tiên 1 - `allAlerts[0]`)**: Hiển thị **LỚN** (`dominant = true`, `iconRadius = 20`/`22` nếu đứng một mình, hoặc `16`/`18` nếu có biển thứ 2).
      - **Tầng dưới (Biển ưu tiên 2 - `allAlerts[1]`)**: Hiển thị **NHỎ** (`dominant = false`, dùng asset `entry.small` / `SpeedSignContext::AlertSmall`, `iconRadius = 13`) ngay bên dưới để tài xế nắm bắt trước cảnh báo kế tiếp mà không làm lu mờ biển ưu tiên số 1.

---

## 4. Quy tắc Ghi nhận Tác giả Gốc & Tuyên bố Dự án trong Mọi Bản Phát Hành (`Attribution & AI Disclaimer`)

Trong **tất cả** các bản phát hành (GitHub Releases, `CHANGELOG.md`, `README.md`, bài giới thiệu cộng đồng):

1. **Luôn gửi lời cảm ơn & dẫn link tới Tác giả gốc và Cộng đồng**:
   - Tác giả gốc dự án WazeHUD: **[ShindouAris (WazeHUD)](https://github.com/ShindouAris/WazeHUD)**.
   - Cộng đồng hỗ trợ và phát triển WazeMod: **[WazeMod Vietnam](https://wazemod.io.vn)**.
   - Luôn tôn trọng bản quyền, ghi rõ nguồn gốc và gửi lời tri ân chân thành tới tác giả gốc đã tạo dựng nền tảng ban đầu tuyệt vời.

2. **Tuyên bố minh bạch về việc phát triển bằng AI & Mục đích Cá nhân**:
   - Bản phát triển WazeHUD trên kho mã nguồn này là phiên bản cá nhân hóa, được xây dựng và tối ưu hóa mở rộng hoàn toàn dựa trên sự hỗ trợ của **AI** (Google Gemini / Antigravity pair programming).
   - Mục đích chính của dự án là phục vụ nhu cầu nghiên cứu, độ chế và sử dụng thực tế trên xe của **cá nhân tác giả**, đồng thời chia sẻ công khai mã nguồn mở cho các anh em cộng đồng có cùng sở thích tham khảo.
