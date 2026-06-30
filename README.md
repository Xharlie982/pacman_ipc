<div align="center">

# Pac-Man POSIX Concurrent

**Proyecto de Sistemas Operativos — Concurrencia, IPC y Planificación de Procesos**

![Language](https://img.shields.io/badge/C-C11-blue?logo=c&logoColor=white)
![API](https://img.shields.io/badge/API-POSIX-orange)
![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20WSL2-lightgrey)
![Renderer](https://img.shields.io/badge/renderer-ANSI%20%7C%20SDL2-green)
![Branch](https://img.shields.io/badge/branch-main-brightgreen)

</div>

---

## Índice

| | |
|---|---|
| [🎮 Descripción General](#-descripción-general) | [🧪 Suite de Tests Científicos](#-suite-de-tests-científicos-make-test1test15) |
| [📦 Instalación de Dependencias](#-1-instalación-de-dependencias) | [🤖 Script Automatizado](#-6-script-de-ejecución-automatizada) |
| [🔨 Compilación](#-2-compilación-del-proyecto) | [📊 Benchmark IPC](#-7-benchmark-ipc--justificación-empírica-de-mmap) |
| [▶️ Casos de Prueba](#️-3-ejecución-de-los-casos-de-prueba) | [🔒 Primitivas POSIX](#-8-registro-completo-de-primitivas-posix) |
| [⚙️ Interruptores de Arquitectura](#️-4-interruptores-de-arquitectura-macros-de-preprocesador) | [⚡ Comandos Esenciales](#-resumen-de-comandos-esenciales) |

---

## 🎮 Descripción General

Este proyecto implementa una simulación del videojuego Pac-Man utilizando exclusivamente primitivas POSIX de bajo nivel: procesos independientes (`fork`/`exec`), memoria compartida (`shm_open`), semáforos (`sem_open`) y mutex POSIX. El sistema se compone de cuatro procesos independientes que cooperan en tiempo real a través de un segmento de memoria compartida (`/pacman_shm_game`):

| Proceso | Archivo fuente | Responsabilidad |
|---|---|---|
| **P0 — Planificador** | `scheduler_process.c` | Orquesta Round-Robin, gestiona el reloj maestro y recolecta 8 métricas de kernel vía `getrusage()` |
| **P1 — Pac-Man** | `pacman_process.c` | Controla el movimiento del jugador mediante patrón productor-consumidor con hilos internos |
| **P2 — Enemigos** | `enemy_process.c` | Administra 4 hilos de fantasmas concurrentes (Blinky, Pinky, Inky, Clyde) |
| **P3 — Renderizador** | `renderer_process.c` | Dibuja cada frame en terminal ANSI o ventana SDL2 con menú interactivo al finalizar |

La arquitectura completa se define en **`shared.h`**, que centraliza estructuras de datos, semáforos, mutex y macros de configuración compartidas entre todos los procesos.

---

## 🖥️ Requisitos del Sistema

- **Sistema operativo:** Ubuntu 22.04 LTS o superior / WSL2 con Ubuntu
- **Compilador:** GCC 11 o superior con soporte C11 (`-std=c11`)
- **Bibliotecas opcionales:** SDL2, SDL2\_image, SDL2\_ttf (solo para modo gráfico)

---

## 📦 1. Instalación de Dependencias

```bash
sudo apt update
sudo apt install build-essential libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev
```

> **Nota WSL2:** Si usa el modo gráfico SDL2 desde Windows, necesita un servidor X11 activo (VcXsrv o X410) con la variable `DISPLAY` configurada.

---

## 🔨 2. Compilación del Proyecto

```bash
make clean && make
```

> **Importante:** Cada vez que cambie una macro del preprocesador, ejecute `make clean && make` obligatoriamente. Una compilación incremental sin limpiar puede producir un binario inconsistente.

---

## ▶️ 3. Ejecución de los Casos de Prueba

El directorio `cases/` contiene cuatro escenarios predefinidos:

### Caso 1 — Sincronización Base Round-Robin
```bash
./scheduler_process cases/caso1
```
Evalúa la sincronización fundamental. Round-Robin equitativo entre P1 y P2. Baseline de integridad al 100%.

### Caso 2 — Convergencia Rápida
```bash
./scheduler_process cases/caso2
```
Escenario donde el juego termina pronto (fantasmas alcanzan a Pac-Man). Permite observar comportamiento bajo tiempo de vida reducido.

### Caso 3 — Inversión de Prioridades con `SET_PRIORITY`
```bash
./scheduler_process cases/caso3
```
Activa la directiva `SET_PRIORITY` a mitad de la partida, cambiando dinámicamente las prioridades del planificador.

### Caso 4 — Modo Cacería y Power Pellets
```bash
make test14   # compila con ENABLE_POWER_PELLETS
./scheduler_process cases/caso4
```
> ⚠️ Requiere compilar con `ENABLE_POWER_PELLETS` activo (usar `make test14` o `make test15`).

---

## ⚙️ 4. Interruptores de Arquitectura (Macros de Preprocesador)

| Macro / Flag | Efecto |
|---|---|
| `ENABLE_POWER_PELLETS` | Habilita power pellets y modo cacería (requerido para caso4) |
| `MODO_GRAFICO_SDL` | Cambia renderer de ANSI a ventana SDL2 |
| `DISABLE_SYNC` | Desactiva mutex compartidos → demuestra race conditions |
| `ONLY_SEMAPHORES` | Desactiva solo mutex, mantiene semáforos |
| `ONLY_MUTEX` | Desactiva solo semáforos → crash inmediato del scheduler |
| `GHOSTS_FIRST_PRIORITY` | Fantasmas reciben todos los turnos de CPU |
| `P0_LOWEST_PRIORITY` | Planificador con prioridad más baja |
| `BUFFER_SIZE=N` | Cambia tamaño del buffer circular de P1 (default: 10) |
| `USE_SYSCALL_WRITE` | Usa `write()` directo en lugar de `printf()` |
| `STRESS_TEST` | Activa modo headless: 100 ejecuciones × 0 ms de delay |

---

## 🧪 5. Suite de Tests Científicos (make test1–test15)

Cada target compila los binarios con la configuración exacta. Uso: `make testN && ./scheduler_process cases/casoX`

| Target | Configuración | Propósito científico |
|---|---|---|
| `make test1` | ANSI + sincronización completa | Baseline: integridad 100%, referencia para todos los demás |
| `make test2` | SDL2 + sincronización completa | Cuantifica overhead del renderer gráfico (+20% wall clock) |
| `make test3` | ANSI + `DISABLE_SYNC` | **Race conditions**: sin mutex → 56% de ops registradas en caso1 |
| `make test4` | SDL2 + `DISABLE_SYNC` | Idem con renderer SDL2; pérdida similar (~62%) |
| `make test5` | `ONLY_SEMAPHORES` | Solo semáforos no protegen datos: 58% integridad (≈ test3) |
| `make test6` | `ONLY_MUTEX` | Sin semáforos el scheduler colapsa: **0% en 0.6 ms** |
| `make test7` | `GHOSTS_FIRST_PRIORITY` | Inversión de prioridad: game over en ~6 ticks, 40% de ops |
| `make test8` | `P0_LOWEST_PRIORITY` | Scheduler con menor prioridad: mismo efecto que test7 |
| `make test9` | `BUFFER_SIZE=1` | Buffer mínimo: 100% integridad, más cambios de contexto |
| `make test10` | `BUFFER_SIZE=20` | Buffer grande: 93.3% — race condition al vaciar buffer en fin de partida |
| `make test11` | `USE_SYSCALL_WRITE` | write() vs printf(): más tiempo en modo kernel (CPU kernel sube) |
| `make test12` | `STRESS_TEST` (sync) | 100 partidas × 6M ops esperadas: **100% acumulado** — integridad perfecta a escala |
| `make test13` | `STRESS_TEST` + `DISABLE_SYNC` | 100 partidas × 6M ops: **52.6%** — pérdida de 285M ops por race conditions |
| `make test14` | Power Pellets + ANSI | 133.3% de ops: pellets añaden turnos extra de fantasmas |
| `make test15` | Power Pellets + SDL2 | 133.3% consistente — confirma independencia del renderer |

### Interpretación de la Integridad (%)

```
Integridad = ops_stress_registradas / ops_stress_esperadas × 100
```

- **100%** → sincronización perfecta, ningún dato perdido
- **<100%** → race conditions activos, escrituras concurrentes se solapan
- **0%** → crash antes del primer tick (sin semáforos de turno)
- **>100%** → power pellets generan turnos adicionales no contemplados en expected

---

## 🤖 6. Script de Ejecución Automatizada

```bash
./run_all_tests.sh
```

Ejecuta los 15 tests secuencialmente con pausa interactiva entre casos. Genera dos archivos:

- `resultados_tests.txt` — log legible con todas las métricas oficiales
- `resultados_tests.csv` — datos estructurados por filas para importar en Python/Excel

**Columnas del CSV:**
`test_num, test_desc, caso, sim_time_ms, wall_clock_s, cpu_user_s, cpu_kernel_s, max_rss_kb, ctx_vol, ctx_invol, ops_actual, ops_esperadas, integridad_pct, render_errors`

Al terminar los 15 tests, el script pregunta si ejecutar el benchmark IPC.

**Visualización de resultados (Google Colab):**
[Ver gráficas generadas con los datos del CSV](https://colab.research.google.com/drive/1ggvqJO4pFeRReUGOCMlg9PGrnv4Wsj0J?usp=sharing)

---

## 📊 7. Benchmark IPC — Justificación Empírica de mmap()

```bash
make ipc_benchmark && ./ipc_benchmark
```

Compara tres mecanismos de IPC en 10 ejecuciones × 100,000 operaciones cada una:

| Mecanismo | Throughput promedio | Latencia promedio |
|---|---|---|
| **POSIX Shared Memory (mmap)** | 23,923 ops/seg | 4,180 ms |
| POSIX Pipes (read/write) | 17,394 ops/seg | 5,749 ms |
| Archivo en disco (lseek) | 17,041 ops/seg | 5,868 ms |

**mmap es 1.37× más rápido que pipes y 1.40× más rápido que archivo en disco**, justificando científicamente la elección de memoria compartida como mecanismo IPC del proyecto.

---

## 🔒 8. Registro Completo de Primitivas POSIX

### Hilos POSIX (`pthread_t`) por Proceso

| Proceso | Hilos |
|---|---|
| P0 Planificador | `main`, `p0_tick_thread`, `p0_scheduler_thread`, `p0_signal_thread` |
| P1 Pac-Man | `main`, `p1_movement_reader` (productor), `p1_movement_executor` (consumidor), `p1_pacman_publisher` |
| P2 Enemigos | `main`, `p2_controller_thread`, `p2_tracker_thread`, `p2_collision_thread`, `p2_ghost_thread` ×4 |
| P3 Renderizador | `main` (bucle de render) |

### Semáforos POSIX en Memoria Compartida (`sem_t`)

`sem_tick_start` → `sem_scheduler_start` → `sem_pacman_turn` / `sem_enemy_turn` → `sem_turn_finished` → `sem_check_collision` / `sem_collision_checked` → `sem_renderer_turn` → `sem_renderer_done`

Más: `sem_signal_start`, `sem_ready_done`

### Mutex POSIX (`pthread_mutex_t`)

**En memoria compartida (bypasseables con `DISABLE_SYNC`):**
`mutex_game_state`, `mutex_pacman_state`, `mutex_ghost_state`, `mutex_mailboxes`, `mutex_collisions`

**Locales a cada proceso (siempre activos):**
`p1_mutex_buffer` (buffer circular P1), `p2_mutex_ghosts` (sincroniza acceso a posiciones de fantasmas), `p2_mutex_local`

### Variables de Condición (`pthread_cond_t`) en P1

`p1_cond_not_empty` (bloquea consumidor), `p1_cond_not_full` (bloquea productor cuando buffer lleno)

---

## 📁 Estructura del Repositorio

```
mi_pacman_sandbox/
├── shared.h                  # Estructuras IPC, semáforos, mutex y macros
├── scheduler_process.c       # P0: planificador Round-Robin + métricas kernel
├── pacman_process.c          # P1: Pac-Man, productor-consumidor
├── enemy_process.c           # P2: fantasmas, 4 hilos concurrentes + stress loop
├── renderer_process.c        # P3: renderer ANSI/SDL2 + menú fin de partida
├── ipc_benchmark.c           # Benchmark empírico: mmap vs pipes vs archivo
├── run_all_tests.sh          # Script automatizado: 15 tests → .txt + .csv
├── Makefile                  # 15 targets de test + run_benchmark
├── .gitignore
└── cases/
    ├── caso1/                # Round-Robin base
    ├── caso2/                # Convergencia rápida
    ├── caso3/                # SET_PRIORITY dinámico
    └── caso4/                # Power Pellets (requiere ENABLE_POWER_PELLETS)
```

---

## ⚡ Resumen de Comandos Esenciales

```bash
# Compilar desde cero
make clean && make

# Ejecutar un test (compilar + correr)
make test1 && ./scheduler_process cases/caso1
make test2 && ./scheduler_process cases/caso1   # SDL2

# Demostrar race conditions
make test3 && ./scheduler_process cases/caso1   # sin mutex → ~56% integridad
make test6 && ./scheduler_process cases/caso1   # sin semáforos → crash en 0.6 ms

# Pruebas de estrés (100 partidas, headless, sin delay)
make test12 && ./scheduler_process cases/caso1  # con sync → 100% acumulado
make test13 && ./scheduler_process cases/caso1  # sin sync → 52.6% acumulado

# Power pellets
make test14 && ./scheduler_process cases/caso4  # ANSI, 133% ops
make test15 && ./scheduler_process cases/caso4  # SDL2, 133% ops

# Ejecutar todos los tests automáticamente
./run_all_tests.sh

# Benchmark IPC
make ipc_benchmark && ./ipc_benchmark
```

---

*Proyecto desarrollado para el curso de Sistemas Operativos. Implementación basada en estándares POSIX — IEEE Std 1003.1.*
