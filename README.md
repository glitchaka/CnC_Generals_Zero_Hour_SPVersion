# SPVersion

SPVersion es un proyecto de juego independiente construido a partir del código fuente liberado de **Command & Conquer: Generals / Zero Hour** como base técnica.

Este repositorio **no pretende conservar ni reconstruir C&C Generals**. El código original se está reutilizando y modificando para desarrollar otro juego, con su propia jugabilidad, mapa, presentación, recursos y sistemas.

## Estado actual

El proyecto contiene actualmente una demo jugable tipo MOBA en Windows, con:

- mapa propio;
- tres carriles y zonas transitables;
- suelo modular basado en piezas del pack KayKit;
- estructuras y torres a escala natural;
- oleadas y enemigos;
- combate y respawn;
- assets externos de prueba integrados en el pipeline;
- compilación x64 mediante CMake y MSVC.

## Compilación

En Windows, desde la raíz del repositorio:

```bat
build_sp.cmd
```

El proceso genera:

```text
dist/SPVersion-demo/SPVersion.exe
dist/SPVersion-demo.zip
```

## Estructura relevante

- `SPBuild/` — proyecto moderno usado para generar la demo.
- `tools/` — scripts de preparación, parcheado y validación del build.
- `ExternalAssets/` — recursos externos preparados por los scripts de build.
- `Generals/` y `ZeroHour/` — código heredado utilizado como base técnica.

## Origen del código y licencia

Este proyecto deriva del código fuente publicado de **Command & Conquer: Generals** y **Command & Conquer: Generals – Zero Hour**.

El código heredado mantiene las condiciones de licencia aplicables del proyecto original. Consulta `LICENSE.md` y los avisos de licencia existentes en el repositorio.

Los assets externos incluidos o descargados por los scripts conservan sus respectivas licencias y condiciones de uso.
