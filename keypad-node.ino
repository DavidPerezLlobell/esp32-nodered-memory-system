#include <WiFi.h>
#include <PubSubClient.h>
#include <Keypad.h>
#include <HTTPUpdate.h>
#include <HTTPClient.h>

// ---------------------- CONFIGURACIÓN OTA (NECESARIO) ----------------------
#define OTA_URL "http://10.10.10.20:1880/esp32-ota/update"
#define FW_BOARD_NAME ""
#define HTTP_OTA_VERSION strrchr(strrchr(__FILE__, '/') ? __FILE__ : "", '/') ? strrchr(__FILE__, '/') + 1 : strrchr(__FILE__, '\\') + 1

#define RGB_BUILTIN 15

#define DEBUG_STRING "["+String(pcTaskGetName(NULL))+" - "+String(__FUNCTION__)+"():"+String(__LINE__)+"]   "

// --- NUEVO  ---
#define BOOT_BUTTON 9                 // Botón BOOT ESP32-C3
#define OTA_INTERVAL_MS 600000        // 10 minutos

// ---------------------- VARIABLES GLOBALES ----------------------
volatile bool sistemaOcupado = false;

const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte rowPins[ROWS] = { 4,5,6,7 };  
byte colPins[COLS] = { 0,1,2,3 };

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

#define MAX_SEQ 20
int receivedSequence[MAX_SEQ];
int userSequence[MAX_SEQ];
int seqLength = 0;
int inputIndex = 0;
bool sequenceReceived = false;

bool capturandoUsuario = false;
String usuarioActual = "";

// WiFi / MQTT
WiFiClient wClient;
PubSubClient mqtt_client(wClient);

const String ssid = "infind";
const String password = "1518wifi";
const String mqtt_server = "iot.ac.uma.es";
const String mqtt_user = "II9";
const String mqtt_pass = "xLNxhmOm";

String ID_PLACA;
String topic_RESULTADO;
String topic_SUSCRIPCION;
String topic_SECUENCIA;
String topic_USUARIO;
String topic_ACTUALIZACION; // ahora la inicializamos antes de usarla
String topic_PULSACION;

// FreeRTOS Handles
TaskHandle_t juegoTaskHandle;
TaskHandle_t usuarioTaskHandle;

// ---------------------- FUNCIONES OTA (NECESARIO) ----------------------
void inicio_OTA(){ Serial.println(DEBUG_STRING+"Nuevo Firmware encontrado. Actualizando..."); }
void final_OTA() { Serial.println(DEBUG_STRING+"Fin OTA. Reiniciando..."); }
void error_OTA(int e){ Serial.println(DEBUG_STRING+"ERROR OTA: "+String(e)+" "+httpUpdate.getLastErrorString()); }
void progreso_OTA(int x, int todo)
{
  int progreso=(int)((x*100)/todo);
  if(progreso % 10 == 0) {
    Serial.println(DEBUG_STRING+"Progreso: "+String(progreso)+"% - "+String(x/1024)+"K de "+String(todo/1024)+"K");
  }
}


//--------------NECESARIO OTA----------------------------------------------
void OTA_Task(void* param) {
  WiFiClient otaClient;
  otaClient.setTimeout(15000); // 15 segundos
    Serial.println( "---------------------------------------------" );  
  Serial.print  ( DEBUG_STRING+"MAC de la placa: "); Serial.println(WiFi.macAddress());
  Serial.println( DEBUG_STRING+"Comprobando actualización:" );
  Serial.println( DEBUG_STRING+"URL: "+OTA_URL );
  Serial.println( "---------------------------------------------" );  
  httpUpdate.onStart(inicio_OTA);
  httpUpdate.onProgress(progreso_OTA);
  httpUpdate.onEnd(final_OTA);
  httpUpdate.onError(error_OTA);

  Serial.println(DEBUG_STRING+"Iniciando OTA...");
  t_httpUpdate_return res = httpUpdate.update(otaClient, OTA_URL, HTTP_OTA_VERSION);

  if(res == HTTP_UPDATE_OK) {
    Serial.println(DEBUG_STRING+"OTA completada con éxito.");
  } else if(res == HTTP_UPDATE_NO_UPDATES) {
    Serial.println(DEBUG_STRING+"No hay actualizaciones.");
  } else {
    Serial.println(DEBUG_STRING+"HTTP update failed: Error ("+String(httpUpdate.getLastError())+"): "+httpUpdate.getLastErrorString());
  }

  vTaskDelete(NULL); // eliminar tarea OTA cuando termine
}
//--------------------------------------------------------------------------


// ---------------------- CALLBACK MQTT ----------------------
void procesa_mensaje(char* topic, byte* payload, unsigned int length) { 
  String mensaje="";
  for(int i=0; i<length; i++) mensaje += (char)payload[i];
  Serial.println(DEBUG_STRING+"Mensaje en ["+ String(topic) +"]");

  if(String(topic)==topic_SECUENCIA) {
    Serial.println("Secuencia recibida: " + mensaje);
    sistemaOcupado = true; 
    seqLength = 0;
    inputIndex = 0;
    sequenceReceived = true;
    int start = 0;
    for (int i = 0; i <= mensaje.length(); i++) {
      if (mensaje[i] == ',' || i == mensaje.length()) {
        receivedSequence[seqLength++] = mensaje.substring(start, i).toInt();
        start = i + 1;
      }
    }
    Serial.print("Longitud: "); Serial.println(seqLength);
    Serial.println("Introduce secuencia...");
    xTaskNotifyGive(juegoTaskHandle);
  }
}

// ---------------------- CONEXIÓN WiFi ----------------------
void conecta_wifi() {
  Serial.println(DEBUG_STRING+"Connecting to " + ssid);
 
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(pdMS_TO_TICKS(100));
    Serial.print(".");
  }
  Serial.println();
  Serial.println(DEBUG_STRING+"WiFi connected, IP address: " + WiFi.localIP().toString());
}

// ---------------------- CONEXIÓN MQTT (MEJORADA)

String mqttStateToString(int state) {
  switch(state) {
    case -4: return "MQTT_CONNECTION_TIMEOUT (-4)";
    case -3: return "MQTT_CONNECTION_LOST (-3)";
    case -2: return "MQTT_CONNECT_FAILED (-2)";
    case -1: return "MQTT_DISCONNECTED (-1)";
    case 0: return "MQTT_CONNECTED (0)";
    case 1: return "MQTT_CONNECT_BAD_PROTOCOL (1)";
    case 2: return "MQTT_CONNECT_BAD_CLIENT_ID (2)";
    case 3: return "MQTT_CONNECT_UNAVAILABLE (3)";
    case 4: return "MQTT_CONNECT_BAD_CREDENTIALS (4)";
    case 5: return "MQTT_CONNECT_UNAUTHORIZED (5)";
    default: return "MQTT_STATE_UNKNOWN (" + String(state) + ")";
  }
}

void conecta_mqtt() {
  // Loop until we're reconnected
  while (!mqtt_client.connected()) {
    Serial.println(DEBUG_STRING+"Attempting MQTT connection...");
    // Attempt to connect
    if (mqtt_client.connect(ID_PLACA.c_str(), mqtt_user.c_str(), mqtt_pass.c_str(),"II9/HA/ESP32C3/online",1,true,"false")) {
      mqtt_client.publish("II9/HA/ESP32C3/online", "true");
      Serial.println(DEBUG_STRING+" conectado a broker: " + mqtt_server);
      mqtt_client.subscribe(topic_SECUENCIA.c_str());
      mqtt_client.subscribe(topic_USUARIO.c_str());
    } else {
      Serial.println(DEBUG_STRING+"ERROR:"+ String(mqtt_client.state()) +" reintento en 5s" );
      // Wait 5 seconds before retrying
      vTaskDelay(pdMS_TO_TICKS(5000));
    }
  }
}

// ---------------------- TAREAS ----------------------
void taskMQTTService(void *pvParameters) {
  // ID y topics se inicializan aquí (antes de conectar)
  ID_PLACA = String(WiFi.getHostname());

  topic_RESULTADO = "II9/comparacion";
  topic_SUSCRIPCION = "infind/"+ ID_PLACA +"/recepcion";
  topic_SECUENCIA = "II9/secuencia_actual";
  topic_USUARIO = "II9/usuario";
  topic_ACTUALIZACION = "II9/actualizacion"; // <-- inicializado
  topic_PULSACION = "II9/pulsacion";

  Serial.println(DEBUG_STRING+"Identificador placa : "+ ID_PLACA);
  Serial.println(DEBUG_STRING+"Topics -> secuencia: " + topic_SECUENCIA + " actualizacion: " + topic_ACTUALIZACION);

  mqtt_client.setServer(mqtt_server.c_str(), 1883);
  mqtt_client.setBufferSize(512);
  mqtt_client.setCallback(procesa_mensaje);

  conecta_mqtt();

  while(true) {
    if (!mqtt_client.connected()) {
      Serial.println(DEBUG_STRING + "MQTT desconectado en loop, reintentando conecta_mqtt()...");
      conecta_mqtt();
    }
    mqtt_client.loop();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void JuegoTask(void* param) {
  for (;;) {
    // Espera a que llegue una nueva secuencia
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    Serial.println("JuegoTask: Introduce secuencia...");
    inputIndex = 0;
    bool fallo = false;

    while (inputIndex < seqLength) {
      char key = keypad.getKey();

      if (key) {
        // Solo aceptar dígitos
        if (key < '0' || key > '9') {
          Serial.println("Tecla inválida");
          continue;
        }

        int valor = key - '0';
        Serial.printf("Tecla %d -> %d\n", inputIndex, valor);
        mqtt_client.publish(topic_PULSACION.c_str(), String(valor-1).c_str());

        // COMPARACIÓN INMEDIATA
        if (valor != receivedSequence[inputIndex]) {
          Serial.println("FALLO en pulsación");
          mqtt_client.publish(topic_RESULTADO.c_str(), "FAIL");

          sistemaOcupado = false;
          fallo = true;
          break;  // salir inmediatamente
        }

        inputIndex++;
      }

      vTaskDelay(pdMS_TO_TICKS(10));
    }

    // Si terminó toda la secuencia sin fallos
    if (!fallo && inputIndex == seqLength) {
      mqtt_client.publish(topic_RESULTADO.c_str(), "OK");
      Serial.println("Resultado: OK");
      sistemaOcupado = false;
    }

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// ---------------------- NUEVA TAREA: BOTÓN + OTA PERIÓDICA (NECESARIO OTA) ----------------------
void OTATriggerTask(void *param) {
  unsigned long lastCheck = millis();

  for (;;) {
    // --- OTA POR BOTÓN BOOT ---
    if (digitalRead(BOOT_BUTTON) == LOW) {
      Serial.println(DEBUG_STRING + String("OTA manual por botón BOOT"));
      xTaskCreate(OTA_Task, "OTA Task Button", 8192, NULL, 5, NULL);
      vTaskDelay(pdMS_TO_TICKS(500)); // evita rebotes
    }

    // --- OTA PERIÓDICA ---
    if (millis() - lastCheck > OTA_INTERVAL_MS) {
      lastCheck = millis();
      Serial.println(DEBUG_STRING + String("OTA periódica programada"));
      xTaskCreate(OTA_Task, "OTA Task Timer", 8192, NULL, 5, NULL);
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// ---------------------- SETUP ----------------------
void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println(DEBUG_STRING+"Fuente: "+String(__FILE__));

  conecta_wifi();

  //NECESARIO OTA
  pinMode(BOOT_BUTTON, INPUT_PULLUP);   // <--- NUEVO

  // TAREA OTA INICIAL
  xTaskCreate(OTA_Task, "OTA Task", 8192, NULL, 5, NULL);

  // TAREAS PRINCIPALES
  xTaskCreate(taskMQTTService, "MQTT Service", 8192, NULL, 2, NULL); 
  xTaskCreate(JuegoTask,"JuegoTask",4096,NULL,1,&juegoTaskHandle);

  // NUEVA TAREA DE MONITORIZACIÓN (NECESARIO OTA)
  xTaskCreate(OTATriggerTask, "OTATriggerTask", 4096, NULL, 2, NULL);

  Serial.println(DEBUG_STRING+"Sistema arrancado.");
  vTaskDelete(NULL);
}

void loop() {}
