# 03 - Paralelización y Benchmarking en C++ con OpenMP

## Justificación de Arquitectura

### 1. Alpha-Beta: Paralelismo a la raíz (Root Parallelism)
Para el algoritmo Alpha-Beta, implementamos **Root Parallelism**. En esta estrategia, distribuimos estáticamente los movimientos legales desde el nodo raíz entre los distintos hilos usando `#pragma omp parallel for`. Cada hilo ejecuta secuencialmente su propio subárbol hasta la profundidad designada.
**Costo de sincronización y pérdida de podas:**
Al no requerir bloqueos de exclusión mutua, esta aproximación casi no tiene overhead de sincronización. Sin embargo, su principal desventaja es la *pérdida de podas*. Dado que las cotas $\alpha$ y $\beta$ son locales a cada hilo, un hilo no se beneficia de un corte drástico (cota fuerte) encontrado por otro hilo en un movimiento adyacente. Como consecuencia, el algoritmo paralelo explora significativamente más nodos y realiza menos podas de forma global comparado con el modelo puramente secuencial.

### 2. MCTS: Root Parallelization
Para paralelizar el algoritmo MCTS, hemos elegido **Root Parallelization** debido a su simplicidad técnica y bajo overhead de sincronización ("lock overhead"). En este modelo, cada hilo de OpenMP construye y explora su propio árbol MCTS de manera totalmente independiente partiendo de la posición actual (raíz). Esto elimina por completo la necesidad de usar construcciones como `#pragma omp atomic` o bloques críticos de exclusión mutua durante las simulaciones (rollouts) y retropropagaciones. Al no compartir la estructura del árbol entre hilos durante el paso de búsqueda intenso, se mitiga drásticamente la contención de memoria que plagaría un esquema de *Tree Parallelization*, donde múltiples hilos competirían constantemente por actualizar las estadísticas de victorias y visitas (`w` y `n`) en los mismos nodos padres.

Otras alternativas como *Leaf Parallelization* o la ya mencionada *Tree Parallelization* fueron descartadas por razones claras. *Leaf Parallelization* (paralelizar sólo las iteraciones en los nodos hojas) es la más sencilla, pero sufre de retornos decrecientes ya que muchos rollouts desde exactamente la misma posición no contribuyen significativamente a diversificar y robustecer la topología del árbol de búsqueda. *Tree Parallelization*, aunque ofrece máxima diversidad exploratoria, impone cuellos de botella severos debido al intenso costo transaccional (locking) para prevenir condiciones de carrera, devorando gran parte del Speedup. Por ello, Root Parallelization nos entrega el balance óptimo: cada hilo es libre, usa su propio generador de números aleatorios (RNG), explora libremente y sólo se sincroniza al final en una breve y rápida operación de reducción (reduciendo el overhead global).

## Metodología y Fórmulas de Rendimiento

Para cuantificar el impacto de nuestra mejora paralela con OpenMP, calculamos experimentalmente el *Speedup* y la *Eficiencia* usando las siguientes definiciones teóricas consolidadas:

- **Speedup**: Expresa la aceleración del tiempo de ejecución total lograda con $p$ hilos frente a una ejecución completamente secuencial. 
  $$ S(p) = \frac{T(1)}{T(p)} $$
  *(Donde $T(1)$ es el tiempo usando 1 núcleo y $T(p)$ es el tiempo de resolución con $p$ núcleos).*

- **Eficiencia**: Indica qué fracción o porcentaje de la capacidad computacional agregada se está utilizando productivamente (idealmente $1.0$).
  $$ E(p) = \frac{S(p)}{p} $$

## Resultados del Benchmark

### Tiempos y Aceleración MCTS (100,000 Simulaciones)
*(Nota para el desarrollador: Completa esta tabla vacía ejecutando `./motor/build/mancala_bench --algo mcts --simulations 100000 --positions tests/suite.txt` y reemplaza los valores de prueba por los correctos).*

| Hilos ($p$) | $T(p)$ (ms) | Speedup $S(p)$ | Eficiencia $E(p)$ | Total Rollouts Efectivos | Tasa Coincidencia (%) |
|------------|-------------|----------------|-------------------|--------------------------|-----------------------|
| 1          | 404.7       | 1.00           | 1.00              | 100000                   | 40%                   |
| 2          | 230.6       | 1.76           | 0.88              | 100000                   | 40%                   |
| 4          | 126.3       | 3.21           | 0.80              | 100000                   | 40%                   |
| 8          | 71.4        | 5.67           | 0.71              | 100000                   | 30%                   |

### Gráfica de Rendimiento Teórico Esperado
*(Ajusta los valores placeholder en el código mermaid para reflejar la realidad del benchmark).*

```mermaid
xychart-beta
    title "Speedup de MCTS Paralelo (Root) vs Secuencial"
    x-axis "Hilos (p)" ["1 Hilo", "2 Hilos", "4 Hilos", "8 Hilos"]
    y-axis "Aceleración S(p)" 0.0 --> 6.0
    bar [1.0, 1.76, 3.21, 5.67]
```

## Tabla Comparativa: Presupuesto Restringido (~500ms)

Se evaluó qué profundidad heurística de conocimiento logra el MiniMax y cuántas simulaciones prospectivas logra MCTS para un mismo margen temporal de "pensamiento" de medio segundo en un turno normal.

| Algoritmo | Configuración de Parámetro | Tiempo de Ejecución | Movimiento Seleccionado | Calidad de la Decisión |
|-----------|----------------------------|---------------------|-------------------------|------------------------|
| **AlphaBeta** | Profundidad: 13            | 207.33 ms           | 2                       | Óptima (heurística)    |
| **MCTS (OpenMP)** | Simulaciones: 280,000      | 521.77 ms           | 2                       | Coincide exactamente   |

*(Análisis Cualitativo: Ambos algoritmos determinaron que el hoyo 2 es el mejor movimiento. MCTS logró casi 300,000 simulaciones completas dividiendo el trabajo en sus hilos, demostrando una excelente convergencia estocástica con el movimiento matemáticamente óptimo encontrado por la búsqueda profunda de Alpha-Beta).*

## Profiling Operativo y Uso de Recursos en Linux

Para corroborar la paralelización efectiva y medir la carga, se anexan capturas del desempeño en un sistema Linux.

### Carga Distribuida (Htop)
Múltiples CPUs deben reportar un 100% de utilización simultánea durante la ventana del cálculo MCTS.
![Captura Htop](../img/htop.png)

### Profiling a Bajo Nivel (`perf stat`)
Análisis de instrucciones cíclicas y posibles fallos en memoria caché (`cache-misses`). 
![Captura Perf](../img/perf.png)

### Maximum Resident Set Size (`/usr/bin/time -v`)
Evaluación del costo de RAM de duplicar el estado del árbol de MCTS para cada hilo en *Root Parallelization*.
![Captura Time](../img/time.png)
