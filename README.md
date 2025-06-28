# STR-2025: Sistema de Reconocimiento de Patentes Vehiculares

Este proyecto implementa un sistema en tiempo real para el reconocimiento automático de patentes vehiculares, integrando hardware para control de barreras físicas, procesamiento de imágenes y comunicación con un backend HTTP.

## 📚 Descripción

STR-2025 está compuesto por dos módulos principales:
- **Backend (Python)**: expone una API REST para registrar vehículos, recibir eventos de reconocimiento y gestionar estados. Incluye un contenedor Docker para despliegue sencillo.
- **Principal (C++)**: ejecuta el reconocimiento de placas usando procesamiento de video, controla sensores y actuadores conectados a hardware como Raspberry Pi a través de GPIO, y se comunica con el backend.

## 🚀 Tecnologías

- **C++**: procesamiento en tiempo real, control de hardware.
- **Python (Flask/FastAPI)**: backend REST.
- **Docker**: contenedores para backend y servicios auxiliares.
- **PostgreSQL**: base de datos relacional para persistencia de vehículos y eventos.
- **pigpio**: librería para manejo de GPIO en Raspberry Pi.

## 🗂️ Estructura del proyecto

```
repo/
├── backend/
│   ├── app.py                # API REST
│   ├── requirements.txt      # dependencias Python
│   ├── docker-compose.yml    # orquestación de servicios
│   └── db/init.sql           # script de inicialización de la base de datos
└── principal/ src
    ├── main.cpp              # lógica principal en C++
    ├── threads               # directorio con los componentes del sistema
        ├── sensor.cpp        # módulo del sensor para deteccion del vehiculo 
        ├── camera.cpp        # módulo de la camara para la captura de imagenes
        ├── communicator.cpp  # módulo de comunicacion con el backend
        ├── barrera.cpp       # módulo de la barrera fisica
    ├── shared_data.h         # archivo de cabecera para gestión de variables compartidas 
    └── CMakeLists.txt        # configuración de compilación
```

## 🔌 Pines GPIO utilizados
El sistema utiliza los siguientes pines de la Raspberry Pi, controlados mediante la librería pigpio:

Pin	GPIO	    Función
BARRIER_PIN	18	Control del servomotor de la barrera
TRIGGER_PIN	23	Señal de trigger para el sensor ultrasónico
ECHO_PIN	24	Señal de eco del sensor ultrasónico
LED_RED	    27	Indicador LED rojo (vehiculo no autorizado)
LED_GREEN	17	Indicador LED verde (vehiculo autorizado)

Notas:

- Los LEDs LED_RED y LED_GREEN se inicializan y controlan como salidas digitales para señalización visual.
- El pin BARRIER_PIN se usa con gpioServo() para controlar el movimiento de la barrera.
- Los pines TRIGGER_PIN y ECHO_PIN permiten medir distancias mediante un sensor ultrasónico, utilizado para detectar la presencia o ausencia de un vehículo en la zona de paso.

## ⚙️ Instalación

### Requisitos

- Docker y Docker Compose instalados en tu máquina.
- Una Raspberry Pi con Raspbian si vas a ejecutar la parte C++ en hardware real.

### Backend

1. Clona este repositorio:
   ```bash
   git clone https://github.com/Pandulc/STR-2025.git
   cd STR-2025/backend
   ```
2. Configura tus variables de entorno en `backend/.env`.
3. Construye e inicia los servicios:
   ```bash
   docker-compose up --build
   ```
   Esto levantará el backend en `localhost:8000` y la base de datos PostgreSQL.

### Principal (C++)

1. Instala las dependencias en tu Raspberry Pi:
   ```bash
   sudo apt update
   sudo apt install cmake g++ libpigpio-dev
   ```
2. Compila el proyecto:
   ```bash
   cd STR-2025/principal
   mkdir build && cd build
   cmake ..
   make
   ```
3. Ejecuta el programa principal:
   ```bash
   sudo ./str
   ```
