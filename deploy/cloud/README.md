# deploy/cloud/

Manifiestos YAML del despliegue en un servicio gestionado de Kubernetes
(AWS EKS, Azure AKS, GCP GKE, DigitalOcean DOKS u otro).

**Preparacion Fase 4 (hacer ya):** leer [PREPARACION-FASE4.md](./PREPARACION-FASE4.md) — cuenta cloud, cluster de prueba, ImagePullSecret GHCR.

Reglas innegociables:

- Toda la configuracion del cluster debe quedar declarada en archivos YAML versionados aqui.
- El balanceo de carga lo maneja el Service (LoadBalancer) o Ingress.
- Cada contenedor declara `requests` y `limits` de CPU y memoria.
- Imagenes GHCR con tag **SHA inmutable** (sin `:latest`).
- Imagenes **privadas**: crear `ghcr-credentials` antes del apply (ver `ghcr-secret.yaml.example`).

## Checklist de manifiestos

| Archivo | Verificacion |
|---------|--------------|
| `backend.yaml` | `replicas: 3`, probes, resources, `imagePullSecrets` |
| `motor.yaml` | `resources.requests` / `limits`, probes, `imagePullSecrets` |
| `frontend.yaml` | resources, probes, `imagePullSecrets` |
| `ingress.yaml` | `ingressClassName: nginx`, anotaciones proxy timeout/body (EKS + NGINX Ingress) |
| `configmap.yaml` | `USE_MOCK=false`, `MOTOR_URL=http://motor:8080`, CORS con dominio real |

Alternativas de Ingress por proveedor: `ingress-aws-alb.yaml.example`, `ingress-gcp-gke.yaml.example`.

## Orden de aplicacion

```bash
bash deploy/scripts/create-ghcr-secret.sh default   # GHCR_USER + GHCR_PAT en entorno

kubectl apply -f deploy/cloud/configmap.yaml
kubectl apply -f deploy/cloud/motor.yaml
kubectl apply -f deploy/cloud/backend.yaml
kubectl apply -f deploy/cloud/frontend.yaml
kubectl apply -f deploy/cloud/ingress.yaml

kubectl rollout status deployment/motor
kubectl rollout status deployment/backend
kubectl get ingress
```

## GHCR desde el cluster

Prueba local con PAT:

```bash
export GHCR_USER=<github_user>
export GHCR_PAT=<pat_read_packages>
echo "$GHCR_PAT" | docker login ghcr.io -u "$GHCR_USER" --password-stdin
docker pull ghcr.io/juandavidturriago/mancala-motor:<SHA>
```

Si el pull anonimo devuelve `unauthorized`, el ImagePullSecret es obligatorio en Kubernetes.
