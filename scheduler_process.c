#include "shared.h"

#define CHK(call) do { if((call) < 0) { perror(#call); exit(EXIT_FAILURE); } } while(0)
#define CHK_PTR(call) do { if((call) == MAP_FAILED || (call) == NULL) { perror(#call); exit(EXIT_FAILURE); } } while(0)
#define CHK_ERR(call) do { int _rc = (call); if(_rc != 0) { fprintf(stderr, #call " failed: %s\n", strerror(_rc)); exit(EXIT_FAILURE); } } while(0)

SharedMemory *shm; char case_path[256];

int check_game_over_safe() {
    pthread_mutex_lock(&shm->mutex_game_state); // 1. Bloquea el mutex
    int go = shm->game_over;
    pthread_mutex_unlock(&shm->mutex_game_state); // 2. Libera el mutex
    return go;
}

void p0_wake_all_clean() {
    sem_post(&shm->sem_tick_start); sem_post(&shm->sem_scheduler_start); // 3. Libera los semáforos
    sem_post(&shm->sem_signal_start); sem_post(&shm->sem_pacman_turn);
    sem_post(&shm->sem_enemy_turn); sem_post(&shm->sem_turn_finished);
    sem_post(&shm->sem_check_collision); sem_post(&shm->sem_collision_checked);
    sem_post(&shm->sem_renderer_turn); sem_post(&shm->sem_renderer_done);
}

int get_visible_length(const char *str) {
    int len = 0, in_ansi = 0;
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\033') in_ansi = 1; else if (in_ansi && str[i] == 'm') in_ansi = 0; else if (!in_ansi) len++;
    }
    return len;
}

void pad_and_append_sep(char *hist, int target_len) {
    int current_len = get_visible_length(hist); int spaces_needed = target_len - current_len;
    if (spaces_needed < 0) spaces_needed = 0;
    int raw_len = strlen(hist);
    #define HIST_SEP_LEN 20 
    int max_allowed = 8192 - HIST_SEP_LEN - 1; 
    for (int i = 0; i < spaces_needed && raw_len < max_allowed; i++) hist[raw_len++] = ' ';
    hist[raw_len] = '\0';
    int remaining = 8191 - strlen(hist);
    if (remaining > 0) strncat(hist, HIST_SEP, remaining);
}

void* p0_tick_thread(void* arg) {
    (void)arg;
    while(1) {
        sem_wait(&shm->sem_tick_start); // 2. Espera a que P0 inicie el tick
        if (check_game_over_safe()) { p0_wake_all_clean(); break; }

        pthread_mutex_lock(&shm->mutex_game_state); // 1. Bloquea el mutex
        int all_eof = shm->pacman_eof;
        for(int i=0; i<4; i++) all_eof &= shm->ghost_eof[i];
        
#ifdef ENABLE_POWER_PELLETS
        if (shm->power_ticks_left > 0) {
            shm->power_ticks_left--;
            if (shm->power_ticks_left == 0) {
                for(int i=0; i<4; i++) shm->ghost_is_scared[i] = 0;
            }
        }
        for(int i=0; i<4; i++) {
            if (shm->ghost_dead_timer[i] > 0) {
                shm->ghost_dead_timer[i]--;
                if (shm->ghost_dead_timer[i] == 0) {
                    shm->ghost_is_scared[i] = 0;
                    // NUEVO: Desaparece la cruz al revivir
                    shm->tombstones[shm->ghost_dead_y[i]][shm->ghost_dead_x[i]] = 0; 
                }
            }
        }
#endif

        if (shm->global_tick >= shm->max_ticks || shm->pacman_lives <= 0 || all_eof) {
            shm->game_over = 1; pthread_mutex_unlock(&shm->mutex_game_state); // 2. Libera el mutex
            p0_wake_all_clean(); break;
        }
        shm->global_tick++;
        pthread_mutex_unlock(&shm->mutex_game_state);
        sem_post(&shm->sem_scheduler_start); // 3. Libera el semáforo
    }
    return NULL;
}

void* p0_scheduler_thread(void* arg) {
    (void)arg;
    while(1) {
        sem_wait(&shm->sem_scheduler_start); // 1. Espera a que P0 inicie el tick
        if (check_game_over_safe()) { p0_wake_all_clean(); break; }

        pthread_mutex_lock(&shm->mutex_mailboxes); // 2. Bloquea el mutex
        if (shm->pacman_priority_request_active) { 
            if (shm->use_dynamic_priority) shm->pacman_priority = shm->pending_pacman_priority; 
            shm->pacman_priority_request_active = 0; 
        }
        if (shm->enemy_priority_request_active) { 
            if (shm->use_dynamic_priority) shm->enemy_priority = shm->pending_enemy_priority; 
            shm->enemy_priority_request_active = 0; 
        }
        pthread_mutex_unlock(&shm->mutex_mailboxes); // 3. Libera el mutex

        pthread_mutex_lock(&shm->mutex_game_state); // 2. Bloquea el mutex
        if (shm->pacman_priority > shm->enemy_priority) {
            shm->current_turn = 1; shm->last_played_process = 1;
        } else if (shm->pacman_priority < shm->enemy_priority) {
            shm->current_turn = 2; shm->last_played_process = 2;
        } else {
            shm->current_turn = (shm->last_played_process == 2) ? 1 : 2;
            shm->last_played_process = shm->current_turn;
        }
        pthread_mutex_unlock(&shm->mutex_game_state); // 3. Libera el mutex
        sem_post(&shm->sem_signal_start); // 4. Libera el semáforo
    }
    return NULL;
}

void* p0_signal_thread(void* arg) {
    (void)arg;
    while(1) {
        sem_wait(&shm->sem_signal_start); // 1. Espera a que P0 inicie el tick
        if (check_game_over_safe()) { p0_wake_all_clean(); break; }

        pthread_mutex_lock(&shm->mutex_game_state); // 2. Bloquea el mutex
        int turn = shm->current_turn;
        pthread_mutex_unlock(&shm->mutex_game_state); // 3. Libera el mutex

        if (turn == 1) sem_post(&shm->sem_pacman_turn); else sem_post(&shm->sem_enemy_turn); // 4. Libera el semáforo
        sem_wait(&shm->sem_turn_finished); // 1. Espera a que P0 inicie el tick
        if (check_game_over_safe()) { p0_wake_all_clean(); break; }

        sem_post(&shm->sem_check_collision); // 4. Libera el semáforo
        sem_wait(&shm->sem_collision_checked); // 1. Espera a que P0 inicie el tick

        pthread_mutex_lock(&shm->mutex_collisions); // 2. Bloquea el mutex
        if (shm->collision_detected) {
            shm->collision_detected = 0;
            
            pthread_mutex_lock(&shm->mutex_game_state); // 3. Bloquea el mutex
            int gid = shm->collision_ghost_id;
            char const *colors[] = {COLOR_BLINKY, COLOR_PINKY, COLOR_INKY, COLOR_CLYDE};
            char const *names[] = {"Blinky", "Pinky", "Inky", "Clyde"}; 

#ifdef ENABLE_POWER_PELLETS
            if (shm->power_ticks_left > 0 && shm->ghost_is_scared[gid]) {
                shm->ghosts_eaten_combo++;
                shm->pacman_score += 100 * (1 << shm->ghosts_eaten_combo); 
                
                // Reducimos a 9 (8 ticks reales) para verlos revivir
                shm->ghost_dead_timer[gid] = 9; 
                shm->ghost_is_scared[gid] = 0; 
                
                // Guardamos dónde está la cruz
                shm->ghost_dead_x[gid] = shm->ghost_x[gid];
                shm->ghost_dead_y[gid] = shm->ghost_y[gid];
                shm->tombstones[shm->ghost_y[gid]][shm->ghost_x[gid]] = 1;
                
                pthread_mutex_lock(&shm->mutex_ghost_state); // 4. Bloquea el mutex
                shm->ghost_x[gid] = shm->ghost_init_x[gid]; shm->ghost_y[gid] = shm->ghost_init_y[gid];
                shm->ghost_old_x[gid] = shm->ghost_init_x[gid]; shm->ghost_old_y[gid] = shm->ghost_init_y[gid];
                pthread_mutex_unlock(&shm->mutex_ghost_state); // 5. Libera el mutex
                
                int k_idx = shm->kill_feed_count % 4;
                snprintf(shm->kill_feed[k_idx].name, sizeof(shm->kill_feed[k_idx].name), "%s%s%s by %sPac-Man%s", colors[gid], names[gid], COLOR_RESET, COLOR_PACMAN, COLOR_RESET);
                shm->kill_feed[k_idx].tick = shm->global_tick;
                shm->kill_feed[k_idx].active = 1;
                shm->kill_feed_count++;
            } else {
#endif
                shm->pacman_lives--; shm->just_died = 1;
                shm->killer_id = gid; shm->death_count++;
                shm->collision_tick = shm->global_tick;
                
                int k_idx = shm->kill_feed_count % 4;
                snprintf(shm->kill_feed[k_idx].name, sizeof(shm->kill_feed[k_idx].name), "%sPac-Man%s by %s%s%s", COLOR_PACMAN, COLOR_RESET, colors[shm->killer_id], names[shm->killer_id], COLOR_RESET);
                shm->kill_feed[k_idx].tick = shm->global_tick;
                shm->kill_feed[k_idx].active = 1;
                shm->kill_feed_count++;

                if (shm->pacman_lives <= 0) shm->game_over = 1;
#ifdef ENABLE_POWER_PELLETS
            }
#endif
            pthread_mutex_unlock(&shm->mutex_game_state); // 4. Libera el mutex
        }
        pthread_mutex_unlock(&shm->mutex_collisions); // 3. Libera el mutex

        if (check_game_over_safe()) { p0_wake_all_clean(); break; }
        
        sem_post(&shm->sem_renderer_turn); sem_wait(&shm->sem_renderer_done); // 4. Libera el semáforo

        pthread_mutex_lock(&shm->mutex_game_state); // 2. Bloquea el mutex
        if (shm->just_died) {
            shm->just_died = 0;
            int max_len = get_visible_length(shm->pacman_history);
            for(int i=0; i<4; i++) {
                int l = get_visible_length(shm->ghost_history[i]);
                if(l > max_len) max_len = l;
            }
            pthread_mutex_lock(&shm->mutex_pacman_state); // 3. Bloquea el mutex
            shm->pacman_x = shm->pacman_init_x; shm->pacman_y = shm->pacman_init_y;
            shm->pacman_old_x = shm->pacman_init_x; shm->pacman_old_y = shm->pacman_init_y;
            pad_and_append_sep(shm->pacman_history, max_len);
            pthread_mutex_unlock(&shm->mutex_pacman_state); // 4. Libera el mutex
            
            pthread_mutex_lock(&shm->mutex_ghost_state); // 3. Bloquea el mutex
            for(int i=0; i<4; i++) {
                shm->ghost_x[i] = shm->ghost_init_x[i]; shm->ghost_y[i] = shm->ghost_init_y[i];
                shm->ghost_old_x[i] = shm->ghost_init_x[i]; shm->ghost_old_y[i] = shm->ghost_init_y[i];
                pad_and_append_sep(shm->ghost_history[i], max_len);
            }
            pthread_mutex_unlock(&shm->mutex_ghost_state); // 4. Libera el mutex
        }
        pthread_mutex_unlock(&shm->mutex_game_state); // 3. Libera el mutex

        sem_post(&shm->sem_tick_start); // 4. Libera el semáforo
    }
    return NULL;
}

void init_map(const char *path) {
    char filepath[300]; snprintf(filepath, sizeof(filepath), "%s/map.txt", path);
    FILE *file = fopen(filepath, "r");
    if (!file) { perror("Map file not found"); exit(1); }

    memset(shm->dots_eaten, 0, sizeof(shm->dots_eaten));
    memset(shm->tombstones, 0, sizeof(shm->tombstones));
#ifdef ENABLE_POWER_PELLETS
    memset(shm->power_pellets, 0, sizeof(shm->power_pellets));
#endif

    char line[256]; int y = 0, w = 0;
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '\n' || line[0] == '\r') continue;
        int len = strlen(line);
        while(len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) { line[len-1] = '\0'; len--; }
        if (w == 0) w = len;
        for (int x = 0; x < len && x < 20; x++) {
            char c = line[x];
#ifdef ENABLE_POWER_PELLETS
            if (c == '*') { shm->power_pellets[y][x] = 1; shm->dots_eaten[y][x] = 1; c = 'O'; }
#endif
            if (c == 'P') { shm->pacman_init_x = x; shm->pacman_init_y = y; shm->pacman_x = x; shm->pacman_y = y; shm->pacman_old_x = x; shm->pacman_old_y = y; shm->dots_eaten[y][x] = 1; c = 'O'; }
            else if (c == 'A') { shm->ghost_init_x[0] = x; shm->ghost_init_y[0] = y; shm->ghost_x[0] = x; shm->ghost_y[0] = y; shm->ghost_old_x[0] = x; shm->ghost_old_y[0] = y; c = 'O'; }
            else if (c == 'B') { shm->ghost_init_x[1] = x; shm->ghost_init_y[1] = y; shm->ghost_x[1] = x; shm->ghost_y[1] = y; shm->ghost_old_x[1] = x; shm->ghost_old_y[1] = y; c = 'O'; }
            else if (c == 'C') { shm->ghost_init_x[2] = x; shm->ghost_init_y[2] = y; shm->ghost_x[2] = x; shm->ghost_y[2] = y; shm->ghost_old_x[2] = x; shm->ghost_old_y[2] = y; c = 'O'; }
            else if (c == 'D') { shm->ghost_init_x[3] = x; shm->ghost_init_y[3] = y; shm->ghost_x[3] = x; shm->ghost_y[3] = y; shm->ghost_old_x[3] = x; shm->ghost_old_y[3] = y; c = 'O'; }
            shm->map_grid[y][x] = c;
        }
        y++;
    }
    shm->map_width = w; shm->map_height = y;
    fclose(file);
}

int main(int argc, char *argv[]) {
    if (argc != 2) { printf("Usage: %s <path_to_case>\n", argv[0]); return 1; }

    // =====================================================================
    // VALIDACIÓN DE DEPENDENCIAS: Requisito de Power Pellets para Caso 4
    // =====================================================================
    if (strstr(argv[1], "caso4") != NULL) {
#ifndef ENABLE_POWER_PELLETS
        printf("\n%s[Error de Dependencia]%s El escenario 'caso4' requiere la habilitación de Power Pellets.\n", COLOR_ERROR, COLOR_RESET);
        printf("Para continuar, descomente la directiva '#define ENABLE_POWER_PELLETS' en 'shared.h' y recompile el proyecto (make clean && make).\n\n");
        return 1;
#endif
    }

    strncpy(case_path, argv[1], sizeof(case_path)-1);

    shm_unlink(SHM_NAME);
    int fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666); CHK(fd);
    CHK(ftruncate(fd, sizeof(SharedMemory)));
    shm = mmap(NULL, sizeof(SharedMemory), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0); CHK_PTR(shm);
    close(fd);

    pthread_mutexattr_t attr; CHK_ERR(pthread_mutexattr_init(&attr));
    CHK_ERR(pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED));

    int successful_runs = 0;
    long long total_stress_ops = 0;
    long long total_render_errors = 0;
    double total_elapsed_ms = 0.0;

    for (int run = 1; run <= PROGRAM_RUNS; run++) {
        shm->global_tick = 0; shm->max_ticks = MAX_TICKS;
        shm->game_over = 0; shm->pacman_score = 0; shm->pacman_lives = 3; shm->sim_time_ms = 0.0;
        shm->stress_counter = 0;
        shm->render_errors = 0;
        shm->just_died = 0; shm->death_count = 0; shm->pacman_history[0] = '\0';
        shm->kill_feed_count = 0;

        for(int i=0; i<4; i++) shm->ghost_history[i][0] = '\0';
        shm->last_played_process = 2;

#ifdef ENABLE_POWER_PELLETS
        shm->power_ticks_left = 0;
        shm->ghosts_eaten_combo = 0;
        for(int i=0; i<4; i++) {
            shm->ghost_dead_timer[i] = 0;
            shm->ghost_is_scared[i] = 0;
        }
#endif

        shm->use_dynamic_priority = (strstr(argv[1], "caso3") != NULL || strstr(argv[1], "caso4") != NULL) ? 1 : 0;
        shm->p0_priority = 40;
#ifdef GHOSTS_FIRST_PRIORITY
        shm->pacman_priority = 1; shm->enemy_priority = 10;
#elif defined(P0_LOWEST_PRIORITY)
        shm->p0_priority = 5; shm->pacman_priority = 15; shm->enemy_priority = 25;
#else
        if (strstr(argv[1], "caso3") != NULL) { shm->pacman_priority = 20; shm->enemy_priority = 30; }
        else { shm->pacman_priority = 30; shm->enemy_priority = 30; }
#endif
        
        CHK(sem_init(&shm->sem_tick_start, 1, 0)); CHK(sem_init(&shm->sem_scheduler_start, 1, 0));
        CHK(sem_init(&shm->sem_signal_start, 1, 0)); CHK(sem_init(&shm->sem_pacman_turn, 1, 0));
        CHK(sem_init(&shm->sem_enemy_turn, 1, 0)); CHK(sem_init(&shm->sem_turn_finished, 1, 0));
        CHK(sem_init(&shm->sem_check_collision, 1, 0)); CHK(sem_init(&shm->sem_collision_checked, 1, 0));
        CHK(sem_init(&shm->sem_renderer_turn, 1, 0)); CHK(sem_init(&shm->sem_renderer_done, 1, 0));
        CHK(sem_init(&shm->sem_ready_done, 1, 0)); 

        CHK_ERR(pthread_mutex_init(&shm->mutex_game_state, &attr));
        CHK_ERR(pthread_mutex_init(&shm->mutex_pacman_state, &attr));
        CHK_ERR(pthread_mutex_init(&shm->mutex_ghost_state, &attr));
        CHK_ERR(pthread_mutex_init(&shm->mutex_mailboxes, &attr));
        CHK_ERR(pthread_mutex_init(&shm->mutex_collisions, &attr));

        init_map(case_path);
        shm->is_caso4 = (strstr(case_path, "caso4") != NULL);

        pid_t pid_p1 = fork(); CHK(pid_p1); if (pid_p1 == 0) { execl("./pacman_process", "pacman_process", argv[1], NULL); perror("execl P1"); exit(1); }
        pid_t pid_p2 = fork(); CHK(pid_p2); if (pid_p2 == 0) { execl("./enemy_process", "enemy_process", argv[1], NULL); perror("execl P2"); exit(1); }
        pid_t pid_p3 = fork(); CHK(pid_p3); if (pid_p3 == 0) { execl("./renderer_process", "renderer_process", NULL); perror("execl P3"); exit(1); }

        pthread_t tick_t, sched_t, sig_t;
        CHK_ERR(pthread_create(&tick_t, NULL, p0_tick_thread, NULL));
        CHK_ERR(pthread_create(&sched_t, NULL, p0_scheduler_thread, NULL));
        CHK_ERR(pthread_create(&sig_t, NULL, p0_signal_thread, NULL));

        sem_wait(&shm->sem_ready_done);
        struct timespec sim_start, sim_end;
        clock_gettime(CLOCK_MONOTONIC, &sim_start);
        sem_post(&shm->sem_tick_start);
        pthread_join(tick_t, NULL); pthread_join(sched_t, NULL); pthread_join(sig_t, NULL);
        clock_gettime(CLOCK_MONOTONIC, &sim_end);
        double elapsed_ms = (sim_end.tv_sec - sim_start.tv_sec) * 1000.0 + (sim_end.tv_nsec - sim_start.tv_nsec) / 1000000.0;
        shm->sim_time_ms = elapsed_ms;
        total_elapsed_ms += elapsed_ms;

        waitpid(pid_p1, NULL, 0); waitpid(pid_p2, NULL, 0);
        
        struct rusage ru_ch, ru_self;
        getrusage(RUSAGE_CHILDREN, &ru_ch);
        getrusage(RUSAGE_SELF, &ru_self);
        shm->wall_clock_s = elapsed_ms / 1000.0;
        shm->user_cpu_s = (ru_ch.ru_utime.tv_sec + ru_ch.ru_utime.tv_usec/1e6) + (ru_self.ru_utime.tv_sec + ru_self.ru_utime.tv_usec/1e6);
        shm->sys_cpu_s = (ru_ch.ru_stime.tv_sec + ru_ch.ru_stime.tv_usec/1e6) + (ru_self.ru_stime.tv_sec + ru_self.ru_stime.tv_usec/1e6);
        shm->max_rss_kb = (ru_ch.ru_maxrss > ru_self.ru_maxrss) ? ru_ch.ru_maxrss : ru_self.ru_maxrss;
        shm->vol_context_switches = ru_ch.ru_nvcsw + ru_self.ru_nvcsw;
        shm->invol_context_switches = ru_ch.ru_nivcsw + ru_self.ru_nivcsw;
        shm->expected_stress_ops = 6000000LL;

        waitpid(pid_p3, NULL, 0);

        total_stress_ops += shm->stress_counter;
        total_render_errors += shm->render_errors;
        if (shm->game_over) successful_runs++;

        if (PROGRAM_RUNS == 1) {
            char time_buf[2048];
            snprintf(time_buf, sizeof(time_buf),
                "\n=====================================================================\n"
                "🏆  MÉTRICAS OFICIALES DE RENDIMIENTO (TABLA FINAL)\n"
                "=====================================================================\n"
                "1. Tiempo Interno de Simulación (ms) : %.2f ms\n"
                "2. Tiempo Real Total / Wall Clock (s): %.4f s\n"
                "3. Tiempo CPU Modo Usuario (s)       : %.4f s\n"
                "4. Tiempo CPU Modo Kernel (s)        : %.4f s\n"
                "5. Consumo Máximo Memoria RAM (RSS)  : %ld KB\n"
                "6. Cambios Contexto Voluntarios      : %ld\n"
                "7. Cambios Contexto Involuntarios    : %ld\n"
                "8. Integridad Datos (Ops procesadas) : %lld / %lld (%.1f%%)\n"
                "9. Fallos de Renderizado Detectados  : %lld frames\n"
                "=====================================================================\n",
                shm->sim_time_ms, shm->wall_clock_s, shm->user_cpu_s, shm->sys_cpu_s,
                shm->max_rss_kb, shm->vol_context_switches, shm->invol_context_switches,
                (long long)shm->stress_counter, shm->expected_stress_ops,
                (shm->expected_stress_ops > 0
                    ? (double)shm->stress_counter / shm->expected_stress_ops * 100.0
                    : 0.0),
                shm->render_errors);
            print_consola(time_buf);
        }

        pthread_mutex_destroy(&shm->mutex_game_state); pthread_mutex_destroy(&shm->mutex_pacman_state);
        pthread_mutex_destroy(&shm->mutex_ghost_state); pthread_mutex_destroy(&shm->mutex_mailboxes);
        pthread_mutex_destroy(&shm->mutex_collisions);

        sem_destroy(&shm->sem_tick_start); sem_destroy(&shm->sem_scheduler_start);
        sem_destroy(&shm->sem_signal_start); sem_destroy(&shm->sem_pacman_turn);
        sem_destroy(&shm->sem_enemy_turn); sem_destroy(&shm->sem_turn_finished);
        sem_destroy(&shm->sem_check_collision); sem_destroy(&shm->sem_collision_checked);
        sem_destroy(&shm->sem_renderer_turn); sem_destroy(&shm->sem_renderer_done);
        sem_destroy(&shm->sem_ready_done);
    }

    if (PROGRAM_RUNS > 1) {
        char sum_buf[1024];
        snprintf(sum_buf, sizeof(sum_buf),
            "\n=====================================================================\n"
            "⏰ RESULTADO PRUEBA DE ESTRÉS INTEGRADA (%d Ejecuciones x 100,000 Iteraciones)\n"
            "Ejecuciones completadas exitosamente : %d/%d\n"
            "Tiempo acumulado total               : %.2f ms\n"
            "Operaciones de estrés registradas    : %lld / %lld (%.1f%%)\n"
            "Fallos de renderizado acumulados     : %lld frames\n"
            "=====================================================================\n",
            PROGRAM_RUNS, successful_runs, PROGRAM_RUNS, total_elapsed_ms,
            total_stress_ops, (long long)PROGRAM_RUNS * shm->expected_stress_ops,
            (shm->expected_stress_ops > 0
                ? (double)total_stress_ops / ((double)PROGRAM_RUNS * shm->expected_stress_ops) * 100.0
                : 0.0),
            total_render_errors);
        print_consola(sum_buf);
    }

    pthread_mutexattr_destroy(&attr); 
    munmap(shm, sizeof(SharedMemory)); shm_unlink(SHM_NAME);
    
    return 0;
}