# Informe del proyecto Mancala Kalah(6,4)

Este directorio contiene el informe completo del proyecto final de Infraestructuras Paralelas y Distribuidas. El objetivo del repositorio es entregar una aplicacion reproducible que calcule jugadas de Mancala Kalah(6,4) usando un motor C++ con OpenMP, exponga ese calculo por un backend FastAPI y permita probarlo desde un frontend web y desde despliegues Docker/Kubernetes. Todos los documentos estan escritos como Markdown versionado porque el enunciado pide que la evidencia tecnica, las decisiones de arquitectura, los comandos de reproduccion y el analisis experimental vivan dentro del repositorio.

La implementacion final evita el modo simulado como camino principal. El backend no calcula jugadas en Python, no carga el motor con `pybind`, `ctypes` ni librerias en proceso, y tampoco responde una jugada fija. El flujo real es una llamada HTTP desde `backend` hacia el servicio `motor`. El motor recibe el contrato JSON de `POST /move`, ejecuta Alfa-Beta sobre las reglas de Kalah, paraleliza la evaluacion de los movimientos raiz con OpenMP y responde movimiento, evaluacion, tiempo, nodos, podas e hilos usados.

## Mapa de documentos

1. [01-arquitectura.md](01-arquitectura.md): describe los tres servicios, los contratos HTTP, el flujo de datos, los limites de responsabilidad y la razon de separar motor, backend y frontend.
2. [02-motor.md](02-motor.md): documenta la representacion del tablero, reglas implementadas, evaluacion, busqueda Alfa-Beta, metricas y pruebas unitarias.
3. [03-paralelizacion.md](03-paralelizacion.md): explica la estrategia OpenMP, por que se escogio paralelismo en la raiz, como se acumulan estadisticas y como se mide `T(p)`, `S(p)` y `E(p)`.
4. [04-despliegue-local.md](04-despliegue-local.md): contiene instrucciones de Docker Compose, pruebas con `curl`, imagenes locales y manifiestos para kind/minikube.
5. [05-despliegue-nube.md](05-despliegue-nube.md): define el despliegue Kubernetes de nube, replicas, recursos, probes, imagenes GHCR e Ingress.
6. [06-cicd.md](06-cicd.md): resume el pipeline propio, pruebas, build de imagenes, publicacion en GHCR y declaracion de analisis SonarQube.
7. [07-analisis-comparativo.md](07-analisis-comparativo.md): explica como comparar ejecuciones secuenciales y paralelas con el benchmark del motor.
8. [08-conclusiones.md](08-conclusiones.md): sintetiza resultados, aprendizajes y limites reales del proyecto.

## Como reproducir la solucion

El camino recomendado para una persona externa al grupo es Docker Compose:

```bash
docker compose -f deploy/local/docker-compose.yml up --build
```

Si quieres los comandos de prueba y validacion separados por componente, revisa [TESTING.md](../TESTING.md).

Con el stack arriba se esperan estos servicios:

- Frontend: `http://localhost:8080`
- Backend: `http://localhost:8000`
- Motor C++: `http://localhost:9000`

La prueba minima del contrato principal es:

```bash
curl -s -X POST http://localhost:8000/move \
  -H "Content-Type: application/json" \
  -d '{"board":[4,4,4,4,4,4,0,4,4,4,4,4,4,0],"side":0,"depth":8,"threads":4}'
```

La respuesta debe incluir los campos `move`, `evaluation`, `elapsed_ms`, `stats.nodes`, `stats.prunes` y `threads_used`. Si el motor no esta listo, `GET /readyz` del backend responde `503`, lo cual es preferible a esconder el problema con una respuesta falsa.

## Evidencia esperada

El proyecto queda preparado para tres niveles de evidencia. Primero, las pruebas unitarias del motor validan reglas delicadas del juego: conservacion de semillas, salto del store rival, turno extra, captura y barrido de final de partida. Segundo, las pruebas del backend validan salud, readiness, CORS explicito, schema Pydantic y delegacion hacia un cliente de motor intercambiable en pruebas. Tercero, el benchmark `mancala_bench` permite ejecutar las posiciones fijas de `motor/tests/suite.txt` con hilos `1,2,4,8` y producir una tabla CSV de tiempo, speedup, eficiencia, nodos y podas.

La documentacion no pretende reemplazar la sustentacion. Su funcion es dejar claro que la solucion tiene separacion por servicios, usa paralelismo real, es reproducible en local, tiene manifiestos de nube con valores concretos, y cuenta con CI/CD propio sin tocar el workflow `classroom.yml` del profesor.
