# Guia de pruebas del proyecto

Este documento concentra los comandos para verificar la solucion completa en Debian 12 o en cualquier Linux equivalente. La idea es poder probar el motor C++, el backend FastAPI, el frontend estatico y el flujo integrado sin depender de pasos dispersos entre varios archivos.

## 1. Preparacion del entorno

### Dependencias base

En Debian 12 conviene tener instalados al menos:

```bash
sudo apt update
sudo apt install -y python3 python3-pip python3-venv cmake g++ make curl
```

Si vas a ejecutar los contenedores, tambien necesitas Docker y el plugin de Compose.

### Backend Python

```bash
cd backend
python3 -m pip install --user --break-system-packages -r requirements.txt
```

Si prefieres un entorno aislado y tu sistema tiene `python3-venv`, puedes usar:

```bash
cd backend
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -r requirements.txt
```

## 2. Pruebas del motor C++

Los comandos de esta seccion se ejecutan desde la raiz del repositorio, es decir, desde `Juego-Mancala-con-IA-y-paralelizacion/`. Si estas parado dentro de `backend/`, primero ejecuta `cd ..`.

### Configurar y compilar

```bash
cmake -S motor -B motor/build -DCMAKE_BUILD_TYPE=Release
cmake --build motor/build -j
```

### Ejecutar pruebas unitarias

```bash
ctest --test-dir motor/build --output-on-failure
```

Si quieres correr un binario puntual:

```bash
./motor/build/test_board
./motor/build/test_alphabeta
./motor/build/test_mcts
```

### Ejecutar el benchmark

```bash
./motor/build/mancala_bench --depth 8 --positions motor/tests/suite.txt --threads 1,2,4,8
```

Para guardar la salida en CSV:

```bash
./motor/build/mancala_bench --depth 8 --positions motor/tests/suite.txt --threads 1,2,4,8 > bench-depth8.csv
```

## 3. Pruebas del backend

Los comandos de esta seccion se ejecutan desde `backend/`.

### Levantar el backend en modo mock

Esto sirve mientras el motor HTTP real no esta disponible:

```bash
cd backend
USE_MOCK=true MOTOR_URL=http://localhost:9000 ~/.local/bin/uvicorn app.main:app --host 0.0.0.0 --port 8000
```

### Ejecutar los tests del backend

```bash
cd backend
~/.local/bin/pytest tests/test_api.py
```

### Probar endpoints con curl

Health y readiness:

```bash
curl -s http://localhost:8000/healthz
curl -s http://localhost:8000/readyz
```

Solicitud valida con Alpha-Beta:

```bash
curl -s -X POST http://localhost:8000/move \
  -H 'Content-Type: application/json' \
  -d '{"board":[4,4,4,4,4,4,0,4,4,4,4,4,4,0],"side":0,"algo":"alphabeta","depth":8,"threads":4}'
```

Solicitud valida con MCTS:

```bash
curl -s -X POST http://localhost:8000/move \
  -H 'Content-Type: application/json' \
  -d '{"board":[4,4,4,4,4,4,0,4,4,4,4,4,4,0],"side":1,"algo":"mcts","simulations":1000,"threads":4}'
```

Errores de validacion esperados:

```bash
# Tablero con 13 elementos
curl -i -X POST http://localhost:8000/move \
  -H 'Content-Type: application/json' \
  -d '{"board":[4,4,4,4,4,4,0,4,4,4,4,4,4],"side":0,"algo":"alphabeta","depth":8,"threads":4}'

# Tablero con suma distinta de 48
curl -i -X POST http://localhost:8000/move \
  -H 'Content-Type: application/json' \
  -d '{"board":[3,4,4,4,4,4,0,4,4,4,4,4,4,0],"side":0,"algo":"alphabeta","depth":8,"threads":4}'

# Falta depth para alphabeta
curl -i -X POST http://localhost:8000/move \
  -H 'Content-Type: application/json' \
  -d '{"board":[4,4,4,4,4,4,0,4,4,4,4,4,4,0],"side":0,"algo":"alphabeta","threads":4}'
```

## 4. Pruebas del frontend

### Levantar el stack completo

Antes de ejecutar Compose, asegúrate de que los puertos `8000`, `8080` y `9000` esten libres. Si dejaste un backend o motor corriendo en otra terminal, detenlo primero. Si quieres comprobarlo rapidamente:

```bash
ss -ltnp | grep -E ':8000|:8080|:9000'
```

Si alguno esta ocupado por un proceso previo, cierralo o mata el PID correspondiente. Para un backend lanzado manualmente en otra terminal, `Ctrl+C` suele ser suficiente.

```bash
docker compose -f deploy/local/docker-compose.yml up --build
```

### Verificar el flujo manual

1. Abrir `http://localhost:8080`.
2. Elegir `algoritmo`.
3. Seleccionar `jugador activo`.
4. Hacer clic en un hoyo activo del tablero o pulsar `Calcular jugada`.
5. Confirmar que se muestran `Jugada`, `Evaluación`, `Tiempo`, `Algoritmo`, `Estadísticas` y `Hilos`.

### Comprobar la API desde el navegador

Si el frontend muestra error, revisar primero:

```bash
curl -s http://localhost:8000/readyz
curl -s http://localhost:9000/readyz
```

## 5. Flujo integrado completo

Con el stack arriba, la secuencia completa es:

```bash
curl -s http://localhost:8000/healthz
curl -s http://localhost:8000/readyz
curl -s -X POST http://localhost:8000/move \
  -H 'Content-Type: application/json' \
  -d '{"board":[4,4,4,4,4,4,0,4,4,4,4,4,4,0],"side":0,"algo":"alphabeta","depth":8,"threads":4}'
```

Si el motor no responde y `USE_MOCK=true`, el backend sigue contestando con datos simulados para no bloquear el desarrollo. Para la entrega final, deja `USE_MOCK=false`.

## 6. Limpieza

```bash
docker compose -f deploy/local/docker-compose.yml down
```

Si lanzaste servicios manualmente fuera de Compose, detenlos antes de volver a levantar el stack para evitar errores como `address already in use` en el puerto `8000`.

Si creaste un virtualenv local:

```bash
deactivate
rm -rf backend/.venv
```
