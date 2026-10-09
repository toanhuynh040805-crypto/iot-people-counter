# Node-RED Middleware & Dashboard

## 1. Import flow

Mở Node-RED tại:

```text
http://127.0.0.1:1880
```

Sau đó:

```text
Menu → Import → select a file → node-red/flow.json → Import → Deploy
```

Broker mặc định trong flow:

```text
127.0.0.1:1883
```

## 2. Topic nhận dữ liệu

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

## 3. Lệnh gửi ngược về ESP32

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

## 4. Dashboard 2.0

Sau khi flow MQTT cơ bản chạy đúng, cài Dashboard 2.0 trong Node-RED:

```text
@flowfuse/node-red-dashboard
```

Dashboard nên có:

- IN
- OUT
- CURRENT
- MAX
- Trạng thái `CÒN CHỖ / ĐÃ ĐẦY`
- Biểu đồ CURRENT theo thời gian
- Lịch sử sự kiện IN/OUT
- Nút RESET
- Ô nhập SET MAX

## 5. Logic cảnh báo

Node `Check Capacity` kiểm tra:

```text
current >= max
```

Nếu đúng:

```text
alert = true
message = KHU VUC DA DAY
```

Nếu sai:

```text
alert = false
message = CON CHO
```
