# deploy/cloud/

Manifiestos del despliegue activo en GKE Autopilot, proyecto
`mancala-kalah`, region `us-central1`.

## Arquitectura

| Archivo | Recurso principal |
|---|---|
| `configmap.yaml` | URL interna del motor, CORS, OpenMP y profundidad |
| `motor.yaml` | 2 replicas C++, Service interno `motor:8080` |
| `backend.yaml` | 3 replicas FastAPI y NEG para GKE Ingress |
| `frontend.yaml` | 1 replica Nginx y NEG para GKE Ingress |
| `ingress.yaml` | Ingress GCE con IP global `mancala-ip` |

Las imagenes se leen desde Artifact Registry con tags SHA inmutables. No se
usa `latest` ni se necesita `imagePullSecret`, porque GKE y el registro estan
en el mismo proyecto de Google Cloud.

## Despliegue

```bash
gcloud container clusters get-credentials mancala-gke \
  --region us-central1 \
  --project mancala-kalah

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

URLs de prueba:

- Frontend: `http://mancala.8.232.201.150.nip.io`
- API: `http://api.mancala.8.232.201.150.nip.io`

## Costos

GKE, el balanceador y la IP reservada consumen credito mientras existan. Al
terminar las pruebas:

```bash
gcloud container clusters delete mancala-gke --region us-central1
gcloud compute addresses delete mancala-ip --global
```

Los ejemplos `ghcr-secret.yaml.example` e `ingress-aws-alb.yaml.example` se
conservan solo como alternativas para otros proveedores.
