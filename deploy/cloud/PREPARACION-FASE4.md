# Preparacion Fase 4 - Google Cloud

## Estado actual

- Proyecto: `mancala-kalah`.
- Cuenta activa y facturacion educativa habilitada.
- Region: `us-central1`.
- Cluster: `mancala-gke`, GKE Autopilot.
- Registro: `us-central1-docker.pkg.dev/mancala-kalah/mancala`.
- IP global: `8.232.201.150`, recurso `mancala-ip`.

## Herramientas

Instalar Google Cloud CLI y los componentes de GKE:

```powershell
winget install --id Google.CloudSDK
gcloud components install gke-gcloud-auth-plugin
gcloud auth login
gcloud auth application-default login
```

Configurar el proyecto:

```bash
gcloud config set project mancala-kalah
gcloud config set compute/region us-central1
gcloud config set compute/zone us-central1-a
```

## APIs y cluster

```bash
gcloud services enable \
  container.googleapis.com \
  compute.googleapis.com \
  cloudresourcemanager.googleapis.com \
  iam.googleapis.com \
  artifactregistry.googleapis.com

gcloud container clusters create-auto mancala-gke \
  --region us-central1

gcloud container clusters get-credentials mancala-gke \
  --region us-central1
```

## Artifact Registry

```bash
gcloud artifacts repositories create mancala \
  --repository-format=docker \
  --location=us-central1

gcloud auth configure-docker us-central1-docker.pkg.dev
```

Las imagenes deben llevar un tag inmutable, preferiblemente el SHA del commit:

```bash
docker tag mancala-motor:local \
  us-central1-docker.pkg.dev/mancala-kalah/mancala/mancala-motor:<SHA>
docker push \
  us-central1-docker.pkg.dev/mancala-kalah/mancala/mancala-motor:<SHA>
```

Repetir para backend y frontend.

## IP e Ingress

```bash
gcloud compute addresses create mancala-ip \
  --global \
  --ip-version=IPV4

gcloud compute addresses describe mancala-ip \
  --global \
  --format="value(address)"
```

`ingress.yaml` usa el controlador GCE administrado por Google. No se instala
NGINX Ingress dentro del cluster, lo que reduce consumo de CPU y evita chocar
con la cuota educativa.

## Verificacion

```bash
kubectl get nodes
kubectl get deployments,pods,services,ingress
kubectl describe ingress mancala
curl http://api.mancala.8.232.201.150.nip.io/readyz
```

## Apagado

El cluster y el balanceador consumen creditos mientras existan:

```bash
gcloud container clusters delete mancala-gke --region us-central1
gcloud compute addresses delete mancala-ip --global
```

No ejecutar estos comandos hasta terminar capturas, pruebas de carga y
sustentacion remota.
