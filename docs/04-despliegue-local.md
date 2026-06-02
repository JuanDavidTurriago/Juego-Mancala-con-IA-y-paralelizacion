# 04 - Despliegue local

## Docker Compose

El archivo `deploy/local/docker-compose.yml` levanta los tres servicios del proyecto: `motor`, `backend` y `frontend`. Es la forma recomendada de reproducir la solucion completa en una maquina de desarrollo porque construye imagenes desde las carpetas evaluables y usa la red interna de Compose para que el backend hable con el motor.

Comando principal:

```bash
docker compose -f deploy/local/docker-compose.yml up --build
```

Puertos publicados:

- `9000`: motor C++ OpenMP, util para pruebas directas.
- `8000`: backend FastAPI.
- `8080`: frontend web servido por Nginx.

El servicio `motor` define `MOTOR_PORT=9000` y `OMP_NUM_THREADS=4`. El servicio `backend` define `MOTOR_URL=http://motor:9000`, `MOTOR_TIMEOUT_SECONDS=10` y `CORS_ORIGINS=http://localhost:8080,http://127.0.0.1:8080`. No se define una ruta de respuestas simuladas porque el motor real es parte obligatoria del camino principal. Si el motor no esta sano, el backend debe fallar readiness y no debe inventar una jugada.

## Healthchecks

Compose usa healthchecks para ordenar el arranque. El motor se considera sano cuando `curl -fsS http://localhost:9000/healthz` responde correctamente dentro del contenedor. El backend se considera sano cuando una llamada local a `http://127.0.0.1:8000/healthz` responde. El frontend depende de que el backend este sano antes de arrancar.

Esta configuracion no garantiza que todo el trafico real este perfecto, pero reduce errores comunes: backend arrancando antes de que el motor escuche, frontend servido mientras la API aun no esta lista, o pruebas manuales ejecutadas en medio de la construccion.

## Pruebas manuales con curl

Con el stack arriba, primero se prueban endpoints de salud:

```bash
curl -s http://localhost:9000/healthz
curl -s http://localhost:9000/readyz
curl -s http://localhost:8000/healthz
curl -s http://localhost:8000/readyz
```

Luego se prueba el contrato principal por el backend:

```bash
curl -s -X POST http://localhost:8000/move \
  -H "Content-Type: application/json" \
  -d '{"board":[4,4,4,4,4,4,0,4,4,4,4,4,4,0],"side":0,"depth":8,"threads":4}'
```

La misma prueba puede enviarse directo al motor para aislar problemas:

```bash
curl -s -X POST http://localhost:9000/move \
  -H "Content-Type: application/json" \
  -d '{"board":[4,4,4,4,4,4,0,4,4,4,4,4,4,0],"side":0,"depth":8,"threads":4}'
```

Si directo al motor funciona y por backend no, el problema esta en `MOTOR_URL`, red de Compose o timeout. Si ambos fallan, el problema esta en motor, contrato JSON o tablero invalido. Si `/readyz` del backend devuelve `503`, el backend no puede contactar el motor.

## Validaciones esperadas

Una solicitud invalida debe fallar. Por ejemplo, un tablero con 13 posiciones debe producir error de schema en backend:

```bash
curl -i -X POST http://localhost:8000/move \
  -H "Content-Type: application/json" \
  -d '{"board":[4,4,4,4,4,4,0,4,4,4,4,4,4],"side":0,"depth":8,"threads":4}'
```

Un tablero con 14 posiciones pero total distinto de 48 tambien falla. Esta regla evita que el motor explore posiciones imposibles. En una herramienta de analisis se podria permitir cualquier tablero, pero el proyecto modela partidas de Kalah(6,4), asi que el total de semillas es parte del contrato.

## Frontend local

El frontend esta disponible en `http://localhost:8080`. Por defecto llama al backend en `http://localhost:8000`. La UI permite editar el arreglo del tablero, seleccionar `side`, `depth` y `threads`, y enviar la solicitud. Tambien pinta stores y pits para que sea mas facil detectar errores de indices. El navegador solo puede llamar al backend si CORS permite explicitamente el origen local; por eso el backend configura `http://localhost:8080` y `http://127.0.0.1:8080` en lugar de `"*"`.

## Kubernetes local

Los manifiestos para kind, minikube o k3d estan en `deploy/local/k8s/`. Se declaran:

- `configmap.yaml`: variables compartidas de motor y backend.
- `motor.yaml`: Deployment y Service interno del motor.
- `backend.yaml`: Deployment con 3 replicas y Service NodePort.
- `frontend.yaml`: Deployment y Service NodePort en `30080`.

Flujo recomendado con kind:

```bash
kind create cluster --name mancala-local
docker build -t mancala-motor:local ./motor
docker build -t mancala-backend:local ./backend
docker build -t mancala-frontend:local ./frontend
kind load docker-image mancala-motor:local --name mancala-local
kind load docker-image mancala-backend:local --name mancala-local
kind load docker-image mancala-frontend:local --name mancala-local
kubectl apply -f deploy/local/k8s/
```

Verificacion:

```bash
kubectl get pods
kubectl get svc
kubectl rollout status deployment/motor
kubectl rollout status deployment/backend
kubectl rollout status deployment/frontend
```

Para probar el backend desde la maquina local:

```bash
kubectl port-forward svc/backend 8000:8000
curl -s http://localhost:8000/readyz
```

Para probar el frontend:

```bash
kubectl port-forward svc/frontend 8080:80
```

Luego abrir `http://localhost:8080`.

## Diagnostico

Si un pod queda en `ImagePullBackOff`, probablemente la imagen local no fue cargada al cluster kind o el nombre no coincide con el manifiesto. Si el backend queda `Running` pero `readyz` falla, revisar logs:

```bash
kubectl logs deployment/backend --tail=200
kubectl logs deployment/motor --tail=200
```

Si el motor arranca pero las busquedas tardan demasiado, bajar `depth` o `threads` en la solicitud. Para pruebas de humo se recomienda `depth=4`; para evidencia de rendimiento se recomienda `depth=8` o superior, siempre observando los recursos disponibles.

## Limpieza

Para detener Compose:

```bash
docker compose -f deploy/local/docker-compose.yml down
```

Para eliminar el cluster kind:

```bash
kind delete cluster --name mancala-local
```

Estos comandos no eliminan el codigo ni modifican la plantilla; solo limpian procesos y recursos locales.
