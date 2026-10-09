# IoT People Counter — Đề tài 52, Nhóm 17

Hệ thống đếm khách vào/ra và giới hạn số người, triển khai theo mô hình 4 tầng IoT.

## Kiến trúc

```text
HC-SR04 A + HC-SR04 B
        ↓
ESP32
├── OLED
├── LED xanh
├── LED đỏ
└── Buzzer
        ↓ Wi-Fi + MQTT
Mosquitto Broker
        ↓
Node-RED Middleware
        ↓
Dashboard / cảnh báo
```

## Logic bắt buộc

- `A → B` = khách vào → `IN +1`
- `B → A` = khách ra → `OUT +1`
- `CURRENT = IN - OUT`
- `CURRENT >= MAX` → LED đỏ + buzzer + OLED báo `DA DAY`
- Nếu chỉ một cảm biến được kích hoạt và hết thời gian chờ → hủy lượt, không đếm.

## Cấu trúc repository

```text
iot-people-counter/
├── firmware/
│   ├── sketch.ino
│   ├── diagram.json
│   └── libraries.txt
├── node-red/
│   ├── flow.json
│   └── README.md
├── docs/
│   └── architecture.md
├── bonus-camera/
│   └── README.md
├── .gitignore
└── README.md
```

## Pin ESP32

| Thành phần | GPIO |
|---|---:|
| HC-SR04 A TRIG | 5 |
| HC-SR04 A ECHO | 18 |
| HC-SR04 B TRIG | 19 |
| HC-SR04 B ECHO | 23 |
| OLED SDA | 21 |
| OLED SCL | 22 |
| LED xanh | 25 |
| LED đỏ | 26 |
| Buzzer | 27 |

## MQTT topics

### Uplink

`nhom17/people/status`

```json
{
  "in": 5,
  "out": 2,
  "current": 3,
  "max": 3,
  "status": "FULL"
}
```

`nhom17/people/event`

```json
{
  "event": "IN",
  "in": 5,
  "out": 2,
  "current": 3
}
```

### Downlink

Reset bộ đếm:

```text
Topic: nhom17/people/cmd/reset
Payload: RESET
```

Đổi sức chứa tối đa:

```text
Topic: nhom17/people/cmd/max
Payload: 5
```

## Chạy mô phỏng

### 1. Wokwi

Mở thư mục `firmware/` bằng Wokwi hoặc Wokwi for VS Code.

Trong firmware mặc định:

```text
Wi-Fi: Wokwi-GUEST
MQTT: host.wokwi.internal:1883
```

`host.wokwi.internal` phù hợp khi dùng Wokwi for VS Code/Private IoT Gateway để ESP32 mô phỏng truy cập Mosquitto đang chạy trên máy tính.

### 2. Mosquitto

Kiểm tra mọi topic của nhóm:

```powershell
& "C:\Program Files\mosquitto\mosquitto_sub.exe" -h 127.0.0.1 -p 1883 -t "nhom17/#" -v
```

### 3. Node-RED

Import file:

```text
node-red/flow.json
```

Broker Node-RED mặc định:

```text
127.0.0.1:1883
```

## Kịch bản demo nghiệm thu

1. Ban đầu `IN=0`, `OUT=0`, `CURRENT=0`.
2. A trước B → `IN +1`.
3. B trước A → `OUT +1` nếu `CURRENT > 0`.
4. Chỉ A → timeout → không đếm.
5. Chỉ B → timeout → không đếm.
6. Lặp A→B đến khi `CURRENT = MAX`.
7. LED đỏ + buzzer + OLED báo đầy.
8. Cho một người đi ra B→A → `CURRENT` giảm → LED xanh trở lại.
9. Kiểm tra Node-RED nhận `status` và `event`.
10. Node-RED gửi `RESET` hoặc `SET MAX` về ESP32.

## Bonus

Thư mục `bonus-camera/` dành cho bản mở rộng Camera + YOLO tracking nhiều người. Đây là phần bonus, không thay thế yêu cầu bắt buộc 2 cảm biến IR/Ultrasonic.
