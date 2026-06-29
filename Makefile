# =========================================================================
# MAKEFILE — PAC-MAN IPC MULTIPROCESO
# Compilación por defecto: Caso Ideal (Test 1).
# Para ejecutar tests manuales, puedes pasar EXTRA_CFLAGS por consola:
# Ejemplo: make clean && make EXTRA_CFLAGS="-DDISABLE_SYNC"
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
# ATAJOS PARA COMPILAR TESTS MANUALMENTE (Puedes combinar con SDL=1 y POWER=1)
# Ejemplo: make test1 SDL=1 POWER=1
# =========================================================================
test1:
	$(MAKE) clean && $(MAKE)

test2:
	$(MAKE) clean && $(MAKE) EXTRA_CFLAGS="-DDISABLE_SYNC"

test3:
	$(MAKE) clean && $(MAKE) EXTRA_CFLAGS="-DONLY_SEMAPHORES"

test4:
	$(MAKE) clean && $(MAKE) EXTRA_CFLAGS="-DONLY_MUTEX"

test5:
	$(MAKE) clean && $(MAKE) EXTRA_CFLAGS="-DBUFFER_SIZE=1"

test6:
	$(MAKE) clean && $(MAKE) EXTRA_CFLAGS="-DBUFFER_SIZE=20"

test7:
	$(MAKE) clean && $(MAKE) EXTRA_CFLAGS="-DUSE_SYSCALL_WRITE"

test8:
	$(MAKE) clean && $(MAKE) EXTRA_CFLAGS="-DGHOSTS_FIRST_PRIORITY"

test9:
	$(MAKE) clean && $(MAKE) EXTRA_CFLAGS="-DP0_LOWEST_PRIORITY"

test10:
	$(MAKE) clean && $(MAKE) STRESS=1

test11:
	$(MAKE) clean && $(MAKE) EXTRA_CFLAGS="-DDISABLE_SYNC" STRESS=1

test12:
	$(MAKE) clean && $(MAKE) EXTRA_CFLAGS="-DENABLE_POWER_PELLETS"

benchmark_test:
	$(MAKE) clean && $(MAKE)
	./ipc_benchmark
