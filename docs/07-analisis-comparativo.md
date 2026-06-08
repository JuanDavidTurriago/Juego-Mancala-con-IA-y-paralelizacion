# 07 - Análisis Comparativo: Local vs. Nube

## Propósito
Este análisis evalúa el rendimiento del sistema completo (Backend HTTP + Motor C++) bajo carga concurrente, contrastando el **escalamiento vertical** (añadir más hilos de OpenMP a una sola instancia local) contra el **escalamiento horizontal** (añadir más réplicas de pods en el clúster de Kubernetes en la nube). 

Para generar la carga sintética, utilizaremos la herramienta **k6** (o equivalente como `wrk`/`ab`), enviando múltiples peticiones concurrentes al endpoint `/move`.

---

## 1. Configuración del Experimento

### Carga Sintética (Payload)
Todas las peticiones HTTP POST al endpoint `/move` utilizarán la misma posición y profundidad fija para garantizar consistencia:
```json
{
  "board": [4,4,4,4,4,4,0,4,4,4,4,4,4,0],
  "side": 0,
  "depth": 8
}
```

### Entorno A: Local (Escalamiento Vertical)
- **Infraestructura:** Docker Compose local (`deploy/local/docker-compose.yml`).
- **Instancias:** 1 sola réplica del backend y 1 del motor.
- **Variable a probar:** Número de hilos del motor (`OMP_NUM_THREADS` $\in \{1, 2, 4, 8\}$).

### Entorno B: Nube Kubernetes (Escalamiento Horizontal)
- **Infraestructura:** Clúster gestionado en la nube (GCP GKE).
- **Instancias:** Despliegue de Kubernetes (`deploy/cloud/`).
- **Configuración fija:** `OMP_NUM_THREADS = 2`.
- **Variable a probar:** Número de réplicas del backend y motor ($r \in \{1, 3\}$).

---

## 2. Resultados de las Métricas

*(Nota para el desarrollador: Realiza las pruebas de carga y reemplaza los espacios vacíos con los datos reales)*

### Entorno A: Local (1 Instancia, variando Hilos)
| Hilos de OpenMP | Throughput (Req/sec) | Latencia p50 (ms) | Latencia p95 (ms) |
|-----------------|----------------------|-------------------|-------------------|
| 1 hilo          | 93.85                | 105.81            | 110.06            |
| 2 hilos         | 93.54                | 106.27            | 110.04            |
| 4 hilos         | 93.33                | 106.52            | 111.03            |
| 8 hilos         | 93.80                | 106.18            | 109.64            |

### Entorno B: Nube Kubernetes (2 Hilos fijos, variando Réplicas)
| Número de Réplicas ($r$) | Throughput (Req/sec) | Latencia p50 (ms) | Latencia p95 (ms) |
|--------------------------|----------------------|-------------------|-------------------|
| $r = 1$ réplica          |                      |                   |                   |
| $r = 3$ réplicas         |                      |                   |                   |

---

## 3. Conclusión Cualitativa

*(Redacta aquí una o dos frases finales analizando los resultados de las tablas. Ejemplo: "Los datos revelan que el escalamiento vertical reduce drásticamente la latencia individual p50 gracias a OpenMP, pero se satura rápido ante alta concurrencia. Por el contrario, el escalamiento horizontal en la nube (más réplicas) mantiene estable la latencia p95 bajo carga pesada y multiplica el Throughput total del sistema, siendo la estrategia ideal para servir a múltiples usuarios simultáneos.")*
