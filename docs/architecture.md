# Kiến trúc 4 tầng — Đề tài 52

## Tầng 1 — Sensing / Physical

Thiết bị:

- 2 × HC-SR04
- ESP32
- OLED SSD1306
- LED xanh
- LED đỏ
- Buzzer

Logic cảm biến:

```text
A → B = IN
B → A = OUT
CURRENT = IN - OUT
```

Cảnh báo tại thiết bị:

```text
CURRENT < MAX
→ LED xanh
→ buzzer tắt
→ OLED còn chỗ

CURRENT >= MAX
→ LED đỏ
→ buzzer kêu
→ OLED báo DA DAY
```

## Tầng 2 — Network

Công nghệ:

- Wi-Fi
- MQTT

ESP32 publish:

```text
nhom17/people/status
nhom17/people/event
```

ESP32 subscribe:

```text
nhom17/people/cmd/reset
nhom17/people/cmd/max
```

## Tầng 3 — Middleware

Thành phần:

- Mosquitto MQTT Broker
- Node-RED

Nhiệm vụ:

- nhận MQTT
- parse JSON
- xử lý trạng thái
- kiểm tra sức chứa
- tạo cảnh báo
- chuyển dữ liệu cho tầng Application
- gửi lệnh downlink về ESP32

## Tầng 4 — Application / Platform

Node-RED Dashboard hiển thị:

- IN
- OUT
- CURRENT
- MAX
- AVAILABLE / FULL
- cảnh báo Over Capacity
- biểu đồ CURRENT theo thời gian
- lịch sử sự kiện

Điều khiển từ Dashboard:

- Reset Counter
- Set Max

## Luồng end-to-end

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
Dashboard
```

Chiều điều khiển ngược:

```text
Dashboard
    ↓
Node-RED
    ↓ MQTT
ESP32
    ↓
RESET / SET MAX
```

## Kịch bản nghiệm thu

1. A→B: tăng IN.
2. B→A: tăng OUT.
3. CURRENT không âm.
4. Chỉ A hoặc chỉ B: timeout, không đếm.
5. CURRENT đạt MAX: LED đỏ + buzzer + OLED cảnh báo.
6. Node-RED nhận được MQTT status/event.
7. Dashboard hiển thị occupancy.
8. Node-RED gửi RESET / SET MAX xuống ESP32.
