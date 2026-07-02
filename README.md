<div align="center">

# Pac-Man POSIX Concurrent

**Proyecto de Sistemas Computacionales — Concurrencia, IPC y Planificación de Procesos**

![Language](https://img.shields.io/badge/C-C11-blue?logo=c&logoColor=white)
![API](https://img.shields.io/badge/API-POSIX-orange)
![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20WSL2-lightgrey)
![Renderer](https://img.shields.io/badge/renderer-ANSI%20%7C%20SDL2-green)

**Integrantes:** Quispe Curay Carlos Alith · Loyola Guevara Andrea · Pérez del Aguila Mayorga Amira  
**Docente:** Adanaqué, Luz Antuanet — **Curso:** Sistemas Computacionales — UTEC

</div>

---

## Descripción General

Simulación del videojuego Pac-Man construida exclusivamente con primitivas POSIX de bajo nivel: cuatro procesos independientes (`fork`/`exec`) que se comunican únicamente a través de un segmento de memoria compartida (`shm_open` + `mmap`) en `/pacman_shm_game`.

| Proceso | Archivo | Responsabilidad |
|---|---|---|
| **P0 — Planificador** | `scheduler_process.c` | Round-Robin entre P1 y P2; reloj maestro; recolecta métricas de kernel vía `getrusage()` |
| **P1 — Pac-Man** | `pacman_process.c` | Movimiento del jugador con patrón productor-consumidor (3 hilos internos) |
| **P2 — Enemigos** | `enemy_process.c` | 4 hilos de fantasmas concurrentes (Blinky, Pinky, Inky, Clyde) |
| **P3 — Renderizador** | `renderer_process.c` | Frame a frame en terminal ANSI o ventana SDL2; menú interactivo al finalizar |

Toda la arquitectura compartida (structs, semáforos, mutex, macros) está centralizada en `shared.h`.

---

## Requisitos

- Ubuntu 22.04+ / WSL2 con Ubuntu
- GCC 11+ con soporte C11
- `libsdl2-dev`, `libsdl2-image-dev`, `libsdl2-ttf-dev` (solo modo SDL2)

```bash
sudo apt update && sudo apt install build-essential libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev
```

---

## Compilación

```bash
make clean && make
```

> Ejecutar `make clean && make` obligatoriamente después de cambiar cualquier macro del preprocesador.

---

## Casos de Prueba

```bash
./scheduler_process cases/caso1   # Round-Robin base
./scheduler_process cases/caso2   # Juego corto (convergencia rápida)
./scheduler_process cases/caso3   # SET_PRIORITY dinámico a mitad de partida
./scheduler_process cases/caso4   # Power Pellets (requiere make test14 o test15)
```

---

## Interruptores de Arquitectura

| Macro | Efecto |
|---|---|
| `ENABLE_POWER_PELLETS` | Habilita pellets y modo cacería — requerido para caso4 |
| `MODO_GRAFICO_SDL` | Cambia renderer de ANSI a ventana SDL2 |
| `DISABLE_SYNC` | Desactiva mutex compartidos → demuestra race conditions |
| `ONLY_SEMAPHORES` | Desactiva solo mutex, mantiene semáforos |
| `ONLY_MUTEX` | Desactiva solo semáforos → colapso del scheduler |
| `GHOSTS_FIRST_PRIORITY` | Fantasmas con prioridad mayor que Pac-Man |
| `P0_LOWEST_PRIORITY` | Scheduler con prioridad mínima |
| `BUFFER_SIZE=N` | Tamaño del buffer circular de P1 (default: 5) |
| `USE_SYSCALL_WRITE` | Usa `write()` en lugar de `printf()` — mide overhead de syscall |
| `HEADLESS` | Sin renderizador P3 — modo stress/benchmark puro |

---

## Suite de Tests (make test1–test15)

`make testN` compila con la configuración exacta. Ejecutar con `./scheduler_process cases/casoX`.

| Target | Configuración | Resultado esperado |
|---|---|---|
| `make test1` | ANSI + sync completo | **100% integridad** — baseline de referencia |
| `make test2` | SDL2 + sync completo | 100% integridad; overhead de renderer: +150 MB RAM en P3 |
| `make test3` | ANSI + `DISABLE_SYNC` | **~50% integridad** — race conditions con pérdida masiva de ops |
| `make test4` | SDL2 + `DISABLE_SYNC` | ~50% integridad con renderer SDL2 |
| `make test5` | `ONLY_SEMAPHORES` | ~50–75% — los semáforos solos no protegen datos compartidos |
| `make test6` | `ONLY_MUTEX` | **0% — colapso inmediato** sin semáforos de turno |
| `make test7` | `GHOSTS_FIRST_PRIORITY` | Inversión de prioridad observable en integridad y ticks |
| `make test8` | `P0_LOWEST_PRIORITY` | Scheduler deprimido — efecto similar a test7 |
| `make test9` | `BUFFER_SIZE=1` | 100% integridad; más cambios de contexto por buffer saturado |
| `make test10` | `BUFFER_SIZE=20` | ~93% — race al vaciar buffer grande al finalizar partida |
| `make test11` | `USE_SYSCALL_WRITE` | 100% integridad; CPU kernel notablemente más alto |
| `make test12` | `HEADLESS` + sync | **100% acumulado** en 100 partidas × 500k ops/hilo |
| `make test13` | `HEADLESS` + `DISABLE_SYNC` | **~52% acumulado** — pérdida de millones de ops demostrada |
| `make test14` | Power Pellets + ANSI | 100% integridad — caso4 con modo cacería activo |
| `make test15` | Power Pellets + SDL2 | 100% integridad — confirma independencia del renderer |

### Fórmula de integridad

```
integridad (%) = ops_stress_registradas / ops_stress_esperadas × 100
```

- **100%** → sincronización perfecta
- **< 100%** → race conditions; escrituras concurrentes se solapan
- **0%** → colapso antes del primer tick

---

## Script Automatizado

```bash
./run_all_tests.sh          # interactivo (pausa entre tests)
./run_all_tests.sh < /dev/null   # no interactivo (CI/automatización)
```

Ejecuta los 15 tests y guarda los resultados en `data/resultados_tests.csv`.

### Columnas del CSV

| Columna | Descripción |
|---|---|
| `test_num`, `test_desc`, `caso` | Identificación del test y caso |
| `sim_time_ms` | Tiempo interno de simulación (ms) |
| `wall_clock_s` | Tiempo real de pared (s) |
| `cpu_user_s`, `cpu_kernel_s` | CPU en modo usuario y kernel (s) |
| `p0_rss_kb`…`p3_rss_kb` | RAM pico por proceso (KB) |
| `max_rss_proceso`, `max_rss_kb` | Proceso con mayor consumo y su valor |
| `mem_total_kb` | Estimación de memoria total del sistema |
| `ctx_vol`, `ctx_invol` | Cambios de contexto voluntarios e involuntarios |
| `vol_ratio_pct` | Fracción voluntaria sobre total (%) |
| `ops_actual`, `ops_esperadas` | Operaciones registradas vs esperadas |
| `integridad_pct` | Integridad de datos (%) |
| `coord_cost` | Costo de coordinación por operación (ctx/op) |
| `kernel_per_ctx` | Tiempo kernel por cambio de contexto (s/ctx) |
| `throughput_ops_s` | Operaciones por segundo |
| `render_errors` | Frames con salto de posición detectado |
| `ratio_parallelismo` | CPU total / tiempo real (> 1.0 = múltiples núcleos) |

---

## Benchmark IPC

```bash
make ipc_benchmark && ./ipc_benchmark
```

Compara tres mecanismos en 10 rondas × 100 000 operaciones:

| Mecanismo | Throughput promedio |
|---|---|
| **POSIX Shared Memory (mmap)** | ~24 000 ops/seg |
| POSIX Pipes (read/write) | ~17 400 ops/seg |
| Archivo en disco (lseek) | ~17 000 ops/seg |

`mmap` es ~1.4× más rápido que las alternativas — justificación empírica de la elección de IPC.

---

## Primitivas POSIX del Proyecto

### Hilos por proceso

| Proceso | Hilos |
|---|---|
| P0 | `p0_tick_thread`, `p0_scheduler_thread`, `p0_signal_thread` |
| P1 | `p1_movement_reader`, `p1_movement_executor`, `p1_pacman_publisher` |
| P2 | `p2_controller_thread`, `p2_tracker_thread`, `p2_collision_thread`, `p2_ghost_thread` ×4 |
| P3 | hilo principal (bucle de render) |

### Semáforos en memoria compartida (`sem_t`)

```
sem_tick_start → sem_scheduler_start → sem_signal_start
→ sem_pacman_turn / sem_enemy_turn → sem_turn_finished
→ sem_check_collision → sem_collision_checked
→ sem_renderer_turn → sem_renderer_done
```

`sem_ready_done` sincroniza el arranque de P3 con P0.

### Mutex (`pthread_mutex_t`)

**En memoria compartida** (bypasseables con `DISABLE_SYNC`/`ONLY_SEMAPHORES`):  
`mutex_game_state` · `mutex_pacman_state` · `mutex_ghost_state` · `mutex_mailboxes` · `mutex_collisions`

**Locales a cada proceso** (siempre activos):  
`p1_mutex_buffer` · `p2_mutex_ghosts` · `p2_mutex_local`

### Variables de condición (`pthread_cond_t`)

`p1_cond_not_empty` (despierta al consumidor) · `p1_cond_not_full` (bloquea al productor)

---

## Estructura del Repositorio

```
mi_pacman_sandbox/
├── shared.h                  # Structs IPC, semáforos, mutex y macros
├── scheduler_process.c       # P0: planificador + métricas kernel
├── pacman_process.c          # P1: Pac-Man, productor-consumidor
├── enemy_process.c           # P2: 4 hilos de fantasmas
├── renderer_process.c        # P3: renderer ANSI/SDL2
├── ipc_benchmark.c           # Benchmark empírico mmap vs pipes vs archivo
├── run_all_tests.sh          # Ejecuta los 15 tests → data/resultados_tests.csv
├── Makefile
├── cases/
│   ├── caso1/                # Round-Robin base
│   ├── caso2/                # Convergencia rápida
│   ├── caso3/                # SET_PRIORITY dinámico
│   └── caso4/                # Power Pellets
├── data/
│   └── resultados_tests.csv  # Datos CSV generados por run_all_tests.sh
└── Images/                   # Gráficas generadas (PNG)
```

---

## Comandos Esenciales

```bash
# Compilar
make clean && make

# Un test
make test1 && ./scheduler_process cases/caso1

# Race conditions
make test3 && ./scheduler_process cases/caso1   # ~50% integridad
make test6 && ./scheduler_process cases/caso1   # colapso en < 1 ms

# Stress (100 partidas, sin delay)
make test12 && ./scheduler_process cases/caso1  # 100% acumulado
make test13 && ./scheduler_process cases/caso1  # ~52% acumulado

# Power Pellets
make test14 && ./scheduler_process cases/caso4

# Todos los tests → CSV
./run_all_tests.sh

# Benchmark IPC
make ipc_benchmark && ./ipc_benchmark
```

---

*Implementación basada en estándares POSIX — IEEE Std 1003.1. Curso IS2021 — UTEC.*
