# 04 - Despliegue Local

## Docker Compose

Pendiente por completar.

## Kubernetes Local (kind / minikube / k3d)

En esta fase se pide dejar listo un cluster Kubernetes local con kind. kind significa Kubernetes in Docker: crea un cluster de Kubernetes usando contenedores Docker, asi que no necesitas una instalacion pesada de Kubernetes real en la maquina.

Sirve para probar los manifiestos de `deploy/local/` antes de ir a la nube y para validar que backend, frontend y motor levantan dentro de Kubernetes.

### Instalacion de kind

1. Instalar Docker Desktop y verificar que este corriendo.
2. Instalar `kind`.
3. Instalar `kubectl`.
4. Crear el cluster local.

Pasos exactos (PowerShell). Ejecuta cada bloque por separado en PowerShell con privilegios de usuario regular (no es necesario elevar):

1) Asegúrate de que Docker Desktop está instalado y en ejecución.

```powershell
# Verifica que Docker esté running
docker version
docker info
```

2) Instala `kind` y `kubectl` (winget):

```powershell
winget install Kubernetes.kind -e --source winget
winget install Kubernetes.kubectl -e --source winget
```

3) Crea el cluster local con kind:

```powershell
kind create cluster --name mancala-local
```

4) Verifica el estado del cluster y el nodo:

```powershell
kubectl cluster-info --context kind-mancala-local
kubectl get nodes
```

5) Aplica los manifiestos del proyecto (desde la raíz del repo):

```powershell
kubectl apply -f deploy/local/
```

6) Comprueba pods y servicios:

```powershell
kubectl get pods -A
kubectl get svc -A
```

Qué deberías ver (resumen rápido):

- `kind create cluster` termina con mensaje de creación y el contexto `kind-mancala-local` se añade a kubectl.
- `kubectl get nodes` muestra al menos un nodo en estado `Ready`.
- `kubectl get pods -A` muestra los pods del `motor`, `backend` y `frontend` arrancando o en `Running`.
- `kubectl get svc -A` lista los servicios creados (puedes usar `kubectl port-forward` o `NodePort` según los manifiestos para acceder a la app).

### Uso basico

```powershell
kubectl apply -f deploy/local/
kubectl get pods
kubectl get svc
```

Si prefieren, tambien se puede usar minikube o k3d, pero kind suele ser la opcion mas simple para un cluster local reproducible.

### Construir, cargar y desplegar (comandos copy/paste)

Estos son los pasos que hemos seguido y que puedes ejecutar manualmente en PowerShell desde la raíz del repo. También hay un script automático `scripts/deploy-kind.ps1` que hace lo mismo.

1) Construir imágenes:

```powershell
docker build -t backend:local ./backend
docker build -t frontend:local ./frontend
docker build -t motor:local ./motor
```

2) Cargar imágenes en kind (cluster `mancala-local`):

```powershell
kind load docker-image backend:local --name mancala-local
kind load docker-image frontend:local --name mancala-local
kind load docker-image motor:local --name mancala-local
```

3) Reiniciar despliegues (forzar que usen la imagen local cargada):

```powershell
kubectl rollout restart deployment/backend
kubectl rollout restart deployment/frontend
kubectl rollout restart deployment/motor
```

4) Verificar estado:

```powershell
kubectl get pods -A
kubectl get svc -A
```

### Diagnóstico y pasos adicionales (logs / describe / port-forward)

Si algún pod no queda en `Running` o aparece `ErrImagePull`/`CrashLoopBackOff`/`Completed`, estos comandos ayudan a diagnosticar:

```powershell
# Listar y describir pods problemáticos
kubectl describe pods -l app=backend
kubectl describe pods -l app=motor

# Obtener logs
POD_BACKEND=$(kubectl get pods -l app=backend -o jsonpath='{.items[0].metadata.name}')
kubectl logs $POD_BACKEND -c backend --tail=200

POD_MOTOR=$(kubectl get pods -l app=motor -o jsonpath='{.items[0].metadata.name}')
kubectl logs $POD_MOTOR --tail=200

# Probar endpoints localmente con port-forward
kubectl port-forward svc/api-svc 8000:8000
# luego en otra terminal:
curl http://localhost:8000/healthz
curl http://localhost:8000/readyz
```

### Script automatizado

Ejecuta el script (desde la raíz del repo) para hacer todo automáticamente:

```powershell
.\scripts\deploy-kind.ps1
```

Breve descripción del script: `scripts/deploy-kind.ps1` automatiza los pasos comunes de desarrollo local: construye las imágenes Docker (`backend`, `frontend`, `motor`), aplica únicamente los manifiestos Kubernetes válidos de `deploy/local/`, carga las imágenes en el cluster `kind` (`mancala-local`), reinicia los deployments para forzar que usen las imágenes locales y muestra el estado y logs básicos para diagnóstico.




