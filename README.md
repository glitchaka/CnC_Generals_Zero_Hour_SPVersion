# SPVersion

SPVersion es un proyecto de juego independiente para Windows.

La demo actual se compila desde `SPBuild/` como un ejecutable C++17 x64 con Direct3D 11. El pipeline genera `demo_d3d11_human_release.cpp`, aplica los parches de `tools/`, compila el ejecutable y empaqueta sus recursos externos.

El repositorio se originó a partir de una copia del código fuente liberado de **Command & Conquer: Generals / Zero Hour**, por lo que todavía contiene los directorios heredados `Generals/` y `ZeroHour/`. Esos directorios no forman parte del target actual `SPVersion.exe` ni son la implementación de la demo actual.

## Estado actual

El proyecto contiene una demo jugable tipo MOBA con:

- mapa propio;
- tres carriles y zonas transitables;
- suelo modular basado en piezas KayKit;
- estructuras y torres;
- oleadas y enemigos;
- combate y respawn;
- assets externos preparados por el pipeline;
- compilación x64 mediante CMake y MSVC;
- renderizado Direct3D 11.

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

## Arquitectura actual

- `SPBuild/` — código fuente y configuración CMake del ejecutable actual.
- `tools/` — scripts que transforman, preparan y validan el fuente de release antes de compilar.
- `ExternalAssets/` — recursos externos utilizados por la demo.
- `Generals/` — código heredado de Command & Conquer: Generals conservado en el repositorio.
- `ZeroHour/` — código heredado de Command & Conquer: Generals – Zero Hour conservado en el repositorio.

El target actual de SPVersion no compila `Generals/` ni `ZeroHour/` como parte de `SPVersion.exe`.

## Origen y licencias

Los directorios heredados de Command & Conquer conservan las condiciones de licencia que les correspondan según el código fuente publicado originalmente. Consulta `LICENSE.md` y los avisos existentes en esos componentes.

Los recursos externos utilizados por SPVersion conservan sus propias licencias y condiciones de uso.
