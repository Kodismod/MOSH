#include <LiquidCrystal.h>

// ================= НАСТРОЙКИ ПИНОВ =================
// Датчики (как и были)
const int TRIG_1 = 5;
const int ECHO_1 = 18;
const int TRIG_2 = 19;
const int ECHO_2 = 21;

// Кнопка сброса (D26 и GND)
const int BTN_PIN = 26; 

// Экран (RS, E, D4, D5, D6, D7)
const int rs = 22, en = 23, d4 = 4, d5 = 16, d6 = 17, d7 = 25;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// ================= НАСТРОЙКИ ИГРЫ =================
#define SOUND_SPEED 0.0343
// Расстояние срабатывания (ширина ворот).
// Если датчик видит что-то ближе 80 см — считаем гол.
const int GATE_WIDTH = 80; 

int totalScore = 0;      // Общий счет
bool goalRegistered = false; // Флаг: "мяч сейчас в воротах"

void setup() {
  Serial.begin(115200);

  // Настройка пинов
  pinMode(TRIG_1, OUTPUT); pinMode(ECHO_1, INPUT);
  pinMode(TRIG_2, OUTPUT); pinMode(ECHO_2, INPUT);
  pinMode(BTN_PIN, INPUT_PULLUP);

  // Экран
  lcd.begin(16, 2);
  updateScreen(); // Рисуем ноль при старте
}

void loop() {
  // 1. СБРОС СЧЕТА КНОПКОЙ
  if (digitalRead(BTN_PIN) == LOW) {
    totalScore = 0;
    goalRegistered = false;
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Sbros scheta..."); // "Сброс счета"
    delay(1000);
    updateScreen();
  }

  // 2. СЧИТЫВАЕМ ДАННЫЕ (с небольшой паузой, чтобы не мешали друг другу)
  float dist1 = readSensor(TRIG_1, ECHO_1);
  delay(30); 
  float dist2 = readSensor(TRIG_2, ECHO_2);

  // 3. ПРОВЕРКА НА ГОЛ
  // Логика: (Датчик1 сработал ИЛИ Датчик2 сработал)
  bool sensor1_Hit = (dist1 > 0 && dist1 < GATE_WIDTH);
  bool sensor2_Hit = (dist2 > 0 && dist2 < GATE_WIDTH);

  if ((sensor1_Hit || sensor2_Hit) && !goalRegistered) {
    // ГОООЛ!
    totalScore++;       // Увеличиваем счет
    goalRegistered = true;  // Блокируем счетчик, пока мяч не уберут
    
    // Эффект на экране
    lcd.clear();
    lcd.setCursor(5, 0);
    lcd.print("GOOL!"); 
    delay(1000); // Показываем надпись "ГОЛ" 1 секунду
    
    updateScreen(); // Возвращаем цифры
  }
  
  // 4. СБРОС ФЛАГА (Мяч улетел)
  // Счетчик разблокируется только когда ОБА датчика снова видят пустоту
  if (dist1 > GATE_WIDTH && dist2 > GATE_WIDTH) {
    goalRegistered = false;
  }
  
  delay(50); // Небольшая задержка цикла
}

// === ФУНКЦИЯ ОТРИСОВКИ ЭКРАНА ===
void updateScreen() {
  lcd.clear();
  
  // Строка 1
  lcd.setCursor(0, 0);
  lcd.print("Schetchik golov:"); // "Счетчик голов"
  
  // Строка 2 (Счет по центру)
  lcd.setCursor(6, 1);
  lcd.print(totalScore);
}

// === ФУНКЦИЯ ИЗМЕРЕНИЯ ===
float readSensor(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW); delayMicroseconds(2);
  digitalWrite(trigPin, HIGH); delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH, 10000); 
  if (duration == 0) return 999;
  return duration * SOUND_SPEED / 2;
}