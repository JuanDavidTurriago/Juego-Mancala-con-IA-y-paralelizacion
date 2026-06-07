# 05 - Despliegue en la nube

## Plataforma desplegada

La aplicacion se desplego el 7 de junio de 2026 en Google Kubernetes Engine
(GKE) Autopilot, region `us-central1`, proyecto `mancala-kalah`. Se eligio
Autopilot para delegar la administracion de nodos y ajustar la capacidad a los
recursos solicitados por los pods.

La topologia desplegada es:

- 2 replicas del motor C++.
- 3 replicas del backend FastAPI, requisito de balanceo horizontal.
- 1 replica del frontend Nginx.
- Services `ClusterIP` para los tres componentes.
- Ingress nativo de GKE con una IP global estatica.

El motor no se publica en Internet. El navegador accede al frontend y al
backend mediante hosts separados, mientras el backend llama al motor por
`http://motor:8080` dentro del cluster.

## Imagenes

Las imagenes estan en Artifact Registry, repositorio
`us-central1-docker.pkg.dev/mancala-kalah/mancala`. Los manifiestos fijan el
tag inmutable:

```text
cd2a98f04462fded40ffc034a4c1c52f707f09f3
```

Al estar el registro y GKE en el mismo proyecto, los nodos usan IAM de Google
y no se necesita un `imagePullSecret` de GHCR.

## Recursos y actualizaciones

Cada Deployment declara `requests` y `limits`. El motor solicita `500m` de CPU
por replica y permite hasta 2 CPUs. El ConfigMap fija
`OMP_NUM_THREADS=2`, valor coherente con ese limite.

Los Deployments usan:

```yaml
strategy:
  type: RollingUpdate
  rollingUpdate:
    maxSurge: 0
    maxUnavailable: 1
```

Esta estrategia evita pedir capacidad temporal adicional durante una
actualizacion. Fue necesaria porque la cuota educativa de Compute Engine no
permitia crear un nodo extra para los pods de `maxSurge`.

Los tres componentes incluyen `startupProbe`, `readinessProbe` y
`livenessProbe`. `startupProbe` evita reinicios falsos mientras Autopilot
descarga imagenes y prepara el nodo. El `/readyz` del backend solo responde
200 cuando puede comunicarse con el motor.

## Red publica

Se reservo la direccion global `8.232.201.150` con el nombre `mancala-ip`.
El Ingress nativo de GKE usa Network Endpoint Groups para enviar trafico
directamente a los pods.

| Componente | URL |
|---|---|
| Frontend | `http://mancala.8.232.201.150.nip.io` |
| Backend | `http://api.mancala.8.232.201.150.nip.io` |
| Motor | Solo interno: `http://motor:8080` |

`nip.io` resuelve ambos hosts a la IP estatica y evita depender de un dominio
comprado durante la entrega. El ConfigMap permite por CORS solamente el origen
del frontend. Para produccion se debe agregar un dominio propio y TLS.

## Creacion del cluster

```bash
gcloud auth login
gcloud config set project mancala-kalah
gcloud config set compute/region us-central1

gcloud services enable \
  container.googleapis.com \
  compute.googleapis.com \
  artifactregistry.googleapis.com

gcloud container clusters create-auto mancala-gke \
  --region us-central1

gcloud container clusters get-credentials mancala-gke \
  --region us-central1

gcloud compute addresses create mancala-ip \
  --global \
  --ip-version=IPV4
```

## Aplicacion y verificacion

```bash
kubectl apply -f deploy/cloud/configmap.yaml
kubectl apply -f deploy/cloud/motor.yaml
kubectl apply -f deploy/cloud/backend.yaml
kubectl apply -f deploy/cloud/frontend.yaml
kubectl apply -f deploy/cloud/ingress.yaml

kubectl rollout status deployment/motor
kubectl rollout status deployment/backend
kubectl rollout status deployment/frontend
kubectl get pods,svc,ingress
```

Comprobaciones ejecutadas:

```bash
curl http://api.mancala.8.232.201.150.nip.io/healthz
curl http://api.mancala.8.232.201.150.nip.io/readyz
```

Resultado observado:

```text
frontend_status=200
healthz=ok
readyz=ready
cors_status=200
move=1 algorithm=alphabeta threads=2 nodes=363 prunes=72 seed_total=48
```

La interfaz tambien se probo en navegador. Mostro `API: conectada`, proceso
una jugada humana y recibio una respuesta Alpha-Beta sin errores de consola.

## Seguridad y limitaciones

No hay secretos en los YAML. La URL interna, los hilos, la profundidad y CORS
son configuracion no sensible. El backend y el frontend se publican por HTTP
solo para la demostracion academica; un despliegue permanente debe usar
certificado administrado y HTTPS.

La IP global, el balanceador y el cluster consumen credito aunque no haya
trafico. Despues de recopilar las evidencias se debe eliminar el cluster:

```bash
gcloud container clusters delete mancala-gke \
  --region us-central1
```

Cuando ya no se vaya a reconstruir el Ingress, tambien se libera la IP:

```bash
gcloud compute addresses delete mancala-ip --global
```

Artifact Registry puede conservarse para la entrega. Sus imagenes ocupan
almacenamiento, pero su costo es mucho menor que mantener GKE y el balanceador
encendidos.
