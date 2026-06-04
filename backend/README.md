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

## Contrato de `/move`

El cuerpo de la petición acepta:

- `board`: 14 posiciones del tablero, con suma total 48.
- `side`: `0` o `1`, según el jugador activo.
- `algo`: `alphabeta` o `mcts`.
- `depth`: obligatorio cuando `algo = alphabeta`.
- `simulations`: obligatorio cuando `algo = mcts`.
- `threads`: número de hilos para la consulta al motor.

Si `USE_MOCK=true`, el backend responde con una implementación simulada útil
para trabajar mientras el motor HTTP real termina de estar disponible. En
entorno normal, deje `USE_MOCK=false` y configure `MOTOR_URL` contra el
servicio del motor.
