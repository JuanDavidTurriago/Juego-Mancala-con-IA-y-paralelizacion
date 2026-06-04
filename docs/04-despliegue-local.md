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

## Kubernetes local (kind)

Los manifiestos estan en `deploy/local/k8s/`:

- `configmap.yaml`: `MOTOR_URL=http://motor-svc:8080`, `MOTOR_PORT=8080`, CORS y timeouts.
- `motor.yaml`: Deployment (2 replicas) y Service `motor-svc` (ClusterIP, puerto 8080).
- `backend.yaml`: Deployment con **3 replicas** y Service `backend` (NodePort).
- `frontend.yaml`: Deployment (2 replicas) y Service `frontend` (NodePort `30080`).

### 1. Cluster e imagenes (GHCR)

Los Deployments en `deploy/local/k8s/` referencian imágenes publicadas en GHCR con tag **SHA del commit** (sin `:latest`), por ejemplo:

`ghcr.io/juandavidturriago/mancala-motor:7f923db8c7091ff0b1b75cc06fffdba313f288be`

El job `publish` de CI solo corre en `push` a `main`; el tag debe coincidir con el commit publicado.

```bash
kind create cluster --name mancala

kubectl create secret docker-registry ghcr-credentials \
  --docker-server=ghcr.io \
  --docker-username=<GITHUB_USER> \
  --docker-password=<PAT_con_read:packages>

# Verificar pull desde el host (opcional)
docker pull ghcr.io/juandavidturriago/mancala-motor:7f923db8c7091ff0b1b75cc06fffdba313f288be
```

### 2. Aplicar manifiestos (orden)

```bash
kubectl apply -f deploy/local/k8s/configmap.yaml
kubectl apply -f deploy/local/k8s/motor.yaml
kubectl apply -f deploy/local/k8s/backend.yaml
kubectl apply -f deploy/local/k8s/frontend.yaml
```

### 3. Verificacion de pods y replicas

Todos los pods deben quedar en `Running` y `READY 1/1`:

```bash
kubectl get pods
kubectl get deployment backend
```

Evidencia de despliegue (backend **3/3 READY**):

```text
$ kubectl get deployment backend
NAME      READY   UP-TO-DATE   AVAILABLE   AGE
backend   3/3     3            3           35s

$ kubectl get pods
NAME                        READY   STATUS    RESTARTS   AGE
backend-8669684f9b-6j8q4    1/1     Running   0          35s
backend-8669684f9b-dsw4b    1/1     Running   0          35s
backend-8669684f9b-wr42s    1/1     Running   0          35s
frontend-67fd899df5-m7r8l   1/1     Running   0          35s
frontend-67fd899df5-ww9mt   1/1     Running   0          35s
motor-fc896d77b-5bnc6       1/1     Running   0          35s
motor-fc896d77b-jsdwn       1/1     Running   0          35s
```

> **Captura de pantalla:** ejecutar `kubectl get deployment backend` y `kubectl get pods` en la terminal y pegar la imagen aqui si el informe lo exige visualmente. La salida anterior corresponde al cluster `mancala` verificado en integracion.

### 4. Probes del backend

```bash
kubectl describe pod $(kubectl get pods -l app=backend -o jsonpath='{.items[0].metadata.name}')
```

Debe listar `Liveness` en `/healthz` y `Readiness` en `/readyz` (puerto 8000). Al final, `Conditions` con `Ready: True`. Un aviso transitorio de readiness al arrancar (mientras el motor aun no responde) es normal; si persiste, el pod entra en `CrashLoopBackOff` y conviene revisar logs del backend y del motor.

### 5. Red interna backend → motor

Desde un pod del backend (el contenedor no trae `curl`; usar Python):

```bash
kubectl exec deploy/backend -- python -c "import urllib.request; print(urllib.request.urlopen('http://motor-svc:8080/healthz', timeout=5).read().decode())"
```

Respuesta esperada: `{"status":"ok"}`.

### 6. Acceso desde el host

```bash
kubectl port-forward svc/backend 8000:8000
curl -s http://localhost:8000/readyz
```

Frontend:

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
kind delete cluster --name mancala
```

Estos comandos no eliminan el codigo ni modifican la plantilla; solo limpian procesos y recursos locales.

## Guia de pruebas

Si ademas de desplegar necesitas una lista ordenada de comandos para validar el motor, el backend, el frontend y el flujo completo, revisa [TESTING.md](../TESTING.md).
