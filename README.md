# Brazo robótico clasificador de residuos reciclables

Proyecto de Sistemas Embebidos orientado al desarrollo de
un brazo robótico para la clasificación automatizada de
residuos reciclables.

## Descripción

El sistema utiliza un brazo robótico de cuatro grados de
libertad y una pinza para manipular objetos.

El control de los servomotores se realiza mediante un
microcontrolador ESP32. El proyecto contempla incorporar
visión por computadora para identificar y clasificar
residuos reciclables.

## Tecnologías

- ESP32
- Framework Arduino
- PlatformIO
- C/C++
- Comunicación WiFi
- Servomotores MG90S y SG90

## Estructura del proyecto

- ESP32/brazo4DOF_control/: firmware del microcontrolador.
- ESP32/brazo4DOF_control/src/: código fuente.
- ESP32/brazo4DOF_control/include/: archivos de cabecera.

## Estado del proyecto

En desarrollo.

El repositorio contiene actualmente el firmware de control
del brazo robótico. Los demás módulos se incorporarán
conforme avance su implementación.

## Entorno de desarrollo

Visual Studio Code con PlatformIO.

Placa: ESP32 Dev Module.
Framework: Arduino.
Monitor serial: 115200 baudios.
