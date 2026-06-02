# deploy/local/

Despliegue local en dos modalidades:

1. `docker-compose.yml` — la aplicación completa se levanta con
   `docker compose -f deploy/local/docker-compose.yml up --build`.
2. Manifiestos YAML de Kubernetes local (`kind`, `minikube` o `k3d`).
   Mínimo: `Deployment` del backend con ≥ 3 réplicas, `Service`s,
   `ConfigMap` para variables del motor, `liveness`/`readiness` *probes*
   en `/healthz` y `/readyz`.

Estructura sugerida:

```text
deploy/local/
├── docker-compose.yml
└── k8s/
    ├── backend-deployment.yaml
    ├── backend-service.yaml
    ├── frontend-deployment.yaml
    ├── frontend-service.yaml
    └── configmap.yaml
```
