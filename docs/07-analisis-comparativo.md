# 07 - Analisis comparativo

## Proposito

El analisis comparativo busca responder si la paralelizacion con OpenMP mejora el tiempo de busqueda del motor y bajo que condiciones. La comparacion no se debe hacer a traves del frontend ni del backend, porque la latencia HTTP, el navegador, Docker networking y FastAPI agregan ruido. Por eso se usa `mancala_bench`, un ejecutable del motor que corre directamente sobre posiciones fijas y reporta datos en CSV.

La unidad de comparacion es la misma posicion de Kalah, misma profundidad, mismo algoritmo Alfa-Beta y distinto numero de hilos. Para cada posicion se toma `T(1)` como linea base y se calculan:

```text
S(p) = T(1) / T(p)
E(p) = S(p) / p
```

`S(p)` mide speedup y `E(p)` mide eficiencia. Si `E(4)` se acerca a 1, los cuatro hilos se aprovechan muy bien. Si se acerca a 0.25, la mejora es baja o inexistente. En busquedas Alfa-Beta reales, lo normal es encontrar valores intermedios.

## Comando base

Despues de compilar el motor:

```bash
cmake -S motor -B motor/build -DCMAKE_BUILD_TYPE=Release
cmake --build motor/build -j
./motor/build/mancala_bench --depth 8 --positions motor/tests/suite.txt --threads 1,2,4,8
```

La salida tiene columnas:

```text
position,depth,threads,move,evaluation,elapsed_ms,speedup,efficiency,nodes,prunes
```

Para una tabla de informe se recomienda guardar la salida:

```bash
./motor/build/mancala_bench --depth 8 --positions motor/tests/suite.txt --threads 1,2,4,8 > bench-depth8.csv
```

En Windows con Docker se puede construir la imagen del motor y ejecutar el benchmark dentro del contenedor:

```bash
docker build -t mancala-motor:bench ./motor
docker run --rm mancala-motor:bench /usr/local/bin/mancala_bench --depth 8 --positions /app/tests/suite.txt --threads 1,2,4,8
```

## Posiciones

`motor/tests/suite.txt` contiene una posicion inicial, posiciones de medio juego, casos con capturas inmediatas, turnos extra y finales cercanos a barrido. Es importante no medir solo la posicion inicial. En Kalah, la forma del tablero cambia mucho el factor de ramificacion. Una posicion con muchos pits vacios puede tener pocas jugadas legales y mostrar poco beneficio paralelo. Una posicion equilibrada con muchas semillas en juego puede generar ramas mas costosas y mostrar mayor diferencia entre hilos.

La suite fija evita seleccionar solo casos favorables. Tambien permite repetir mediciones despues de cambios en el motor y comparar si una optimizacion mejora de forma general o solo en una posicion.

## Interpretacion de nodos y podas

El tiempo no es la unica metrica. `nodes` indica cuantas posiciones evaluo la busqueda y `prunes` cuantas veces Alfa-Beta corto una rama. En una version secuencial, el orden de movimientos puede permitir podas tempranas. En root parallelism, las ramas raiz se exploran simultaneamente y no siempre comparten cotas globales. Por eso es posible que `threads=4` explore mas nodos que `threads=1` y aun asi sea mas rapido. Eso no es un error: significa que el paralelismo compenso trabajo adicional.

Tambien puede ocurrir que `threads=8` no mejore frente a `threads=4`. Hay como maximo seis movimientos legales desde un tablero, de modo que con ocho hilos algunos quedan sin rama raiz. Ademas, si las ramas tienen costos desbalanceados, el hilo que recibe la rama mas pesada domina el tiempo total.

## Ejemplo de tabla esperada

Los valores concretos dependen del procesador, Docker Desktop, carga del sistema y profundidad. Una tabla de informe podria verse asi:

```text
position depth threads elapsed_ms speedup efficiency nodes prunes
0        8     1       120.0      1.00    1.00       184000 31000
0        8     2        72.0      1.67    0.83       190000 30000
0        8     4        48.0      2.50    0.62       201000 28000
0        8     8        47.0      2.55    0.32       201000 28000
```

La lectura seria: cuatro hilos mejoran el tiempo, pero la eficiencia baja porque no hay ocho ramas suficientes y porque existen costos de sincronizacion. Si los nodos suben, se explica por menor reutilizacion de cotas globales entre ramas raiz.

## Metodologia recomendada

Para producir resultados confiables se recomienda:

1. Ejecutar en modo Release.
2. Cerrar aplicaciones pesadas antes del benchmark.
3. Correr cada configuracion al menos tres veces.
4. Reportar mediana o promedio, indicando la maquina usada.
5. No mezclar resultados de host Windows, contenedor Linux y CI como si fueran equivalentes.
6. Mantener fija la profundidad para todas las comparaciones de una tabla.

La profundidad debe elegirse para que `T(1)` sea medible pero no excesivo. Si `depth=4` tarda menos de un milisegundo, el ruido domina. Si `depth=12` tarda minutos, la experimentacion se vuelve poco practica. Un rango razonable es probar `depth=6`, `depth=8` y, si la maquina lo permite, `depth=10`.

## Limitaciones

El benchmark mide una implementacion concreta de root parallelism. No prueba que sea la mejor estrategia posible. Tecnicas como ordenamiento avanzado de movimientos, tablas de transposicion, iterative deepening o Young Brothers Wait Concept podrian cambiar el resultado. Tampoco se modelan multiples solicitudes concurrentes al motor; cada fila del benchmark es una busqueda aislada.

Otra limitacion es el uso de elapsed time de pared. En ambientes virtualizados, Docker Desktop puede compartir CPU con otros procesos y producir variaciones fuertes. Para una sustentacion se debe hablar de tendencia, no prometer speedups universales. Lo defendible es mostrar que existe `#pragma omp`, que el numero de hilos afecta la ejecucion, que las estadisticas se reportan y que las conclusiones reconocen overhead y desbalance.

## Relacion con el backend

El backend tambien expone `elapsed_ms`, `nodes`, `prunes` y `threads_used` en cada respuesta de `/move`. Esos datos sirven para depuracion y demostraciones rapidas desde la UI. Sin embargo, no reemplazan el benchmark porque incluyen red, serializacion JSON y planificacion del servidor. El analisis formal debe basarse en `mancala_bench`; la API se usa para demostrar integracion distribuida.
