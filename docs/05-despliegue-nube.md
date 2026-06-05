# 05 - Despliegue en la nube

## Objetivo del despliegue

El despliegue de nube busca ejecutar la misma arquitectura local en un cluster Kubernetes administrado o autogestionado. Los manifiestos estan en `deploy/cloud/` y no dependen de consola web. La configuracion queda declarada como YAML: Deployments, Services, ConfigMap e Ingress. El backend se despliega con 3 replicas para cumplir el requisito de balanceo; el motor y el frontend se despliegan con 2 replicas como base razonable para disponibilidad.

Los manifiestos usan imagenes GHCR con tag versionado `v1.0.0` en lugar de `latest`. En una entrega real, el pipeline puede sustituir esos tags por `${{ github.sha }}` despues de publicar imagenes. La regla importante es que el cluster no apunte a marcadores genericos ni a tags ambiguos que cambien sin control.

## Recursos declarados

Cada contenedor declara `requests` y `limits`. El motor solicita mas CPU porque ejecuta Alfa-Beta con OpenMP. El backend solicita menos CPU pero memoria suficiente para FastAPI, Pydantic y httpx. El frontend es liviano porque Nginx sirve archivos estaticos.

Ejemplo conceptual:

```yaml
resources:
  requests:
    cpu: "500m"
    memory: "256Mi"
  limits:
    cpu: "2000m"
    memory: "768Mi"
```

La diferencia entre request y limit permite que Kubernetes reserve capacidad minima y, al mismo tiempo, deje usar mas CPU cuando el nodo lo permita. Para el motor esto importa porque `threads` puede crear varios hilos por solicitud. Si el limite es bajo, pedir demasiados hilos no mejora tiempos y puede empeorarlos por contencion.

## Servicios internos

El Service `motor` es `ClusterIP` y escucha en `8080`, el mismo puerto interno del binario `motor_server`. Solo el backend necesita llamarlo. El Service `backend` tambien es `ClusterIP` en nube; queda expuesto por Ingress bajo el host `api.mancala.example.edu.co`. El Service `frontend` se expone por Ingress bajo `mancala.example.edu.co`.

Esta separacion de host simplifica CORS. El navegador entra por `https://mancala.example.edu.co` y llama al backend en `https://api.mancala.example.edu.co`. El ConfigMap define `CORS_ORIGINS=https://mancala.example.edu.co`, que es un origen explicito y evita usar `"*"`.

## Probes

Todos los servicios tienen readiness y liveness probes. El motor responde `/healthz` y `/readyz` directamente. El backend responde `/healthz` si el proceso vive, pero `/readyz` solo responde 200 cuando puede contactar al motor. Esta diferencia es importante: un backend sin motor no esta listo para recibir trafico real de `/move`.

Las probes del frontend apuntan a `/`, suficiente para un servidor estatico. En produccion se podria servir una ruta `/healthz` desde Nginx, pero no es necesario para la rubrica.

## Ingress

`deploy/cloud/ingress.yaml` declara dos hosts:

- `mancala.example.edu.co`: frontend.
- `api.mancala.example.edu.co`: backend.

Antes de aplicar en un cluster real, se reemplazan esos dominios por dominios del grupo y se crean los registros DNS hacia el balanceador del Ingress Controller. Aunque el manifiesto usa dominios de ejemplo, es una plantilla concreta y valida que documenta el patron esperado. Si el profesor exige un dominio especifico, basta con cambiar host y `CORS_ORIGINS` en el ConfigMap.

## Publicacion de imagenes

El workflow propio publica imagenes en GHCR con tag `${{ github.sha }}` cuando hay push a `main`. Los manifiestos cloud pueden actualizarse manualmente o por una etapa CD para apuntar a ese SHA. El patron recomendado es:

```bash
kubectl set image deployment/motor motor=ghcr.io/<owner>/mancala-motor:<sha>
kubectl set image deployment/backend backend=ghcr.io/<owner>/mancala-backend:<sha>
kubectl set image deployment/frontend frontend=ghcr.io/<owner>/mancala-frontend:<sha>
```

Para una entrega academica, tambien se puede fijar un tag semantico como `v1.0.0` si corresponde a una imagen ya publicada. Lo que se evita es `latest`, porque impide saber que version exacta esta corriendo.

## Aplicacion de manifiestos

Con `kubectl` apuntando al contexto de nube:

```bash
kubectl apply -f deploy/cloud/configmap.yaml
kubectl apply -f deploy/cloud/motor.yaml
kubectl apply -f deploy/cloud/backend.yaml
kubectl apply -f deploy/cloud/frontend.yaml
kubectl apply -f deploy/cloud/ingress.yaml
```

Luego se revisa:

```bash
kubectl get deploy
kubectl get pods
kubectl get svc
kubectl get ingress
kubectl rollout status deployment/backend
```

Una prueba basica despues de DNS:

```bash
curl -s https://api.mancala.example.edu.co/healthz
curl -s https://api.mancala.example.edu.co/readyz
curl -s -X POST https://api.mancala.example.edu.co/move \
  -H "Content-Type: application/json" \
  -d '{"board":[4,4,4,4,4,4,0,4,4,4,4,4,4,0],"side":0,"depth":8,"threads":4}'
```

## Seguridad y configuracion

No se guardan secretos dentro de los manifiestos. Las variables actuales no son sensibles: URL interna del motor, timeout, CORS y parametros de puerto/hilos. Si en el futuro se agrega autenticacion, tokens o credenciales de observabilidad, deben ir en Kubernetes Secrets o en el sistema de secretos del proveedor.

El frontend no habla directo con el motor. Esto evita exponer el motor C++ a internet y mantiene una sola frontera publica: el backend. El motor queda en red interna del cluster y solo recibe trafico del Service de backend.

## Riesgos y mitigaciones

El riesgo principal en nube es sobredimensionar hilos frente a CPU real. Un pod con limite de 2 CPUs y solicitudes con `threads=8` puede tener peor rendimiento que `threads=2`. La mitigacion es medir con el benchmark, ajustar limites y documentar valores recomendados. Otro riesgo es CORS mal configurado despues de cambiar dominio. La solucion es actualizar `CORS_ORIGINS` junto con el Ingress y verificar con una solicitud `OPTIONS`.

Tambien hay que considerar cold starts de imagenes y pull desde GHCR. Si el cluster no tiene permisos para leer paquetes privados, las imagenes deben ser publicas o debe declararse `imagePullSecrets`. Para simplificar la entrega, se asume que las imagenes GHCR publicadas son publicas.
