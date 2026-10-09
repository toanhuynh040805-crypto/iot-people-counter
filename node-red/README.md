# Node-RED Middleware & Dashboard 2.0

## 1. Cài Node-RED Dashboard 2.0

Mở Node-RED:

```text
http://127.0.0.1:1880
```

Vào:

```text
Menu → Manage palette → Install
```

Tìm và cài:

```text
@flowfuse/node-red-dashboard
```

Lưu ý: dùng Dashboard 2.0 của FlowFuse, không dùng gói `node-red-dashboard` cũ.

## 2. Import flow

Trong Node-RED:

```text
Menu → Import → select a file → node-red/flow.json → Import → Deploy
```

Broker mặc định trong flow:

```text
127.0.0.1:1883
```

Sau khi Deploy, mở Dashboard:

```text
http://127.0.0.1:1880/dashboard/people-counter
```

## 3. Dashboard hiện có

Dashboard hiển thị:

- IN
- OUT
- CURRENT
- MAX
- Trạng thái `CÒN CHỖ / ĐÃ ĐẦY`
- Sự kiện gần nhất
- Biểu đồ CURRENT theo thời gian
- Nút RESET bộ đếm
- Thanh chỉnh MAX từ 1 đến 20

## 4. Topic nhận dữ liệu

### Trạng thái hiện tại

```text
nhom17/people/status
```

Payload mẫu:

```json
{
  "in": 5,
  "out": 2,
  "current": 3,
  "max": 3,
  "status": "FULL"
}
```

### Sự kiện vào/ra

```text
nhom17/people/event
```

Payload mẫu:

```json
{
  "event": "IN",
  "in": 5,
  "out": 2,
  "current": 3
}
```

## 5. Lệnh gửi ngược về ESP32

### RESET

```text
Topic: nhom17/people/cmd/reset
Payload: RESET
```

### SET MAX

```text
Topic: nhom17/people/cmd/max
Payload: 5
```

## 6. Kiểm tra MQTT thủ công

Mở một PowerShell khác:

```powershell
& "C:\Program Files\mosquitto\mosquitto_sub.exe" -h 127.0.0.1 -p 1883 -t "nhom17/#" -v
```

Nếu ESP32/Wokwi kết nối đúng, bạn sẽ thấy các topic `status` và `event` xuất hiện tại đây.

## 7. Luồng end-to-end cần demo

```text
HC-SR04 A/B
   ↓
ESP32 State Machine
   ↓
Wi-Fi + MQTT
   ↓
Mosquitto Broker
   ↓
Node-RED Middleware
   ↓
Dashboard 2.0
```

Demo tối thiểu:

1. A → B → IN tăng.
2. B → A → OUT tăng.
3. CURRENT cập nhật trên Dashboard.
4. CURRENT đạt MAX → trạng thái ĐÃ ĐẦY, LED đỏ và buzzer.
5. Bấm RESET trên Dashboard → ESP32 về 0.
6. Thay đổi MAX trên Dashboard → ESP32 cập nhật MAX qua MQTT.
