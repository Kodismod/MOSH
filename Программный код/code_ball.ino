#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <MPU6500_WE.h>

// Настройки WiFi
const char* ssid = "Polygon-admin";
const char* password = "internet9041";

// Создаем объекты
WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);
MPU6500_WE mpu = MPU6500_WE(0x68);

// ===========================================
// 1. ДОБАВЛЕНО: Константа радиуса мяча (12 см = 0.12 м)
// ===========================================
const float BALL_RADIUS = 0.12;  // в метрах

// ===========================================
// 2. ДОБАВЛЕНО: Параметры для автономной работы и батареи
// ===========================================
const int batteryPin = 34;  // Пин для измерения напряжения
float batteryMin = 3.0;      // Минимальное напряжение
float batteryMax = 4.2;      // Максимальное напряжение

// ===========================================
// 3. РАСШИРЕНА: Структура данных с новыми полями
// ===========================================
struct SensorData {
  // Существующие поля
  float ax, ay, az;          // Акселерометр (g)
  float gx, gy, gz;          // Гироскоп (град/сек)
  float temperature;          // Температура
  float roll, pitch, yaw;     // Углы ориентации
  float rotationSpeed;        // Общая скорость вращения (град/сек)
  bool isRotating;            // Наличие вращения
  String rotationAxis;        // Ось вращения
  
  // НОВЫЕ ПОЛЯ для линейных параметров
  float linearSpeed;          // Линейная скорость на поверхности (м/с)
  float rpm;                  // Обороты в минуту
  float rotationsPerSecond;   // Обороты в секунду
  float centripetalAccel;     // Центростремительное ускорение (g)
  float batteryVoltage;       // Напряжение батареи
} sensorData;

unsigned long lastTime = 0;
float dt = 0.01;

// ===========================================
// 4. ДОБАВЛЕНО: Функция для чтения напряжения батареи
// ===========================================
float readBatteryVoltage() {
  int rawValue = analogRead(batteryPin);
  float voltage = (rawValue / 4095.0) * 3.3;
  float actualVoltage = voltage * 2;  // Коэффициент для делителя 1:2
  return actualVoltage;
}
// ===========================================
// 5. ОБНОВЛЕНА: HTML страница с новыми полями
// ===========================================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>MPU6500 - Визуализация мяча (R=12см)</title>
    <meta charset="utf-8">
    <meta http-equiv="Content-Type" content="text/html; charset=utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1">
    <style>
        * { box-sizing: border-box; }
        body { 
            font-family: 'Segoe UI', Arial, sans-serif; 
            text-align: center; 
            margin: 0; 
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            color: #333;
        }
        .container { 
            max-width: 1200px; 
            margin: 0 auto; 
            padding: 20px; 
        }
        h1 {
            color: white;
            text-shadow: 2px 2px 4px rgba(0,0,0,0.3);
            margin-bottom: 30px;
            font-size: 2.5em;
        }
        .data-panel { 
            background: rgba(255,255,255,0.95); 
            border-radius: 20px; 
            padding: 30px; 
            margin: 20px 0;
            box-shadow: 0 10px 40px rgba(0,0,0,0.2);
            backdrop-filter: blur(10px);
        }
        .sensor-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
            gap: 20px;
            margin-top: 20px;
        }
        .sensor-item {
            background: white;
            padding: 20px;
            border-radius: 15px;
            text-align: left;
            box-shadow: 0 5px 15px rgba(0,0,0,0.1);
            transition: transform 0.3s ease;
        }
        .sensor-item:hover {
            transform: translateY(-5px);
        }
        .sensor-item h3 {
            margin: 0 0 15px 0;
            color: #667eea;
            font-size: 1.2em;
            border-bottom: 2px solid #e0e0e0;
            padding-bottom: 10px;
        }
        .value {
            font-size: 28px;
            font-weight: bold;
            color: #333;
            font-family: 'Courier New', monospace;
        }
        .unit {
            font-size: 14px;
            color: #666;
            margin-left: 5px;
        }
        .status {
            padding: 15px;
            border-radius: 10px;
            margin: 15px 0;
            font-size: 1.2em;
            font-weight: bold;
            text-align: center;
        }
        .rotating { 
            background: #d4edda; 
            color: #155724;
            border: 2px solid #c3e6cb;
        }
        .not-rotating { 
            background: #f8d7da; 
            color: #721c24;
            border: 2px solid #f5c6cb;
        }
        canvas { 
            max-width: 500px; 
            width: 100%;
            margin: 20px auto;
            display: block;
            background: white;
            border-radius: 20px;
            box-shadow: 0 10px 30px rgba(0,0,0,0.2);
            border: 4px solid white;
        }
        .axis-info {
            font-size: 20px;
            margin: 15px 0;
            padding: 15px;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            border-radius: 10px;
            color: white;
            font-weight: bold;
        }
        .ball-container {
            position: relative;
            display: inline-block;
        }
        .ip-address {
            background: rgba(255,255,255,0.2);
            color: white;
            padding: 10px;
            border-radius: 10px;
            font-size: 14px;
            margin-top: 10px;
        }
        .footer {
            color: white;
            margin-top: 20px;
            font-size: 14px;
            opacity: 0.8;
        }
        .battery {
            background: rgba(255,255,255,0.2);
            color: white;
            padding: 8px 15px;
            border-radius: 20px;
            font-size: 14px;
            display: inline-block;
margin-bottom: 10px;
        }
        .battery-low { background: #ff4444; }
        .battery-medium { background: #ffaa00; }
        .battery-high { background: #00aa00; }
        .radius-badge {
            background: #667eea;
            color: white;
            padding: 5px 15px;
            border-radius: 20px;
            font-size: 16px;
            display: inline-block;
            margin-bottom: 10px;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>⚽️ Мониторинг вращения мяча</h1>
        
        <div class="radius-badge">
            Радиус мяча: 12 см
        </div>
        
        <div class="battery" id="batteryStatus">
            🔋 Заряд: --%
        </div>
        
        <div class="ball-container">
            <canvas id="ballCanvas" width="400" height="400"></canvas>
        </div>
        
        <div class="data-panel">
            <h2 style="color: #333; margin-top: 0;">Данные с датчика MPU6500</h2>
            
            <div class="status" id="rotationStatus">
                Статус: подключение...
            </div>
            
            <div class="axis-info" id="axisInfo">
                Ось вращения: --
            </div>
            
            <div class="sensor-grid">
                <div class="sensor-item">
                    <h3>📊 Акселерометр</h3>
                    <div>X: <span class="value" id="ax">0.00</span> <span class="unit">g</span></div>
                    <div>Y: <span class="value" id="ay">0.00</span> <span class="unit">g</span></div>
                    <div>Z: <span class="value" id="az">0.00</span> <span class="unit">g</span></div>
                </div>
                
                <div class="sensor-item">
                    <h3>🔄 Гироскоп</h3>
                    <div>X: <span class="value" id="gx">0.00</span> <span class="unit">°/с</span></div>
                    <div>Y: <span class="value" id="gy">0.00</span> <span class="unit">°/с</span></div>
                    <div>Z: <span class="value" id="gz">0.00</span> <span class="unit">°/с</span></div>
                </div>
                
                <div class="sensor-item">
                    <h3>🧭 Ориентация</h3>
                    <div>Roll: <span class="value" id="roll">0.00</span> <span class="unit">°</span></div>
                    <div>Pitch: <span class="value" id="pitch">0.00</span> <span class="unit">°</span></div>
                    <div>Yaw: <span class="value" id="yaw">0.00</span> <span class="unit">°</span></div>
                </div>
                
                <div class="sensor-item">
                    <h3>⚡️ Вращение</h3>
                    <div>Скорость: <span class="value" id="rotSpeed">0.00</span> <span class="unit">°/с</span></div>
                    <div>Температура: <span class="value" id="temp">0.00</span> <span class="unit">°C</span></div>
                </div>

                <!-- НОВЫЙ БЛОК: Линейные параметры с учетом радиуса -->
                <div class="sensor-item">
                    <h3>📏 Линейные параметры</h3>
                    <div>Скорость пов-ти: <span class="value" id="linearSpeed">0.00</span> <span class="unit">м/с</span></div>
                    <div>Обороты: <span class="value" id="rpm">0</span> <span class="unit">об/мин</span></div>
                    <div>Центростр. ус.: <span class="value" id="centripetal">0.00</span> <span class="unit">g</span></div>
                </div>
            </div>
        </div>
        
        <div class="ip-address" id="ipDisplay">
            IP адрес: получение...
        </div>
        <div class="footer">
            MPU6500 | Радиус мяча 12см | Автономный режим
        </div>
    </div>
<script>
        const canvas = document.getElementById('ballCanvas');
        const ctx = canvas.getContext('2d');
        let ws;
        
        const currentIP = window.location.hostname;
        document.getElementById('ipDisplay').textContent = 'IP адрес: ' + currentIP;
        
        function connectWebSocket() {
            ws = new WebSocket('ws://' + currentIP + ':81/');
            
            ws.onopen = function() {
                console.log('WebSocket connected');
                document.getElementById('rotationStatus').textContent = '⚡️ Статус: ПОДКЛЮЧЕНО';
                document.getElementById('rotationStatus').className = 'status rotating';
            };
            
            ws.onclose = function() {
                console.log('WebSocket disconnected');
                document.getElementById('rotationStatus').textContent = '❌ Статус: ОТКЛЮЧЕНО';
                document.getElementById('rotationStatus').className = 'status not-rotating';
                setTimeout(connectWebSocket, 1000);
            };
            
            ws.onerror = function(error) {
                console.log('WebSocket error:', error);
                document.getElementById('rotationStatus').textContent = '⚠️ Ошибка подключения';
            };
            
            ws.onmessage = function(event) {
                try {
                    const data = JSON.parse(event.data);
                    updateData(data);
                    drawBall(data);
                    updateBattery(data.battery);
                } catch (e) {
                    console.log('Error parsing data:', e);
                }
            };
        }
        
        function updateBattery(voltage) {
            const batteryEl = document.getElementById('batteryStatus');
            if (voltage) {
                let percent = ((voltage - 3.0) / 1.2) * 100;
                percent = Math.min(100, Math.max(0, percent));
                
                batteryEl.innerHTML = 🔋 Заряд: ${percent.toFixed(0)}% (${voltage.toFixed(2)}V);
                
                batteryEl.classList.remove('battery-low', 'battery-medium', 'battery-high');
                if (percent < 20) batteryEl.classList.add('battery-low');
                else if (percent < 50) batteryEl.classList.add('battery-medium');
                else batteryEl.classList.add('battery-high');
            }
        }
        
        function updateData(data) {
            // Основные датчики
            document.getElementById('ax').textContent = data.ax.toFixed(2);
            document.getElementById('ay').textContent = data.ay.toFixed(2);
            document.getElementById('az').textContent = data.az.toFixed(2);
            
            document.getElementById('gx').textContent = data.gx.toFixed(2);
            document.getElementById('gy').textContent = data.gy.toFixed(2);
            document.getElementById('gz').textContent = data.gz.toFixed(2);
            
            document.getElementById('roll').textContent = data.roll.toFixed(1);
            document.getElementById('pitch').textContent = data.pitch.toFixed(1);
            document.getElementById('yaw').textContent = data.yaw.toFixed(1);
            
            document.getElementById('rotSpeed').textContent = data.rotationSpeed.toFixed(1);
            document.getElementById('temp').textContent = data.temperature.toFixed(1);
            
            // НОВЫЕ линейные параметры
            document.getElementById('linearSpeed').textContent = data.linearSpeed.toFixed(2);
            document.getElementById('rpm').textContent = Math.round(data.rpm);
            document.getElementById('centripetal').textContent = data.centripetalAccel.toFixed(2);
            
            const statusEl = document.getElementById('rotationStatus');
            const axisEl = document.getElementById('axisInfo');
            
            if (data.isRotating) {
                statusEl.textContent = '⚡️ ВРАЩЕНИЕ ОБНАРУЖЕНО';
                statusEl.className = 'status rotating';
axisEl.innerHTML = 🎯 Ось вращения: <strong>${data.rotationAxis}</strong>;
            } else {
                statusEl.textContent = '⏸️ В СОСТОЯНИИ ПОКОЯ';
                statusEl.className = 'status not-rotating';
                axisEl.innerHTML = '⏹️ Ось вращения: НЕТ ВРАЩЕНИЯ';
            }
        }
        
        function drawBall(data) {
            ctx.clearRect(0, 0, 400, 400);
            
            // Фон
            const gradient = ctx.createRadialGradient(200, 200, 0, 200, 200, 200);
            gradient.addColorStop(0, '#f0f0f0');
            gradient.addColorStop(1, '#e0e0e0');
            ctx.fillStyle = gradient;
            ctx.fillRect(0, 0, 400, 400);
            
            // Мяч
            ctx.beginPath();
            ctx.arc(200, 200, 120, 0, 2 * Math.PI);
            ctx.strokeStyle = '#333';
            ctx.lineWidth = 3;
            ctx.stroke();
            
            const ballGradient = ctx.createRadialGradient(150, 150, 20, 200, 200, 120);
            ballGradient.addColorStop(0, '#fff');
            ballGradient.addColorStop(0.5, '#ddd');
            ballGradient.addColorStop(1, '#aaa');
            ctx.fillStyle = ballGradient;
            ctx.fill();
            
            // Линии
            ctx.beginPath();
            ctx.strokeStyle = '#999';
            ctx.lineWidth = 1;
            ctx.setLineDash([5, 5]);
            ctx.moveTo(80, 200);
            ctx.lineTo(320, 200);
            ctx.stroke();
            ctx.moveTo(200, 80);
            ctx.lineTo(200, 320);
            ctx.stroke();
            ctx.setLineDash([]);
            
            // Векторы
            const scale = 30;
            drawArrow(200, 200, 200 + data.ax * scale, 200, '#ff4444', 'X');
            drawArrow(200, 200, 200, 200 - data.ay * scale, '#44ff44', 'Y');
            
            ctx.beginPath();
            ctx.arc(200 + data.az * 20, 200 + data.az * 10, 8 + Math.abs(data.az) * 5, 0, 2 * Math.PI);
            ctx.fillStyle = 'rgba(68, 68, 255, 0.3)';
            ctx.fill();
            ctx.strokeStyle = '#4444ff';
            ctx.lineWidth = 2;
            ctx.stroke();
            
            // Текст на канвасе
            ctx.font = 'bold 14px Arial';
            ctx.fillStyle = '#333';
            ctx.fillText('Скорость: ' + data.rotationSpeed.toFixed(1) + ' °/с', 20, 40);
            ctx.fillText('На пов-ти: ' + data.linearSpeed.toFixed(2) + ' м/с', 20, 70);
            
            if (data.isRotating) {
                ctx.fillStyle = '#00aa00';
                ctx.fillText('⚡️ ' + data.rpm + ' об/мин', 20, 100);
            }
        }
        
        function drawArrow(fromX, fromY, toX, toY, color, label) {
            ctx.beginPath();
            ctx.moveTo(fromX, fromY);
            ctx.lineTo(toX, toY);
            ctx.strokeStyle = color;
            ctx.lineWidth = 3;
            ctx.stroke();
            
            const angle = Math.atan2(toY - fromY, toX - fromX);
            const arrowSize = 15;
            
            ctx.beginPath();
            ctx.moveTo(toX, toY);
            ctx.lineTo(toX - arrowSize * Math.cos(angle - 0.3), toY - arrowSize * Math.sin(angle - 0.3));
            ctx.lineTo(toX - arrowSize * Math.cos(angle + 0.3), toY - arrowSize * Math.sin(angle + 0.3));
            ctx.closePath();
            ctx.fillStyle = color;
            ctx.fill();
            
            ctx.fillStyle = color;
            ctx.font = 'bold 14px Arial';
            ctx.fillText(label, (fromX + toX) / 2 + 10, (fromY + toY) / 2 - 10);
        }
        
        window.onload = function() {
            connectWebSocket();
        };
    </script>
</body>
</html>
)rawliteral";
void setup() {
    // ===========================================
    // 6. ДОБАВЛЕНО: Задержка для автономного режима
    // ===========================================
    delay(2000);  // Ждем стабилизации питания от батарей
    
    Serial.begin(115200);
    delay(500);
    
    Serial.println();
    Serial.println("=================================");
    Serial.println("Запуск программы для MPU6500...");
    Serial.println("Радиус мяча: 12 см");
    Serial.println("Автономный режим с батарейками");
    Serial.println("=================================");
    
    // ИНИЦИАЛИЗАЦИЯ I2C
    Wire.begin(21, 22);
    Wire.setClock(100000);
    delay(100);
    
    Serial.println("I2C инициализирован:");
    Serial.println("  SDA (data)  -> GPIO21 (D21)");
    Serial.println("  SCL (clock) -> GPIO22 (D22)");
    
    // Проверка датчика
    Serial.println("Поиск MPU6500 по адресу 0x68...");
    Wire.beginTransmission(0x68);
    byte error = Wire.endTransmission();
    
    if (error == 0) {
        Serial.println("✅ Датчик найден по адресу 0x68!");
        
        Serial.println("Инициализация MPU6500...");
        if (!mpu.init()) {
            Serial.println("❌ Ошибка инициализации MPU6500!");
            while (1) {
                digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
                delay(500);
            }
        }
        
        Serial.println("✅ MPU6500 инициализирован!");
        
        Serial.println("Калибровка... Не двигайте датчик!");
        delay(1000);
        mpu.autoOffsets();
        Serial.println("✅ Калибровка завершена!");
        
        mpu.setAccRange(MPU6500_ACC_RANGE_2G);
        mpu.setGyrRange(MPU6500_GYRO_RANGE_250);
        mpu.setAccDLPF(MPU6500_DLPF_6);
        mpu.setGyrDLPF(MPU6500_DLPF_6);
        
        Serial.println("✅ Датчик настроен и готов к работе!");
        
    } else {
        Serial.println("❌ Ошибка: MPU6500 не найден!");
        Serial.println("Проверьте подключение:");
        Serial.println("  MPU6500 -> ESP32");
        Serial.println("  VCC     -> 3.3V");
        Serial.println("  GND     -> GND");
        Serial.println("  SCL     -> D22 (GPIO22)");
        Serial.println("  SDA     -> D21 (GPIO21)");
        
        pinMode(LED_BUILTIN, OUTPUT);
        while (1) {
            for(int i = 0; i < 5; i++) {
                digitalWrite(LED_BUILTIN, HIGH);
                delay(100);
                digitalWrite(LED_BUILTIN, LOW);
                delay(100);
            }
            delay(2000);
        }
    }
    
    // Подключение к WiFi
    Serial.println("Подключение к WiFi...");
    WiFi.begin(ssid, password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println();
        Serial.println("✅ WiFi подключен!");
        Serial.print("IP адрес: ");
        Serial.println(WiFi.localIP());
        Serial.print("MAC адрес: ");
        Serial.println(WiFi.macAddress());
        
        pinMode(LED_BUILTIN, OUTPUT);
        for(int i = 0; i < 3; i++) {
            digitalWrite(LED_BUILTIN, HIGH);
            delay(200);
            digitalWrite(LED_BUILTIN, LOW);
            delay(200);
        }
    } else {
        Serial.println();
        Serial.println("❌ Ошибка подключения к WiFi!");
        Serial.println("Проверьте SSID и пароль");
    }
    
    server.on("/", handleRoot);
    server.begin();
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
    
    lastTime = micros();
    
    Serial.println("✅ Программа полностью загружена!");
    Serial.println("Можно отключать USB - ESP32 будет работать от батареек");
}
void loop() {
    webSocket.loop();
    server.handleClient();
    
    // Получение данных с MPU6500
    xyzFloat gValue = mpu.getGValues();
    xyzFloat gyr = mpu.getGyrValues();
    
    unsigned long now = micros();
    dt = (now - lastTime) / 1000000.0;
    lastTime = now;
    
    // Заполнение основных данных
    sensorData.ax = gValue.x;
    sensorData.ay = gValue.y;
    sensorData.az = gValue.z;
    
    sensorData.gx = gyr.x;
    sensorData.gy = gyr.y;
    sensorData.gz = gyr.z;
    
    sensorData.temperature = mpu.getTemperature();
    
    // ===========================================
    // 7. ДОБАВЛЕНО: Измерение напряжения батареи
    // ===========================================
    sensorData.batteryVoltage = readBatteryVoltage();
    
    // Вычисление ориентации
    static float roll = 0, pitch = 0, yaw = 0;
    
    float accRoll = atan2(sensorData.ay, sensorData.az) * 180.0 / PI;
    float accPitch = atan2(-sensorData.ax, sqrt(sensorData.ay * sensorData.ay + sensorData.az * sensorData.az)) * 180.0 / PI;
    
    float alpha = 0.98;
    roll = alpha * (roll + sensorData.gx * dt) + (1 - alpha) * accRoll;
    pitch = alpha * (pitch + sensorData.gy * dt) + (1 - alpha) * accPitch;
    yaw += sensorData.gz * dt;
    
    sensorData.roll = roll;
    sensorData.pitch = pitch;
    sensorData.yaw = yaw;
    
    // Параметры вращения
    sensorData.rotationSpeed = sqrt(sensorData.gx * sensorData.gx + 
                                    sensorData.gy * sensorData.gy + 
                                    sensorData.gz * sensorData.gz);
    
    sensorData.isRotating = sensorData.rotationSpeed > 10.0;
    
    float maxGyro = max(max(abs(sensorData.gx), abs(sensorData.gy)), abs(sensorData.gz));
    if (maxGyro == abs(sensorData.gx)) sensorData.rotationAxis = "X";
    else if (maxGyro == abs(sensorData.gy)) sensorData.rotationAxis = "Y";
    else sensorData.rotationAxis = "Z";
    
    // ===========================================
    // 8. ДОБАВЛЕНО: Расчеты с учетом радиуса мяча
    // ===========================================
    
    // Угловая скорость в рад/с
    float omegaX = sensorData.gx * PI / 180.0;
    float omegaY = sensorData.gy * PI / 180.0;
    float omegaZ = sensorData.gz * PI / 180.0;
    
    // Модуль угловой скорости в рад/с
    float omega = sqrt(omegaX*omegaX + omegaY*omegaY + omegaZ*omegaZ);
    
    // Обороты
    sensorData.rotationsPerSecond = omega / (2 * PI);
    sensorData.rpm = sensorData.rotationsPerSecond * 60;
    
    // Линейная скорость на поверхности мяча
    // v = ω * R, где R = 0.12 м
    sensorData.linearSpeed = omega * BALL_RADIUS;
    
    // Центростремительное ускорение (в g)
    // a = ω² * R / 9.81
    sensorData.centripetalAccel = (omega * omega * BALL_RADIUS) / 9.81;
    
    sendSensorData();
    
    delay(20);
}

void sendSensorData() {
    StaticJsonDocument<512> doc;
    
    // Основные данные
    doc["ax"] = sensorData.ax;
    doc["ay"] = sensorData.ay;
    doc["az"] = sensorData.az;
    doc["gx"] = sensorData.gx;
    doc["gy"] = sensorData.gy;
    doc["gz"] = sensorData.gz;
    doc["roll"] = sensorData.roll;
    doc["pitch"] = sensorData.pitch;
    doc["yaw"] = sensorData.yaw;
    doc["rotationSpeed"] = sensorData.rotationSpeed;
    doc["isRotating"] = sensorData.isRotating;
    doc["rotationAxis"] = sensorData.rotationAxis;
    doc["temperature"] = sensorData.temperature;
    
    // НОВЫЕ данные
    doc["linearSpeed"] = sensorData.linearSpeed;
    doc["rpm"] = sensorData.rpm;
    doc["rotationsPerSecond"] = sensorData.rotationsPerSecond;
    doc["centripetalAccel"] = sensorData.centripetalAccel;
    doc["battery"] = sensorData.batteryVoltage;
    
    String jsonString;
    serializeJson(doc, jsonString);
    webSocket.broadcastTXT(jsonString);
}

void handleRoot() {
    server.send(200, "text/html", index_html);
}
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("[%u] Отключен!\n", num);
            break;
        case WStype_CONNECTED:
            Serial.printf("[%u] Подключен!\n", num);
            break;
    }
}