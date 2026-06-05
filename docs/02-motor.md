# 02 - Motor

## Representacion del tablero

El motor usa la representacion canonica de Kalah(6,4) con 14 posiciones. Los pits del jugador 0 son `0..5`, su store es `6`, los pits del jugador 1 son `7..12` y su store es `13`. El tablero inicial es:

```text
[4,4,4,4,4,4,0,4,4,4,4,4,4,0]
```

La estructura `Board` vive en `motor/src/board.h` y guarda un arreglo plano `int pits[14]` junto con `side_to_move`. Se usa un arreglo fijo porque el tamano del tablero nunca cambia, no requiere memoria dinamica y permite copias baratas durante la busqueda. Cada nodo del arbol crea una copia del tablero al aplicar un movimiento; esto simplifica la implementacion de minimax y evita estado compartido entre ramas paralelas.

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

Por defecto `pit_weight` vale `0.5`. La diferencia de stores es el indicador mas fuerte porque esas semillas ya no se pueden perder. La diferencia de pits agrega informacion sobre movilidad y potencial de capturas, pero tiene menor peso porque esas semillas aun estan en juego. En posiciones terminales se retorna `INT_MAX` si gana jugador 0, `INT_MIN` si gana jugador 1 y cero si hay empate.

Las funciones de Alpha-Beta usan esta evaluacion como contrato global: positivo favorece al jugador 0 y negativo favorece al jugador 1. En la raiz, jugador 0 maximiza y jugador 1 minimiza.

## Busqueda Alfa-Beta

El motor implementa minimax con poda Alfa-Beta. En cada nodo se generan movimientos legales, se aplica cada movimiento a una copia del tablero y se llama recursivamente hasta alcanzar profundidad cero o una posicion terminal. Si el turno del tablero coincide con el jugador de referencia, el nodo maximiza; si el turno es del rival, minimiza. La poda ocurre cuando `alpha >= beta`, y cada corte incrementa `stats.prunes`.

Pseudocodigo de Minimax puro usado como oraculo de correccion:

```text
minimax(board, depth, is_max):
  si depth == 0 o is_terminal(board):
    retornar evaluate(board)

  si is_max:
    best = -infinito
    para mv en legal_moves(board):
      next, extra = apply_move(board, mv)
      next_is_max = is_max si extra, si no !is_max
      best = max(best, minimax(next, depth - 1, next_is_max))
    retornar best

  best = +infinito
  para mv en legal_moves(board):
    next, extra = apply_move(board, mv)
    next_is_max = is_max si extra, si no !is_max
    best = min(best, minimax(next, depth - 1, next_is_max))
  retornar best
```

Pseudocodigo de Alfa-Beta:

```text
alphabeta(board, depth, alpha, beta, is_max):
  nodes++
  si depth == 0 o is_terminal(board):
    retornar evaluate(board)

  si is_max:
    best = -infinito
    para mv en legal_moves(board):
      next, extra = apply_move(board, mv)
      child_is_max = is_max si extra, si no !is_max
      best = max(best, alphabeta(next, depth - 1, alpha, beta, child_is_max))
      alpha = max(alpha, best)
      si alpha >= beta:
        prunes++
        cortar
    retornar best

  best = +infinito
  para mv en legal_moves(board):
    next, extra = apply_move(board, mv)
    child_is_max = is_max si extra, si no !is_max
    best = min(best, alphabeta(next, depth - 1, alpha, beta, child_is_max))
    beta = min(beta, best)
    si beta <= alpha:
      prunes++
      cortar
  retornar best
```

La regla critica es el turno extra: si `apply_move` retorna `extra_turn=true`, el siguiente nodo conserva el mismo rol MAX/MIN. Alternar incondicionalmente despues de cada jugada seria incorrecto en Kalah, porque una semilla final en el kalaha propio permite que el mismo jugador vuelva a mover.

La busqueda cuenta nodos en `nodes`. Cada llamada recursiva a `alphabeta` incrementa el contador local. El resultado final incluye:

- `move`: mejor pit absoluto encontrado.
- `value`: valor heuristico desde la perspectiva global del tablero.
- `nodes`: nodos explorados.
- `prunes`: cortes Alfa-Beta.
- `threads_used`: hilos realmente creados por OpenMP.

El desempate entre movimientos con igual evaluacion favorece el pit de menor indice. Esto hace que la respuesta sea deterministica y permite comparar ejecuciones secuenciales y paralelas sin ruido por orden de planificacion.

Evidencia de correccion: `test_alphabeta.cpp` compara Minimax puro contra Alfa-Beta sobre posiciones de prueba a profundidades 4 y 6. La corrida actual reporta:

| Test | Resultado |
|------|-----------|
| Equivalencia Minimax == Alpha-Beta | PASS |
| Turno extra respetado | PASS |
| Terminal positivo para P1 | PASS |
| `prunes > 0` | PASS |
| Alpha-Beta explora menos nodos que Minimax | PASS |

## Servidor HTTP

El ejecutable `motor_server` levanta un servidor HTTP con `cpp-httplib` en `0.0.0.0:8080`. Expone `GET /healthz`, `GET /readyz`, `GET /metrics` y `POST /move`. La ruta `/healthz` responde siempre `{"status":"ok"}` mientras el proceso viva. La ruta `/readyz` responde `{"status":"ready"}` cuando el servidor termino de registrar rutas e inicializar el flag atomico global de readiness.

`POST /move` parsea el cuerpo con `nlohmann/json`. El contrato recibe `board`, `side`, `algo`, `depth`, `simulations` y `threads`. El servidor reconstruye un `Board` desde el arreglo, valida que tenga exactamente 14 enteros no negativos y que la suma sea 48. Si el tablero no cumple, responde `400 {"error":"invalid board"}`. Si el algoritmo es desconocido, responde `400 {"error":"unknown algo"}`. Errores internos no controlados responden `500`.

Para `algo="alphabeta"`, el servidor llama `best_move_parallel(board, depth, threads)`. Para `algo="mcts"`, llama `best_move_mcts(board, simulations, 1.41421f)`. El tiempo de busqueda se mide con `omp_get_wtime()` alrededor de la llamada al motor y se reporta como `elapsed_ms`. La respuesta mantiene el contrato que consume el backend: `move`, `evaluation`, `elapsed_ms`, `stats` y `threads_used`.

## Pruebas unitarias

`motor/tests/test_board.cpp` cubre estado inicial, conservacion de semillas, turno extra, salto del store rival, captura, no-captura con opuesto vacio y barrido de final de partida. `motor/tests/test_alphabeta.cpp` compara Minimax contra Alpha-Beta, valida terminales, podas y equivalencia entre ejecucion secuencial y paralela. `motor/tests/test_mcts.cpp` valida jugadas legales, convergencia basica, comparacion reportada contra Alpha-Beta y que los rollouts coincidan con `simulations`. El `CMakeLists.txt` registra estas pruebas con `add_test`, por lo que `ctest --test-dir motor/build --output-on-failure` las ejecuta en CI.

## Benchmark

`motor/bench/bench.cpp` construye el ejecutable `bench`. Lee posiciones desde `motor/tests/suite.txt`. Con `--algo alphabeta` ejecuta Alpha-Beta con profundidades configurables y compara hilos `1,2,4,8`; la salida incluye tiempo, speedup, eficiencia, nodos y podas. Con `--algo mcts` ejecuta MCTS con el presupuesto de simulaciones indicado y reporta tiempo promedio, rollouts, win rate y profundidad promedio de rollout. Esta herramienta mide el motor sin pasar por red, frontend ni backend.
