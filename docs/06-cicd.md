# 06 - CI/CD

## Workflow propio

El repositorio conserva `.github/workflows/classroom.yml` como workflow del profesor y agrega `.github/workflows/ci.yml` como pipeline propio del grupo. Este segundo workflow se ejecuta en `push` y `pull_request` hacia `main`. Su objetivo es detectar errores antes del autograding: compilar el motor, ejecutar pruebas C++, ejecutar pruebas Python, construir imagenes Docker y declarar analisis SonarQube.

La separacion es importante. `classroom.yml` pertenece al profesor y no debe modificarse innecesariamente. `ci.yml` pertenece al grupo y puede evolucionar con la implementacion. Si falla el workflow propio, el grupo tiene una senal clara de que la solucion no esta lista aunque el autograding solo revise estructura o tamanos de documentos.

## Job del motor

El job `motor` corre en Ubuntu. Instala `cmake`, `g++` y `libgomp1`, configura CMake en modo Release, compila y ejecuta los ejecutables de prueba `test_board`, `test_alphabeta` y `test_mcts` (reglas del tablero, busqueda Alfa-Beta y MCTS). Despues ejecuta un benchmark de humo:

```bash
./motor/build/bench --algo alphabeta --depth 4 --positions motor/tests/suite.txt
./motor/build/bench --algo mcts --simulations 100 --positions motor/tests/suite.txt
```

La profundidad 4 y las 100 simulaciones de MCTS son deliberadamente pequenas para CI. No buscan medir rendimiento definitivo; buscan confirmar que el benchmark compila, lee posiciones, ejecuta ambos algoritmos y produce tablas sin bloquear el pipeline. Las mediciones formales se hacen manualmente con profundidades y presupuestos mayores.

## Job del backend

El job `backend` instala Python 3.11, lee `backend/requirements.txt` y ejecuta `pytest backend/tests`. Las pruebas no dependen de que el motor real este corriendo. En su lugar reemplazan el cliente `motor` por un fake controlado. Esto permite validar schema, CORS, readiness y delegacion sin crear un entorno distribuido completo dentro de pytest.

Aunque el fake existe en pruebas, no hay un modo simulado en produccion. El codigo de aplicacion instancia `MotorClient`, que llama por HTTP a `MOTOR_URL`. La prueba de delegacion verifica que `/move` usa ese cliente y que la respuesta respeta `MoveResponse`.

## Build de imagenes

El job `docker` construye las tres imagenes:

```bash
docker build -t mancala-motor:<sha> motor
docker build -t mancala-backend:<sha> backend
docker build -t mancala-frontend:<sha> frontend
```

El build del motor ejecuta CMake y `ctest` dentro del Dockerfile, por lo que tambien valida el entorno Linux que se usara en despliegue. El build del backend instala dependencias pinneadas. El build del frontend copia archivos estaticos y configuracion Nginx.

## Publicacion en GHCR

El job `publish` solo corre en `push` a `main`. Hace login a `ghcr.io` con `GITHUB_TOKEN` y publica:

- `ghcr.io/<owner>/mancala-motor:<sha>`
- `ghcr.io/<owner>/mancala-backend:<sha>`
- `ghcr.io/<owner>/mancala-frontend:<sha>`

El owner se normaliza a minusculas porque algunos registros rechazan nombres con mayusculas. El tag `${{ github.sha }}` es inmutable desde el punto de vista del repositorio: identifica exactamente el commit que produjo la imagen. **No se publica `latest`** en produccion; solo tags por SHA de commit. Esto permite relacionar un despliegue de nube con una version de codigo concreta.

## SonarQube

La integracion esta **solo en YAML** (`.github/workflows/sonar.yml`). No se usa el plugin de SonarQube del marketplace de GitHub; la rúbrica exige configuracion en el repositorio.

El workflow `sonar.yml` ejecuta `SonarSource/sonarqube-scan-action@v5` con:

- `SONAR_TOKEN` — secret del repositorio (Settings → Secrets and variables → Actions).
- `SONAR_HOST_URL` — secret del repositorio (misma seccion; no variable).

Configuracion del proyecto en `sonar-project.properties` (fuentes, tests y exclusiones). El job compila el motor antes del escaneo para dar contexto al analizador.

Verificacion en GitHub: pestaña **Actions** → workflow **SonarQube** → run exitoso y resultado visible en el servidor Sonar asociado al `SONAR_HOST_URL`.

## CD hacia Kubernetes

El pipeline actual publica imagenes, pero no aplica manifiestos automaticamente al cluster. Esta decision es conservadora porque el repositorio de Classroom normalmente no tiene kubeconfig ni credenciales de nube. El paso CD manual consiste en actualizar tags de `deploy/cloud/*.yaml` o usar `kubectl set image` con el SHA publicado.

Una extension futura seria agregar un job protegido por ambiente `production`, con aprobacion manual y secreto `KUBECONFIG`. Ese job ejecutaria:

```bash
kubectl apply -f deploy/cloud/
kubectl rollout status deployment/backend
```

No se implementa por defecto para evitar que un push academico intente desplegar a un cluster inexistente.

## Politicas de calidad

El pipeline verifica tres capas. La primera es correccion funcional: reglas de tablero, busqueda y endpoints. La segunda es empaquetamiento: las imagenes Docker deben construir desde cero. La tercera es mantenibilidad: SonarQube queda declarado para revisar olores de codigo, duplicacion y riesgos. Ninguna capa reemplaza la revision manual, pero juntas reducen la probabilidad de entregar una solucion que solo funciona en la maquina de un integrante.

## Fallos comunes

Si el job del motor falla por OpenMP, revisar que `find_package(OpenMP REQUIRED)` encuentre soporte en Ubuntu y que el compilador sea `g++`. Si fallan pruebas del backend por import, revisar que `pytest` se ejecute desde la raiz y que `backend/` este en el path, como ocurre por defecto al apuntar a `backend/tests`. Si falla GHCR con permisos, revisar que `packages: write` este declarado y que la configuracion del repositorio permita publicar paquetes con `GITHUB_TOKEN`.

Si SonarQube falla por falta de token, el pipeline no se bloquea por `continue-on-error`, pero el log mostrara el motivo. Para una sustentacion se puede explicar que la integracion esta declarada y que requiere credenciales externas para ejecutarse completamente.
