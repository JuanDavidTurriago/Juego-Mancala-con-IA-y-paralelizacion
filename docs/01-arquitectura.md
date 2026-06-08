# 01 - Arquitectura

## Vision general

La solucion se organiza como una aplicacion distribuida pequena, formada por tres servicios con responsabilidades separadas. El servicio `motor` es un proceso C++17 independiente que implementa las reglas de Kalah(6,4), ejecuta busqueda Alfa-Beta y MCTS, y usa OpenMP para paralelizar la evaluacion de los movimientos de la raiz en Alpha-Beta. El servicio `backend` es una API FastAPI que valida el contrato publico, aplica CORS explicito y delega el calculo al motor por HTTP dentro de la red del despliegue. El servicio `frontend` es una interfaz web estatica servida por Nginx que permite editar el tablero, escoger jugador, algoritmo, profundidad o simulaciones, hilos, y visualizar la jugada recomendada junto con estadisticas.

Esta separacion es intencional. El motor concentra el costo computacional y puede escalarse, medirse y optimizarse sin mezclarlo con logica web. El backend actua como frontera de entrada para navegadores y clientes externos: valida JSON con Pydantic, traduce errores del motor a codigos HTTP y expone endpoints de salud para Docker, Kubernetes y CI. El frontend no contiene logica de IA; solamente construye solicitudes y presenta respuestas. Asi se evita que una parte de la aplicacion tenga que conocer detalles internos de otra.

```mermaid
flowchart LR
  Browser["Frontend web"] -->|POST /move| API["Backend FastAPI"]
  API -->|HTTP interno| Engine["Motor C++ OpenMP"]
  API -->|GET /readyz| Engine
  Engine -->|JSON con move y stats| API
  API -->|JSON validado| Browser
```

## Contrato de comunicacion

El contrato principal es `POST /move`. El cliente envia un tablero de 14 posiciones, el lado activo, el algoritmo (`alphabeta` o `mcts`) y el numero de hilos solicitados. Para Alpha-Beta tambien envia `depth`; para MCTS envia `simulations`. El backend valida que el tablero tenga exactamente 14 enteros no negativos, que el total de semillas sea 48, que `side` sea `0` o `1`, que `depth` este entre `1` y `32` cuando aplica, que `simulations` este entre `1` y `100000` cuando aplica, y que `threads` este entre `1` y `64`. Despues envia el mismo JSON al motor en `MOTOR_URL`.

Ejemplo de entrada:

```json
{
  "board": [4,4,4,4,4,4,0,4,4,4,4,4,4,0],
  "side": 0,
  "algo": "alphabeta",
  "depth": 8,
  "threads": 4
}
```

Ejemplo de salida:

```json
{
  "algo": "alphabeta",
  "move": 3,
  "evaluation": 7,
  "elapsed_ms": 124,
  "stats": {
    "algo": "alphabeta",
    "nodes": 1845210,
    "prunes": 312083
  },
  "threads_used": 4
}
```

Para MCTS, `stats` cambia a `{"algo":"mcts","rollouts":...,"win_rate":...}` y `threads_used` se reporta como `1`, porque el MCTS actual es secuencial.

El campo `move` usa indices absolutos del arreglo canonico. Para jugador 0 los pits legales son `0..5`; para jugador 1 los pits legales son `7..12`. Esta decision reduce ambiguedades entre servicios: todos hablan en la misma representacion.

## Endpoints

El backend expone cuatro endpoints minimos:

- `GET /healthz`: confirma que el proceso FastAPI vive.
- `GET /readyz`: confirma que el backend puede contactar al motor real.
- `GET /metrics`: publica metricas de backend y, si esta disponible, concatena metricas del motor.
- `POST /move`: valida la solicitud y delega el calculo al motor.

El motor expone endpoints equivalentes para que el backend y Kubernetes puedan verificarlo: `GET /healthz`, `GET /readyz`, `GET /metrics` y `POST /move`. El motor no es un modulo importado por Python; es un proceso separado. En Docker Compose, el backend lo alcanza con `http://motor:8080` dentro de la red interna, aunque el host publica ese servicio como `localhost:9000`. En Kubernetes, el backend lo alcanza por el Service interno configurado en cada manifiesto.

## Decisiones de diseno

La primera decision fue mantener el backend delgado. En versiones tempranas habia un camino simulado controlado por una variable de entorno; ese camino se elimino de la ruta principal porque ocultaba fallas del motor y podia aprobar pruebas superficiales sin cumplir el requisito distribuido. Ahora, si el motor no responde, readiness falla y `/move` devuelve `503`. Este comportamiento es mas honesto operacionalmente y ayuda a detectar problemas de despliegue.

La segunda decision fue usar HTTP simple entre backend y motor. gRPC tambien era viable, pero habria requerido mas generacion de codigo y dependencias. HTTP con JSON es suficiente para el contrato pequeno del proyecto, facilita pruebas con `curl`, simplifica healthchecks y encaja bien con FastAPI y Kubernetes. En el motor C++ se implemento un servidor HTTP minimo sin dependencias externas para evitar que el build dependa de paquetes no disponibles en el entorno del profesor.

La tercera decision fue mantener el frontend estatico. No se requiere un framework pesado para enviar un JSON, pintar un tablero y mostrar metricas. Nginx sirve `index.html`, `style.css` y `app.js`, lo que reduce el tiempo de build y elimina una fuente de fallas por dependencias de Node. La interfaz usa el backend local por defecto y, en nube, puede resolver un host `api.<dominio>` para separar frontend y API.

## Despliegue y escalabilidad

En local, Docker Compose levanta los tres servicios y usa healthchecks para ordenar el arranque. En Kubernetes local y nube se declaran Deployments, Services, ConfigMap, probes y recursos. El backend tiene al menos 3 replicas, cumpliendo el requisito de balanceo. El motor puede tener 2 replicas porque cada solicitud es independiente: no hay estado compartido entre busquedas. Si el trafico crece, se escala horizontalmente el backend para absorber conexiones y el motor para absorber calculo.

La escalabilidad vertical del motor se controla con `threads`. Cada solicitud decide cuantos hilos usar, pero el contenedor tambien define `OMP_NUM_THREADS` como valor operativo por defecto. En ambientes compartidos no conviene permitir que todas las replicas usen mas hilos que CPUs disponibles; por eso los manifiestos incluyen `requests` y `limits` de CPU y memoria. Esa declaracion ayuda al scheduler de Kubernetes a ubicar pods y evita que un benchmark agresivo ahogue otros procesos.

## Observabilidad

El backend cuenta solicitudes totales y solicitudes `/move`. El motor cuenta solicitudes HTTP, busquedas, nodos, podas y tiempo acumulado. Las metricas se exponen en texto estilo Prometheus para que puedan leerse directamente o conectarse a un scraper. En un proyecto mas grande se agregarian histogramas por profundidad y por hilos, pero para esta entrega el foco esta en demostrar que el calculo real se ejecuta y que el paralelismo produce datos comparables.

## Riesgos de arquitectura

El principal riesgo es que una profundidad alta con muchos hilos puede tardar mas de lo esperado si el tablero tiene alto factor de ramificacion. Para mitigarlo, el backend tiene timeout configurable y el motor valida limites de profundidad e hilos. Otro riesgo es que el frontend de nube necesite el dominio final para CORS; por eso los origenes se cargan desde `CORS_ORIGINS` y no se usa `"*"`. Finalmente, la implementacion HTTP del motor es deliberadamente pequena; es adecuada para el proyecto, pero no reemplaza un framework de produccion con TLS, streaming y control avanzado de concurrencia.


## Política CORS

### Configuración actual del backend

El backend configura CORS con los siguientes orígenes permitidos:

```python
CORS_ORIGINS = [
    "http://localhost:8080",
    "http://127.0.0.1:8080",
    "http://frontend"  # Para comunicación interna en Docker
]
```

### Headers de respuesta CORS

| Header | Valor | Descripción |
|--------|-------|-------------|
| `Access-Control-Allow-Origin` | `http://localhost:8080` | Origen específico permitido |
| `Access-Control-Allow-Methods` | `GET, POST, OPTIONS` | Métodos HTTP permitidos |
| `Access-Control-Allow-Headers` | `Accept, Accept-Language, Content-Language, Content-Type` | Headers permitidos |
| `Access-Control-Max-Age` | `600` | Tiempo de cacheo preflight (10 minutos) |

### Verificación de CORS

```bash
curl -X OPTIONS http://localhost:8000/move \
  -H "Origin: http://localhost:8080" \
  -H "Access-Control-Request-Method: POST" \
  -v 2>&1 | grep -i "access-control"
```

**Salida esperada:**
```
< access-control-allow-methods: GET, POST, OPTIONS
< access-control-max-age: 600
< access-control-allow-headers: Accept, Accept-Language, Content-Language, Content-Type
< access-control-allow-origin: http://localhost:8080
```

### Justificación de seguridad

1. **Orígenes explícitos**: No se usa `"*"` para prevenir ataques CSRF y garantizar que solo el frontend autorizado (local o en contenedor) pueda consumir la API.

2. **Métodos restringidos**: Solo `GET`, `POST` y `OPTIONS` (pre-flight) son necesarios; se excluyen `PUT`, `DELETE`, `PATCH` que no forman parte del contrato.

3. **Headers mínimos**: Se permiten solo headers esenciales para la operación del juego.

4. **Entorno de desarrollo**: La configuración actual es para desarrollo local. En producción, los orígenes deben reemplazarse por el dominio real (ej. `https://mancala.dominio.com`).

## Códigos de Error HTTP

| Código | Significado | Escenario | Respuesta Ejemplo |
|--------|-------------|-----------|-------------------|
| 200 | OK | Movimiento válido procesado | `{"movimiento": 2, ...}` |
| 400 | Bad Request | Tablero inválido, movimiento ilegal o payload mal formado | `{"detail": "Invalid board length"}` |
| 503 | Service Unavailable | Motor no responde o MCTS no implementado | `{"detail": "Motor not reachable"}` |
| 500 | Internal Server Error | Error inesperado en backend | `{"detail": "Internal server error"}` |

## Esquema Completo de API

### POST /move - Request Completo

```json
{
  "tablero": [4, 4, 4, 4, 4, 4, 0, 4, 4, 4, 4, 4, 4, 0],
  "lado": 0,
  "algoritmo": "alphabeta",
  "parametros": {
    "profundidad": 8,
    "simulaciones": 1000,
    "hilos": 4
  }
}
```

### POST /move - Response Completo (Alpha-Beta)

```json
{
  "movimiento": 2,
  "tablero_nuevo": [4, 4, 0, 5, 5, 5, 1, 4, 4, 4, 4, 4, 4, 0],
  "lado_siguiente": 0,
  "terminal": false,
  "ganador": null,
  "estadisticas": {
    "algoritmo": "alphabeta",
    "tiempo_ms": 1,
    "hilos": 1,
    "evaluacion": 0,
    "profundidad": 6,
    "nodos": 72,
    "podas": 18
  }
}
```

### POST /move - Response Completo (MCTS)

```json
{
  "movimiento": 2,
  "tablero_nuevo": [4, 4, 0, 5, 5, 5, 1, 4, 4, 4, 4, 4, 4, 0],
  "lado_siguiente": 0,
  "terminal": false,
  "ganador": null,
  "estadisticas": {
    "algoritmo": "mcts",
    "tiempo_ms": 125,
    "simulaciones": 1000,
    "win_rate": 0.62,
    "rollouts": 1000
  }
}
```

## Diagrama de Secuencia Detallado

```mermaid
sequenceDiagram
    participant U as Usuario
    participant F as Frontend (Nginx:8080)
    participant B as Backend (FastAPI:8000)
    participant M as Motor (C++:8080)

    U->>F: 1. Clic en hoyo (índice 2)
    F->>F: 2. Validar movimiento básico
    F->>B: 3. POST /move (lado=0, tablero actual)
    
    B->>B: 4. Validar schema y reglas Kalah
    B->>M: 5. POST /move (mismo payload)
    
    M->>M: 6. Calcular mejor movimiento<br/>(Alpha-Beta/MCTS paralelizado)
    M-->>B: 7. {move: 2, estadisticas}
    
    B->>B: 8. Aplicar movimiento al tablero
    B-->>F: 9. {movimiento, tablero_nuevo, estadisticas}
    
    F->>F: 10. Actualizar UI + panel estadísticas
    F-->>U: 11. Mostrar resultado
    
    alt lado_siguiente == 1 (turno IA)
        F->>B: 12. POST /move automático (lado=1)
        B->>M: 13. POST /move (lado=1)
        M-->>B: 14. {move, estadisticas}
        B-->>F: 15. {tablero_nuevo, estadisticas}
        F-->>U: 16. Actualizar con jugada IA
    end
    
    alt terminal == true
        F-->>U: 17. Mostrar mensaje ganador + botón reiniciar
        U->>F: 18. Clic "Reiniciar"
        F->>B: 19. POST /reset
        B-->>F: 20. {status: "ok"}
        F->>F: 21. Resetear tablero a estado inicial
    end
```

## Resumen de Puertos y Comunicación

| Componente | Puerto Interno | Puerto Host | Protocolo | Propósito |
|------------|---------------|-------------|-----------|-----------|
| Frontend (Nginx) | 80 | 8080 | HTTP | Servir interfaz web |
| Backend (FastAPI) | 8000 | 8000 | HTTP | API REST |
| Motor (C++) | 8080 | 9000 | HTTP | Cálculo de IA |

**Red Docker interna:** `local_default` (bridge)
- Backend → Motor: `http://motor:8080`
- Frontend → Backend: `http://backend:8000`