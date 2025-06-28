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
        ├── sensor.cpp        # modulo del sensor para deteccion del vehiculo 
        ├── camera.cpp        # modulo de la camara para la captura de imagenes
        ├── communicator.cpp  # modulo de comunicacion con el backend
        ├── barrera.cpp       # modulo de la barrera fisica
    └── CMakeLists.txt        # configuración de compilación
```

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
