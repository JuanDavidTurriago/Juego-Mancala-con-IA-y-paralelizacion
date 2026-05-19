# 06 - CI/CD

## GitHub Actions

La fase 1 usa un workflow base en `.github/workflows/ci.yml` que solo confirma que GitHub Actions funciona y que el repositorio ya esta conectado al pipeline.

Este tipo de CI vacio sirve para:

- verificar que el workflow dispara en `push` y `pull_request`
- confirmar que la sintaxis del archivo YAML es correcta
- tener una primera senal verde antes de agregar build, pruebas o analisis estatico

En esta etapa no compila ni prueba el proyecto completo; solo deja una base estable para crecer despues.

## SonarQube / SonarCloud

Pendiente por completar.

