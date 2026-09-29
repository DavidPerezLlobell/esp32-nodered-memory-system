#include <WiFi.h>
#include <PubSubClient.h>
#include <vector>
#include <HTTPUpdate.h>
#include <HTTPClient.h>

// ---------------------- CONFIGURACIÓN OTA (NECESARIO) ----------------------
#define OTA_URL "http://10.10.10.20:1880/esp32-ota/update"
#define FW_BOARD_NAME ""
#define HTTP_OTA_VERSION strrchr(strrchr(__FILE__, '/') ? __FILE__ : "", '/') ? strrchr(__FILE__, '/') + 1 : strrchr(__FILE__, '\\') + 1

#define RGB_BUILTIN 15

#define DEBUG_STRING "["+String(pcTaskGetName(NULL))+" - "+String(__FUNCTION__)+"():"+String(__LINE__)+"]   "

// --- NUEVO ---
#define BOOT_BUTTON 9                 // Botón BOOT ESP32-C3
#define OTA_INTERVAL_MS 600000        // 10 minutos

// --- WiFi/MQTT ---
WiFiClient wClient;
PubSubClient mqtt_client(wClient);

const String ssid = "infind";
const String password = "1518wifi";
const String mqtt_server = "iot.ac.uma.es";
const String mqtt_user = "II9";
const String mqtt_pass = "xLNxhmOm";

String ID_PLACA;
//String topic_SUSCRIPCION_usuario
String topic_PUBLICACION_Secuencia_LEDs;
String topic_PUBLICACION_Almacena_Nivel;
String topic_SUSCRIPCION_comparacion;
String topic_PUBLICACION_Nivel_Actual;

// secuencia de LEDs
std::vector<int> secuencia;
String seq_string = "";
int nivel = 0;

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

#define DEBUG_STRING "[" + String(pcTaskGetName(NULL)) + " - " + String(__FUNCTION__) + "():" + String(__LINE__) + "]   "

//-----------------------------------------------------
// Callback MQTT
//-----------------------------------------------------
void procesa_mensaje(char* topic, uint8_t* payload, unsigned int length) {
  String mensaje = "";
  for (int i = 0; i < length; i++) mensaje += (char)payload[i];

  Serial.println(DEBUG_STRING + "Mensaje recibido [" + String(topic) + "] \"" + mensaje + "\"");

  if (String(topic) == topic_SUSCRIPCION_comparacion) {
    if (mensaje == "OK") {
      nivel++;
      calculaCombinacion(nivel);
      
      
    } else {
      digitalWrite(RGB_BUILTIN, LOW);

      String nivel_str = String(nivel);
      mqtt_client.publish(topic_PUBLICACION_Almacena_Nivel.c_str(), nivel_str.c_str());

      nivel = 1;
      calculaCombinacion(nivel);
    }
  }
}

//-----------------------------------------------------
// Genera y muestra secuencia
//-----------------------------------------------------
void calculaCombinacion(int nivel) {
  if (nivel == 1) {
    secuencia.clear();
    seq_string = "";
  }

  // nuevo LED random
  int nuevo = random(0, 8);
  secuencia.push_back(nuevo);

  if (seq_string.length() > 0) seq_string += ",";
  seq_string += String(nuevo);

  // Reproducir secuencia
  for (int i = 0; i < secuencia.size(); i++) {
    int led = secuencia[i];
    digitalWrite(led, HIGH);
    delay(500);
    digitalWrite(led, LOW);
    delay(500);
  }

  // publicar secuencia
  mqtt_client.publish(topic_PUBLICACION_Secuencia_LEDs.c_str(), seq_string.c_str());
  String nivel_str = String(nivel);
  mqtt_client.publish(topic_PUBLICACION_Nivel_Actual.c_str(), nivel_str.c_str());

  Serial.println(DEBUG_STRING + "Secuencia enviada: " + seq_string);
}

//-----------------------------------------------------
void conecta_wifi() {
  Serial.println(DEBUG_STRING + "Connecting to " + ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(pdMS_TO_TICKS(100));
    Serial.print(".");
  }
    // TAREA OTA INICIAL
  xTaskCreate(OTA_Task, "OTA Task", 8192, NULL, 5, NULL);
  Serial.println();
  Serial.println(DEBUG_STRING + "WiFi connected, IP: " + WiFi.localIP().toString());
}

//-----------------------------------------------------
void conecta_mqtt() {
  while (!mqtt_client.connected()) {
    Serial.println(DEBUG_STRING + "Attempting MQTT connection...");
    if (mqtt_client.connect(ID_PLACA.c_str(), mqtt_user.c_str(), mqtt_pass.c_str())) {
      Serial.println(DEBUG_STRING + "Conectado a broker");
      mqtt_client.subscribe(topic_SUSCRIPCION_comparacion.c_str());
    } else {
      Serial.println(DEBUG_STRING + "ERROR:" + String(mqtt_client.state()) + " reintento en 5s");
      vTaskDelay(pdMS_TO_TICKS(5000));
    }
  }
}

//-----------------------------------------------------
void taskMQTTService(void* pvParameters) {
  conecta_wifi();

  ID_PLACA = String(WiFi.getHostname());
  topic_PUBLICACION_Secuencia_LEDs = "II9/secuencia_actual";
  topic_PUBLICACION_Almacena_Nivel = "II9/almacena_nivel";
  topic_SUSCRIPCION_comparacion = "II9/comparacion";
  topic_PUBLICACION_Nivel_Actual = "II9/nivel_actual";

  mqtt_client.setServer(mqtt_server.c_str(), 1883);
  mqtt_client.setCallback(procesa_mensaje);

  conecta_mqtt();

  while (true) {
    if (!mqtt_client.connected()) conecta_mqtt();
    mqtt_client.loop();
    vTaskDelay(pdMS_TO_TICKS(10));
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

//-----------------------------------------------------
void setup() {
  Serial.begin(115200);
  Serial.println();

  //NECESARIO OTA
  pinMode(BOOT_BUTTON, INPUT_PULLUP);   // <--- NUEVO

  // Crear tareas
  xTaskCreate(taskMQTTService, "MQTT Service", 4096, NULL, 2, NULL);

  // NUEVA TAREA DE MONITORIZACIÓN (NECESARIO OTA)
  xTaskCreate(OTATriggerTask, "OTATriggerTask", 4096, NULL, 2, NULL);

  pinMode(1, OUTPUT);
  pinMode(2, OUTPUT);
  pinMode(3, OUTPUT);
  pinMode(4, OUTPUT);
  pinMode(5, OUTPUT);
  pinMode(6, OUTPUT);
  pinMode(7, OUTPUT);
  pinMode(8, OUTPUT);
  pinMode(0, OUTPUT);

  vTaskDelete(NULL);  // eliminar loopTask
}

//-----------------------------------------------------
void loop() {}
