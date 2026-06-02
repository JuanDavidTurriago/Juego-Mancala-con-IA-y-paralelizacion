# 03 - Paralelizacion

## Objetivo

El requisito de paralelizacion pide que el motor C++ use OpenMP de forma real, no solo flags de compilacion. En esta solucion el paralelismo se aplica en la raiz del arbol de busqueda Alfa-Beta. Cada movimiento legal del jugador activo genera una rama independiente; esas ramas se reparten entre hilos con `#pragma omp for schedule(dynamic)`. Cada hilo ejecuta una busqueda recursiva Alfa-Beta serial sobre su rama, acumula nodos y podas en contadores locales, y al final combina resultados con reducciones y una seccion critica pequena para decidir el mejor movimiento global.

Esta estrategia se conoce como root parallelism. Es sencilla, segura y adecuada para Kalah porque el factor de ramificacion inicial tiene hasta 6 movimientos. No aprovecha paralelismo ilimitado, pero evita los problemas mas complicados de paralelizar poda Alfa-Beta dentro de cada nivel: ventanas inconsistentes, alto costo de sincronizacion, cancelacion de tareas y resultados no deterministas.

## Implementacion OpenMP

La funcion `AlphaBetaEngine::SearchBestMove` recibe `depth` y `threads`. Primero genera los movimientos legales. Si no hay movimientos o la profundidad es cero, evalua directamente. Si hay movimientos, inicializa contadores compartidos para nodos y podas y entra a una region paralela:

```cpp
#pragma omp parallel num_threads(requested_threads) reduction(+ : total_nodes, total_prunes)
{
  #pragma omp single
  { threads_used = omp_get_num_threads(); }

  #pragma omp for schedule(dynamic)
  for (int i = 0; i < moves.size(); ++i) {
    // busqueda Alfa-Beta de la rama moves[i]
  }
}
```

La reduccion suma `total_nodes` y `total_prunes` sin bloquear en cada nodo. La seccion critica solo protege la comparacion entre el resultado de una rama y el mejor resultado global. Ese bloqueo es barato porque ocurre una vez por movimiento raiz, no una vez por nodo interno.

Cada rama recibe una copia del tablero producida por `apply_move`. No hay escritura concurrente sobre el tablero raiz ni sobre tableros hijos compartidos. Esta propiedad hace que el algoritmo sea naturalmente thread-safe. El unico estado compartido dentro de la busqueda es el mejor movimiento raiz, y se actualiza de forma controlada.

## Relacion con Alfa-Beta

Alfa-Beta obtiene gran parte de su eficiencia por el orden en que explora movimientos: una buena rama temprana estrecha `alpha` o `beta` y permite cortar mas tarde. En una implementacion paralela agresiva, varias ramas se exploran al mismo tiempo y algunas no aprovechan los cortes descubiertos por otras ramas. Por eso el root parallelism puede explorar mas nodos que una version secuencial muy bien ordenada. Sin embargo, al ejecutar ramas independientes al mismo tiempo, el tiempo total puede bajar en maquinas con varios nucleos.

La solucion mantiene la poda dentro de cada rama. Es decir, no se elimina Alfa-Beta. Cada hilo ejecuta el minimax con ventanas `alpha` y `beta` sobre su subarbol. Lo que no se comparte agresivamente es la ventana global entre ramas raiz. Esta decision reduce complejidad y conserva determinismo. Para la entrega, el criterio principal es mostrar paralelismo real, medirlo y documentar sus limites.

## Medicion

El benchmark imprime una tabla CSV con estas columnas:

- `position`: indice de la posicion cargada desde `suite.txt`.
- `depth`: profundidad usada.
- `threads`: hilos solicitados.
- `move`: mejor movimiento devuelto.
- `evaluation`: evaluacion del movimiento.
- `elapsed_ms`: tiempo de busqueda.
- `speedup`: `T(1) / T(p)`.
- `efficiency`: `speedup / p`.
- `nodes`: nodos explorados.
- `prunes`: cortes realizados.

El comando recomendado es:

```bash
./motor/build/mancala_bench --depth 8 --positions motor/tests/suite.txt --threads 1,2,4,8
```

Para profundidades pequenas, el overhead de crear la region paralela y sincronizar resultados puede dominar. En ese caso `T(4)` puede ser similar o incluso peor que `T(1)`. Para profundidades medias, el trabajo por rama crece y se espera ver mejor aprovechamiento de hilos. Para profundidades muy altas, el tiempo puede crecer mucho por el factor de ramificacion y por tableros que no permiten podas tempranas.

## Speedup y eficiencia

El speedup ideal con `p` hilos seria `S(p) = p`, pero en busquedas Alfa-Beta rara vez se alcanza. Hay varias razones. Primero, solo hay hasta 6 ramas raiz, asi que con 8 hilos algunos hilos pueden quedar sin trabajo si el scheduler no encuentra mas ramas. Segundo, las ramas no tienen el mismo costo: una jugada puede terminar rapido por capturas o final de partida, mientras otra conserva muchos movimientos legales. Tercero, la poda depende del orden de evaluacion; una ejecucion paralela puede explorar nodos que la secuencial habria evitado despues de encontrar una buena cota.

Por eso el analisis no debe mirar solo `elapsed_ms`. Tambien debe comparar `nodes` y `prunes`. Si la version paralela es mas rapida pero explora mas nodos, la mejora viene de repartir trabajo. Si explora menos nodos, puede deberse al orden de movimientos o a caracteristicas de esa posicion. Si `threads_used` es menor que `threads`, probablemente el runtime OpenMP ajusto la region o el entorno limito recursos.

## Eleccion frente a otras estrategias

Otra alternativa era paralelizar con tareas recursivas (`#pragma omp task`) en varios niveles del arbol. Esa tecnica puede crear mas trabajo paralelo, pero introduce riesgos: demasiadas tareas pequenas, contadores compartidos, cancelacion compleja y ventanas Alfa-Beta menos efectivas. Tambien se podia implementar Young Brothers Wait Concept, donde se explora primero una rama fuerte de forma secuencial y luego se paralelizan hermanas con una mejor cota. Es una opcion mas avanzada, pero excede el alcance razonable de esta entrega.

Root parallelism permite explicar y defender el resultado: cada hilo toma una jugada candidata, ejecuta la misma busqueda correcta que la version secuencial, y al final se elige el mejor valor. La reproducibilidad es alta y las pruebas pueden comparar movimiento y evaluacion entre `threads=1` y `threads=4`.

## Consideraciones operativas

En Docker y Kubernetes hay que alinear `threads`, `OMP_NUM_THREADS` y limites de CPU. Si el contenedor tiene limite de `1000m` y una solicitud pide 8 hilos, el runtime puede crear hilos que compiten por un solo CPU asignado, causando overhead. Por eso los manifiestos declaran recursos y el backend limita `threads` a 64, aunque en ejecuciones normales se recomiendan 1, 2, 4 u 8.

El motor reporta `threads_used` para que el cliente sepa cuantos hilos OpenMP creo realmente. Este dato se guarda junto a tiempo, nodos y podas, y ayuda a interpretar resultados cuando el entorno local, Docker Desktop o el cluster no entregan todos los nucleos esperados.
