#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "BluetoothSerial.h"
#include "driver/ledc.h"

// MAC ID del logcat (¡Reemplaza esto con el valor correcto!)
#define LOGCAT_MAC "AA:BB:CC:DD:EE:FF" 

// Pines de control y luces
#define MOTOR_A_IN1 26
#define MOTOR_A_IN2 27
#define MOTOR_B_IN3 14
#define MOTOR_B_IN4 12
#define MOTOR_A_ENA 13
#define MOTOR_B_ENB 15
#define WHITE_LIGHT_PIN_FRONT_LEFT 4
#define WHITE_LIGHT_PIN_FRONT_RIGHT 16
#define RED_LIGHT_PIN_REAR_LEFT 17
#define RED_LIGHT_PIN_REAR_RIGHT 5
// PIN para lectura de voltaje de la batería
#define BATTERY_VOLTAGE_PIN 34

// Enumeraciones para estados del carro
enum CarMovementState {
    STATE_STOPPED,
    STATE_FORWARD,
    STATE_BACKWARD,
    STATE_BRAKING,
    STATE_TURNING_LEFT,
    STATE_TURNING_RIGHT,
    STATE_TESTING_COMPONENTS
};

// Variables de estado
CarMovementState currentCarState = STATE_STOPPED;
long lastCommandTime = 0;
const int TIMEOUT_MS = 1000;
bool whiteLightsOn = false;
bool isLogcatConnected = false;

// Variables de monitoreo (algunas son simuladas)
float batteryVoltage = 0.0;
float batteryPercentage = 0.0;
float temperature = 0.0; // Simulación de temperatura
int motorSpeedLeft = 0;
int motorSpeedRight = 0;
String motorState = "STOPPED";

// Bluetooth
BluetoothSerial SerialBT;
BLEServer* pServer = NULL;
BLECharacteristic* pTxCharacteristic = NULL;
BLEAdvertising* pAdvertising = NULL;

// Tareas de FreeRTOS
TaskHandle_t carControlTask;
TaskHandle_t bluetoothManagerTask;

// --- Funciones de control de motores y luces ---

void setMotorsSpeed(int leftSpeed, int rightSpeed) {
    ledcWrite(0, leftSpeed);
    ledcWrite(1, rightSpeed);
    motorSpeedLeft = leftSpeed;
    motorSpeedRight = rightSpeed;
}

void goForward() {
    digitalWrite(MOTOR_A_IN1, HIGH);
    digitalWrite(MOTOR_A_IN2, LOW);
    digitalWrite(MOTOR_B_IN3, HIGH);
    digitalWrite(MOTOR_B_IN4, LOW);
    motorState = "FORWARD";
}

void goBackward() {
    digitalWrite(MOTOR_A_IN1, LOW);
    digitalWrite(MOTOR_A_IN2, HIGH);
    digitalWrite(MOTOR_B_IN3, LOW);
    digitalWrite(MOTOR_B_IN4, HIGH);
    motorState = "BACKWARD";
}

void stopMotors() {
    digitalWrite(MOTOR_A_IN1, LOW);
    digitalWrite(MOTOR_A_IN2, LOW);
    digitalWrite(MOTOR_B_IN3, LOW);
    digitalWrite(MOTOR_B_IN4, LOW);
    setMotorsSpeed(0, 0);
    motorState = "STOPPED";
}

void setRedLights(bool state) {
    digitalWrite(RED_LIGHT_PIN_REAR_LEFT, state);
    digitalWrite(RED_LIGHT_PIN_REAR_RIGHT, state);
}

void blinkRedLights(bool state) {
    if (state) {
        digitalWrite(RED_LIGHT_PIN_REAR_LEFT, !digitalRead(RED_LIGHT_PIN_REAR_LEFT));
        digitalWrite(RED_LIGHT_PIN_REAR_RIGHT, !digitalRead(RED_LIGHT_PIN_REAR_RIGHT));
    }
}

void setWhiteLights(bool state) {
    digitalWrite(WHITE_LIGHT_PIN_FRONT_LEFT, state);
    digitalWrite(WHITE_LIGHT_PIN_FRONT_RIGHT, state);
}

void runQuickTest(char testCommand) {
    Serial.println("Running quick test...");
    switch(testCommand) {
        case 'F': // Test de movimiento
            goForward();
            setMotorsSpeed(255, 255);
            vTaskDelay(pdMS_TO_TICKS(1000));
            stopMotors();
            break;
        case 'L': // Test de luces
            setWhiteLights(true);
            setRedLights(true);
            vTaskDelay(pdMS_TO_TICKS(2000));
            setWhiteLights(false);
            setRedLights(false);
            break;
        // Puedes agregar más pruebas aquí
    }
    currentCarState = STATE_STOPPED;
}

// --- Tarea de control del carro (Núcleo 0) ---

void carControl(void* pvParameters) {
    while (true) {
        if (currentCarState != STATE_TESTING_COMPONENTS) {
            switch (currentCarState) {
                case STATE_FORWARD:
                    goForward();
                    setMotorsSpeed(255, 255);
                    setRedLights(LOW);
                    break;
                case STATE_BACKWARD:
                    goBackward();
                    setMotorsSpeed(255, 255);
                    blinkRedLights(true);
                    break;
                case STATE_BRAKING:
                    stopMotors();
                    setRedLights(HIGH);
                    vTaskDelay(pdMS_TO_TICKS(300));
                    currentCarState = STATE_STOPPED;
                    break;
                case STATE_TURNING_LEFT:
                    goForward();
                    setMotorsSpeed(100, 255);
                    setRedLights(LOW);
                    break;
                case STATE_TURNING_RIGHT:
                    goForward();
                    setMotorsSpeed(255, 100);
                    setRedLights(LOW);
                    break;
                case STATE_STOPPED:
                    stopMotors();
                    setRedLights(LOW);
                    break;
                default:
                    break;
            }
        }
        setWhiteLights(whiteLightsOn);

        if (millis() - lastCommandTime > TIMEOUT_MS && lastCommandTime != 0) {
            currentCarState = STATE_STOPPED;
            lastCommandTime = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// --- Tarea para gestionar la comunicación Bluetooth (Núcleo 1) ---

void bluetoothManager(void* pvParameters) {
    char command = 'S';
    char lastBTCommand = 'S';
    
    // Configuración BLE para el logcat
    BLEDevice::init("Carro_Logcat");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    BLEService* pService = pServer->createService("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
    pTxCharacteristic = pService->createCharacteristic(
        "beb5483e-36e1-4688-b7f5-ea07361b26a8",
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    pService->start();
    pAdvertising = pServer->getAdvertising();
    pAdvertising->start();

    // Configuración BT Classic para el mando
    SerialBT.begin("Carro_Mando");

    while (true) {
        // Lee comandos del mando (BT Classic)
        if (SerialBT.available()) {
            String receivedCmd = SerialBT.readStringUntil('\n');
            receivedCmd.trim();
            if (receivedCmd.startsWith("#")) {
                currentCarState = STATE_TESTING_COMPONENTS;
                runQuickTest(receivedCmd.charAt(1));
            } else {
                command = receivedCmd.charAt(0);
                lastCommandTime = millis();

                switch (command) {
                    case 'F': currentCarState = STATE_FORWARD; break;
                    case 'B': currentCarState = STATE_BACKWARD; break;
                    case 'L': currentCarState = STATE_TURNING_LEFT; break;
                    case 'R': currentCarState = STATE_TURNING_RIGHT; break;
                    case 'S': currentCarState = STATE_STOPPED; break;
                    case 'P': currentCarState = STATE_BRAKING; break;
                    case 'W': whiteLightsOn = true; break;
                    case 'V': whiteLightsOn = false; break;
                }
            }
        }

        // Monitoreo y logcat (BLE)
        if (pServer->getConnectedCount() > 0) {
            // Lectura de voltaje de batería (Nota: necesita un divisor de voltaje)
            batteryVoltage = analogRead(BATTERY_VOLTAGE_PIN) * (5.0 / 4095.0) * 2;
            batteryPercentage = constrain(((batteryVoltage - 6.0) / (8.4 - 6.0)) * 100, 0, 100);
            
            // Simulación de temperatura y estabilidad
            temperature = 25.0 + random(0, 50) / 10.0;
            String stability = (millis() - lastCommandTime < 500) ? "BUENA" : "DEBIL";

            // Formato de los datos a enviar
            String logData = "Estado_Motor:" + motorState + "|Velocidad:" + String(motorSpeedLeft) + "," + String(motorSpeedRight) + "|Bateria:" + String(batteryPercentage) + "%|Voltaje:" + String(batteryVoltage) + "V|Temp:" + String(temperature) + "C|Estabilidad:" + stability;
            
            pTxCharacteristic->setValue(logData.c_str());
            pTxCharacteristic->notify();
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// --- Callbacks y configuración inicial ---

class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer, esp_ble_gatts_cb_param_t* param) {
        std::string peerAddress = pServer->getPeerAddress().toString();
        isLogcatConnected = (peerAddress == LOGCAT_MAC);
    }
    void onDisconnect(BLEServer* pServer) {
        isLogcatConnected = false;
        if (!pServer->getConnectedCount()) {
             pServer->startAdvertising();
        }
    }
};

void setup() {
    Serial.begin(115200);

    // Pines de motor y luces
    pinMode(MOTOR_A_IN1, OUTPUT);
    pinMode(MOTOR_A_IN2, OUTPUT);
    pinMode(MOTOR_B_IN3, OUTPUT);
    pinMode(MOTOR_B_IN4, OUTPUT);
    pinMode(WHITE_LIGHT_PIN_FRONT_LEFT, OUTPUT);
    pinMode(WHITE_LIGHT_PIN_FRONT_RIGHT, OUTPUT);
    pinMode(RED_LIGHT_PIN_REAR_LEFT, OUTPUT);
    pinMode(RED_LIGHT_PIN_REAR_RIGHT, OUTPUT);
    pinMode(BATTERY_VOLTAGE_PIN, INPUT);
    
    // Configuración PWM
    ledcSetup(0, 5000, 8);
    ledcSetup(1, 5000, 8);
    ledcAttachPin(MOTOR_A_ENA, 0);
    ledcAttachPin(MOTOR_B_ENB, 1);
    
    xTaskCreatePinnedToCore(carControl, "Car Control Task", 10000, NULL, 1, &carControlTask, 0);
    xTaskCreatePinnedToCore(bluetoothManager, "Bluetooth Manager", 10000, NULL, 1, &bluetoothManagerTask, 1);
}

void loop() {
    vTaskDelete(NULL);
}
