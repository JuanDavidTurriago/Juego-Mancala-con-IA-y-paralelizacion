<#
Script: deploy-kind.ps1
Propósito: Construir imágenes Docker locales, cargarlas en kind, aplicar manifests, reiniciar despliegues y mostrar estado.
Ejecutar desde la raíz del repositorio en PowerShell:
  .\scripts\deploy-kind.ps1
#>

function Run-Check([ScriptBlock]$cmd, [string]$errorMessage) {
    & $cmd
    if ($LASTEXITCODE -ne 0) {
        Write-Error $errorMessage
        exit $LASTEXITCODE
    }
}

Write-Host "=== 1) Construir imágenes Docker ==="
Run-Check { docker build -t backend:local ./backend } "Fallo al construir backend"
Run-Check { docker build -t frontend:local ./frontend } "Fallo al construir frontend"
Run-Check { docker build -t motor:local ./motor } "Fallo al construir motor"

Write-Host "=== 2) Aplicar manifests Kubernetes (deploy/local) ==="
$manifests = Get-ChildItem -Path "deploy/local" -Include *.yaml,*.yml -File -Recurse | Select-Object -ExpandProperty FullName
if (-not $manifests) {
    Write-Host "No se encontraron manifiestos YAML en deploy/local/"
} else {
    foreach ($m in $manifests) {
        # Leer primeras lineas para verificar si parece un manifiesto K8s
        $head = Get-Content -Path $m -TotalCount 20 -ErrorAction SilentlyContinue | Out-String
        if ($head -match '^[\s]*apiVersion\s*:' -or $head -match '^[\s]*kind\s*:') {
            Write-Host "Aplicando $m"
            kubectl apply -f $m
            if ($LASTEXITCODE -ne 0) {
                Write-Error "Fallo al aplicar $m"
                exit $LASTEXITCODE
            }
        } else {
            Write-Host "Omitiendo $m (no parece un manifiesto K8s)"
        }
    }
}

Write-Host "=== 3) Cargar imágenes en kind (cluster: mancala-local) ==="
Run-Check { kind load docker-image backend:local --name mancala-local } "Fallo al cargar backend en kind"
Run-Check { kind load docker-image frontend:local --name mancala-local } "Fallo al cargar frontend en kind"
Run-Check { kind load docker-image motor:local --name mancala-local } "Fallo al cargar motor en kind"

Write-Host "=== 4) Reiniciar despliegues para forzar pull de imagen local ==="
Run-Check { kubectl rollout restart deployment/backend } "Fallo al reiniciar deployment/backend"
Run-Check { kubectl rollout restart deployment/frontend } "Fallo al reiniciar deployment/frontend"
Run-Check { kubectl rollout restart deployment/motor } "Fallo al reiniciar deployment/motor"

Write-Host "=== 5) Estado de cluster: pods y servicios ==="
& kubectl get pods -A
& kubectl get svc -A

Write-Host "=== 6) Logs (primer pod backend y motor) ==="
$backendPod = kubectl get pods -l app=backend -o jsonpath='{.items[0].metadata.name}' 2>$null
if ($backendPod) {
    Write-Host "--- Logs backend ($backendPod) ---"
    kubectl logs $backendPod -c backend --tail=200
} else {
    Write-Host "No se encontró pod backend para mostrar logs"
}

$motorPod = kubectl get pods -l app=motor -o jsonpath='{.items[0].metadata.name}' 2>$null
if ($motorPod) {
    Write-Host "--- Logs motor ($motorPod) ---"
    kubectl logs $motorPod --tail=200
} else {
    Write-Host "No se encontró pod motor para mostrar logs"
}

Write-Host "=== Fin: si algo falla, usa 'kubectl describe pod <pod>' o pega aquí el error para ayuda." 
