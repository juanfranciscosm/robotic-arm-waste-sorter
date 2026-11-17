# Brazo clasificador 4DOF - Prototipo APWifi

Integracion del brazo ESP32 con un Access Point WiFi, control web, comandos seriales,
EEPROM para dos redes y PWM progresivo no bloqueante. Basado funcionalmente en el
repositorio APWifi de la materia: https://github.com/lab-microcon/APWifi

## Archivos
- `platformio.ini`: configuracion del entorno esp32dev.
- `include/apwifieeprommode.h`: ADAPTACION del proyecto APWifi; no es copia literal.
- `src/main.cpp`: control de servos, servidor, interfaz web y monitor serial.

## Uso
1. Copia la carpeta en VS Code y abre como proyecto de PlatformIO.
2. Antes de encender el brazo, verifica fuentes de 5 V adecuadas para los servos,
   GND comun, soporte mecanico de J3 y que no haya topes forzados.
3. Conecta ESP32 al PC por USB. Fuente de servos externa y GND comun; no conectes
   positivo de alimentacion externa al pin 3V3.
4. Sube el firmware con PlatformIO (Upload).
5. Abre el monitor serie a 115200. Conectate al WiFi "BrazoReciclaje"
   usando la clave "CambiarClave123" (cambiala en el codigo).
6. Abre http://192.168.4.1 desde un dispositivo conectado al AP.
7. Mueve cada articulacion individualmente desde el panel, comenzando cerca
   de sus posiciones centrales. La articulacion J3 permanece fija en 1400 us.
8. Para guardar hasta dos redes WiFi externas, usa /wifi-config. El AP no se
   apaga aunque no consiga conectarse a una red externa.

## Calibraciones guardadas
- J1 MG90S, GPIO25: 0..180 grados = 700..2500 us.
- J2 MG90S, GPIO26: 0..90 grados = 1000..2200 us.
- J3 MG90S, GPIO27: fijado en 1400 us (sin control externo).
- J4 SG90, GPIO32: 0..150 grados = 1000..2500 us.
- G SG90, GPIO33: 0..45 grados = 1000..2000 us.

## Seguridad y limitaciones
- ATENCION: J3 ya mostro bloqueo/zumbido. Si se bloquea o calienta, corta
  la alimentacion y cambia `j3Habilitado` a `false` en `src/main.cpp`.
  En ese caso necesita soporte mecanico, no solo desactivar el PWM.
- El comando OFF corta PWM, NO la energia de los servos. Se requiere
  interruptor fisico para desconexion electrica y parada de emergencia.
- No existe realimentacion de posicion: los valores son consignas, no mediciones.
- En la primera orden a cada servo, se manda la posicion central estimada.
  Mantener espacio libre y evitar colisiones durante el arranque.
- Todavia NO implementa reconocimiento de residuos; eso ira en la Raspberry Pi.
- Fuente MB102 de baja corriente puede ser insuficiente para varios servos;
  verificar corriente maxima y caidas de tension antes de mover bajo carga.
- La interfaz web no tiene autenticacion independiente del WiFi; usar
  una contrasena AP robusta y no exponer el dispositivo a redes no confiables.

## Protocolos
Comandos monitor serie: `J1 90`, `J2 45`, `J4 75`, `G 45`, `G 0`, `J3`,
`J3OFF`, `OFF`, `ON`.

API HTTP:
- GET / -> panel de control
- GET /api/status -> estado
- POST /api/move -> joint=J1&angle=90
- POST /api/power -> accion=OFF|ON|J3OFF
- GET /wifi-config -> configuracion de hasta dos redes
- POST /wifi-save -> ssid=...&password=...
