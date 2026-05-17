# Juego Mancala (Kalah) con IA y Paralelizacion

Proyecto final del curso: motores paralelos de Mancala (Kalah) y despliegue en Kubernetes.

## Integrantes

Reemplace los campos pendientes con la informacion real del grupo:

- P1: Juan David Turriago - 2477182 - juan.turriago@correounivalle.edu.co
- P2: NOMBRE APELLIDO - CODIGO - correo@correounivalle.edu.co
- P3: NOMBRE APELLIDO - CODIGO - correo@correounivalle.edu.co
- P4: NOMBRE APELLIDO - CODIGO - correo@correounivalle.edu.co

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
