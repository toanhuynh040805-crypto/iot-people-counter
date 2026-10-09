# Bonus — Camera + YOLO Tracking

Phần này là hướng mở rộng để xử lý nhiều người đồng thời.

Không dùng phần Camera/YOLO để thay thế yêu cầu bắt buộc của đề tài là `2 × IR/Ultrasonic`.

## Kiến trúc bonus

```text
Camera
  ↓
Python + YOLO Tracking
  ↓
MQTT
  ↓
Mosquitto
  ↓
Node-RED
```

Mục tiêu mở rộng:

- gán ID riêng cho từng người
- đếm nhiều người cùng lúc
- nhận biết hướng qua hai line ảo A/B
- giảm hạn chế của 2 HC-SR04 khi nhiều người đi quá sát nhau

Nên triển khai phần này sau khi hệ thống core 4 tầng đã chạy end-to-end ổn định.
