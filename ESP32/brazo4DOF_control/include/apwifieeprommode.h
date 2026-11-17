#pragma once

// Adaptacion para ESP32 de la logica del repositorio lab-microcon/APWifi:
// AP permanente + formulario WiFi + almacenamiento de dos redes en EEPROM.
// No es una copia literal: evita el bucle bloqueante del ejemplo original.

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <string.h>

namespace APWifi {

static WebServer server(80);

struct Credencial {
  char ssid[33];
  char password[65];
};
struct DatosWifi {
  uint32_t magic;
  uint8_t cantidad;
  uint8_t ultimo;
  Credencial redes[2];
};

static const uint32_t MAGIC = 0xA9012026UL;
static DatosWifi datos = {};
static bool intentoActivo = false;
static int indiceIntento = -1;
static bool alternativaIntentada = false;
static unsigned long inicioIntento = 0;
static unsigned long ultimoReintento = 0;

static String escaparHTML(const String &s) {
  String r;
  for (size_t i = 0; i < s.length(); ++i) {
    switch (s[i]) {
      case '&': r += F("&amp;"); break;
      case '<': r += F("&lt;"); break;
      case '>': r += F("&gt;"); break;
      case '"': r += F("&quot;"); break;
      case '\'': r += F("&#39;"); break;
      default: r += s[i]; break;
    }
  }
  return r;
}

static void cargarDatos() {
  EEPROM.get(0, datos);
  if (datos.magic != MAGIC || datos.cantidad > 2 || datos.ultimo > 1) {
    memset(&datos, 0, sizeof(datos));
    datos.magic = MAGIC;
  }
  for (int i = 0; i < 2; i++) {
    datos.redes[i].ssid[32] = '\0';
    datos.redes[i].password[64] = '\0';
  }
}

static void conectarSlot(int idx) {
  if (idx < 0 || idx >= datos.cantidad) return;
  if (!datos.redes[idx].ssid[0]) return;
  WiFi.disconnect();  // Solo desconecta STA, no el SoftAP.
  WiFi.begin(datos.redes[idx].ssid, datos.redes[idx].password);
  indiceIntento = idx;
  intentoActivo = true;
  inicioIntento = millis();
  Serial.printf("APWifi: intentando conectar a %s\n", datos.redes[idx].ssid);
}

static void paginaConfig() {
  String pagina = F("<!doctype html><html lang='es'><meta charset='utf-8'>"
                    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
                    "<title>Configurar WiFi</title>"
                    "<style>body{font:16px Arial;max-width:500px;margin:40px auto;padding:0 18px}"
                    "input,button{display:block;width:100%;box-sizing:border-box;padding:12px;margin:10px 0}"
                    "a{color:#1766aa}</style><h2>Configuracion WiFi del brazo</h2>"
                    "<p>Este ESP32 mantiene su propio punto de acceso. Se pueden guardar hasta dos redes.</p>");
  pagina += F("<form method='POST' action='/wifi-save'>"
              "<label>Nombre de red (SSID)</label><input required maxlength='32' name='ssid'>"
              "<label>Contrasena</label><input type='password' maxlength='64' name='password'>"
              "<button type='submit'>Guardar y conectar</button></form>");
  pagina += F("<h3>Redes guardadas</h3><ul>");
  for (int i = 0; i < datos.cantidad; ++i) {
    pagina += "<li>" + escaparHTML(String(datos.redes[i].ssid)) + "</li>";
  }
  pagina += F("</ul><p><a href='/'>Regresar al control del brazo</a></p></html>");
  server.send(200, "text/html; charset=utf-8", pagina);
}

static void guardarConfig() {
  String ssid = server.arg("ssid");
  String pass = server.arg("password");
  ssid.trim();
  if (ssid.length() == 0 || ssid.length() > 32 || pass.length() > 64 ||
      (pass.length() > 0 && pass.length() < 8)) {
    server.send(400, "text/plain; charset=utf-8",
                "SSID invalido o clave WiFi de menos de 8 caracteres.");
    return;
  }
  int idx = -1;
  for (int i = 0; i < datos.cantidad; i++) {
    if (ssid == datos.redes[i].ssid) { idx = i; break; }
  }
  if (idx < 0) {
    if (datos.cantidad < 2) idx = datos.cantidad++;
    else idx = (datos.ultimo + 1) % 2;
  }
  memset(&datos.redes[idx], 0, sizeof(Credencial));
  ssid.toCharArray(datos.redes[idx].ssid, sizeof(datos.redes[idx].ssid));
  pass.toCharArray(datos.redes[idx].password, sizeof(datos.redes[idx].password));
  datos.ultimo = idx;
  EEPROM.put(0, datos);
  if (!EEPROM.commit()) {
    server.send(500, "text/plain", "Error al guardar EEPROM");
    return;
  }
  // Responder antes de iniciar una conexion STA que puede alterar el canal del AP.
  server.send(200, "text/html; charset=utf-8",
              "<p>Red guardada. El ESP32 intentara conectarse sin apagar su AP.</p>"
              "<p><a href='/'>Volver al brazo</a></p>");
  alternativaIntentada = false;
  conectarSlot(idx);
}

static void begin(const char* nombreAP, const char* claveAP) {
  EEPROM.begin(512);
  cargarDatos();

  WiFi.mode(WIFI_AP_STA);
  bool ok = WiFi.softAP(nombreAP, claveAP);
  Serial.printf("APWifi: AP %s (%s)\n", nombreAP, ok ? "activo" : "ERROR");
  Serial.print("APWifi: IP local del AP: ");
  Serial.println(WiFi.softAPIP().toString());

  server.on("/wifi-config", HTTP_GET, paginaConfig);
  server.on("/wifi-save", HTTP_POST, guardarConfig);
  server.begin();

  if (datos.cantidad > 0) {
    conectarSlot(datos.ultimo);
  }
}

static void loopAP() {
  server.handleClient();
  if (WiFi.status() == WL_CONNECTED) {
    intentoActivo = false;
    alternativaIntentada = false;
    return;
  }
  if (intentoActivo && millis() - inicioIntento > 10000UL) {
    intentoActivo = false;
    if (!alternativaIntentada && datos.cantidad == 2) {
      alternativaIntentada = true;
      conectarSlot(1 - indiceIntento);
    } else {
      ultimoReintento = millis();
    }
  }
  // Reintentar periodicamente la red mas reciente sin bloquear el servidor.
  if (!intentoActivo && datos.cantidad > 0 &&
      millis() - ultimoReintento > 30000UL && WiFi.status() != WL_CONNECTED) {
    ultimoReintento = millis();
    alternativaIntentada = false;
    conectarSlot(datos.ultimo);
  }
}

}  // namespace APWifi
