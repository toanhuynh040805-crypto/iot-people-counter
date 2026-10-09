# IoT People Counter — Đề tài 52, Nhóm 17

Hệ thống đếm khách vào/ra và giới hạn số người, triển khai theo mô hình 4 tầng IoT và bám đúng yêu cầu A→B/B→A của đề tài.

## Kiến trúc

```text
HC-SR04 A + HC-SR04 B
        ↓
ESP32 State Machine
├── LCD 16x2 I2C
├── LED xanh
├── LED đỏ
└── Buzzer
        ↓ Wi-Fi + MQTT
Mosquitto Broker
        ↓
Node-RED Middleware
        ↓
Node-RED Dashboard 2.0
```

## Logic bắt buộc

- `A → B` = khách vào → `IN +1`
- `B → A` = khách ra → `OUT +1`
- `CURRENT = IN - OUT`
- `CURRENT >= MAX` → LED đỏ + buzzer + LCD báo `FULL`
- Nếu chỉ một cảm biến được kích hoạt và hết thời gian chờ → hủy lượt, không đếm.

## Cấu trúc repository

```text
iot-people-counter/
├── platformio.ini
├── wokwi.toml
├── diagram.json
├── src/
│   └── main.cpp
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

`src/main.cpp` là code chính khi chạy bằng PlatformIO + Wokwi for VS Code.

## Pin ESP32

| Thành phần | GPIO |
|---|---:|
| HC-SR04 A TRIG | 5 |
| HC-SR04 A ECHO | 18 |
| HC-SR04 B TRIG | 19 |
| HC-SR04 B ECHO | 23 |
| LCD SDA | 21 |
| LCD SCL | 22 |
| LED xanh | 25 |
| LED đỏ | 26 |
| Buzzer | 27 |

LCD I2C dùng địa chỉ `0x27`.

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

## Chạy mô phỏng Wokwi + PlatformIO

Kéo code mới:

```powershell
git pull origin main
```

Build bằng `PlatformIO: Build`. Khi thành công sẽ có:

```text
.pio/build/esp32dev/firmware.bin
.pio/build/esp32dev/firmware.elf
```

Sau đó chạy:

```text
F1 → Wokwi: Start Simulator
```

Firmware dùng:

```text
Wi-Fi: Wokwi-GUEST
MQTT: host.wokwi.internal:1883
```

`host.wokwi.internal` được dùng để ESP32 mô phỏng truy cập dịch vụ trên máy tính khi Wokwi IoT Gateway hỗ trợ kết nối local.

## Mosquitto

Kiểm tra mọi topic của nhóm:

```powershell
& "C:\Program Files\mosquitto\mosquitto_sub.exe" -h 127.0.0.1 -p 1883 -t "nhom17/#" -v
```

## Node-RED Dashboard 2.0

Cài package:

```text
@flowfuse/node-red-dashboard
```

Import:

```text
node-red/flow.json
```

Broker Node-RED:

```text
127.0.0.1:1883
```

Dashboard:

```text
http://127.0.0.1:1880/dashboard/people-counter
```

Dashboard có IN, OUT, CURRENT, MAX, trạng thái, sự kiện gần nhất, biểu đồ CURRENT, RESET và chỉnh MAX.

## Kịch bản demo nghiệm thu

1. Ban đầu `IN=0`, `OUT=0`, `CURRENT=0`.
2. A trước B → `IN +1`.
3. B trước A → `OUT +1` nếu `CURRENT > 0`.
4. Chỉ A → timeout → không đếm.
5. Chỉ B → timeout → không đếm.
6. Lặp A→B đến khi `CURRENT = MAX`.
7. LED đỏ + buzzer + LCD báo `FULL`.
8. Cho một người đi ra B→A → `CURRENT` giảm → LED xanh trở lại.
9. Dashboard nhận và hiển thị dữ liệu MQTT.
10. Dashboard gửi `RESET` hoặc thay đổi `MAX` về ESP32.

## Bonus

Thư mục `bonus-camera/` dành cho Camera + YOLO tracking nhiều người. Đây chỉ là hướng mở rộng, không thay thế yêu cầu bắt buộc 2 cảm biến IR/Ultrasonic.
