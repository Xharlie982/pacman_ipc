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

| Macro | Estado | Atajo Modular |
|---|---|---|
| `ENABLE_POWER_PELLETS` | ❌ Desactivado | `make test1` |
| `MODO_GRAFICO_SDL` | ❌ Desactivado | |

La configuración mínima del sistema. El juego se ejecuta íntegramente en la **terminal ANSI**, sin soporte gráfico ni lógica de poderes.

```bash
make test1
./scheduler_process cases/caso1
```

---

#### ② Versión Base + Power Pellets

| Macro | Estado | Atajo Modular |
|---|---|---|
| `ENABLE_POWER_PELLETS` | ✅ Activado | `make test_bonus` o `make test1 POWER=1` |
| `MODO_GRAFICO_SDL` | ❌ Desactivado | |

Habilita la lógica completa de _power pellets_ y modo cacería en terminal ANSI.

```bash
make test1 POWER=1
./scheduler_process cases/caso4
```

---

#### ③ Versión Base + SDL2

| Macro | Estado | Atajo Modular |
|---|---|---|
| `ENABLE_POWER_PELLETS` | ❌ Desactivado | `make test1 SDL=1` |
| `MODO_GRAFICO_SDL` | ✅ Activado | |

Activa el renderizado en **ventana gráfica SDL2 (redimensionable y auto-escalable)**.

```bash
make test1 SDL=1
./scheduler_process cases/caso1
```

---

#### ④ Versión Completa (SDL2 + Power Pellets)

| Macro | Estado | Atajo Modular |
|---|---|---|
| `ENABLE_POWER_PELLETS` | ✅ Activado | `make test1 SDL=1 POWER=1` |
| `MODO_GRAFICO_SDL` | ✅ Activado | |

La **experiencia arcade completa** que activa simultáneamente gráficos SDL2 y poderes.

```bash
make test1 SDL=1 POWER=1
./scheduler_process cases/caso4
```

---

## 5. Registro Completo de Concurrencia e IPC

A continuación se detalla el inventario riguroso de todos los hilos POSIX, semáforos, mutex y variables de condición implementados por componente:

### 🧵 Hilos POSIX (`pthread_t`) por Proceso
1. **Planificador (`scheduler_process.c` - P0)**:
   - `main`: Hilo principal de inicialización y sincronización general.
   - `p0_tick_thread`: Generador del reloj maestro (ticks del juego).
   - `p0_scheduler_thread`: Árbitro Round-Robin y asignador de turnos de CPU.
   - `p0_signal_thread`: Receptor asíncrono de comandos y señales directas.
2. **Pac-Man (`pacman_process.c` - P1)**:
   - `main`: Hilo principal de control P1.
   - `p1_movement_reader`: Productor que lee las instrucciones de movimiento (archivo/pipe).
   - `p1_movement_executor`: Consumidor que procesa físicas y actualiza la posición del jugador.
   - `p1_pacman_publisher`: Publicador del estado hacia la memoria compartida.
3. **Enemigos (`enemy_process.c` - P2)**:
   - `main`: Hilo principal de control P2.
   - `p2_controller_thread`: Controlador general y receptor del turno de CPU.
   - `p2_tracker_thread`: Evaluador de las coordenadas del jugador.
   - `p2_collision_thread`: Monitor de colisiones físicas entre Pac-Man y fantasmas.
   - `p2_ghost_thread` (x4 hilos): Inteligencia artificial concurrente independiente para `Blinky`, `Pinky`, `Inky` y `Clyde`.
4. **Renderizador (`renderer_process.c` - P3)**:
   - `main`: Bucle continuo de renderizado (terminal ANSI o motor SDL2).

### 🚦 Semáforos POSIX en Memoria Compartida (`sem_t` en `shared.h`)
- `sem_tick_start`: Dispara el inicio de un nuevo tick cronometrado.
- `sem_scheduler_start`: Despierta al hilo planificador.
- `sem_signal_start`: Sincroniza la recepción de señales externas.
- `sem_pacman_turn`: Otorga el quantum de ejecución al proceso Pac-Man.
- `sem_enemy_turn`: Otorga el quantum de ejecución al proceso Fantasmas.
- `sem_turn_finished`: Notifica al planificador la culminación de un turno de CPU.
- `sem_check_collision`: Ordena al hilo de colisiones realizar el cálculo físico.
- `sem_collision_checked`: Confirma al planificador el término del chequeo de impacto.
- `sem_renderer_turn`: Autoriza al renderizador a refrescar la pantalla.
- `sem_renderer_done`: Confirma que el dibujo del fotograma ha terminado.
- `sem_ready_done`: Sincronización de la pantalla inicial "READY!".

### 🔒 Mutex POSIX (`pthread_mutex_t`)
- **En Memoria Compartida (`shared.h`)**:
  - `mutex_game_state`: Candado global sobre el estado general (ticks, game over, score, vidas, grilla).
  - `mutex_pacman_state`: Candado sobre las coordenadas e historial de movimientos del Pac-Man.
  - `mutex_ghost_state`: Candado sobre coordenadas, orientaciones y temporizadores de los 4 fantasmas.
  - `mutex_mailboxes`: Candado para lectura/escritura en buzones de comunicación IPC.
  - `mutex_collisions`: Candado para reportar muertes y actualizar el *kill feed*.
- **Internos de Procesos**:
  - `p1_mutex_buffer` (`pacman_process.c`): Candado del búfer circular interno productor-consumidor.
  - `p2_mutex_ghosts` (`enemy_process.c`): Candado interno de sincronización de hilos de fantasmas.
  - `p2_mutex_local` (`enemy_process.c`): Candado de variables locales de control IA.

### 🛎️ Variables de Condición (`pthread_cond_t`)
- **En `pacman_process.c`**:
  - `p1_cond_not_empty`: Bloquea al consumidor hasta que haya comandos en el búfer circular.
  - `p1_cond_not_full`: Bloquea al lector cuando el búfer circular alcanza su capacidad máxima.

---

## Estructura del Repositorio

```
pacman_posix/
├── shared.h                  # Definiciones globales, estructuras IPC y macros de configuración
├── scheduler_process.c       # Proceso planificador (Round-Robin, gestión de casos)
├── pacman_process.c          # Proceso Pac-Man (movimiento y puntuación)
├── enemy_process.c           # Proceso de enemigos (comportamiento de fantasmas)
├── renderer_process.c        # Proceso renderizador (ANSI / SDL2)
├── ipc_benchmark.c           # Benchmark de IPC (SHM vs Pipes vs Archivos)
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

# 2. Compilar modularmente con atajos (Combinaciones libres de SDL=1 y POWER=1)
make test1 SDL=1          # Caso Ideal en modo Gráfico SDL2
make test1 POWER=1        # Caso Ideal con Power Pellets activados
make test2 SDL=1 POWER=1  # Test sin sincronización con Gráficos y Poderes

# 3. Pruebas de Estrés Integradas (Sin interfaz visual, 100 Ejecuciones x 100,000 Iteraciones por turno)
make test10               # Valida 100% de estabilidad y precisión con Mutex POSIX activo
make test11               # Demuestra pérdida masiva de datos (Race Conditions) al apagar Mutex
```

---

*Proyecto desarrollado para el curso de Sistemas Operativos. Implementación basada en estándares POSIX — IEEE Std 1003.1.*
