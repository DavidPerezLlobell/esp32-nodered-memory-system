# Sistema IoT Distribuido de Memoria en ESP32 (Simón Dice)

[![ESP32](https://img.shields.io/badge/Hardware-ESP32--C3-blue?logo=espressif&logoColor=white)](https://www.espressif.com/)
[![FreeRTOS](https://img.shields.io/badge/RTOS-FreeRTOS-green?logo=freertos&logoColor=white)](https://www.freertos.org/)
[![MQTT](https://img.shields.io/badge/Protocolo-MQTT-purple?logo=mqtt&logoColor=white)](https://mqtt.org/)
[![Node-RED](https://img.shields.io/badge/Backend-Node--RED-red?logo=nodered&logoColor=white)](https://nodered.org/)
[![MongoDB](https://img.shields.io/badge/Base_de_Datos-MongoDB-brightgreen?logo=mongodb&logoColor=white)](https://www.mongodb.com/)
[![Telegram](https://img.shields.io/badge/Bot-Telegram-blue?logo=telegram&logoColor=white)](https://core.telegram.org/bots)

Un ecosistema IoT distribuido y modular diseñado para ejecutar el juego de memoria interactivo "Simón Dice" mediante microcontroladores ESP32 independientes. La arquitectura desacopla las operaciones de E/S de hardware en nodos de procesamiento dedicados que se comunican en tiempo real mediante **MQTT** y multitarea en **FreeRTOS**, respaldados por una capa de orquestación en **Node-RED**, almacenamiento persistente en **MongoDB** y un sistema automatizado de actualización remota **FOTA (Firmware Over-The-Air)**.

---

## 📐 Arquitectura del Sistema

El sistema segrega las responsabilidades en tres nodos de microcontrolador dedicados y una capa de servicios de backend/cloud:

```
                  +-----------------------------------+
                  |        Node-RED / Docker Hub      |
                  |  (MQTT Broker, MongoDB, Telegram) |
                  +-----------------+-----------------+
                                    |
          +-------------------------+-------------------------+
          | MQTT (II9/#)            | MQTT (II9/#)            | MQTT (II9/#)
          v                         v                         v
+-------------------+     +-------------------+     +-------------------+
|   Nodo Secuencias |     |    Nodo Teclado   |     |   Nodo Interfaz   |
|  (Lógica y LEDs)  |     |  (Entrada 4x4)    |     |  (LCD 16x2/Buzzer)|
|   [ESP32-DevC3]   |     |   [ESP32-DevC3]   |     |   [ESP32-DevC3]   |
+-------------------+     +-------------------+     +-------------------+
```

### 1. Nodos de Hardware (Firmware)

* **Nodo Secuencias (`firmware/sequence-node/sequence-node.ino`)**:
  * **Función**: Motor lógico central del juego.
  * **Características**: Genera patrones dinámicos pseudoaleatorios (`std::vector<int>`), incrementa los niveles de dificultad, controla 9 LEDs de salida (GPIOs 0-8) y transmite la secuencia en formato CSV mediante MQTT.
* **Nodo Teclado (`firmware/keypad-node/keypad-node.ino`)**:
  * **Función**: Captura de entradas de usuario y validación en tiempo real.
  * **Características**: Escanea un teclado matricial 4x4 (GPIOs 0-7), implementa antirrebote mediante `Keypad.h` y ejecuta una comparación inmediata de tipo **fail-fast** respecto a la secuencia objetivo mediante sincronización por notificaciones de tarea en FreeRTOS (`xTaskNotifyGive`).
* **Nodo Interfaz (`firmware/interface-node/interface-node.ino`)**:
  * **Función**: Retroalimentación audiovisual y visualización de estado.
  * **Características**: Controla una pantalla LCD 16x2 I2C (SDA: GPIO 1, SCL: GPIO 2) mostrando mensajes rotativos de ánimo y error, genera señales acústicas con un zumbador pasivo (GPIO 5) y conmuta indicadores de estado externos.

---

## 🔄 Mantenimiento Transversal FOTA

Cada nodo ESP32 integra una tarea en segundo plano (`OTATriggerTask`) que gestiona actualizaciones de firmware HTTP Over-The-Air desde un servidor central (`http://10.10.10.20:1880/esp32-ota/update`):

* **Comprobación al Arranque**: Verifica la existencia de nuevas versiones al encender o reiniciar el dispositivo.
* **Sondeo Periódico**: Ejecuta una verificación automática cada 10 minutos (`OTA_INTERVAL_MS = 600000`).
* **Disparador Manual por Hardware**: La pulsación del botón físico `BOOT` (GPIO 9) fuerza una actualización inmediata por aire.

---

## 📡 Diccionario de Mensajería MQTT

| Tópico | Tipo | Dirección | Descripción |
| :--- | :--- | :--- | :--- |
| `II9/secuencia_actual` | Carga útil | Secuencias -> Teclado | Secuencia numérica objetivo en formato CSV (ej. `1,4,2`). |
| `II9/pulsacion` | Telemetría | Teclado -> Broker | Flujo de pulsaciones en tiempo real. |
| `II9/comparacion` | Control | Teclado -> Todos | Resultado de la validación (`OK` / `FAIL`). |
| `II9/nivel_actual` | Telemetría | Secuencias -> LCD/Dash | Nivel activo de dificultad del juego. |
| `II9/almacena_nivel` | Datos | Secuencias -> MongoDB | Carga útil enviada al finalizar la partida para registro. |
| `II9/muestra_usuario` | Estado | Backend -> LCD | Nombre del perfil del jugador activo. |
| `II9/led` | Indicador | Interfaz -> Hardware | Señal visual externa (`1` = acierto, `0` = fallo). |

---

## 📊 Backend y Servicios

* **Dashboard en Node-RED**:
  * Visualización de telemetría en tiempo real, indicadores de estado e interfaz de consulta de récords históricos por usuario y fecha.
  * Nodos de agregación para el cálculo de estadísticas (nivel mínimo, máximo y promedio por jugador).
* **Persistencia en MongoDB**:
  * Almacenamiento de sesiones en la colección `niveles_simon_dice_json`, registrando marcas temporales (`Date`), identificador de usuario (`usuario`) y nivel máximo alcanzado (`nivel`).
* **Bot de Telegram**:
  * `/start`: Muestra el menú interactivo y la visión general del sistema.
  * `/Nuevo_usuario [nombre]`: Registra la sesión del jugador e inicia una nueva partida.
  * `/Nivel_actual`: Consulta la puntuación en vivo y el estado actual.

---

## 🛠️ Configuración de Hardware y Patillaje (Pinout)

| Nodo | Periférico | Interfaz / Asignación de Pines |
| :--- | :--- | :--- |
| **Nodo Secuencias** | Matriz de 9 LEDs | GPIO 0, 1, 2, 3, 4, 5, 6, 7, 8 |
| **Nodo Teclado** | Teclado Matricial 4x4 | Filas: GPIO 4, 5, 6, 7 \| Columnas: GPIO 0, 1, 2, 3 |
| **Nodo Interfaz** | Pantalla LCD 16x2 I2C | SDA: GPIO 1 \| SCL: GPIO 2 |
| **Nodo Interfaz** | Zumbador Pasivo | GPIO 5 |
| **Todos los Nodos** | Botón Manual FOTA | GPIO 9 (`BOOT_BUTTON`) |

---

## 📁 Estructura del Repositorio

```text
.
├── firmware/
│   ├── keypad-node/
│   │   └── keypad-node.ino        # Controlador del teclado matricial y validación fail-fast
│   ├── interface-node/
│   │   └── interface-node.ino     # Pantalla LCD I2C, zumbador y retroalimentación
│   └── sequence-node/
│       └── sequence-node.ino      # Motor de lógica de juego y matriz de LEDs
├── backend/
│   └── node-red-flows.json        # Flujos de Node-RED (MQTT, MongoDB, Telegram, Dashboard)
├── docs/
│   └── Proyecto_IoT_Simon_Dice.pdf # Memoria técnica y especificación de arquitectura
└── README.md
```

---

## 🚀 Guía de Inicio Rápido

### 1. Compilación y Carga de Firmware
1. Abre cualquiera de los proyectos `.ino` dentro de `firmware/` utilizando **Arduino IDE** o **PlatformIO**.
2. Instala las librerías requeridas: `PubSubClient`, `Keypad`, `LiquidCrystal_I2C`, `HTTPUpdate`, `HTTPClient`.
3. Selecciona la placa **ESP32-C3** y conecta el módulo correspondiente por USB.
4. Carga el firmware en cada placa. Las actualizaciones posteriores podrán desplegarse mediante el servidor FOTA.

### 2. Despliegue de Servicios Backend
1. Importa el archivo `backend/node-red-flows.json` en tu instancia de **Node-RED**.
2. Configura la conexión al broker MQTT (`1883`) y a la base de datos MongoDB (`27017`).
3. Introduce el Token de tu bot de Telegram en la configuración del nodo correspondiente.
4. Despliega el flujo para activar el panel web, el registro en base de datos y la interacción por Telegram.

---

## 📄 Licencia

Este proyecto está bajo la Licencia MIT. Consulta el archivo `LICENSE` para más información.
