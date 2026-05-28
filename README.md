# Juego Mancala (Kalah) con IA y Paralelizacion

Proyecto final del curso: motores paralelos de Mancala (Kalah) y despliegue en Kubernetes.

## Integrantes

Reemplace los campos pendientes con la informacion real del grupo:

- P1: Juan David Turriago - 2477182 - juan.turriago@correounivalle.edu.co
- P2: Javier Andres Muñoz Tavera - 2380 - correo@correounivalle.edu.co
- P3: Andres Felipe Castrillon Martinez - 2380664 - correo@correounivalle.edu.co
- P4: Bryan Steven Panesso Avila - 2380701 -BryanPanessocorreo@correounivalle.edu.co

## Informe

El informe vive en `docs/` (formato Markdown, diagramas en Mermaid).

- Indice del informe: `docs/README.md`

## Estructura del Repo

- `motor/`: Motor C++/OpenMP (Alfa-Beta y MCTS) + tests + bench
- `backend/`: API publica (FastAPI) con CORS y validacion JSON
- `frontend/`: Cliente web estatico servido por nginx
- `deploy/`: Docker Compose + manifiestos Kubernetes (local y nube)
- `.github/workflows/`: CI + Sonar

## Build y Ejecucion Local (dev)

1. Docker Compose (app completa):
   - `docker compose -f deploy/local/docker-compose.yml up --build`
   - Frontend: `http://localhost:8080`
   - Backend: `http://localhost:8000`

2. Motor (solo compilacion + tests):
   - `cmake -S motor -B motor/build`
   - `cmake --build motor/build -j`
   - `./motor/build/test_board`

## Guia rapida en PowerShell

### 1) Backend FastAPI

Usa esta secuencia si quieres verificar la API sola antes de levantar todo el stack.

```powershell
cd C:\Users\andre\OneDrive\Documentos\infra\Juego-Mancala-con-IA-y-paralelizacion\backend
pip install -r requirements.txt
uvicorn app.main:app --reload --port 8000
```

En otra ventana de PowerShell puedes validar lo que responde el backend:

```powershell
curl http://localhost:8000/healthz
```

Respuesta esperada:

```json
{"status":"ok"}
```

Prueba del endpoint `POST /move` con un board valido:

```powershell
$body = @{ board = @(4,4,4,4,4,4,0,4,4,4,4,4,4,0); side = 0; algo = 'alphabeta'; depth = 8; threads = 4 } | ConvertTo-Json -Compress
Invoke-RestMethod -Uri http://localhost:8000/move -Method Post -ContentType 'application/json' -Body $body
```

Prueba de validacion con un board invalido de 13 elementos:

```powershell
$bad = @{ board = @(4,4,4,4,4,4,0,4,4,4,4,4,4); side = 0; algo = 'alphabeta'; depth = 8; threads = 4 } | ConvertTo-Json -Compress
Invoke-WebRequest -Uri http://localhost:8000/move -Method Post -ContentType 'application/json' -Body $bad
```

### 2) Docker Compose

Usa esta opcion para levantar frontend, backend y motor juntos.

```powershell
cd C:\Users\andre\OneDrive\Documentos\infra\Juego-Mancala-con-IA-y-paralelizacion
docker compose -f deploy/local/docker-compose.yml up --build
```

Luego abre:

- Frontend: http://localhost:8080
- Backend: http://localhost:8000

Para detener todo:

```powershell
docker compose -f deploy/local/docker-compose.yml down
```

### 3) Que valida cada opcion

- Backend: confirma que FastAPI arranca y responde `/healthz` y `/move`.
- Docker Compose: confirma el flujo completo frontend → backend → mock.
- El frontend usa CORS para hablar con el backend desde `http://localhost:8080`.
