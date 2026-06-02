# 02 - Motor

## Representacion del tablero

El motor usa la representacion canonica de Kalah(6,4) con 14 posiciones. Los pits del jugador 0 son `0..5`, su store es `6`, los pits del jugador 1 son `7..12` y su store es `13`. El tablero inicial es:

```text
[4,4,4,4,4,4,0,4,4,4,4,4,4,0]
```

La estructura `Board` vive en `motor/src/board.h` y guarda un `std::array<int, 14>` junto con `side_to_move`. Se prefirio `std::array` sobre un vector dinamico porque el tamano del tablero nunca cambia y porque permite copias baratas y predecibles durante la busqueda. Cada nodo del arbol crea una copia del tablero al aplicar un movimiento; esto simplifica la implementacion de minimax y evita estado compartido entre ramas paralelas.

El motor valida invariantes importantes. El total de semillas debe ser 48, los pits no pueden ser negativos y el lado activo debe ser `0` o `1`. En modo debug, `apply_move` verifica con `assert` que el total de semillas antes y despues de la jugada se conserva. Esta clase de prueba es valiosa porque los errores en captura o barrido suelen manifestarse como semillas perdidas o duplicadas.

## Reglas implementadas

La funcion `legal_moves` retorna los indices absolutos de los pits propios que tienen semillas. Si `side_to_move` es `0`, solo considera `0..5`; si es `1`, solo considera `7..12`. La funcion `apply_move` recibe un pit legal, toma todas sus semillas y las distribuye en sentido ascendente modulo 14. Cuando la distribucion llega al store rival lo salta, tal como exige Kalah.

El turno extra se activa si la ultima semilla cae en el store propio. En ese caso `side_to_move` no cambia. Si la ultima semilla no cae en store propio, el turno pasa al otro jugador. La regla de captura se aplica cuando la ultima semilla cae en un pit propio que estaba vacio antes del movimiento y el pit opuesto del rival tiene semillas. En esa situacion, la semilla recien sembrada y todas las semillas opuestas se mueven al store del jugador que movio.

El final de partida se detecta cuando todos los pits regulares de un lado quedan vacios. La sobrecarga mutable de `is_terminal` aplica el barrido: mueve todas las semillas restantes del lado contrario a su store y deja esos pits en cero. Esto significa que los tableros generados por `apply_move` ya quedan normalizados si la jugada termina el juego. La sobrecarga constante permite preguntar si una posicion es terminal sin modificarla.

## Evaluacion

La funcion `evaluate` calcula una heuristica desde la perspectiva del jugador 0. En posiciones no terminales combina diferencia de stores y diferencia de semillas en pits regulares:

```text
score = store_diff + pit_weight * pit_diff
```

Por defecto `pit_weight` vale `0.5`. La diferencia de stores es el indicador mas fuerte porque esas semillas ya no se pueden perder. La diferencia de pits agrega informacion sobre movilidad y potencial de capturas, pero tiene menor peso porque esas semillas aun estan en juego. En posiciones terminales se retorna un valor alto positivo si gana jugador 0, alto negativo si gana jugador 1 y cero si hay empate.

La clase `AlphaBetaEngine` convierte esta evaluacion a la perspectiva del jugador que pidio la busqueda. Si `side_to_move` es `1`, invierte el signo. Asi el algoritmo siempre maximiza desde el punto de vista del jugador activo, y el backend no tiene que reinterpretar la evaluacion.

## Busqueda Alfa-Beta

El motor implementa minimax con poda Alfa-Beta. En cada nodo se generan movimientos legales, se aplica cada movimiento a una copia del tablero y se llama recursivamente hasta alcanzar profundidad cero o una posicion terminal. Si el turno del tablero coincide con el jugador de referencia, el nodo maximiza; si el turno es del rival, minimiza. La poda ocurre cuando `alpha >= beta`, y cada corte incrementa `stats.prunes`.

La busqueda cuenta nodos en `stats.nodes`. El nodo raiz se contabiliza una vez y cada llamada recursiva incrementa el contador local. El resultado final incluye:

- `move`: mejor pit absoluto encontrado.
- `evaluation`: valor heuristico desde la perspectiva del jugador activo.
- `stats.nodes`: nodos explorados.
- `stats.prunes`: cortes Alfa-Beta.
- `threads_used`: hilos realmente creados por OpenMP.

El desempate entre movimientos con igual evaluacion favorece el pit de menor indice. Esto hace que la respuesta sea deterministica y permite comparar ejecuciones secuenciales y paralelas sin ruido por orden de planificacion.

## Servidor HTTP

El ejecutable `mancala_engine` levanta un servidor HTTP en el puerto `MOTOR_PORT`, con valor por defecto `9000`. Expone `GET /healthz`, `GET /readyz`, `GET /metrics` y `POST /move`. El parser JSON es pequeno y especifico al contrato de la entrega: busca los campos `board`, `side`, `depth` y `threads`, valida rangos y responde errores `400` cuando la solicitud no cumple.

No se agrego una dependencia externa de JSON para mantener el Dockerfile y el CI simples. La API publica sigue usando JSON estandar y el backend hace una segunda capa de validacion con Pydantic. En un motor de produccion convendria usar una libreria como `nlohmann/json` o exponer gRPC, pero para esta entrega el contrato es fijo y el parser acotado reduce dependencias.

## Pruebas unitarias

`motor/tests/test_board.cpp` cubre estado inicial, conservacion de semillas, turno extra, salto del store rival, captura y barrido de final de partida. `motor/tests/test_alphabeta.cpp` verifica que la posicion inicial produce un movimiento legal, que la busqueda visita nodos y que la ejecucion paralela coincide con la secuencial en movimiento y evaluacion para una profundidad fija. El `CMakeLists.txt` registra ambas pruebas con `add_test`, por lo que `ctest --test-dir motor/build --output-on-failure` las ejecuta en CI.

## Benchmark

`motor/bench/bench.cpp` construye el ejecutable `mancala_bench`. Lee posiciones desde `motor/tests/suite.txt`, ejecuta Alfa-Beta con profundidades configurables y compara hilos `1,2,4,8` o la lista indicada por `--threads`. La salida CSV incluye tiempo, speedup, eficiencia, nodos y podas. Esta herramienta es la base del analisis comparativo porque mide el motor sin pasar por red, frontend ni backend.
