# motor/

Contenedor 1: motor de busqueda C++17 con OpenMP.

Subdirectorios principales:

- `src/`: codigo fuente C++ del motor, incluyendo tablero, Alpha-Beta y MCTS.
- `tests/`: pruebas unitarias del motor.
- `bench/`: ejecutable de benchmark que lee posiciones fijas desde `suite.txt`,
  mide tiempo, speedup, eficiencia, nodos y podas.

Archivos de la raiz:

- `CMakeLists.txt`: build local y targets de pruebas.
- `Dockerfile`: imagen del motor. Expone un endpoint HTTP interno consumible por
  el backend.
- `motor_server`: binario HTTP generado por CMake; escucha en `0.0.0.0:8080`.

Contrato de `POST /move`:

- `algo="alphabeta"` requiere `depth` y puede usar varios hilos con OpenMP.
- `algo="mcts"` requiere `simulations`; la version actual de MCTS es secuencial
  y reporta `threads_used=1`.

El motor no se enlaza al backend en el mismo proceso (`pybind11`/`ctypes`).
La separacion se hace a nivel de contenedor y la comunicacion usa HTTP dentro de
la red interna del despliegue.
