# =========================================================================
# MAKEFILE — PAC-MAN IPC MULTIPROCESO
#
# USO RÁPIDO:
#   make testN                → compila y deja binarios listos para el test N
#   ./scheduler_process cases/casoX  → ejecuta una partida con los binarios del testN
#
# FLAGS EXTRA COMBINABLES:
#   SDL=1     → activa renderer SDL2 (en lugar de ANSI)
#   POWER=1   → activa power pellets (solo para caso4)
#   VISUAL=1  → 1 ejecución con delay 400ms, menú M/V interactivo (demo)
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
ifeq ($(VISUAL),1)
    COMBO_FLAGS += -DVISUAL -DTICK_DELAY_MS=400
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

# -- Test 1-2: caso base (sync completo, ANSI / SDL2) --
test1:
	$(MAKE) clean && $(MAKE)

test2:
	$(MAKE) clean && $(MAKE) SDL=1

# -- Tests 3-6: ablación de sincronización --
test3:
	$(MAKE) clean && $(MAKE) EXTRA_CFLAGS="-DDISABLE_SYNC"

test4:
	$(MAKE) clean && $(MAKE) EXTRA_CFLAGS="-DDISABLE_SYNC" SDL=1

test5:
	$(MAKE) clean && $(MAKE) EXTRA_CFLAGS="-DONLY_SEMAPHORES"

test6:
	$(MAKE) clean && $(MAKE) EXTRA_CFLAGS="-DONLY_MUTEX"

# -- Tests 7-11: prioridad y configuración --
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

# -- Tests 12-13: headless (sin P3); test12=sync, test13=race conditions --
test12:
	$(MAKE) clean && $(MAKE) EXTRA_CFLAGS="-DHEADLESS"

test13:
	$(MAKE) clean && $(MAKE) EXTRA_CFLAGS="-DDISABLE_SYNC -DHEADLESS"

# -- Tests 14-15: power pellets (requiere caso4) --
test14:
	$(MAKE) clean && $(MAKE) POWER=1

test15:
	$(MAKE) clean && $(MAKE) SDL=1 POWER=1

# -- Benchmark IPC --
run_benchmark: ipc_benchmark
	./ipc_benchmark

# -- Utilidades --
run_tests:
	chmod +x run_all_tests.sh && ./run_all_tests.sh

run_tests_auto:
	chmod +x run_all_tests.sh && ./run_all_tests.sh < /dev/null

clean_csv:
	rm -f data/resultados_tests.csv

