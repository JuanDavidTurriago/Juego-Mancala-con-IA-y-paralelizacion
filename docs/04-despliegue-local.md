# 04 - Despliegue Local

## Docker Compose

Esta es la forma recomendada para correr el proyecto completo en local mientras el motor real sigue en desarrollo. El archivo [deploy/local/docker-compose.yml](../deploy/local/docker-compose.yml) levanta tres servicios:

Importante: no existe un `docker-compose.yml` en la raiz del repositorio. El comando correcto siempre debe apuntar a `deploy/local/docker-compose.yml`.

- `motor`: el contenedor del motor C++/OpenMP.
- `backend`: FastAPI con CORS y validacion del board.
- `frontend`: cliente web estatico servido por nginx.

### Flujo general

1. Docker construye las tres imagenes.
2. El motor levanta primero.
3. El backend arranca con FastAPI y responde usando el mock del motor.
4. El frontend se sirve en `http://localhost:8080` y consume el backend en `http://localhost:8000`.

### Guia copiar y pegar en PowerShell

#### A. Levantar todo el proyecto

```powershell
cd C:\Users\andre\OneDrive\Documentos\infra\Juego-Mancala-con-IA-y-paralelizacion
docker compose -f deploy/local/docker-compose.yml up --build
```

Cuando termine de construir y arrancar, abre:

- Frontend: `http://localhost:8080`
- Backend: `http://localhost:8000/healthz`
- Swagger UI: `http://localhost:8000/docs`

#### B. Verificar el backend desde PowerShell

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

En PowerShell, este comando lanza una excepcion porque el backend responde `422 Unprocessable Entity`. Eso es lo esperado y confirma que la validacion del schema esta funcionando.

Si quieres ver solo el codigo de estado sin que PowerShell interrumpa la ejecucion, usa este comando:

```powershell
try {
	Invoke-WebRequest -Uri http://localhost:8000/move -Method Post -ContentType 'application/json' -Body $bad
} catch {
	$_.Exception.Response.StatusCode.value__
}
```

La salida esperada es `422`.

#### C. Detener el stack

```powershell
docker compose -f deploy/local/docker-compose.yml down
```

### Opcion backend manual

Si quieres depurar solo la API sin contenedores, entra a `backend/` y ejecuta:

```powershell
cd C:\Users\andre\OneDrive\Documentos\infra\Juego-Mancala-con-IA-y-paralelizacion\backend
pip install -r requirements.txt
uvicorn app.main:app --reload --port 8000
```

Esta opcion deja la API corriendo directamente en la maquina, util para revisar errores de validacion o probar `/docs` sin levantar el resto del stack.

## Kubernetes Local (kind / minikube / k3d)

En esta fase se pide dejar listo un cluster Kubernetes local con kind. kind significa Kubernetes in Docker: crea un cluster usando contenedores Docker, asi que no necesitas una instalacion pesada de Kubernetes real en la maquina.

Sirve para probar los manifiestos de `deploy/local/` antes de ir a la nube y para validar que backend, frontend y motor levantan dentro de Kubernetes.

### Guia copiar y pegar en PowerShell

#### A. Verificar Docker Desktop

```powershell
docker version
docker info
```

Si estos comandos fallan, primero abre Docker Desktop y espera a que termine de iniciar.

#### B. Instalar kind y kubectl

```powershell
winget install Kubernetes.kind -e --source winget
winget install Kubernetes.kubectl -e --source winget
```

#### C. Crear el cluster local

```powershell
kind create cluster --name mancala-local
```

Si aparece el error `node(s) already exist for a cluster with the name "mancala-local"`, significa que el cluster ya fue creado antes. En ese caso no lo vuelvas a crear: sigue con el paso D y reutiliza el cluster existente.

#### D. Verificar que el cluster quedo listo

```powershell
kubectl cluster-info --context kind-mancala-local
kubectl get nodes
```

#### E. Construir las imagenes del proyecto

```powershell
cd C:\Users\andre\OneDrive\Documentos\infra\Juego-Mancala-con-IA-y-paralelizacion
docker build -t backend:local ./backend
docker build -t frontend:local ./frontend
docker build -t motor:local ./motor
```

#### F. Cargar las imagenes en kind

```powershell
kind load docker-image backend:local --name mancala-local
kind load docker-image frontend:local --name mancala-local
kind load docker-image motor:local --name mancala-local
```

#### G. Aplicar los manifiestos de Kubernetes

```powershell
kubectl apply -f deploy/local/backend-deployment.yaml
kubectl apply -f deploy/local/backend-service.yaml
kubectl apply -f deploy/local/configmap.yaml
kubectl apply -f deploy/local/frontend-deployment.yaml
kubectl apply -f deploy/local/frontend-service.yaml
kubectl apply -f deploy/local/motor-deployment.yaml
kubectl apply -f deploy/local/motor-service.yaml
```

No uses `kubectl apply -f deploy/local/` porque esa carpeta tambien contiene `docker-compose.yml`, y ese archivo no es un manifiesto de Kubernetes. Si apuntas al directorio completo, `kubectl` intenta leer el compose y falla con `apiVersion not set` y `kind not set`.

#### H. Forzar que los deployments usen las imagenes locales

```powershell
kubectl rollout restart deployment/backend
kubectl rollout restart deployment/frontend
kubectl rollout restart deployment/motor
```

#### I. Verificar el estado del cluster

```powershell
kubectl get pods -A
kubectl get svc -A
```

### Qué deberías ver

- `kind create cluster` termina sin error y crea el contexto `kind-mancala-local`.
- `kubectl get nodes` muestra al menos un nodo en estado `Ready`.
- `kubectl get pods -A` muestra los pods de `motor`, `backend` y `frontend` arrancando o en `Running`.
- `kubectl get svc -A` lista los servicios del proyecto.

### Probar la API dentro de Kubernetes

Si quieres revisar el backend desde tu maquina, usa `port-forward`:

```powershell
kubectl port-forward svc/api-svc 8000:8000
```

Si ya tienes el backend local corriendo en `http://localhost:8000`, ese puerto quedara ocupado y `port-forward` fallara. En ese caso usa otro puerto local, por ejemplo:

```powershell
kubectl port-forward svc/api-svc 8001:8000
```

Luego, en otra ventana de PowerShell:

```powershell
curl http://localhost:8000/healthz
curl http://localhost:8000/readyz
```

Si usaste `8001:8000`, cambia las pruebas a `http://localhost:8001/healthz` y `http://localhost:8001/readyz`.

### Diagnóstico rápido

Si un pod no queda en `Running` o aparece `ErrImagePull`, `CrashLoopBackOff` o `Completed`, usa esto:

```powershell
kubectl describe pods -l app=backend
kubectl describe pods -l app=motor
kubectl logs (kubectl get pods -l app=backend -o jsonpath='{.items[0].metadata.name}') -c backend --tail=200
kubectl logs (kubectl get pods -l app=motor -o jsonpath='{.items[0].metadata.name}') --tail=200
```

### Script automatizado

Si prefieres no ejecutar todo manualmente, corre el script desde la raíz del repo:

```powershell
.\scripts\deploy-kind.ps1
```

Ese script construye las imagenes Docker, aplica los manifiestos de `deploy/local/`, carga las imagenes en el cluster `kind` llamado `mancala-local`, reinicia los deployments y muestra pods, servicios y logs basicos.

Ademas, valida el stack completo: espera a que backend, frontend y motor queden listos, hace `port-forward` temporal para probar `GET /healthz`, `GET /readyz`, `POST /move` con board valido e invalido, y confirma que el frontend sirve el HTML esperado.




