#include <EncButton.h>                                // Библиотека для работы с кнопкой.
#include <GyverStepper.h>                             // Библиотека для работы с шаговыми двигателями.

// Настройка работы.
const uint8_t BTN_PIN = 2;                            // Пин к которому подключена кнопка (второй вывод кнопки подключен к GND).
const uint16_t HOLD_TIME_OUT = 3000;                  // Время удержания кнопки для переключения режима настройки.
const uint8_t LED_PIN = 13;
const uint32_t ledInterval = 1500;                    // Интервал мигания калибровочного светодиода.
const uint32_t ledStepInterval = 150;                 // Интервал мигания калибровочного светодиода для индикации этапа настройки. (1 вспышка - режим 1, 2 вспышки - режим 2).

// Сервисные переменные.
uint8_t settingMode = 0;                              // Режим калибровки штор. (0 - выключен, 1 - калибровка первой шторы, 2 - калибровка второй шторы).
bool debugMode = true;                                // Режим отладки. При включении режима отладки выводятся отладочные сообщения в COM порт.

uint32_t ledMillis = 0;                               // Переменная для задержки мигания калибровочного светодиода.
uint32_t ledStepMillis = 0;                           // Переменная для задержки мигания калибровочного светодиода для индикации этапа настройки.
uint8_t ledFlashCount = 0;                            // Колчиство миганий светодиода за цикл.
bool ledState = false;                                // Текущий режим светодиода. true - горит, false - не горит.

Button settingButton(BTN_PIN);                        // Кнопка настройки.

GStepper<STEPPER4WIRE> stepper1(2048, 11, 9, 10, 8);  // Моторы с драйвером ULN2003 подключается по порядку пинов, но крайние нужно поменять местами
GStepper<STEPPER4WIRE> stepper2(2048, 7, 5, 6, 4);    // подключено D2-IN1, D3-IN2, D4-IN3, D5-IN4, но в программе 5 и 2 меняются местами.

void setup() {
  // Начало инициализации.
  Serial.begin(115200);
  log("Start init");

  log("Init led");
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  log("Init button");
  settingButton.setHoldTimeout(HOLD_TIME_OUT);

  log("Init stepper 1");
  stepper1.setSpeed(400);    // Скорость в шагах/сек.
  stepper1.autoPower(true);  // Отключать мотор при достижении цели.

  log("Init stepper 2");
  stepper2.setSpeed(400);    // Скорость в шагах/сек.
  stepper2.autoPower(true);  // Отключать мотор при достижении цели.

  // Конец инициализации.
  log("End init");
  digitalWrite(LED_PIN, LOW);
}

void loop() {
  settingButton.tick();
  stepper1.tick();
  stepper2.tick();

  if(settingButton.hold()) {
    changeDebugMode();
  }

  if (settingMode != 0) {
    flashLed();
  }
}

// Переключение режима калибровки.
void changeDebugMode() {
  log("Btn settings holded");
  if (settingMode != 2) {
    log("next setting mode");
    settingMode++;
  } else {
    log("exit setting mode");
    settingMode = 0;
  }
}

// Вывод лога в порт.
void log(String text) {
  if (debugMode) {
    Serial.println(text);
  }
}

// Мигание светодиода для индикации режима калибровки.
void flashLed() {
  if (millis() - ledMillis > ledInterval) {
    ledMillis = millis();
    resetFlashCount();
  }

  if (ledFlashCount == 0) 
    return;

  if (millis() - ledStepMillis > ledStepInterval) {
    ledStepMillis = millis();

    changeLedState();
    if (!ledState) {
      ledFlashCount = ledFlashCount -1;
    }
  }
}

// Сброс количества вспыщек светодиода.
void resetFlashCount() {
  switch (settingMode) {
    case 0:
      ledFlashCount = 0;
      break;
    case 1:
      ledFlashCount = 1;
      break;
    case 2:
      ledFlashCount = 2;
      break;
  }
}

// Изменить статус светодиода.
void changeLedState() {
  log("change led State");
  if (ledState) {
    digitalWrite(LED_PIN, LOW);
    ledState = false;
  } else {
    digitalWrite(LED_PIN, HIGH);
    ledState = true;
  }
}
