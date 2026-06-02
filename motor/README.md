# motor/

Contenedor 1 — motor de búsqueda C++ con OpenMP.

Subdirectorios obligatorios:

- `src/` — código fuente C++ del motor (Minimax con poda Alfa--Beta).
- `tests/` — pruebas unitarias del motor.
- `bench/` — modo `benchmark`: ejecutable que lee posiciones fijas desde
  un archivo, mide tiempo, *speedup* y eficiencia, y reporta tablas.

Archivo en la raíz del directorio:

- `Dockerfile` — imagen del motor. Expone un *endpoint* interno
  (HTTP/gRPC) consumible por el `backend`.

El motor **no** se enlaza al backend en el mismo proceso
(`pybind11`/`ctypes`): la separación se hace a nivel de contenedor y la
comunicación va por la red interna del clúster.
