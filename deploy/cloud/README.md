# deploy/cloud/

Manifiestos YAML del despliegue en un servicio gestionado de Kubernetes
(AWS EKS, Azure AKS, GCP GKE, DigitalOcean DOKS u otro).

Reglas innegociables:

- Toda la configuración del clúster debe quedar declarada en archivos
  YAML versionados aquí. No se acepta configurar el despliegue
  exclusivamente desde la consola web del proveedor.
- El balanceo de carga lo maneja el propio `Service` (tipo
  `LoadBalancer` o vía `Ingress`).
- Cada contenedor debe declarar `requests` y `limits` de CPU y memoria.
- Las imágenes vienen de un registro accesible por el clúster
  (Docker Hub, GHCR, ECR, ACR, GCR) con `tag` inmutable; no `latest`.

Estructura sugerida:

```text
deploy/cloud/
├── backend-deployment.yaml
├── backend-service.yaml
├── frontend-deployment.yaml
├── frontend-service.yaml
├── ingress.yaml
└── configmap.yaml
```
