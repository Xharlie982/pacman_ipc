# Pac-Man POSIX Concurrent

**Proyecto de Sistemas Operativos — Concurrencia, IPC y Planificación de Procesos**

---

## Descripción General

Este proyecto implementa una simulación del videojuego Pac-Man utilizando exclusivamente primitivas POSIX de bajo nivel: procesos independientes (`fork`/`exec`), memoria compartida (`shm_open`), semáforos (`sem_open`) y pipes anónimos. El sistema se compone de cuatro procesos independientes que cooperan en tiempo real:

| Proceso | Archivo fuente | Responsabilidad |
|---|---|---|
| **Planificador** | `scheduler_process.c` | Orquesta la ejecución mediante Round-Robin y gestiona casos de prueba |
| **Pac-Man** | `pacman_process.c` | Controla el movimiento del jugador y la lógica de puntuación |
| **Enemigos** | `enemy_process.c` | Administra el comportamiento de los fantasmas |
| **Renderizador** | `renderer_process.c` | Dibuja el estado del juego (terminal ANSI o ventana SDL2) |

La arquitectura completa se define en **`shared.h`**, que centraliza estructuras de datos, constantes y macros de configuración compartidas entre todos los procesos.

---

## Requisitos del Sistema

- **Sistema operativo:** Ubuntu 22.04 LTS o superior / WSL2 con Ubuntu
- **Compilador:** GCC 11 o superior
- **Estándar C:** C11 (`-std=c11`)
- **Bibliotecas opcionales:** SDL2, SDL2_image, SDL2_ttf (requeridas únicamente para el modo gráfico)

---

## 1. Instalación de Dependencias

Antes de compilar el proyecto, asegúrese de que el sistema esté actualizado y de que todas las herramientas y bibliotecas necesarias estén instaladas. Ejecute los siguientes comandos en la terminal de Ubuntu o WSL2:

```bash
sudo apt update
sudo apt install build-essential libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev
```

- **`build-essential`** — Instala GCC, G++ y Make, indispensables para compilar código C.
- **`libsdl2-dev`** — Biblioteca principal de SDL2 para gráficos, entrada y audio.
- **`libsdl2-image-dev`** — Extensión de SDL2 para carga de imágenes (PNG, JPG, etc.).
- **`libsdl2-ttf-dev`** — Extensión de SDL2 para renderizado de fuentes TrueType.

> **Nota:** Si trabaja en WSL2, asegúrese de contar con un servidor X11 activo (por ejemplo, VcXsrv o X410) y de tener configurada correctamente la variable de entorno `DISPLAY` para poder visualizar ventanas gráficas.

---

## 2. Compilación del Proyecto

La compilación se gestiona mediante el **`Makefile`** incluido en el repositorio. Para compilar el proyecto desde cero, o para recompilar después de realizar cualquier modificación en los interruptores de arquitectura descritos en la sección 4, ejecute siempre el siguiente comando:

```bash
make clean && make
```

- **`make clean`** — Elimina todos los objetos compilados anteriores (archivos `.o`) y los binarios generados, garantizando una compilación limpia sin artefactos obsoletos.
- **`make`** — Compila el código fuente completo y genera el ejecutable `scheduler_process`.

> **Importante:** Cada vez que modifique una macro del preprocesador (como `ENABLE_POWER_PELLETS` o `MODO_GRAFICO_SDL`), es **obligatorio** ejecutar `make clean && make` para que los cambios surtan efecto. Una compilación incremental sin limpiar puede producir un binario inconsistente que no refleje la configuración deseada.

---

## 3. Ejecución de los Casos de Prueba

El proyecto incluye cuatro casos de prueba predefinidos, ubicados en el directorio `cases/`, que permiten evaluar distintos escenarios de planificación y concurrencia. Cada caso se ejecuta pasando su ruta como argumento al planificador:

### Caso 1 — Sincronización Base Round-Robin

```bash
./scheduler_process cases/caso1
```

Evalúa la sincronización fundamental entre procesos utilizando el algoritmo de planificación **Round-Robin**. Verifica que el quantum de tiempo se respete correctamente y que todos los procesos accedan a la memoria compartida sin condiciones de carrera.

---

### Caso 2 — Inanición y Comportamiento Asíncrono

```bash
./scheduler_process cases/caso2
```

Simula un escenario donde uno o más procesos experimentan **inanición** (_starvation_) debido a la naturaleza asíncrona de las operaciones concurrentes. Permite observar cómo el planificador maneja situaciones en las que un proceso no recibe tiempo de CPU de forma equitativa.

---

### Caso 3 — Inversión de Prioridades con `SET_PRIORITY`

```bash
./scheduler_process cases/caso3
```

Demuestra el fenómeno de **inversión de prioridades**, activando la directiva `SET_PRIORITY` para asignar niveles de prioridad diferenciados a los procesos. Evalúa si el planificador resuelve correctamente los conflictos de acceso a recursos compartidos entre procesos de distinta prioridad.

---

### Caso 4 — Modo Cacería, Masacre de Fantasmas y Time Out

```bash
./scheduler_process cases/caso4
```

Activa el **modo de cacería** (_hunt mode_), en el que Pac-Man puede eliminar fantasmas durante un periodo limitado. Evalúa la lógica de inversión de roles, el contador de tiempo (_time out_) y la correcta contabilización de puntos por masacre.

> ⚠️ **Dependencia crítica:** El `caso4` **no se ejecutará correctamente** a menos que la macro `ENABLE_POWER_PELLETS` esté activa en **`shared.h`** (línea 22). Si dicha macro permanece comentada, el sistema omitirá la lógica de cacería. Consulte la sección 4 para instrucciones de activación y posterior recompilación.

---

## 4. Guía de Testing — Interruptores de Arquitectura

El proyecto ha sido diseñado con una arquitectura **modular y configurable** mediante macros del preprocesador de C. Este enfoque permite activar o desactivar subsistemas completos del juego editando únicamente dos líneas de código, sin necesidad de alterar la lógica principal. Esto facilita la evaluación progresiva e independiente de cada componente del sistema.

Se pueden generar **4 configuraciones distintas** combinando los dos interruptores disponibles.

---

### Interruptor 1 — Power Pellets

**Ubicación:** **`shared.h`**, línea 22

Este interruptor controla si el sistema incluye la lógica de _power pellets_ (píldoras de poder). Cuando está activo, Pac-Man puede ingerir una píldora especial que activa el modo de cacería durante un tiempo limitado, permitiéndole devorar fantasmas y sumar puntos adicionales.

**Para activar:**
```c
#define ENABLE_POWER_PELLETS
```

**Para desactivar:**
```c
// #define ENABLE_POWER_PELLETS
```

---

### Interruptor 2 — Interfaz Gráfica SDL2

**Ubicación:** **`renderer_process.c`**, línea 8

Este interruptor determina qué sistema de renderizado utiliza el proceso de visualización. Cuando está activo, el juego se muestra en una ventana gráfica SDL2. Cuando está desactivado, el renderizado se realiza directamente en la terminal mediante secuencias de escape **ANSI**.

**Para activar (ventana SDL2):**
```c
#define MODO_GRAFICO_SDL
```

**Para desactivar (terminal ANSI):**
```c
// #define MODO_GRAFICO_SDL
```

---

### Configuraciones Disponibles

A continuación se describen los cuatro escenarios de prueba que el evaluador puede generar combinando ambos interruptores. Tras cada modificación, recuerde siempre recompilar con `make clean && make`.

---

#### ① Versión Base

| Macro | Estado |
|---|---|
| `ENABLE_POWER_PELLETS` | ❌ Desactivado |
| `MODO_GRAFICO_SDL` | ❌ Desactivado |

La configuración mínima del sistema. El juego se ejecuta íntegramente en la **terminal ANSI**, sin soporte gráfico ni lógica de poderes. Ideal para verificar la corrección de la sincronización POSIX, los semáforos y la memoria compartida en un entorno sin dependencias externas.

```bash
# En shared.h (línea 22):       // #define ENABLE_POWER_PELLETS
# En renderer_process.c (línea 8): // #define MODO_GRAFICO_SDL

make clean && make
./scheduler_process cases/caso1
```

---

#### ② Versión Base + Power Pellets

| Macro | Estado |
|---|---|
| `ENABLE_POWER_PELLETS` | ✅ Activado |
| `MODO_GRAFICO_SDL` | ❌ Desactivado |

Habilita la lógica completa de _power pellets_ y modo cacería, manteniéndose en el renderizado por **terminal ANSI**. Permite evaluar la correcta implementación del cambio de estado de los fantasmas, el temporizador de cacería y el sistema de puntuación extendido, sin requerir un servidor gráfico.

```bash
# En shared.h (línea 22):       #define ENABLE_POWER_PELLETS
# En renderer_process.c (línea 8): // #define MODO_GRAFICO_SDL

make clean && make
./scheduler_process cases/caso4
```

---

#### ③ Versión Base + SDL2

| Macro | Estado |
|---|---|
| `ENABLE_POWER_PELLETS` | ❌ Desactivado |
| `MODO_GRAFICO_SDL` | ✅ Activado |

Activa el renderizado en **ventana gráfica SDL2** sin incluir la lógica de poderes. Permite evaluar de forma aislada la integración del sistema de renderizado gráfico con la arquitectura de procesos POSIX, verificando que la memoria compartida se lea y visualice correctamente en tiempo real.

```bash
# En shared.h (línea 22):       // #define ENABLE_POWER_PELLETS
# En renderer_process.c (línea 8): #define MODO_GRAFICO_SDL

make clean && make
./scheduler_process cases/caso1
```

---

#### ④ Versión Completa (SDL2 + Power Pellets)

| Macro | Estado |
|---|---|
| `ENABLE_POWER_PELLETS` | ✅ Activado |
| `MODO_GRAFICO_SDL` | ✅ Activado |

La **experiencia arcade completa**. Activa simultáneamente el renderizado SDL2 y la lógica de _power pellets_, reproduciendo el comportamiento del juego original con toda la riqueza visual e interactividad del sistema. Esta configuración ejercita la totalidad del código del proyecto.

```bash
# En shared.h (línea 22):       #define ENABLE_POWER_PELLETS
# En renderer_process.c (línea 8): #define MODO_GRAFICO_SDL

make clean && make
./scheduler_process cases/caso4
```

---

## Estructura del Repositorio

```
pacman_posix/
├── shared.h                  # Definiciones globales, estructuras IPC y macros de configuración
├── scheduler_process.c       # Proceso planificador (Round-Robin, gestión de casos)
├── pacman_process.c          # Proceso Pac-Man (movimiento y puntuación)
├── enemy_process.c           # Proceso de enemigos (comportamiento de fantasmas)
├── renderer_process.c        # Proceso renderizador (ANSI / SDL2)
├── Makefile                  # Sistema de compilación
└── cases/
    ├── caso1                 # Caso: Sincronización base Round-Robin
    ├── caso2                 # Caso: Inanición y comportamiento asíncrono
    ├── caso3                 # Caso: Inversión de prioridades con SET_PRIORITY
    └── caso4                 # Caso: Modo cacería y Time Out (requiere ENABLE_POWER_PELLETS)
```

---

## Resumen de Comandos Esenciales

```bash
# 1. Instalar dependencias (una sola vez)
sudo apt update && sudo apt install build-essential libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev

# 2. Compilar (o recompilar tras modificar macros)
make clean && make

# 3. Ejecutar cualquier caso de prueba
./scheduler_process cases/caso1
./scheduler_process cases/caso2
./scheduler_process cases/caso3
./scheduler_process cases/caso4
```

---

*Proyecto desarrollado para el curso de Sistemas Operativos. Implementación basada en estándares POSIX — IEEE Std 1003.1.*
