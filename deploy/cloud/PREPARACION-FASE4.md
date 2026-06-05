# Preparacion Fase 4 — cuenta cloud y cluster

No esperar a la entrega para abrir la cuenta. Registro, verificacion de identidad y creditos pueden tardar varios dias.

## 1. Elegir proveedor y crear cuenta

| Proveedor | Servicio K8s | Enlace registro | Notas |
|-----------|--------------|-----------------|-------|
| **AWS** (recomendado para este repo) | EKS | https://aws.amazon.com/free/ | Free tier 12 meses; EKS cobra ~$0.10/h por cluster ademas de nodos |
| GCP | GKE | https://cloud.google.com/free | $300 creditos nuevos; Autopilot simplifica operacion |
| Azure | AKS | https://azure.microsoft.com/free/ | Creditos iniciales; AKS no cobra plano de control en tier free limitado |

### Verificacion minima (cuenta activa)

- Tarjeta o metodo de pago verificado (aunque uses free tier).
- MFA habilitado en la cuenta root / admin.
- Cuota suficiente para al menos 1 cluster y 2 nodos pequenos.

## 2. Crear cluster gestionado (smoke test)

### AWS EKS (alineado con `ingress.yaml` + NGINX Ingress)

```bash
# Instalar: aws cli v2, kubectl, eksctl
aws sts get-caller-identity

eksctl create cluster \
  --name mancala-eks \
  --region us-east-1 \
  --nodes 2 \
  --node-type t3.medium \
  --with-oidc

kubectl get nodes

# NGINX Ingress Controller (LoadBalancer AWS crea ELB/NLB)
kubectl apply -f https://raw.githubusercontent.com/kubernetes/ingress-nginx/controller-v1.11.3/deploy/static/provider/cloud/deploy.yaml
kubectl wait --namespace ingress-nginx \
  --for=condition=ready pod \
  --selector=app.kubernetes.io/component=controller \
  --timeout=120s
```

### GCP GKE (alternativa)

```bash
gcloud auth login
gcloud config set project <PROJECT_ID>

gcloud container clusters create mancala-gke \
  --zone us-central1-a \
  --num-nodes 2 \
  --machine-type e2-medium

gcloud container clusters get-credentials mancala-gke
kubectl get nodes
```

Usar anotaciones GKE en `ingress-gcp-gke.yaml.example` si no instalas NGINX.

### Azure AKS (alternativa)

```bash
az login
az group create --name mancala-rg --location eastus

az aks create \
  --resource-group mancala-rg \
  --name mancala-aks \
  --node-count 2 \
  --node-vm-size Standard_B2s \
  --generate-ssh-keys

az aks get-credentials --resource-group mancala-rg --name mancala-aks
kubectl get nodes
```

## 3. GHCR — ImagePullSecret obligatorio

Las imagenes del proyecto en GHCR son **privadas**. Antes de `kubectl apply -f deploy/cloud/`:

```bash
kubectl create secret docker-registry ghcr-credentials \
  --docker-server=ghcr.io \
  --docker-username=<GITHUB_USER> \
  --docker-password=<PAT_read_packages>
```

Los Deployments en `motor.yaml`, `backend.yaml` y `frontend.yaml` referencian `imagePullSecrets: ghcr-credentials`.

Prueba de acceso al registry:

```bash
echo "$PAT" | docker login ghcr.io -u <GITHUB_USER> --password-stdin
docker pull ghcr.io/juandavidturriago/mancala-motor:<SHA_del_manifiesto>
```

Alternativa academica: hacer publicos los paquetes en GitHub → Packages → Package settings → Change visibility (solo si el curso lo permite).

## 4. Checklist de manifiestos (`deploy/cloud/`)

| Archivo | Requisito | Estado |
|---------|-----------|--------|
| `backend.yaml` | `replicas: 3` | Verificar en YAML |
| `motor.yaml` | `resources.requests` y `resources.limits` | Verificar en YAML |
| `ingress.yaml` | Anotaciones NGINX para EKS + timeout POST `/move` | Verificar en YAML |
| `configmap.yaml` | `USE_MOCK=false`, CORS con dominio real | Ajustar host antes de prod |
| `ghcr-secret.yaml.example` | Instrucciones ImagePullSecret | Crear secret en cluster |

## 5. Orden de despliegue en nube

```bash
kubectl create secret docker-registry ghcr-credentials ...   # si no existe
kubectl apply -f deploy/cloud/configmap.yaml
kubectl apply -f deploy/cloud/motor.yaml
kubectl apply -f deploy/cloud/backend.yaml
kubectl apply -f deploy/cloud/frontend.yaml
kubectl apply -f deploy/cloud/ingress.yaml

kubectl rollout status deployment/motor
kubectl rollout status deployment/backend
kubectl get ingress
```

Actualizar en `ingress.yaml` y `configmap.yaml` los hosts `mancala.example.edu.co` por el dominio real del grupo y crear registros DNS al balanceador del Ingress Controller.
