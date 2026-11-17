#include <Arduino.h>
#include <string.h>
#include <stdlib.h>
#include "apwifieeprommode.h"

// Proyecto: brazo clasificador de residuos - primer prototipo APWifi.
// Arduino-ESP32 2.x / PlatformIO esp32dev.
// Conserva las calibraciones definidas en pruebas anteriores.

const char* SSID_AP = "BrazoReciclaje";
const char* PASS_AP = "CambiarClave123"; // Cambiar antes de demostraciones publicas.

const uint32_t PWM_FRECUENCIA = 50;
const uint8_t PWM_BITS = 16;
const uint32_t PWM_PERIODO_US = 20000;
const uint32_t PASO_CADA_MS = 20;
const int PASO_US = 10;

struct ServoConfig {
  const char* nombre;
  uint8_t gpio;
  uint8_t canal;
  int anguloMax;
  int minUS;
  int maxUS;
  int centroUS;
  int actualUS;
  int objetivoUS;
  bool iniciado;
};

ServoConfig servos[5] = {
  {"J1", 25, 0, 180,  2500, 700, 1600, 1600, 1600, false},
  {"J2", 26, 1,  90, 1000, 2200, 1600, 1600, 1600, false},
  {"J3", 27, 2,   0, 1400, 1400, 1400, 1400, 1400, false},
  {"J4", 32, 3, 150, 1000, 2500, 1750, 1750, 1750, false},
  {"G",  33, 4,  45, 1000, 2000, 1550, 1550, 1550, false}
};

bool sistemaActivo = true;
bool j3Habilitado = true;  // Poner false si J3 se bloquea o se calienta.
unsigned long ultimoPaso = 0;
String lineaSerial;

void escribirPWM(int i, int pulso) {
  uint32_t duty = ((uint64_t)pulso * (1ULL << PWM_BITS)) / PWM_PERIODO_US;
  ledcWrite(servos[i].canal, duty);
}

int anguloAPulso(int idx, int angulo) {
  const ServoConfig &s = servos[idx];
  return s.minUS + (s.maxUS - s.minUS) * angulo / s.anguloMax;
}

bool programarMovimiento(int idx, int angulo) {
  if (!sistemaActivo || idx == 2 || idx < 0 || idx > 4) return false;
  ServoConfig &s = servos[idx];
  if (angulo < 0 || angulo > s.anguloMax) return false;
  if (!s.iniciado) {
    s.actualUS = s.centroUS;
    escribirPWM(idx, s.actualUS); // Primer comando: inicia desde centro estimado.
    s.iniciado = true;
  }
  s.objetivoUS = anguloAPulso(idx, angulo);
  Serial.printf("OBJETIVO %s: %d grados -> %d us\n",
                s.nombre, angulo, s.objetivoUS);
  return true;
}

void actualizarMovimiento() {
  if (!sistemaActivo || millis() - ultimoPaso < PASO_CADA_MS) return;
  ultimoPaso = millis();
  for (int i = 0; i < 5; i++) {
    if (i == 2 || !servos[i].iniciado) continue;
    ServoConfig &s = servos[i];
    if (s.actualUS == s.objetivoUS) continue;
    int diferencia = s.objetivoUS - s.actualUS;
    if (diferencia > PASO_US) diferencia = PASO_US;
    if (diferencia < -PASO_US) diferencia = -PASO_US;
    s.actualUS += diferencia;
    escribirPWM(i, s.actualUS);
  }
}

void desactivarTodo() {
  sistemaActivo = false;
  for (int i = 0; i < 5; i++) {
    ledcWrite(servos[i].canal, 0);
    servos[i].iniciado = false;
  }
  Serial.println("OFF: PWM desactivado (no corta la corriente)");
}

void activarSistema() {
  sistemaActivo = true;
  if (j3Habilitado) escribirPWM(2, 1400);
  Serial.println("ON: sistema habilitado; enviar orden a cada articulacion");
}

void desactivarJ3() {
  j3Habilitado = false;
  ledcWrite(servos[2].canal, 0);
  Serial.println("J3OFF: PWM de J3 desactivado");
}

int buscarServo(const String &nombre) {
  for (int i = 0; i < 5; i++) {
    if (nombre.equalsIgnoreCase(servos[i].nombre)) return i;
  }
  return -1;
}

bool parsearEntero(const String &cadena, int &salida) {
  if (cadena.length() == 0) return false;
  int n = 0;
  for (size_t i = 0; i < cadena.length(); i++) {
    if (!isDigit(cadena[i])) return false;
    n = n * 10 + (cadena[i] - '0');
    if (n > 1000) return false;
  }
  salida = n;
  return true;
}

void respuestaJson(int codigo, bool ok, const String &mensaje) {
  // Los mensajes utilizados son constantes, sin comillas ni saltos de linea.
  APWifi::server.send(codigo, "application/json",
                      String("{\"ok\":") + (ok ? "true" : "false") +
                      ",\"mensaje\":\"" + mensaje + "\"}");
}

// Interfaz HTTP. Los deslizadores solo cambian valores en el navegador;
// el servo se mueve al pulsar el boton Mover.
static const char PAGINA[] PROGMEM = R"HTML(
<!doctype html><html lang="es"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Brazo clasificador</title>
<style>
:root{color-scheme:dark}*{box-sizing:border-box}
body{font-family:system-ui,Arial;margin:0;background:#101827;color:#edf2f7}
main{max-width:760px;margin:auto;padding:22px}h1{font-size:1.5rem;margin-bottom:7px}
p{color:#aabbd0}header{display:flex;justify-content:space-between;gap:12px;flex-wrap:wrap}
.badge{padding:7px 10px;border:1px solid #385170;border-radius:14px;color:#bcd3f0;align-self:center}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(260px,1fr));gap:12px}
.card{background:#1a2638;border:1px solid #31465d;border-radius:13px;padding:17px}
.card h2{margin:0 0 12px;font-size:1.07rem}
input[type=range]{width:100%;accent-color:#49a6ff;margin:18px 0}
label{color:#cbd9e8;font-size:.95rem}button,a.link{background:#2b83ce;color:white;border:0;padding:11px 16px;border-radius:9px;cursor:pointer;font-size:1rem;text-decoration:none;display:inline-block}
button:active{transform:translateY(1px)}.off{background:#b13c48}.muted{color:#b4c2d3}
.actions{display:flex;gap:9px;flex-wrap:wrap;margin:16px 0}#respuesta{min-height:24px;margin:14px 0;color:#a9d9ff}
small{color:#abbad0}
</style></head><body><main>
<header><div><h1>Brazo clasificador de residuos</h1><p>Control manual del primer prototipo · ESP32 APWifi</p></div>
<div class="badge" id="estado">Consultando estado...</div></header>
<section class="actions">
<button class="off" onclick="potencia('OFF')">Desactivar PWM</button>
<button onclick="potencia('ON')">Reactivar</button>
<button class="off" onclick="potencia('J3OFF')">Desactivar solo J3</button>
<a class="link" href="/wifi-config">Configurar WiFi</a>
</section><p id="respuesta"></p><div class="grid">
<div class="card"><h2>J1 · Base</h2><label>0° a 180° · <b id="vJ1">90°</b></label>
<input id="J1" type="range" min="0" max="180" value="90" oninput="ver('J1')"><button onclick="mover('J1')">Mover J1</button></div>
<div class="card"><h2>J2 · Hombro</h2><label>0° a 90° · <b id="vJ2">45°</b></label>
<input id="J2" type="range" min="0" max="90" value="45" oninput="ver('J2')"><button onclick="mover('J2')">Mover J2</button></div>
<div class="card"><h2>J4 · Muñeca</h2><label>0° a 150° · <b id="vJ4">75°</b></label>
<input id="J4" type="range" min="0" max="150" value="75" oninput="ver('J4')"><button onclick="mover('J4')">Mover J4</button></div>
<div class="card"><h2>Gripper</h2><label>0° cerrado / 45° abierto · <b id="vG">22°</b></label>
<input id="G" type="range" min="0" max="45" value="22" oninput="ver('G')"><button onclick="mover('G')">Mover gripper</button></div>
<div class="card"><h2>J3 · Codo</h2><p>Consigna fija: <b>1400 µs</b></p>
<small>J3 no acepta movimientos desde esta pagina. Si zumba o se calienta, desconectalo y utiliza una sujecion mecanica.</small></div>
</div><p class="muted">Los angulos son consignas estimadas: no hay sensores de posicion. La opcion OFF no desconecta la alimentacion.</p>
<script>
function ver(id){document.getElementById('v'+id).textContent=document.getElementById(id).value+'°';}
async function peticion(path,datos){
  try{
    const r=await fetch(path,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams(datos)});
    const x=await r.json();document.getElementById('respuesta').textContent=x.mensaje;
  }catch(e){document.getElementById('respuesta').textContent='Sin comunicacion con ESP32';}
  estado();
}
function mover(id){peticion('/api/move',{joint:id,angle:document.getElementById(id).value});}
function potencia(accion){peticion('/api/power',{accion:accion});}
async function estado(){try{const r=await fetch('/api/status');const x=await r.json();
 document.getElementById('estado').textContent=x.activo?'PWM habilitado':'PWM desactivado';
}catch(e){document.getElementById('estado').textContent='Sin conexion';}}
setInterval(estado,4000);estado();
</script></main></body></html>
)HTML";

void handleIndex() {
  APWifi::server.send_P(200, "text/html; charset=utf-8", PAGINA);
}

void handleMove() {
  String nombre = APWifi::server.arg("joint");
  int idx = buscarServo(nombre);
  int angulo;
  if (idx == 2) {
    respuestaJson(403, false, "J3 permanece fijo en 1400 us");
    return;
  }
  if (idx < 0 || !parsearEntero(APWifi::server.arg("angle"), angulo)) {
    respuestaJson(400, false, "Articulacion o angulo invalido");
    return;
  }
  if (!sistemaActivo) {
    respuestaJson(409, false, "Sistema desactivado; enviar ON");
    return;
  }
  if (!programarMovimiento(idx, angulo)) {
    respuestaJson(400, false, "Angulo fuera de los limites calibrados");
    return;
  }
  respuestaJson(200, true, String("Objetivo ") + nombre + ": " + angulo + " grados");
}

void handlePower() {
  String a = APWifi::server.arg("accion");
  a.toUpperCase();
  if (a == "OFF") { desactivarTodo(); respuestaJson(200, true, "PWM desactivado"); }
  else if (a == "ON") { activarSistema(); respuestaJson(200, true, "Sistema activo"); }
  else if (a == "J3OFF") { desactivarJ3(); respuestaJson(200, true, "J3 sin PWM"); }
  else respuestaJson(400, false, "Accion no reconocida");
}


void handleStatus() {

  bool conectado = (WiFi.status() == WL_CONNECTED);

  String salida = String("{\"activo\":") +
    (sistemaActivo ? "true" : "false") +

    ",\"j3\":" +
    (j3Habilitado ? "true" : "false") +

    ",\"wifi_conectado\":" +
    (conectado ? "true" : "false") +

    ",\"wifi_estado\":" +
    String((int)WiFi.status()) +

    ",\"ip_ap\":\"" +
    WiFi.softAPIP().toString() + "\"" +

    ",\"ip_router\":\"" +
    WiFi.localIP().toString() + "\"}";

  APWifi::server.send(
    200,
    "application/json",
    salida
  );
}

void comandoSerial(String linea) {
  linea.trim(); linea.toUpperCase();
  if (linea.length() == 0) return;
  if (linea == "OFF") { desactivarTodo(); return; }
  if (linea == "ON") { activarSistema(); return; }
  if (linea == "J3OFF") { desactivarJ3(); return; }
  if (linea == "J3") {
    Serial.printf("J3: %s, consigna 1400 us\n", j3Habilitado ? "PWM activo" : "PWM desactivado");
    return;
  }
  char n[12], extra;
  int angulo;
  if (sscanf(linea.c_str(), "%11s %d %c", n, &angulo, &extra) != 2) {
    Serial.println("ERROR: Usa J1 90, J2 45, J4 75, G 45, OFF, ON o J3OFF");
    return;
  }
  int idx = buscarServo(String(n));
  if (idx == 2) { Serial.println("J3 fijo en 1400 us"); return; }
  if (!programarMovimiento(idx, angulo)) {
    Serial.println("ERROR: angulo invalido o sistema desactivado");
  }
}

void leerSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (lineaSerial.length() != 0) comandoSerial(lineaSerial);
      lineaSerial = "";
    } else if (lineaSerial.length() < 48) lineaSerial += c;
    else lineaSerial = "";
  }
}

void setup() {
  Serial.begin(115200);
  for (int i = 0; i < 5; ++i) {
    ledcSetup(servos[i].canal, PWM_FRECUENCIA, PWM_BITS);
    ledcAttachPin(servos[i].gpio, servos[i].canal);
    ledcWrite(servos[i].canal, 0);
  }
  // J3 se fija segun la consigna del usuario. Ojo: evitar bloqueo por carga.
  if (j3Habilitado) escribirPWM(2, 1400);

  // Registrar el control ANTES de comenzar el servidor APWifi.
  APWifi::server.on("/", HTTP_GET, handleIndex);
  APWifi::server.on("/api/move", HTTP_POST, handleMove);
  APWifi::server.on("/api/power", HTTP_POST, handlePower);
  APWifi::server.on("/api/status", HTTP_GET, handleStatus);
  APWifi::begin(SSID_AP, PASS_AP);

  Serial.println("BRAZO CLASIFICADOR - APWifi");
  Serial.println("Conectar a BrazoReciclaje; abrir http://192.168.4.1");
  Serial.println("J1 0-180, J2 0-90, J4 0-150, G 0-45; J3=1400us");
}


void loop() {

  APWifi::loopAP();
  leerSerial();
  actualizarMovimiento();

  static unsigned long ultimoReporte = 0;

  if (millis() - ultimoReporte >= 5000) {
    ultimoReporte = millis();

    Serial.println("-------- WIFI --------");

    Serial.print("Estado STA: ");
    Serial.println((int)WiFi.status());

    Serial.print("SSID configurado: ");
    Serial.println(WiFi.SSID());

    Serial.print("IP del AP: ");
    Serial.println(WiFi.softAPIP());

    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("CONECTADO AL ROUTER - IP: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("NO CONECTADO AL ROUTER");
    }
  }

  delay(1);
}

