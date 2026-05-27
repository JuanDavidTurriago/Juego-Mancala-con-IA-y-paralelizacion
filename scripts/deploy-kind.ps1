<#
Script: deploy-kind.ps1
Propósito: Construir imágenes Docker locales, cargarlas en kind, aplicar manifiestos, reiniciar despliegues y validar el stack completo.
Ejecutar desde la raíz del repositorio en PowerShell:
  .\scripts\deploy-kind.ps1
#>

$ErrorActionPreference = 'Stop'

function Run-Check([ScriptBlock]$cmd, [string]$errorMessage) {
    & $cmd
    if ($LASTEXITCODE -ne 0) {
        throw $errorMessage
    }
}

function Wait-ForHttp {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Url,
        [int]$TimeoutSeconds = 60
    )

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        try {
            Invoke-WebRequest -Uri $Url -UseBasicParsing | Out-Null
            return
        } catch {
            [System.Threading.Thread]::Sleep(1000)
        }
    }

    throw "Timeout esperando respuesta en $Url"
}

function Start-PortForward {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Resource,
        [Parameter(Mandatory = $true)]
        [string]$Ports
    )

    return Start-Process -FilePath kubectl -ArgumentList @('port-forward', $Resource, $Ports) -PassThru -WindowStyle Hidden
}

function Stop-ProcessSafe {
    param(
        [Parameter(Mandatory = $true)]
        [System.Diagnostics.Process]$Process
    )

    if ($Process -and -not $Process.HasExited) {
        Stop-Process -Id $Process.Id -Force -ErrorAction SilentlyContinue
    }
}

function Invoke-CheckedWebRequest {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Url,
        [string]$Method = 'GET',
        [string]$Body,
        [string]$ContentType = 'application/json',
        [int[]]$ExpectedStatusCodes = @(200)
    )

    try {
        if ($Method -eq 'GET') {
            $response = Invoke-WebRequest -Uri $Url -UseBasicParsing
        } else {
            $response = Invoke-WebRequest -Uri $Url -Method $Method -ContentType $ContentType -Body $Body -UseBasicParsing
        }
    } catch {
        if ($_.Exception.Response -and $_.Exception.Response.StatusCode) {
            $statusCode = [int]$_.Exception.Response.StatusCode
            if ($ExpectedStatusCodes -contains $statusCode) {
                return [pscustomobject]@{
                    StatusCode = $statusCode
                    Content = $_.ErrorDetails.Message
                }
            }
            throw
        }

        throw
    }

    $statusCode = [int]$response.StatusCode
    if (-not ($ExpectedStatusCodes -contains $statusCode)) {
        throw "HTTP $statusCode no esperado para $Url"
    }

    return [pscustomobject]@{
        StatusCode = $statusCode
        Content = $response.Content
    }
}

$backendForward = $null
$frontendForward = $null

try {
    Write-Host "=== 1) Construir imágenes Docker ==="
    Run-Check { docker build -t backend:local ./backend } "Fallo al construir backend"
    Run-Check { docker build -t frontend:local ./frontend } "Fallo al construir frontend"
    Run-Check { docker build -t motor:local ./motor } "Fallo al construir motor"

    Write-Host "=== 2) Aplicar manifiestos Kubernetes ==="
    $manifests = @(
        'deploy/local/backend-deployment.yaml',
        'deploy/local/backend-service.yaml',
        'deploy/local/configmap.yaml',
        'deploy/local/frontend-deployment.yaml',
        'deploy/local/frontend-service.yaml',
        'deploy/local/motor-deployment.yaml',
        'deploy/local/motor-service.yaml'
    )

    foreach ($manifest in $manifests) {
        Write-Host "Aplicando $manifest"
        Run-Check { kubectl apply -f $manifest } "Fallo al aplicar $manifest"
    }

    Write-Host "=== 3) Cargar imágenes en kind (cluster: mancala-local) ==="
    Run-Check { kind load docker-image backend:local --name mancala-local } "Fallo al cargar backend en kind"
    Run-Check { kind load docker-image frontend:local --name mancala-local } "Fallo al cargar frontend en kind"
    Run-Check { kind load docker-image motor:local --name mancala-local } "Fallo al cargar motor en kind"

    Write-Host "=== 4) Reiniciar despliegues y esperar estado listo ==="
    Run-Check { kubectl rollout restart deployment/backend } "Fallo al reiniciar deployment/backend"
    Run-Check { kubectl rollout restart deployment/frontend } "Fallo al reiniciar deployment/frontend"
    Run-Check { kubectl rollout restart deployment/motor } "Fallo al reiniciar deployment/motor"
    Run-Check { kubectl rollout status deployment/backend --timeout=180s } "Backend no quedó listo a tiempo"
    Run-Check { kubectl rollout status deployment/frontend --timeout=180s } "Frontend no quedó listo a tiempo"
    Run-Check { kubectl rollout status deployment/motor --timeout=180s } "Motor no quedó listo a tiempo"

    Write-Host "=== 5) Estado de cluster: pods y servicios ==="
    & kubectl get pods -A
    & kubectl get svc -A

    Write-Host "=== 6) Verificar backend y frontend con port-forward temporal ==="
    $backendForward = Start-PortForward -Resource 'svc/api-svc' -Ports '8001:8000'
    $frontendForward = Start-PortForward -Resource 'svc/front-svc' -Ports '8081:80'

    Wait-ForHttp -Url 'http://localhost:8001/healthz' -TimeoutSeconds 60
    Wait-ForHttp -Url 'http://localhost:8081/' -TimeoutSeconds 60

    $health = Invoke-CheckedWebRequest -Url 'http://localhost:8001/healthz'
    if ($health.Content -notmatch '"status"\s*:\s*"ok"') {
        throw 'La respuesta de /healthz no contiene {"status":"ok"}'
    }

    $ready = Invoke-CheckedWebRequest -Url 'http://localhost:8001/readyz'
    if ($ready.Content -notmatch '"status"\s*:\s*"ready"') {
        throw 'La respuesta de /readyz no contiene {"status":"ready"}'
    }

    $moveBody = '{"board":[4,4,4,4,4,4,0,4,4,4,4,4,4,0],"side":0,"algo":"alphabeta","depth":8,"threads":4}'
    $move = Invoke-CheckedWebRequest -Url 'http://localhost:8001/move' -Method 'POST' -Body $moveBody -ExpectedStatusCodes @(200)
    if ($move.Content -notmatch '"move"\s*:\s*3' -or $move.Content -notmatch '"threads_used"\s*:\s*4') {
        throw 'La respuesta de /move no coincide con el mock esperado'
    }

    $badBody = '{"board":[4,4,4,4,4,4,0,4,4,4,4,4,4],"side":0,"algo":"alphabeta","depth":8,"threads":4}'
    $bad = Invoke-CheckedWebRequest -Url 'http://localhost:8001/move' -Method 'POST' -Body $badBody -ExpectedStatusCodes @(422)
    if ($bad.StatusCode -ne 422) {
        throw 'La validación del board inválido no devolvió 422'
    }

    $frontend = Invoke-CheckedWebRequest -Url 'http://localhost:8081/'
    if ($frontend.Content -notmatch 'Mancala \(Kalah\)' -or $frontend.Content -notmatch 'Solicitar jugada') {
        throw 'El frontend no devolvió el HTML esperado'
    }

    Write-Host "=== 7) Resultado final ==="
    Write-Host "Backend: OK (/healthz, /readyz, /move y validación 422)"
    Write-Host "Frontend: OK (HTML servido correctamente)"
    Write-Host "Motor: OK (deployment listo y pod Running)"
    Write-Host "Stack completo validado en kind"
}
finally {
    Stop-ProcessSafe -Process $backendForward
    Stop-ProcessSafe -Process $frontendForward
}
