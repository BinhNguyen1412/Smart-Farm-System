Hệ thống IoT điều khiển và giám sát nông trại dựa trên ESP32, với các cảm biến (DHT11, MQ135, cảm biến mực nước) và khả năng giao tiếp với một máy chủ (server Raspberry Pi). Dưới đây là giải thích chi tiết:

1. Kết nối Wi-Fi
connectWiFi():
Kết nối ESP32 với mạng Wi-Fi sử dụng SSID và mật khẩu được cung cấp.
Sau khi kết nối thành công, in địa chỉ IP của ESP32.

2. Cảm biến
DHT11 (Nhiệt độ và độ ẩm):
Đọc nhiệt độ và độ ẩm từ cảm biến DHT11 được kết nối qua chân GPIO 4.
MQ135 (Chất lượng không khí):
Đọc giá trị tương tự (analog) từ MQ135 qua chân GPIO 34.
Cảm biến mực nước:
Đọc giá trị tương tự từ cảm biến mực nước qua chân GPIO 33 để giám sát trạng thái mực nước.

3. Điều khiển GPIO
GPIO 22 (Relay/LED):
Có thể được bật/tắt qua các lệnh HTTP từ client.
Tự động tắt sau 5 giây nếu không nhận lệnh "off" từ client.

4. Gửi dữ liệu đến máy chủ
sendDataToServer():
Dữ liệu từ các cảm biến (nhiệt độ, độ ẩm, chất lượng không khí, mực nước) được gửi đến máy chủ Raspberry Pi qua HTTP POST.
Dữ liệu được định dạng dưới dạng JSON sử dụng thư viện ArduinoJson.

5. Server HTTP
server.on("/"):
Trả về một thông báo "ESP32 is running!" để kiểm tra hoạt động của ESP32.
server.on("/control"):
Điều khiển trạng thái GPIO 22 thông qua các tham số state (on hoặc off) từ URL.

6. Xử lý mực nước
handleWaterLevel():
Tự động bật GPIO 22 nếu mực nước cao và không trong trạng thái "controlState = ON".
checkAutoTurnOff():
Tự động tắt GPIO 22 sau 5 giây nếu không có tín hiệu "off".

Tóm tắt chức năng chính
Kết nối Wi-Fi và giám sát cảm biến:
ESP32 đọc dữ liệu từ các cảm biến.
Gửi dữ liệu đến server Raspberry Pi:
Thực hiện qua HTTP POST.
Điều khiển relay/LED qua HTTP server:
Điều khiển bật/tắt GPIO thông qua giao diện web hoặc tự động theo logic.
Tích hợp tính năng tự động:
GPIO tự động bật/tắt dựa vào mực nước hoặc timeout.

Ứng dụng
Hệ thống giám sát môi trường (nhiệt độ, độ ẩm, chất lượng không khí).
Điều khiển thiết bị từ xa qua web server.
Tự động hóa bật/tắt thiết bị dựa trên cảm biến.