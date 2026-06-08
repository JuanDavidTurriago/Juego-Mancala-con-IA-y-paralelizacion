[![Review Assignment Due Date](https://classroom.github.com/assets/deadline-readme-button-22041afd0340ce965d47ae6ef1cefeee28c7c493a6346c4f15d667ab976d596c.svg)](https://classroom.github.com/a/6qItijqM)
# Proyecto Final — Motores paralelos de Mancala (Kalah) y despliegue en Kubernetes

**Curso:** Infraestructuras paralelas y distribuidas — Universidad del Valle
**Periodo:** 2026 — I
**Entrega:** miércoles 10 de junio de 2026, 17:58:59 (push a `main`).
**Sustentación:** jueves 11 de junio de 2026 en horario de clase.

El enunciado oficial se publica en el campus virtual. Este repositorio es el
esqueleto que el grupo debe completar; la rúbrica suma 150 puntos repartidos
en diez criterios, y los pasos de autograding agrupan esos criterios en tres
bloques (local, nube, informe + CI/CD) para dar retroalimentación continua en
cada `push`.

---

## Integrantes del grupo

Los grupos son de **hasta 5 estudiantes**, conformados a través del *roster*
de GitHub Classroom. Diligencie la tabla con la información de todos los
integrantes antes del primer push significativo. Tener menos integrantes
está permitido; la rúbrica y la carga de trabajo no se reducen por ello.
No completar esta sección reduce un 10 % sobre la nota final del proyecto.

| Nombre completo | Código | Correo institucional         |
| --------------- | ------ | ---------------------------- |
| Bryan Steven Panesso Avila        | 2380701 | bryan.panesso@correounivalle.edu.co     |
| Andres Felipe Castrillon Martínez | 2380664 | andres.castrillon.martinez@correounivalle.edu.co |
| Javier Andrés Muñoz Tavera        | 2380421 | javier.tavera@correounivalle.edu.co      |
| Juan David Turriago Orozco        | 2477182 | juan.turriago@correounivalle.edu.co     |

---

## Estructura obligatoria del repositorio

```
.
├── README.md
├── docs/
│   ├── README.md
│   ├── 01-arquitectura.md
│   ├── 02-motor.md
│   ├── 03-paralelizacion.md
│   ├── 04-despliegue-local.md
│   ├── 05-despliegue-nube.md
│   ├── 06-cicd.md
│   ├── 07-analisis-comparativo.md
│   └── 08-conclusiones.md
├── motor/
│   ├── src/
│   ├── tests/
│   ├── bench/
│   └── Dockerfile
├── backend/
│   ├── app/
│   ├── tests/
│   └── Dockerfile
├── frontend/
│   └── Dockerfile
├── deploy/
│   ├── local/      # docker-compose.yml y manifiestos kind/minikube
│   └── cloud/      # manifiestos del despliegue en la nube
└── .github/
    └── workflows/  # pipelines CI/CD + análisis con SonarQube
```

Archivos fuera de estas carpetas no se evalúan. En `docs/` sólo se aceptan
los nueve archivos listados; cualquier otro Markdown adicional dentro de
`docs/` se ignora.

---

## Cómo compilar y ejecutar localmente

Estas instrucciones las completa el grupo según las tecnologías escogidas.
Sustituya los comandos por los que corresponden a su implementación.

```bash
# 1. Construir las imágenes y levantar el stack local
docker compose -f deploy/local/docker-compose.yml up --build

# 2. Probar el endpoint del backend
curl -s -X POST http://localhost:8000/move \
     -H 'Content-Type: application/json' \
     -d '{"board":[4,4,4,4,4,4,0,4,4,4,4,4,4,0],"side":0,"depth":8,"threads":4}'

# 3. Abrir el frontend
xdg-open http://localhost:8080
```

Una persona externa al grupo debe poder reproducir el despliegue siguiendo
únicamente estos pasos.

Para ver comandos detallados de verificación del motor, backend, frontend y
stack completo, consulte [TESTING.md](TESTING.md).

---

## Informe

El informe vive íntegramente en `docs/` repartido en ocho archivos numerados
y un índice (`docs/README.md`). No se aceptan PDFs, DOCX, Notion ni enlaces
externos: todo lo evaluable está versionado en este repositorio. Diagramas
en Mermaid, fórmulas en LaTeX embebido en Markdown, capturas como evidencia
experimental con rutas relativas.

---

## Autograding

El workflow `.github/workflows/classroom.yml` se ejecuta en cada push y
reporta tres bloques de retroalimentación que suman 150 puntos:

| Bloque                  | Puntos | Qué verifica                                                              |
| ----------------------- | ------ | ------------------------------------------------------------------------- |
| Solución local          | 50     | `docs/02..04`, Dockerfiles de motor/backend/frontend, `docker-compose.yml`|
| Solución en la nube     | 50     | `docs/01`, `docs/05`, `docs/07`, manifiestos en `deploy/cloud/`           |
| Informe y CI/CD         | 50     | `docs/README.md`, `docs/06`, `docs/08`, segundo workflow del grupo        |

El autograding no sustituye la calificación de la rúbrica: es un
recordatorio de los entregables mínimos. La nota final la asigna el docente
combinando la rúbrica, el factor de sustentación y el factor de aporte al
repositorio.
