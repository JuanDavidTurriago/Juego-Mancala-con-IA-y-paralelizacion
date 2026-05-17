# 01 - Arquitectura

## Vision General

Pendiente por completar.

## Diagrama (Mermaid)

```mermaid
flowchart LR
  user[Usuario (Navegador)] --> front[Frontend (nginx + JS)]
  user -->|fetch JSON + CORS| api[Backend (FastAPI)]
  api -->|HTTP interno| motor[Motor (C++/OpenMP)]
```

## Contenedores

- motor/: motor-svc (C++/OpenMP)
- backend/: api-svc (FastAPI)
- frontend/: front-svc (nginx + estaticos)
- db/: opcional (si se incluye)

## API REST + CORS

Pendiente por completar.

