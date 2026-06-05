#!/usr/bin/env bash
set -euo pipefail

# Crea el ImagePullSecret ghcr-credentials en el namespace indicado.
# Uso: GHCR_USER=... GHCR_PAT=... bash deploy/scripts/create-ghcr-secret.sh [namespace]

NAMESPACE="${1:-default}"

if [[ -z "${GHCR_USER:-}" || -z "${GHCR_PAT:-}" ]]; then
  echo "Define GHCR_USER y GHCR_PAT (PAT con read:packages)." >&2
  exit 1
fi

kubectl create secret docker-registry ghcr-credentials \
  --docker-server=ghcr.io \
  --docker-username="${GHCR_USER}" \
  --docker-password="${GHCR_PAT}" \
  --namespace="${NAMESPACE}" \
  --dry-run=client -o yaml | kubectl apply -f -

echo "Secret ghcr-credentials aplicado en namespace ${NAMESPACE}."
