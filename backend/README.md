# backend/

Contenedor 2 — *wrapper* HTTP en Python (FastAPI o Flask).

Subdirectorios:

- `app/` — código del *wrapper*, modelos de validación
  (`pydantic`/`marshmallow`) y configuración de CORS.
- `tests/` — pruebas del backend (validación de *schemas*, manejo de
  CORS, códigos de error 4xx/5xx).

Archivo en la raíz del directorio:

- `Dockerfile` — imagen del backend.

*Endpoints* mínimos:

- `POST /move`
- `GET /healthz`
- `GET /readyz`
- `GET /metrics`

El backend **delega** el cálculo al `motor` por la red del clúster. No
incrusta el motor en el mismo proceso.
