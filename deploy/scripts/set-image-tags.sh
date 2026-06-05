#!/usr/bin/env bash
set -euo pipefail

# Sustituye los tags de imagen en los manifiestos K8s por un SHA inmutable.
# Uso (CI): GHCR_OWNER=owner IMAGE_TAG=$GITHUB_SHA bash deploy/scripts/set-image-tags.sh

GHCR_OWNER="${GHCR_OWNER:-${GITHUB_REPOSITORY_OWNER,,}}"
IMAGE_TAG="${IMAGE_TAG:-${GITHUB_SHA:-}}"

if [[ -z "${GHCR_OWNER}" || -z "${IMAGE_TAG}" ]]; then
  echo "GHCR_OWNER e IMAGE_TAG deben estar definidos." >&2
  exit 1
fi

if [[ "${IMAGE_TAG}" == "latest" ]]; then
  echo "El tag 'latest' esta prohibido en produccion." >&2
  exit 1
fi

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MANIFESTS=(
  "${ROOT}/local/k8s/motor.yaml"
  "${ROOT}/local/k8s/backend.yaml"
  "${ROOT}/local/k8s/frontend.yaml"
  "${ROOT}/cloud/motor.yaml"
  "${ROOT}/cloud/backend.yaml"
  "${ROOT}/cloud/frontend.yaml"
)

for manifest in "${MANIFESTS[@]}"; do
  if [[ ! -f "${manifest}" ]]; then
    echo "Manifiesto no encontrado: ${manifest}" >&2
    exit 1
  fi

  sed -i "s|ghcr.io/[^/]*/mancala-motor:[^[:space:]]*|ghcr.io/${GHCR_OWNER}/mancala-motor:${IMAGE_TAG}|g" "${manifest}"
  sed -i "s|ghcr.io/[^/]*/mancala-backend:[^[:space:]]*|ghcr.io/${GHCR_OWNER}/mancala-backend:${IMAGE_TAG}|g" "${manifest}"
  sed -i "s|ghcr.io/[^/]*/mancala-frontend:[^[:space:]]*|ghcr.io/${GHCR_OWNER}/mancala-frontend:${IMAGE_TAG}|g" "${manifest}"
done

echo "Manifiestos actualizados a ghcr.io/${GHCR_OWNER}/*:${IMAGE_TAG}"
