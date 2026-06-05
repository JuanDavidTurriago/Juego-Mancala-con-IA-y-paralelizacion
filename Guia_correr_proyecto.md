

# Guía completa de ejecución del proyecto Mancala Kalah(6,4)

## Requisitos del sistema

- **Sistema operativo:** Linux (Debian/Ubuntu recomendado) o Windows con WSL2
- **Requisitos mínimos:**
  - 4 GB de RAM
  - 2 CPUs
  - Conexión a internet (para descargar dependencias)

---

## 1. Instalación de dependencias del sistema

### 1.1 Dependencias base para Debian/Ubuntu

```bash
# Actualizar repositorios
sudo apt update

# Instalar dependencias esenciales
sudo apt install -y \
    python3 \
    python3-pip \
    python3-venv \
    cmake \
    g++ \
    make \
    curl \
    git \
    docker.io \
    docker-compose-plugin
```

### 1.2 Verificar instalaciones

```bash
# Verificar versiones instaladas
python3 --version      # Debe mostrar Python 3.11+
cmake --version        # Debe mostrar CMake 3.16+
g++ --version          # Debe mostrar GCC 10+
docker --version       # Debe mostrar Docker 20.10+
curl --version         # Debe mostrar curl 7.68+
```

### 1.3 Configurar Docker para usar sin sudo (opcional)

```bash
# Agregar usuario al grupo docker
sudo usermod -aG docker $USER

# Cerrar sesión y volver a entrar, o ejecutar:
newgrp docker
```

---

## 2. Clonar el repositorio

```bash
# Clonar el repositorio
git clone https://github.com/juandavidturriago/Juego-Mancala-con-IA-y-paralelizacion.git

# Entrar al directorio
cd Juego-Mancala-con-IA-y-paralelizacion

# Verificar estructura de carpetas
ls -la
# Deberías ver: backend/ motor/ frontend/ deploy/ docs/ .github/
```

---

## 3. Compilar y probar el motor C++

### 3.1 Configurar y compilar

```bash
# Crear directorio de build y compilar en modo Release
cmake -S motor -B motor/build -DCMAKE_BUILD_TYPE=Release
cmake --build motor/build -j $(nproc)
```

### 3.2 Ejecutar pruebas unitarias del motor

```bash
# Ejecutar todas las pruebas
ctest --test-dir motor/build --output-on-failure

# O ejecutar pruebas individualmente
./motor/build/test_board      # Pruebas de reglas del juego
./motor/build/test_alphabeta  # Pruebas del algoritmo Alfa-Beta
```

**Salida esperada:**
```
100% tests passed, 0 tests failed out of 2
```

### 3.3 Ejecutar benchmark de rendimiento

```bash
# Benchmark con profundidad 8, posiciones de prueba, hilos 1,2,4,8
./motor/build/mancala_bench --depth 8 --positions motor/tests/suite.txt --threads 1,2,4,8
```

**Salida esperada (CSV):**
```
position,depth,threads,move,evaluation,elapsed_ms,speedup,efficiency,nodes,prunes
0,8,1,2,0,120,1.00,1.00,184000,31000
0,8,2,2,0,72,1.67,0.83,190000,30000
0,8,4,2,0,48,2.50,0.62,201000,28000
0,8,8,2,0,47,2.55,0.32,201000,28000
```

### 3.4 Iniciar el motor como servidor (para pruebas)

```bash
# Iniciar el motor en el puerto 9000
./motor/build/mancala_engine

# En otra terminal, probar que responde
curl -s http://localhost:9000/healthz
# Respuesta: {"status":"ok"}
```

---

## 4. Configurar y probar el backend FastAPI

### 4.1 Crear entorno virtual de Python

```bash
# Entrar a la carpeta del backend
cd backend

# Crear entorno virtual
python3 -m venv .venv

# Activar entorno virtual
source .venv/bin/activate

# Verificar que pip está disponible
pip --version
```

### 4.2 Instalar dependencias

```bash
# Instalar dependencias desde requirements.txt
pip install -r requirements.txt

# Verificar instalación
pip list | grep -E "fastapi|uvicorn|pydantic|httpx|pytest"
```

### 4.3 Ejecutar pruebas del backend

```bash
# Ejecutar todas las pruebas (usan motor simulado)
pytest tests/test_api.py -v

# O con salida detallada
pytest tests/test_api.py -v --tb=short
```

**Salida esperada:**
```
============================= test session starts ==============================
collected 12 items

tests/test_api.py ........                                            [100%]

============================= 12 passed in 1.25s ===============================
```

### 4.4 Iniciar el backend manualmente

```bash
# Iniciar servidor FastAPI (en el puerto 8000)
uvicorn app.main:app --host 0.0.0.0 --port 8000 --reload
```

**En otra terminal, probar endpoints:**

```bash
# Health check
curl -s http://localhost:8000/healthz
# {"status":"ok"}

# Readiness (fallará si el motor no está corriendo)
curl -s http://localhost:8000/readyz
# {"status":"not ready","reason":"Motor not reachable"}

# Probar el endpoint principal (con motor real)
curl -s -X POST http://localhost:8000/move \
  -H "Content-Type: application/json" \
  -d '{"board":[4,4,4,4,4,4,0,4,4,4,4,4,4,0],"side":0,"depth":8,"threads":4}'
```

**Salida esperada:**
```json
{
  "algo":"alphabeta",
  "move":2,
  "evaluation":0,
  "elapsed_ms":120,
  "stats":{
    "algo":"alphabeta",
    "nodes":184000,
    "prunes":31000
  },
  "threads_used":4
}
```

---

## 5. Levantar el sistema completo con Docker Compose

Esta es la forma más fácil de tener todos los servicios funcionando juntos.

### 5.1 Construir y levantar todos los servicios

```bash
# Desde la raíz del proyecto
cd /home/andres/Documentos/GitHub/Juego-Mancala-con-IA-y-paralelizacion

# Construir y levantar los contenedores
docker compose -f deploy/local/docker-compose.yml up --build
```

Espera a ver logs similares a:
```
motor-1      | INFO: Server started on port 9000
backend-1    | INFO: Application startup complete.
frontend-1   | ready for start up
```

### 5.2 En otra terminal, verificar que todo está funcionando

```bash
# Verificar contenedores activos
docker ps
# Deberías ver 3 contenedores: motor, backend, frontend

# Probar endpoints del motor
curl -s http://localhost:9000/healthz    # {"status":"ok"}
curl -s http://localhost:9000/readyz     # {"status":"ready"}

# Probar endpoints del backend
curl -s http://localhost:8000/healthz    # {"status":"ok"}
curl -s http://localhost:8000/readyz     # {"status":"ready"}

# Probar el contrato completo
curl -s -X POST http://localhost:8000/move \
  -H "Content-Type: application/json" \
  -d '{"board":[4,4,4,4,4,4,0,4,4,4,4,4,4,0],"side":0,"depth":8,"threads":4}'
```

### 5.3 Acceder al frontend web

Abre tu navegador y ve a: **http://localhost:8080**

Deberías ver:
- El tablero de Mancala representado gráficamente
- Selectores para elegir jugador activo (0 o 1)
- Campo para profundidad de búsqueda (depth)
- Campo para número de hilos (threads)
- Botón "Calcular jugada"
- Área para mostrar resultados (movimiento, evaluación, tiempo, estadísticas)

---

## 6. Prueba del flujo completo desde la interfaz web

### 6.1 Prueba manual desde el navegador

1. Abrir **http://localhost:8080**
2. Seleccionar "Jugador activo: 0"
3. Configurar "Profundidad: 8"
4. Configurar "Hilos: 4"
5. Hacer clic en "Calcular jugada"
6. Observar la respuesta con:
   - Movimiento recomendado (número del pit 0-5)
   - Evaluación heurística
   - Tiempo de cálculo en ms
   - Nodos explorados
   - Podas realizadas
   - Hilos realmente usados

### 6.2 Prueba con diferentes configuraciones

```bash
# Probar con más hilos (menor tiempo de respuesta esperado)
curl -s -X POST http://localhost:8000/move \
  -H "Content-Type: application/json" \
  -d '{"board":[4,4,4,4,4,4,0,4,4,4,4,4,4,0],"side":1,"depth":10,"threads":8}'

# Probar posición de medio juego
curl -s -X POST http://localhost:8000/move \
  -H "Content-Type: application/json" \
  -d '{"board":[0,0,5,3,0,2,12,4,6,2,1,0,8,5],"side":0,"depth":8,"threads":4}'
```

---

## 7. Ejecutar Kubernetes local (kind)

### 7.1 Instalar kind (si no está instalado)

```bash
# Descargar kind
curl -Lo ./kind https://kind.sigs.k8s.io/dl/v0.20.0/kind-linux-amd64
chmod +x ./kind
sudo mv ./kind /usr/local/bin/kind

# Verificar instalación
kind version
```

### 7.2 Crear cluster y desplegar

```bash
# Crear cluster kind
kind create cluster --name mancala

# Verificar que el cluster está corriendo
kubectl cluster-info --context kind-mancala

# Aplicar configuraciones
kubectl apply -f deploy/local/k8s/configmap.yaml
kubectl apply -f deploy/local/k8s/motor.yaml
kubectl apply -f deploy/local/k8s/backend.yaml
kubectl apply -f deploy/local/k8s/frontend.yaml

# Verificar pods (deben estar todos Running)
kubectl get pods
kubectl get deployment backend  # Debe mostrar 3/3 READY
```

### 7.3 Acceder en Kubernetes

```bash
# Forward del puerto del backend
kubectl port-forward svc/backend 8000:8000 &

# Forward del puerto del frontend
kubectl port-forward svc/frontend 8080:80 &

# Probar desde el host
curl -s http://localhost:8000/healthz
```

---

## 8. Limpieza de recursos

### 8.1 Detener Docker Compose

```bash
# Detener y eliminar contenedores
docker compose -f deploy/local/docker-compose.yml down

# Eliminar volúmenes y redes huérfanas
docker compose -f deploy/local/docker-compose.yml down -v
```

### 8.2 Eliminar cluster kind

```bash
# Eliminar cluster
kind delete cluster --name mancala
```

### 8.3 Limpiar archivos compilados y entornos virtuales

```bash
# Eliminar build del motor
rm -rf motor/build/

# Eliminar entorno virtual de Python
rm -rf backend/.venv/

# Eliminar archivos temporales de Python
find . -type d -name "__pycache__" -exec rm -rf {} + 2>/dev/null
find . -type f -name "*.pyc" -delete
```

---

## 9. Verificación final (checklist)

Marca estos ítems para confirmar que todo funciona:

| Componente | Prueba | Comando | ✅ |
|------------|--------|---------|---|
| Motor compila | Build exitoso | `cmake --build motor/build` | ☐ |
| Tests motor pasan | Unitarias OK | `ctest --test-dir motor/build` | ☐ |
| Benchmark ejecuta | CSV generado | `./motor/build/mancala_bench --depth 4` | ☐ |
| Backend pruebas | pytest OK | `pytest backend/tests` | ☐ |
| Docker compose | 3 contenedores | `docker compose ps` | ☐ |
| Motor health | 200 OK | `curl localhost:9000/healthz` | ☐ |
| Backend health | 200 OK | `curl localhost:8000/healthz` | ☐ |
| Backend readiness | 200 OK | `curl localhost:8000/readyz` | ☐ |
| API /move | JSON válido | `POST /move` con tablero inicial | ☐ |
| Frontend | Web visible | http://localhost:8080 | ☐ |
| Frontend calcula | Respuesta UI | Click "Calcular jugada" | ☐ |

---

## 10. Solución de problemas comunes

### Error: `python3-venv` no instalado

```bash
sudo apt install python3-venv
rm -rf backend/.venv
cd backend && python3 -m venv .venv
```

### Error: Puerto 8000, 8080 o 9000 ya en uso

```bash
# Ver qué proceso usa el puerto
sudo lsof -i :8000

# Matar el proceso (reemplazar PID)
sudo kill -9 <PID>

# O cambiar puerto en docker-compose.yml
```

### Error: `Connection refused` al hacer curl

```bash
# Verificar que los servicios están corriendo
docker ps

# Ver logs de un servicio específico
docker compose logs backend
docker compose logs motor
```

### Error: OpenMP no encontrado al compilar

```bash
# Instalar OpenMP en Debian/Ubuntu
sudo apt install libgomp1

# En CMakeLists.txt ya debe tener:
find_package(OpenMP REQUIRED)
```

### Error: El frontend no conecta con el backend

```bash
# Verificar CORS en backend
curl -s http://localhost:8000/readyz

# Reiniciar servicios en orden
docker compose down
docker compose up --build
```

---

## 11. Resumen de URLs después del despliegue

| Servicio | URL | Descripción |
|----------|-----|-------------|
| Frontend | http://localhost:8080 | Interfaz web del juego |
| Backend API | http://localhost:8000 | API REST FastAPI |
| Motor | http://localhost:9000 | Servidor C++ de cálculo |
| Health backend | http://localhost:8000/healthz | Estado del backend |
| Readiness backend | http://localhost:8000/readyz | Si puede contactar motor |
| Metrics backend | http://localhost:8000/metrics | Métricas Prometheus |
| Health motor | http://localhost:9000/healthz | Estado del motor |

---

## Comando único para levantar todo (después de la primera configuración)

```bash
# Desde la raíz del proyecto
docker compose -f deploy/local/docker-compose.yml up --build

# En otra terminal, probar que todo funciona
curl -s http://localhost:8000/readyz && echo "✅ Todo listo"

# Abrir navegador
xdg-open http://localhost:8080  # Linux
# o manualmente: http://localhost:8080
```

---

¡Listo! Con esta guía tienes el proyecto funcionando al 100% con todas las pruebas, el motor C++ con OpenMP paralelizado, el backend FastAPI, el frontend web, Docker Compose y Kubernetes local.