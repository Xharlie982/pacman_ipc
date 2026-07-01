#ifndef SHARED_H
#define SHARED_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>
#include <sys/resource.h>

#define MAX_TICKS 38
#define SHM_NAME "/pacman_shm_game"
#ifndef TICK_DELAY_MS
#define TICK_DELAY_MS 0
#endif
// =========================================================================
// INTERRUPTORES MANUALES PARA PRUEBAS Y BENCHMARKS (TESTS 2 al 9)
// Por defecto están comentados con "//" para ejecutar el CASO IDEAL (Test 1).
// Si compilas manualmente, puedes descomentar el que desees probar.
// =========================================================================
// #define DISABLE_SYNC          // Test 2: Desactiva mutex (fuerza colisiones)
// #define ONLY_SEMAPHORES       // Test 3: Sincronización solo semáforos
// #define ONLY_MUTEX            // Test 4: Sincronización solo mutex
// #define BUFFER_SIZE 1         // Test 5: Buffer circular tamaño mínimo (1)
// #define BUFFER_SIZE 20        // Test 6: Buffer circular tamaño grande (20)
// #define USE_SYSCALL_WRITE     // Test 7: Usar syscall write() en vez de printf()
// #define GHOSTS_FIRST_PRIORITY // Test 8: Inversión de prioridad (fantasmas primero)
// #define P0_LOWEST_PRIORITY    // Test 9: Prioridad P0 más baja (P0 < P1 < P2)
// #define HEADLESS              // Test 10/11: Modo sin renderizado visual
// #define ENABLE_POWER_PELLETS  // Test 12: Habilitar píldoras de poder (caso4)

#ifdef VISUAL
    #define PROGRAM_RUNS 1
#else
    #define PROGRAM_RUNS 100
#endif
#define STRESS_OPS 500000

#ifndef BUFFER_SIZE
#define BUFFER_SIZE 5
#endif

static inline void print_consola(const char *mensaje) {
    if (mensaje) {
        ssize_t ret = write(STDOUT_FILENO, mensaje, strlen(mensaje));
        (void)ret;
    }
}



#define COLOR_RESET     "\033[0m"
#define COLOR_PACMAN    "\033[38;2;255;255;0m"
#define COLOR_BLINKY    "\033[38;2;255;0;0m"
#define COLOR_PINKY     "\033[38;2;255;184;255m"
#define COLOR_INKY      "\033[38;2;0;255;255m"
#define COLOR_CLYDE     "\033[38;2;255;184;82m"
#define COLOR_WALL      "\033[38;2;33;33;222m"
#define COLOR_WALL_CASO4 "\033[38;2;69;179;169m"
#define COLOR_PATH      "\033[38;2;110;110;110m"
#define COLOR_COLLISION "\033[38;2;255;255;255m"
#define COLOR_INFO      "\033[38;2;100;200;255m"
#define COLOR_SUCCESS   "\033[38;2;50;255;50m"
#define COLOR_ERROR     "\033[38;2;255;50;50m"

#define COLOR_POWER     "\033[38;2;255;215;0m" 
#define COLOR_SCARED    "\033[38;2;0;51;255m" 
#define COLOR_DEAD      "\033[90m" 

#define HIST_SEP " \033[38;2;255;150;0m|\033[0m "

typedef enum { CMD_UP, CMD_DOWN, CMD_LEFT, CMD_RIGHT, CMD_SET_PRIORITY, CMD_EOF, CMD_NONE } CommandType;

typedef struct {
    CommandType type;
    int value;
} Command;

typedef struct {
    char name[128];
    int tick;
    int active;
} KillEntry;

typedef struct {
    int global_tick; int max_ticks; int game_over; int pacman_lives;
    int pacman_x; int pacman_y; int pacman_old_x; int pacman_old_y; int pacman_score;
    int ghost_x[4]; int ghost_y[4]; int ghost_old_x[4]; int ghost_old_y[4];
    int pacman_init_x; int pacman_init_y; int ghost_init_x[4]; int ghost_init_y[4];
    
    int collision_detected; int collision_ghost_id; int collision_tick;
    int just_died; int killer_id; int death_count;
    
    KillEntry kill_feed[4];
    int kill_feed_count;
    
    char pacman_history[8192];
    char ghost_history[4][8192];
    
    int dots_eaten[20][20]; 
    int tombstones[20][20];
    
#ifdef ENABLE_POWER_PELLETS
    int power_ticks_left;
    int ghosts_eaten_combo;
    int ghost_dead_timer[4];
    int ghost_is_scared[4];
    int ghost_dead_x[4];     // NUEVO: X de la tumba
    int ghost_dead_y[4];     // NUEVO: Y de la tumba
    int power_pellets[20][20];
#endif

    int use_dynamic_priority;
    int p0_priority;
    int pacman_priority; int pending_pacman_priority; int pacman_priority_request_active;
    int enemy_priority; int pending_enemy_priority; int enemy_priority_request_active;
    
    char map_grid[20][20]; int map_width; int map_height;
    int is_caso4;
    int pacman_eof; int ghost_eof[4];
    int current_turn; int last_played_process;
    double sim_time_ms;
    double wall_clock_s;
    double user_cpu_s;
    double sys_cpu_s;
    long max_rss_kb;
    long vol_context_switches;
    long invol_context_switches;
    volatile long long stress_counter;
    long long expected_stress_ops;
    volatile long long render_errors;
    long p0_rss_kb;
    long p1_rss_kb;
    long p2_rss_kb;
    long p3_rss_kb;

    sem_t sem_tick_start, sem_scheduler_start, sem_signal_start, sem_pacman_turn, sem_enemy_turn;
    sem_t sem_turn_finished, sem_check_collision, sem_collision_checked, sem_renderer_turn, sem_renderer_done;
    sem_t sem_ready_done; 
    
    pthread_mutex_t mutex_game_state, mutex_pacman_state, mutex_ghost_state, mutex_mailboxes, mutex_collisions;
} SharedMemory;

extern SharedMemory *shm;

#if defined(DISABLE_SYNC) || defined(ONLY_SEMAPHORES)
  static inline int disabled_mutex_lock(pthread_mutex_t *m) {
      if ((void*)m < (void*)shm || (void*)m >= (void*)(shm + 1)) {
          return pthread_mutex_lock(m);
      }
      return 0;
  }
  static inline int disabled_mutex_unlock(pthread_mutex_t *m) {
      if ((void*)m < (void*)shm || (void*)m >= (void*)(shm + 1)) {
          return pthread_mutex_unlock(m);
      }
      return 0;
  }
  #define pthread_mutex_lock(m) disabled_mutex_lock(m)
  #define pthread_mutex_unlock(m) disabled_mutex_unlock(m)
#endif

#if defined(ONLY_MUTEX)
  static inline int disabled_sem_wait(sem_t *s) {
      if ((void*)s < (void*)shm || (void*)s >= (void*)(shm + 1)) {
          return sem_wait(s);
      }
      return 0;
  }
  static inline int disabled_sem_post(sem_t *s) {
      if ((void*)s < (void*)shm || (void*)s >= (void*)(shm + 1)) {
          return sem_post(s);
      }
      return 0;
  }
  #define sem_wait(s) disabled_sem_wait(s)
  #define sem_post(s) disabled_sem_post(s)
#endif

static inline Command parse_line(char *line) {
    Command cmd; cmd.type = CMD_EOF; cmd.value = 0;
    if (line == NULL) return cmd;
    while (*line && isspace((unsigned char)*line)) line++;
    if (*line == '\0') return cmd;
    if (strncmp(line, "SET_PRIORITY", 12) == 0) {
        char c = line[12];
        if (c == '\0' || isspace((unsigned char)c)) {
            if (sscanf(line, "SET_PRIORITY %d", &cmd.value) == 1) cmd.type = CMD_SET_PRIORITY;
        }
        return cmd;
    }
    if (strncmp(line, "UP", 2) == 0) {
        char c = line[2]; if (c == '\0' || isspace((unsigned char)c)) cmd.type = CMD_UP;
    } else if (strncmp(line, "DOWN", 4) == 0) {
        char c = line[4]; if (c == '\0' || isspace((unsigned char)c)) cmd.type = CMD_DOWN;
    } else if (strncmp(line, "LEFT", 4) == 0) {
        char c = line[4]; if (c == '\0' || isspace((unsigned char)c)) cmd.type = CMD_LEFT;
    } else if (strncmp(line, "RIGHT", 5) == 0) {
        char c = line[5]; if (c == '\0' || isspace((unsigned char)c)) cmd.type = CMD_RIGHT;
    }
    return cmd;
}

static inline void apply_movement(int *x, int *y, CommandType dir, char map[20][20], int w, int h) {
    int nx = *x, ny = *y;
    if (dir == CMD_UP) ny--; else if (dir == CMD_DOWN) ny++;
    else if (dir == CMD_LEFT) nx--; else if (dir == CMD_RIGHT) nx++;
    if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
        if (map[ny][nx] != 'X') { *x = nx; *y = ny; }
    }
}

static inline void append_history(char *history, Command cmd, int wall_hit) {
    char buffer[64] = {0}; char letter[4] = "";
    if (cmd.type == CMD_UP) strcpy(letter, "U");
    else if (cmd.type == CMD_DOWN) strcpy(letter, "D");
    else if (cmd.type == CMD_LEFT) strcpy(letter, "L");
    else if (cmd.type == CMD_RIGHT) strcpy(letter, "R");
    else if (cmd.type == CMD_SET_PRIORITY) sprintf(buffer, "SP%d", cmd.value);
    else return;

    if (cmd.type != CMD_SET_PRIORITY) {
        if (wall_hit) sprintf(buffer, "\033[38;2;155;89;182m%s\033[0m", letter); 
        else strcpy(buffer, letter);
    }

    int len = strlen(history); int buffer_len = strlen(buffer);
    if (len + buffer_len + 2 < 8192) { 
        if (len > 0 && history[len-1] != ' ' && history[len-1] != '|') {
            strncat(history, ",", 8191 - len); len++;
        }
        strncat(history, buffer, 8191 - len);
    }
}
#endif