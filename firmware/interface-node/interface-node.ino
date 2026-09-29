#include <WiFi.h>
#include <PubSubClient.h>
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
//-------------------------------------------------------------

#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27, 16, 2);//crear un objeto lcd (DIRECCIÓN pantalla, Tamaño x, Tamño y)
//EN DEV_C3 -> pines 8 SDA 9 SCL
//EN DEV -> pines 21 SDA 22 SCL

// definimos macro para indicar tarea, función y línea de código en los mensajes
#define DEBUG_STRING "["+String(pcTaskGetName(NULL))+" - "+String(__FUNCTION__)+"():"+String(__LINE__)+"]   "

// --- WiFi/MQTT ---
WiFiClient wClient;
PubSubClient mqtt_client(wClient);

const String ssid = "infind";//"BRPASILLO2";
const String password = "1518wifi";//"@BdR001-312#";
const String mqtt_server = "iot.ac.uma.es";
const String mqtt_user = "II9";
const String mqtt_pass = "xLNxhmOm";

String ID_PLACA;

String topic_PUBLICACION_led;
String topic_SUSCRIPCION_comparacion;
String topic_SUSCRIPCION_nivel_actual;
String topic_SUSCRIPCION_muestra_usuario;




#define PERIODO_PUBLICACION 3000

// --- FreeRTOS ---
SemaphoreHandle_t semMqttReady;

// --- Variables de programa ---

int frases_good = 1;
int frases_bad = 1; 
int comparacion = 3; 

int pinBuzzer = 5;
int Do = 2088;//1044;//261;
int Re = 2344;//1172;//293;
int Mi = 2632;//1316;//329;
int Fa = 2792;//1396;//349;
int Faa = 2960;//1480;//370;
int Sol = 3136;//1568;//392;
int La = 3520;//1760;//440;
int Si = 3944;//1972;//493;
int DO = 4160;//2080;//520;
int RE = 4800;//2400;//600;

int negra = 100;
int blanca = 200;
int retardo = 3000;

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

//-----------------------------------------------------
// Hace sonar el buzzer
//-----------------------------------------------------

void alarma(){

  if(comparacion==1){

    tone(pinBuzzer,Do, negra);
    vTaskDelay(pdMS_TO_TICKS(negra));
    noTone(pinBuzzer);
    vTaskDelay(pdMS_TO_TICKS(negra));
    tone(pinBuzzer,Mi, negra);
    vTaskDelay(pdMS_TO_TICKS(negra));
    noTone(pinBuzzer);
    vTaskDelay(pdMS_TO_TICKS(negra));
    tone(pinBuzzer,Sol, negra);
    vTaskDelay(pdMS_TO_TICKS(negra));
    noTone(pinBuzzer);
    tone(pinBuzzer,DO, negra);
    vTaskDelay(pdMS_TO_TICKS(negra));
    noTone(pinBuzzer);
    vTaskDelay(pdMS_TO_TICKS(negra));
    tone(pinBuzzer,DO, negra);
    vTaskDelay(pdMS_TO_TICKS(negra/2));
    noTone(pinBuzzer);
    vTaskDelay(pdMS_TO_TICKS(negra/2));
    tone(pinBuzzer,DO, blanca);
    vTaskDelay(pdMS_TO_TICKS(blanca));
    noTone(pinBuzzer);
    vTaskDelay(pdMS_TO_TICKS(blanca));
  }

  else{

    tone(pinBuzzer,Sol, negra);
    vTaskDelay(pdMS_TO_TICKS(negra));
    noTone(pinBuzzer);
    vTaskDelay(pdMS_TO_TICKS(negra));
    tone(pinBuzzer,Faa, negra);
    vTaskDelay(pdMS_TO_TICKS(negra));
    noTone(pinBuzzer);
    vTaskDelay(pdMS_TO_TICKS(negra));
    tone(pinBuzzer,Fa, negra);
    vTaskDelay(pdMS_TO_TICKS(negra));
    noTone(pinBuzzer);
  
  }

}

//-----------------------------------------------------
// muestra información de la tarea
//-----------------------------------------------------
inline void info_tarea_actual() { 
 Serial.println(DEBUG_STRING+"Prioridad de tarea "+ String(pcTaskGetName(NULL))+": "+String(uxTaskPriorityGet(NULL)));
}

//-----------------------------------------------------
// Callback MQTT → recibe mensajes y los procesa
//-----------------------------------------------------
void procesa_mensaje(char* topic, byte* payload, unsigned int length) { 
  String mensaje="";

  for(int i=0; i<length; i++) mensaje += (char)payload[i];
  Serial.println(DEBUG_STRING+"Mensaje recibido ["+ String(topic) +"] \"" + mensaje + "\"");

  
  if(String(topic)==topic_SUSCRIPCION_comparacion) {
    
    if (mensaje =="OK"){comparacion = 1;}
    else if (mensaje =="FAIL"){comparacion = 0;}
      
  }

  if(String(topic)==topic_SUSCRIPCION_nivel_actual) {
    
    lcd.clear();
    lcd.setCursor(1,0);
    lcd.print("Nivel: ");
    lcd.setCursor(1,10);
    lcd.print(mensaje);
  
      
  }

  if(String(topic)==topic_SUSCRIPCION_muestra_usuario) {
    
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Usuario: ");
    lcd.setCursor(1,0);
    lcd.print(mensaje);
          
  }

  Serial.println(DEBUG_STRING+"hemos comparado y entrado");


      
}

//-----------------------------------------------------
// Conexión con WiFi
//-----------------------------------------------------
void conecta_wifi() {

  Serial.println(DEBUG_STRING+"Connecting to " + ssid);
 
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(pdMS_TO_TICKS(100));
    Serial.print(".");
  }
    // TAREA OTA INICIAL
  xTaskCreate(OTA_Task, "OTA Task", 8192, NULL, 5, NULL);
  Serial.println();
  Serial.println(DEBUG_STRING+"WiFi connected, IP address: " + WiFi.localIP().toString());
}

//-----------------------------------------------------
// Conexión con MQTT
//-----------------------------------------------------
void conecta_mqtt() {

  // Loop until we're reconnected
  while (!mqtt_client.connected()) {

    Serial.println(DEBUG_STRING+"Attempting MQTT connection...");
    // Attempt to connect
    if (mqtt_client.connect(ID_PLACA.c_str(), mqtt_user.c_str(), mqtt_pass.c_str(), "II9/HA/ESP32C3/online",1,true,"false")) {//el true es para que se retenga el mensaje, el false es para enviar el mensaje de last will
     
      mqtt_client.publish("II9/HA/ESP32C3/online", "true");
      Serial.println(DEBUG_STRING+" conectado a broker: " + mqtt_server);

      mqtt_client.subscribe(topic_SUSCRIPCION_comparacion.c_str());
      mqtt_client.subscribe(topic_SUSCRIPCION_nivel_actual.c_str());
      mqtt_client.subscribe(topic_SUSCRIPCION_muestra_usuario.c_str());
    
     
    } else {
       Serial.println(DEBUG_STRING+"ERROR:"+ String(mqtt_client.state()) +" reintento en 5s" );
      // Wait 5 seconds before retrying
      vTaskDelay(pdMS_TO_TICKS(5000));
    }
  }
}

//-----------------------------------------------------
//   TAREAS FreeRTOS
//-----------------------------------------------------

//-----------------------------------------------------
// Mantener conexión y ejecutar loop MQTT
void taskMQTTService(void *pvParameters) {

  info_tarea_actual();
  // Inicialización de WiFi
  conecta_wifi();
  // Preparar identificadores
  ID_PLACA = String(WiFi.getHostname());

  topic_PUBLICACION_led = "II9/led";
  topic_SUSCRIPCION_comparacion ="II9/comparacion";
  topic_SUSCRIPCION_nivel_actual ="II9/nivel_actual";
  topic_SUSCRIPCION_muestra_usuario ="II9/muestra_usuario";
  

  Serial.println(DEBUG_STRING+"Identificador placa : "+ ID_PLACA);

  Serial.println(DEBUG_STRING+"Topic publicacion : "+ topic_PUBLICACION_led);

  Serial.println(DEBUG_STRING+"Topic suscripcion : "+ topic_SUSCRIPCION_comparacion);


  // Inicializar cliente MQTT
  mqtt_client.setServer(mqtt_server.c_str(), 1883);
  mqtt_client.setBufferSize(512);
  mqtt_client.setCallback(procesa_mensaje);

  // Conectar a MQTT
  conecta_mqtt();

  Serial.println(DEBUG_STRING+"Semaforo abierto...");

  // Señalizar que MQTT ya está listo
  xSemaphoreGive(semMqttReady);

  // Bucle principal de servicio MQTT
  while(true) {
    if (!mqtt_client.connected()) conecta_mqtt();
    mqtt_client.loop();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}


void Controlador(void *pvParameters) {

  info_tarea_actual();

  // Esperar a que MQTT esté listo
  xSemaphoreTake(semMqttReady, portMAX_DELAY);
  Serial.println(DEBUG_STRING+"Tarea publicadora supera el semáforo");

  while(true){


  if(comparacion==1){

    String mensaje="1";
  
    Serial.println(DEBUG_STRING+"Publicando: " + mensaje);
    mqtt_client.publish(topic_PUBLICACION_led.c_str(), mensaje.c_str());
    
    

   //Escribimos por lcd


    lcd.clear();
    lcd.setCursor(0,0);
    switch (frases_good){

      case 1:
      lcd.print("Sigue asi!!!");
      break;

      case 2:
      lcd.print("Eres un makina");
      break;

      case 3:
      lcd.print("Estas en racha");
      break;

      case 4:
      lcd.print("Que memoria...");
      break;


    }
    
    frases_good++;
    if(frases_good == 5){frases_good = 1;}


    alarma();

    comparacion = 2;


  }
  else if(comparacion == 0){

    String mensaje="0";
      
    Serial.println();
    Serial.println(DEBUG_STRING+"Publicando: " + mensaje);
    mqtt_client.publish(topic_PUBLICACION_led.c_str(), mensaje.c_str());
    
    
    
    lcd.clear();
    lcd.setCursor(0,0);
    switch (frases_bad){

      case 1:
      lcd.print("Ahhh fallaste");
      break;

      case 2:
      lcd.print("Eres lamentable");
      break;

      case 3:
      lcd.print("No vales para na");
      break;

      case 4:
      lcd.print("Memoria de pez");
      break;


    }
    

    frases_bad++;
    if(frases_bad == 5){frases_bad = 1;}

    
    alarma();
    
    comparacion = 2;


  }
  else if(comparacion == 3){

    // Escribimos el Mensaje en el LCD en una posición  central.
    lcd.setCursor(3, 0);
    lcd.print("SIMON DICE:");
    lcd.setCursor(0, 1);
    lcd.print("JUEGO DE MEMORIA");

    comparacion =2;

  }
  /*else if(comparacion == 4){

      lcd.scrollDisplayLeft(); 
      delay(100);


  }
  */


  

    
  vTaskDelay(pdMS_TO_TICKS(PERIODO_PUBLICACION));  

  


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
//   SETUP
//-----------------------------------------------------
void setup() {
  
  
  Serial.begin(115200);
  Serial.println();

  // Inicializa I2C en los nuevos pines
  Wire.begin(1, 2);   // SDA = GPIO1, SCL = GPIO2

  //INICIALIZACIÓN LCD
  lcd.init();
  
  //Encender la luz de fondo.
  lcd.backlight();

    //NECESARIO OTA
  pinMode(BOOT_BUTTON, INPUT_PULLUP);   // <--- NUEVO

  
  //Crear semaforo
  semMqttReady = xSemaphoreCreateBinary();
  info_tarea_actual();

  // Arrancar primero la tarea MQTT (que inicializa conexión)
  xTaskCreate(taskMQTTService, "MQTT Service", 4096, NULL, 2, NULL);

  // Arrancar la otra tarea (esperará al semáforo de MQTT para publicar)
  xTaskCreate(Controlador,   "Publisher",   4096, NULL, 1, NULL);

  
  // NUEVA TAREA DE MONITORIZACIÓN (NECESARIO OTA)
  xTaskCreate(OTATriggerTask, "OTATriggerTask", 4096, NULL, 2, NULL);

  Serial.println(DEBUG_STRING+"Setup terminado, esperando conexión MQTT...");



}

//-----------------------------------------------------
void loop() {
  // vacío: todo lo hacen las tareas FreeRTOS
}
