#include <EncButton.h>                                // Библиотека для работы с кнопкой.
#include <GyverStepper.h>                             // Библиотека для работы с шаговыми двигателями.
#include <EEPROM.h>                                   // Библиотека для работы с EEPROM памятью.
#include <Wire.h>                                     // Библиотека Wire (для работы с шиной I2C).
#include <BH1750.h>                                   // Библиотека для датчика освещенности bh1750.

// Настройка работы.
const uint8_t BTN_PIN = 2;                            // Пин к которому подключена кнопка (второй вывод кнопки подключен к GND).
const uint16_t HOLD_TIME_OUT = 3000;                  // Время удержания кнопки для переключения режима настройки.
const uint8_t LED_PIN = 13;                           // Пин к которому подключен светодиод.
const uint32_t ledInterval = 1500;                    // Интервал мигания калибровочного светодиода.
const uint32_t ledStepInterval = 150;                 // Интервал мигания калибровочного светодиода для индикации этапа настройки. (1 вспышка - режим 1, 2 вспышки - режим 2).

const uint8_t STEPPER1_EEPROM_ADDR = 0;               // Адрес для сохранения в EEPROM целевого положения первого мотора.
const uint8_t STEPPER2_EEPROM_ADDR = 4;               // Адрес для сохранения в EEPROM целевого положения второго мотора.

const float LIGHT_TRESHOLD = 35000;                   // Пороговая яркость, на которой срабатывает активация штор.
const uint32_t DELAY_TO_ACTIVATE = 10000;             // Частота проверки яркости в милисекундах для активации шторы.
const uint32_t DELAY_TO_DEACTIVATE = 30000;           // Частота проверки яркости в милисекундах для деактивации шторы.

// Сервисные переменные.
uint8_t settingMode = 0;                              // Режим калибровки штор. (0 - выключен, 1 - калибровка первой шторы, 2 - калибровка второй шторы).
bool debugMode = true;                                // Режим отладки. При включении режима отладки выводятся отладочные сообщения в COM порт.

uint32_t ledMillis = 0;                               // Переменная для задержки мигания калибровочного светодиода.
uint32_t ledStepMillis = 0;                           // Переменная для задержки мигания калибровочного светодиода для индикации этапа настройки.
uint8_t ledFlashCount = 0;                            // Колчиство миганий светодиода за цикл.
bool ledState = false;                                // Текущий режим светодиода. true - горит, false - не горит.

bool setupRotationDirection = true;                   // Направление вращения мотора в режиме калибровки, true - прямое, false - реверс.
float setupStepperMaxPosition = 0;                    // Верхнее калибровочное положение мотора.
float setupStepperMinPosition = 0;                    // Нижнее калибровочное положение мотора.

float targetPositionStepper1 = 0;                     // Целевое положение первого мотора.
float targetPositionStepper2 = 0;                     // Целевое положение второго мотора.

int lowLightCollector = 0;                            // Счетчик количества обнаружений снижения яркости прежде чем деактивировать штору (стартовое значение 0).
int lowLightCollectorLimit = 30;                      // Значение счетчика при котором будет деактивирована штора (DELAY_TO_DEACTIVATE * lowLightCollectorLimit = количество секунд через которое будет деактивирована штора,
                                                      // если в процессе увеличения счетчика освещённость не вернётся на высокий уровень, то счетчик сбрасывается на начальное значение).
uint32_t checkTime = DELAY_TO_ACTIVATE;               // Текущая частота проверки яркости.
bool blindPosition = false;                           // Положение занавески. true - поднята, false - опущена.
uint32_t lightMeterMillis = 0;                        // Переменная для задержки проверки освещенности.

Button settingButton(BTN_PIN);                        // Кнопка настройки.
BH1750 lightMeter;                                    // Датчик освещенности.

GStepper<STEPPER4WIRE> stepper1(2048, 12, 10, 11, 9); // Моторы с драйвером ULN2003 подключается по порядку пинов, но крайние нужно поменять местами
GStepper<STEPPER4WIRE> stepper2(2048, 8, 6, 7, 5);    // подключено D2-IN1, D3-IN2, D4-IN3, D5-IN4, но в программе 5 и 2 меняются местами.

void setup() {
  // Начало инициализации.
  Serial.begin(115200);
  log("Start init");

  log("Init led");
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  log("Init button");
  settingButton.setHoldTimeout(HOLD_TIME_OUT);

  log("Init lightmeter");
  Wire.begin(); // Инициализируем шину I2C (библиотека BH1750 не делает это автоматически).
  lightMeter.begin(); // Инициализируем и запускаем BH1750.

  log("Init stepper 1");
  stepper1.autoPower(true);  // Отключать мотор при достижении цели.
  EEPROM.get(STEPPER1_EEPROM_ADDR, targetPositionStepper1);

  log("Init stepper 2");
  stepper2.autoPower(true);  // Отключать мотор при достижении цели.
  EEPROM.get(STEPPER2_EEPROM_ADDR, targetPositionStepper2);

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

    if (settingButton.click()) {
      setupStepperRotater();
    }
  } else {
    if (millis() - lightMeterMillis > checkTime) {
      lightMeterMillis = millis();
      checkLight();
    }
  }
}

// Переключение режима калибровки.
void changeDebugMode() {
  log("Btn settings holded");
  saveSetup();

  if (settingMode != 2) {
    log("next setting mode");
    settingMode++;
  } else {
    log("exit setting mode");
    settingMode = 0;

    digitalWrite(LED_PIN, LOW);
    ledState = false;
  }
}

// Сохранить настройки калибровки моторов.
void saveSetup() {
  if (settingMode == 1) {
    log("save stepper 1 dist");
    EEPROM.put(STEPPER1_EEPROM_ADDR, setupStepperMaxPosition - setupStepperMinPosition);
    resetSetupVars();
  }
  if (settingMode == 2) {
    log("save stepper 2 dist");
    EEPROM.put(STEPPER2_EEPROM_ADDR, setupStepperMaxPosition - setupStepperMinPosition);
    resetSetupVars();
  }
}

// Сброс калибровочных перменных.
void resetSetupVars() {
  setupRotationDirection = true;
  setupStepperMaxPosition = 0;
  setupStepperMinPosition = 0;   
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
  if (ledState) {
    digitalWrite(LED_PIN, LOW);
    ledState = false;
  } else {
    digitalWrite(LED_PIN, HIGH);
    ledState = true;
  }
}

// Калибровочное вращение моторов.
void setupStepperRotater() {
  if (settingMode == 1) {
    setupStepper1();
  }

  if (settingMode == 2) {
    setupStepper2();
  }
}

// Калибровочное вращение мотора 1.
void setupStepper1() {
  if (!stepper1.getState()) {
    log("start 1 rotate");
    stepper1.setRunMode(KEEP_SPEED);

    if (setupRotationDirection) {
      stepper1.setSpeedDeg(100);
    } else {
      stepper1.setSpeedDeg(-100);
    }
  } else {
    log("stepper 1 stop");
    if (setupRotationDirection) {
      setupStepperMaxPosition = stepper1.getCurrentDeg();
    } else {
      setupStepperMinPosition = stepper1.getCurrentDeg();
    }

    stepper1.brake();
    setupRotationDirection = !setupRotationDirection;
  }
}

// Калибровочное вращение мотора 2.
void setupStepper2() {
  if (!stepper2.getState()) {
    log("start 2 rotate");
    stepper2.setRunMode(KEEP_SPEED);

    if (setupRotationDirection) {
      stepper2.setSpeedDeg(100);
    } else {
      stepper2.setSpeedDeg(-100);
    }
  } else {
    log("stepper 2 stop");
    if (setupRotationDirection) {
      setupStepperMaxPosition = stepper2.getCurrentDeg();
    } else {
      setupStepperMinPosition = stepper2.getCurrentDeg();
    }

    stepper2.brake();
    setupRotationDirection = !setupRotationDirection;
  }
}

// Проверка текущей яркости
void checkLight() {
  log("Check light");
  float lux = lightMeter.readLightLevel();
  if (debugMode) {
    Serial.print("Light: ");
    Serial.println(lux);
  }

   if (!stepper1.tick() && !stepper2.tick()) {
    if (lux > LIGHT_TRESHOLD) {
      lowLightCollector = 0; // Обнуляем счетчик понижений яркости
      log(lowLightCollector);
      log("lux more than threshold, reset lowLightCollector");
      
      if(!blindPosition) {
        log("Current light more than lightThreshold, and ,blind in bottom - move to top");
        blindPosition = true;
        checkTime = DELAY_TO_DEACTIVATE;
        BlindsActivate();
      }
    }
  
    if (blindPosition && lux < LIGHT_TRESHOLD) {
      lowLightCollector++; // Увеличиваем счетчик понижений яркости
      log("lux less than threshold, up lowLightCollector: ");
      log(lowLightCollector);
      
      if(lowLightCollector > lowLightCollectorLimit) {
        log("Current light less than lightThreshold, and ,blind in top - move to bottom");
        blindPosition = false;
        checkTime = DELAY_TO_ACTIVATE;
        BlindsDeactivate();
        lowLightCollector = 0;
      }
    }
  }
}

// Активировать шторы.
void BlindsActivate() {
  if (!stepper1.tick()) {
    log("Rotate");
    stepper1.setTargetDeg(targetPositionStepper1);
  }

  if (!stepper2.tick()) {
    log("Rotate2");
    stepper2.setTargetDeg(targetPositionStepper2);
  }
}

// Деактивировать шторы.
void BlindsDeactivate() {
  if (!stepper1.tick()) {
    log("Rotate");
    stepper1.setTargetDeg(0);        // Целевая позиция в градусах
  }

  if (!stepper2.tick()) {
    log("Rotate2");
    stepper2.setTargetDeg(0);        // Целевая позиция в градусах
  }
}
