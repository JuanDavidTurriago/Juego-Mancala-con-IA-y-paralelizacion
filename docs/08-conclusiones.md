# 08 - Conclusiones

## Resumen del proyecto

El proyecto entregó un sistema distribuido completo: motor de IA para Kalah(6,4) en C++/OpenMP, backend FastAPI, frontend web estático, despliegue en Docker Compose y en Kubernetes local y en la nube (GKE Autopilot), integración continua con GitHub Actions y análisis estático con SonarCloud. Todos los componentes se desplegaron en contenedores independientes comunicados por HTTP, cumpliendo la separación exigida por la rúbrica.

---

## Conclusiones basadas en métricas

### Motor y paralelismo

La implementación de Root Parallelism en Alpha-Beta produce speedup real $S(8) = 2.90$ a `depth=12` sobre el AMD Ryzen 7 5700G con 8 núcleos disponibles en el contenedor. El techo de paralelismo observado se explica por la pérdida de podas inherente a la estrategia: cada hilo inicia con ventana $(-\infty, +\infty)$, lo que eleva el número de nodos explorados de 712,784 (secuencial) a 1,080,087 por posición (+51.5%). El resultado práctico es que cuatro hilos ya capturan ~93% del beneficio máximo ($S(4) = 2.70$, $S(8) = 2.90$), y agregar más hilos no escala linealmente porque el árbol de Kalah solo tiene 6 movimientos posibles desde la raíz — por encima de 6 hilos los hilos adicionales quedan ociosos o compiten por los mismos movimientos.

La evidencia de paralelismo real está documentada en la salida de `time`: `user 3.845s / real 2.007s`, relación 1.92, confirmando múltiples núcleos activos durante la ejecución a `depth=12`.

### Análisis comparativo

El experimento con k6 (10 VUs, 30 s, `depth=8`) reveló que a profundidades bajas el cómputo del motor (~1 ms) es irrelevante frente a la latencia de red. En local, el throughput se mantiene en ~94 req/s independientemente de los hilos porque el cuello de botella es el think time del cliente (100 ms). En GKE, tres réplicas del backend aumentaron el throughput de 38.7 a 87.2 req/s (+125%) y redujeron la p95 de 472 ms a 288 ms (−39%), demostrando que el escalamiento horizontal es la herramienta correcta bajo alta concurrencia con cómputo ligero.

---

## Retos durante el desarrollo y cómo se resolvieron

**1. Separación motor–backend a nivel de contenedor:** La tentación inicial era importar la lógica C++ desde Python vía `ctypes`. Se descartó porque viola el requisito de separación por contenedor. La solución fue implementar el servidor HTTP en el propio proceso C++ usando `cpp-httplib` y `nlohmann/json` como cabeceras vendorizadas, lo que evitó dependencias externas difíciles de gestionar en el build de Docker.

**2. Corrección del turno extra en Alpha-Beta:** El primer borrador del minimax alternaba el jugador incondicionalmente después de cada jugada. Las pruebas unitarias de `test_alphabeta.cpp` detectaron que los movimientos óptimos diferían entre Minimax y Alpha-Beta en posiciones con turno extra, lo que señaló el bug. La corrección fue propagar `extra_turn` y conservar `is_max` en lugar de negarlo cuando el turno extra se activa.

**3. Pérdida de podas en paralelismo:** La primera versión paralela intentaba compartir `alpha`/`beta` globales entre hilos con `#pragma omp atomic`, pero producía resultados no determinísticos y carreras de datos sutiles. Se optó por ventanas completamente locales por hilo (Root Parallelism puro), sacrificando eficiencia de podas a cambio de corrección verificable y desempate determinístico.

**4. Despliegue en GKE Autopilot y cuota educativa:** El primer intento de despliegue con `maxSurge=1` falló porque la cuota de Compute Engine del proyecto educativo no permitía crear nodos temporales durante rolling updates. La solución fue cambiar la estrategia a `maxSurge=0, maxUnavailable=1`, lo que reduce la disponibilidad momentáneamente durante actualizaciones pero no requiere capacidad extra.

**5. CORS en nube:** El origen de nube (`http://api.mancala.8.232.201.150.nip.io`) no se conoce hasta que GKE asigna la IP global. Se inyectó como variable de entorno `CORS_ORIGINS` en el ConfigMap de Kubernetes, permitiendo actualizar el origen sin reconstruir la imagen.

---

## Recomendaciones de mejoras futuras y lecciones aprendidas

**Mejoras al motor:**
- Implementar **ordenamiento de movimientos** (mejor movimiento primero) antes de la poda para aumentar drásticamente las podas en la versión secuencial y, por ende, reducir el nodo-exceso que genera Root Parallelism.
- Agregar **tabla de transposición** (hash Zobrist) para evitar re-explorar posiciones ya evaluadas.
- Implementar **YBWC** (Young Brothers Wait Concept): explorar el primer hijo secuencialmente para obtener una cota sólida y luego paralelizar los hermanos. Esto preservaría más podas que Root Parallelism puro.
- Usar **Iterative Deepening** con ventana de aspiración para combinar profundidad progresiva y poda eficiente.

**Mejoras a la infraestructura:**
- Agregar **HorizontalPodAutoscaler** en Kubernetes para que el número de réplicas del backend se ajuste automáticamente según la carga de CPU, eliminando el escalado manual entre r=1 y r=3.
- Configurar **TLS** en el Ingress de GKE con certificado administrado por Google para pasar a HTTPS en producción.
- Implementar un pipeline de **despliegue continuo** que actualice los tags de imagen en los manifiestos de `deploy/cloud/` automáticamente tras cada push exitoso a `main`.

**Lecciones aprendidas:**
- Los algoritmos de búsqueda en árboles de juego son sensibles a la estrategia de paralelización: no existe un speedup gratuito. Root Parallelism es simple y correcto, pero paga un costo real en nodos adicionales que crece con la profundidad.
- El análisis comparativo entre entornos debe diseñarse con cuidado para aislar variables. En este experimento, el think time del cliente k6 (`sleep(0.1)`) hizo que `depth=8` fuera demasiado rápido para observar diferencias de throughput por hilos; se requeriría `depth≥12` y sin think time para ver el efecto vertical.
- Kubernetes simplifica el escalamiento horizontal, pero introduce latencia de red que domina para cargas de cómputo ligero. La elección entre escalar vertical u horizontalmente debe guiarse por mediciones, no por intuición.
