### Guía del Proyecto: Carro RC con ESP32

Este documento detalla el funcionamiento, la configuración y el uso de un carro de control remoto avanzado, construido sobre un ESP32, que integra control de movimiento de baja latencia y un sistema de diagnóstico en tiempo real.

---

### 1. Configuración del Hardware

Para que el proyecto funcione correctamente, conecta los componentes como se detalla a continuación. 

* **ESP32 y L298N (Controlador de Motor):**
    * **Pines de Dirección:** Conecta los pines de control `MOTOR_A_IN1`, `MOTOR_A_IN2`, `MOTOR_B_IN3` y `MOTOR_B_IN4` del L298N a los pines GPIO **26, 27, 14, 12** del ESP32, respectivamente.
    * **Pines de Velocidad (PWM):** Conecta los pines de habilitación `MOTOR_A_ENA` y `MOTOR_B_ENB` del L298N a los pines GPIO **13** y **15** del ESP32.
* **Luces:**
    * Conecta las luces delanteras a los pines GPIO **4** y **16**.
    * Conecta las luces traseras (rojas) a los pines GPIO **17** y **5**.
* **Batería (Monitoreo de Voltaje):**
    * Para medir el voltaje de la batería, usa un **divisor de voltaje** (dos resistencias en serie) para bajar el voltaje de la batería al rango seguro del ADC del ESP32 (0-3.3V). Conecta la salida de este divisor al pin GPIO **34**. 

---

### 2. Archivos de Código

El proyecto consta de dos archivos de código principales, diseñados para trabajar en conjunto:

* **`car_esp32.ino`:** El cerebro del carro. Su lógica se divide en dos tareas principales para optimizar el rendimiento.
    * **Núcleo 0 (Control del Carro):** Se encarga de la lógica de movimiento y el control de los pines de los motores, lo que garantiza una respuesta sin demoras a los comandos de control.
    * **Núcleo 1 (Comunicación):** Maneja las conexiones Bluetooth. Usa **Bluetooth Classic** para un mando de baja latencia y **BLE** para la monitorización de datos del logcat, identificando el dispositivo por su MAC ID.

    > **Código de Alta Calidad** 💎
    > **Rendimiento Óptimo:** El uso de FreeRTOS con dos núcleos del ESP32 minimiza la latencia del control, lo que hace que la respuesta del carro sea casi instantánea.
    > **Código Limpio y Modular:** Las funciones de control de motores y luces están separadas y bien definidas, lo que facilita el mantenimiento y la adición de nuevas funciones.
    > **Diagnóstico Avanzado:** La capacidad de monitoreo de batería, temperatura y estabilidad de conexión te permite tener un control total sobre el estado del carro en tiempo real.

* **`Logcat.py`:** El script de Python que se ejecuta en tu teléfono Android usando **Termux**. Se conecta al ESP32 a través de BLE para leer y mostrar los datos de diagnóstico en tiempo real en un formato legible.

    > **Código Eficiente** ⚡
    > **Interfaz de Usuario Limpia:** El script borra la pantalla y presenta los datos de forma organizada y fácil de leer.
    > **Manejo de Errores Robusto:** El código incluye una reconexión automática en caso de pérdida de conexión, lo que garantiza un monitoreo continuo sin interrupciones.

---

### 3. Lógica del Sistema

El sistema del carro se basa en una arquitectura robusta y de doble propósito:

### A. Movimiento y Control (Bluetooth Classic)

Para un control de movimiento ultra-rápido, el carro utiliza **Bluetooth Classic**. Este protocolo, al estar optimizado para la transmisión continua de datos, es ideal para un control de mando que requiere una respuesta instantánea y fluida.

* **Comandos de un solo carácter:** Para reducir la latencia, el mando envía comandos de un solo carácter, que el ESP32 interpreta de manera atómica.
    * `F`: Avanzar a toda velocidad.
    * `B`: Retroceder.
    * `L`: Girar a la izquierda.
    * `R`: Girar a la derecha.
    * `S`: Detenerse.
    * `P`: Frenar (enciende las luces de freno por un momento).
    * `W`: Encender luces blancas.
    * `V`: Apagar luces blancas.

### B. Diagnóstico y Monitoreo (Bluetooth Low Energy)

El carro mantiene un servicio **BLE** activo para la monitorización de datos. El código identifica tu dispositivo Logcat por su **MAC ID**, lo que garantiza que solo tú puedas acceder a esta información. La monitorización incluye:

* **Estado de los Motores:** Muestra si los motores están en modo `FORWARD`, `BACKWARD`, `STOPPED`, etc.
* **Salud de la Batería:** Proporciona el voltaje y el porcentaje de batería restante.
* **Pruebas Rápidas de Componentes:** Puedes enviar comandos de prueba específicos desde el logcat para verificar el funcionamiento de los componentes clave. 

---

### 4. Cómo Usar el Sistema

### Paso 1: Subir el Código al ESP32

1.  Abre el código `car_esp32.ino` en el **IDE de Arduino**.
2.  **Importante:** Reemplaza el valor de `#define LOGCAT_MAC` con la MAC de tu teléfono Android.
3.  Selecciona la placa ESP32 y el puerto correcto, y sube el código.

### Paso 2: Usar Termux como Logcat

1.  En tu teléfono, abre **Termux** y asegúrate de tener las utilidades necesarias instaladas (`python` y `bluepy`).
2.  Crea un archivo llamado `Logcat.py` y copia el código proporcionado. **Recuerda** reemplazar la MAC en el script con la de tu ESP32.
3.  Ejecuta el script con el comando: `python Logcat.py`.

### Paso 3: Realizar Pruebas de Componentes

Para una verificación rápida, puedes usar una aplicación de terminal Bluetooth en tu teléfono.

1.  Instala una aplicación de terminal Bluetooth, como **"Serial Bluetooth Terminal"**.
2.  Conéctate al ESP32 a través de Bluetooth Classic (el nombre del dispositivo será `Carro_Mando`).
3.  Envía los siguientes comandos, seguidos de un salto de línea (`\n`), para iniciar las pruebas:
    * `#F`: Realiza una prueba de movimiento de los motores.
    * `#L`: Realiza una prueba de encendido y apagado de las luces.
