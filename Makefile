# =========================================================================
# MAKEFILE — PAC-MAN IPC MULTIPROCESO
#
# USO RÁPIDO:
#   make testN                → compila y deja binarios listos para el test N
#   ./scheduler_process cases/casoX  → ejecuta una partida con los binarios del testN
#
# FLAGS EXTRA COMBINABLES:
#   SDL=1    → activa renderer SDL2 (en lugar de ANSI)
#   POWER=1  → activa power pellets (solo para caso4)
#   STRESS=1 → modo estrés (HEADLESS, 100 corridas, sin delay)
# =========================================================================
CC = gcc
CFLAGS ?= -Wall -Wextra -pthread -g
EXTRA_CFLAGS ?=

COMBO_FLAGS := $(EXTRA_CFLAGS)
ifeq ($(SDL),1)
    COMBO_FLAGS += -DMODO_GRAFICO_SDL
endif
ifeq ($(POWER),1)
    COMBO_FLAGS += -DENABLE_POWER_PELLETS
endif
ifeq ($(STRESS),1)
    COMBO_FLAGS += -DSTRESS_TEST
endif

ALL_CFLAGS = $(CFLAGS) $(COMBO_FLAGS)
LDFLAGS = -lrt -pthread -lSDL2 -lSDL2_image -lSDL2_ttf

ALL = scheduler_process pacman_process enemy_process renderer_process ipc_benchmark

all: $(ALL)

scheduler_process: scheduler_process.c
	$(CC) $(ALL_CFLAGS) -o $@ $< $(LDFLAGS)

pacman_process: pacman_process.c
	$(CC) $(ALL_CFLAGS) -o $@ $< $(LDFLAGS)

enemy_process: enemy_process.c
	$(CC) $(ALL_CFLAGS) -o $@ $< $(LDFLAGS)

renderer_process: renderer_process.c
	$(CC) $(ALL_CFLAGS) -o $@ $< $(LDFLAGS)

ipc_benchmark: ipc_benchmark.c
	$(CC) $(ALL_CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(ALL)

# =========================================================================
# PARTE 1 — CASO BASE IDEAL
#   Sincronización completa (mutex + semáforos + variables de condición).
#   Ejecutar con: ./scheduler_process cases/caso1  (o caso2, caso3)
# =========================================================================
test1:
	$(MAKE) clean && $(MAKE)

test2:
	$(MAKE) clean && $(MAKE) SDL=1

# =========================================================================
# PARTE 2 — ABLACIÓN DE SINCRONIZACIÓN
#   Demuestran la necesidad de cada mecanismo de sync.
#   test3 (ANSI) y test4 (SDL2) muestran race conditions; los errores
#   gráficos en SDL2 son más espectaculares que en la terminal.
# =========================================================================
test3:
	$(MAKE) clean && $(MAKE) EXTRA_CFLAGS="-DDISABLE_SYNC"

test4:
	$(MAKE) clean && $(MAKE) EXTRA_CFLAGS="-DDISABLE_SYNC" SDL=1

test5:
	$(MAKE) clean && $(MAKE) EXTRA_CFLAGS="-DONLY_SEMAPHORES"

test6:
	$(MAKE) clean && $(MAKE) EXTRA_CFLAGS="-DONLY_MUTEX"

# =========================================================================
# PARTE 3 — VARIACIONES DE PLANIFICACIÓN Y CONFIGURACIÓN (ANSI)
# =========================================================================
test7:
	$(MAKE) clean && $(MAKE) EXTRA_CFLAGS="-DGHOSTS_FIRST_PRIORITY"

test8:
	$(MAKE) clean && $(MAKE) EXTRA_CFLAGS="-DP0_LOWEST_PRIORITY"

test9:
	$(MAKE) clean && $(MAKE) EXTRA_CFLAGS="-DBUFFER_SIZE=1"

test10:
	$(MAKE) clean && $(MAKE) EXTRA_CFLAGS="-DBUFFER_SIZE=20"

test11:
	$(MAKE) clean && $(MAKE) EXTRA_CFLAGS="-DUSE_SYSCALL_WRITE"

# =========================================================================
# PARTE 4 — VALIDACIÓN DE INTEGRIDAD (estrés, 100 corridas, HEADLESS)
#   test12: CON sync → stress_counter/expected ≈ 100%
#   test13: SIN sync → pérdida masiva de datos
# =========================================================================
test12:
	$(MAKE) clean && $(MAKE) STRESS=1

test13:
	$(MAKE) clean && $(MAKE) EXTRA_CFLAGS="-DDISABLE_SYNC" STRESS=1

# =========================================================================
# PARTE 5 — BONUS (ENABLE_POWER_PELLETS)
#   Ejecutar con: ./scheduler_process cases/caso4
# =========================================================================
test14:
	$(MAKE) clean && $(MAKE) POWER=1

test15:
	$(MAKE) clean && $(MAKE) SDL=1 POWER=1

# =========================================================================
# HERRAMIENTA: BENCHMARK IPC
#   Solo ejecuta ./ipc_benchmark. Compila el binario si no existe.
#   No hace make clean — si necesitas recompilar: make clean && make primero.
# =========================================================================
run_benchmark: ipc_benchmark
	./ipc_benchmark

# =========================================================================
# MEDICIÓN DE TIEMPO (ejecutar manualmente en WSL):
#   make test12 && time ./scheduler_process cases/caso1
#   make test12 && time ./scheduler_process cases/caso2
#   make test12 && time ./scheduler_process cases/caso3
#
#   make test13 && time ./scheduler_process cases/caso1
#   make test13 && time ./scheduler_process cases/caso2
#   make test13 && time ./scheduler_process cases/caso3
#
# Alternativa con /usr/bin/time para output más detallado:
#   make test12 && /usr/bin/time -v ./scheduler_process cases/caso1
# =========================================================================
