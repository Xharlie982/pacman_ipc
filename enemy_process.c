#include "shared.h"

SharedMemory *shm;
char case_path[256];

struct { int x; int y; } ghost_positions[4];
struct { int x; int y; } ghost_old_positions[4];
struct { int x; int y; } pacman_last_position;
struct { int x; int y; } pacman_last_old_position;

pthread_mutex_t p2_mutex_ghosts, p2_mutex_local;
int p2_local_priority_request[4];
sem_t p2_sem_ghost_start[4], p2_sem_ghost_done[4];
sem_t p2_sem_tracker_start, p2_sem_tracker_done;

int check_game_over_safe() {
    pthread_mutex_lock(&shm->mutex_game_state);
    int go = shm->game_over;
    pthread_mutex_unlock(&shm->mutex_game_state);
    return go;
}

void p2_wake_all() {
    sem_post(&p2_sem_tracker_start); sem_post(&p2_sem_tracker_done);
    for(int i=0; i<4; i++) { sem_post(&p2_sem_ghost_start[i]); sem_post(&p2_sem_ghost_done[i]); }
}

void* p2_ghost_thread(void* arg) {
    int id = *((int*)arg);
    char filepath[300]; snprintf(filepath, sizeof(filepath), "%s/ghost_%d_moves.txt", case_path, id + 1);
    FILE *file = fopen(filepath, "r"); 
    char line[64];
    
    if (!file) {
        pthread_mutex_lock(&shm->mutex_game_state); shm->ghost_eof[id] = 1; pthread_mutex_unlock(&shm->mutex_game_state);
        while (1) {
            sem_wait(&p2_sem_ghost_start[id]);
            if (check_game_over_safe()) { p2_wake_all(); break; }
            sem_post(&p2_sem_ghost_done[id]);
        }
        return NULL;
    }
    
    while (1) {
        sem_wait(&p2_sem_ghost_start[id]);
        if (check_game_over_safe()) { p2_wake_all(); break; }

#ifdef ENABLE_POWER_PELLETS
        pthread_mutex_lock(&shm->mutex_game_state);
        int is_dead = (shm->ghost_dead_timer[id] > 0);
        pthread_mutex_unlock(&shm->mutex_game_state);
#else
        int is_dead = 0;
#endif

        if (fgets(line, sizeof(line), file)) {
            if (line[0] != '\n' && line[0] != '\r') {
                Command cmd = parse_line(line);
                
                if (is_dead) {
                    pthread_mutex_lock(&shm->mutex_ghost_state);
                    append_history(shm->ghost_history[id], cmd, 1); 
                    pthread_mutex_unlock(&shm->mutex_ghost_state);
                } 
                else {
                    if (cmd.type == CMD_SET_PRIORITY) {
                        pthread_mutex_lock(&p2_mutex_local);
                        if (p2_local_priority_request[id] == -1) p2_local_priority_request[id] = cmd.value;
                        pthread_mutex_unlock(&p2_mutex_local);
                        
                        pthread_mutex_lock(&shm->mutex_ghost_state);
                        append_history(shm->ghost_history[id], cmd, 0);
                        __sync_fetch_and_add(&shm->expected_stress_ops, STRESS_OPS);
                        for(volatile int k=0; k<STRESS_OPS; k++) { shm->stress_counter++; }
                        pthread_mutex_unlock(&shm->mutex_ghost_state);
                    } else if (cmd.type != CMD_EOF) {
                        pthread_mutex_lock(&p2_mutex_ghosts);
                        pthread_mutex_lock(&shm->mutex_ghost_state);
                        ghost_positions[id].x = shm->ghost_x[id]; ghost_positions[id].y = shm->ghost_y[id];
                        pthread_mutex_unlock(&shm->mutex_ghost_state);

                        ghost_old_positions[id].x = ghost_positions[id].x; ghost_old_positions[id].y = ghost_positions[id].y;
                        apply_movement(&ghost_positions[id].x, &ghost_positions[id].y, cmd.type, shm->map_grid, shm->map_width, shm->map_height);
                        
                        int wall_hit = (ghost_positions[id].x == ghost_old_positions[id].x && ghost_positions[id].y == ghost_old_positions[id].y);
                        
                        pthread_mutex_lock(&shm->mutex_ghost_state);
                        shm->ghost_old_x[id] = ghost_old_positions[id].x; shm->ghost_old_y[id] = ghost_old_positions[id].y;
                        shm->ghost_x[id] = ghost_positions[id].x; shm->ghost_y[id] = ghost_positions[id].y;
                        append_history(shm->ghost_history[id], cmd, wall_hit);
                        pthread_mutex_unlock(&shm->mutex_ghost_state);
                        pthread_mutex_unlock(&p2_mutex_ghosts);
                        // mutex_ghost_state es el que DISABLE_SYNC bypassea → race condition observable aquí
                        pthread_mutex_lock(&shm->mutex_ghost_state);
                        __sync_fetch_and_add(&shm->expected_stress_ops, STRESS_OPS);
                        for(volatile int k=0; k<STRESS_OPS; k++) {
                            shm->stress_counter++;
                        }
                        pthread_mutex_unlock(&shm->mutex_ghost_state);
                    }
                }
            }
        } else {
            pthread_mutex_lock(&shm->mutex_game_state); shm->ghost_eof[id] = 1; pthread_mutex_unlock(&shm->mutex_game_state);
        }
        sem_post(&p2_sem_ghost_done[id]);
    }
    if (file) fclose(file);
    return NULL;
}

void* p2_tracker_thread(void* arg) {
    (void)arg;
    while (1) {
        sem_wait(&p2_sem_tracker_start);
        if (check_game_over_safe()) { p2_wake_all(); break; }
        pthread_mutex_lock(&shm->mutex_pacman_state);
        pacman_last_position.x = shm->pacman_x; pacman_last_position.y = shm->pacman_y;
        pacman_last_old_position.x = shm->pacman_old_x; pacman_last_old_position.y = shm->pacman_old_y;
        pthread_mutex_unlock(&shm->mutex_pacman_state);
        sem_post(&p2_sem_tracker_done);
    }
    return NULL;
}

void* p2_collision_thread(void* arg) {
    (void)arg;
    while (1) {
        sem_wait(&shm->sem_check_collision);
        if (check_game_over_safe()) { p2_wake_all(); break; }

        pthread_mutex_lock(&shm->mutex_pacman_state);
        int px = shm->pacman_x, py = shm->pacman_y, px_old = shm->pacman_old_x, py_old = shm->pacman_old_y;
        pthread_mutex_unlock(&shm->mutex_pacman_state);

        pthread_mutex_lock(&shm->mutex_ghost_state);
        for (int i = 0; i < 4; i++) {
#ifdef ENABLE_POWER_PELLETS
            pthread_mutex_lock(&shm->mutex_game_state);
            int is_dead = (shm->ghost_dead_timer[i] > 0);
            pthread_mutex_unlock(&shm->mutex_game_state);
#else
            int is_dead = 0;
#endif
            if (!is_dead) {
                int gx = shm->ghost_x[i], gy = shm->ghost_y[i], gx_old = shm->ghost_old_x[i], gy_old = shm->ghost_old_y[i];
                int same_cell = (px == gx && py == gy);
                int crossed = (px_old == gx && py_old == gy && px == gx_old && py == gy_old);
                if (same_cell || crossed) {
                    pthread_mutex_lock(&shm->mutex_collisions);
                    shm->collision_detected = 1; shm->collision_ghost_id = i; 
                    pthread_mutex_unlock(&shm->mutex_collisions);
                    break; 
                }
            }
        }
        pthread_mutex_unlock(&shm->mutex_ghost_state);
        sem_post(&shm->sem_collision_checked);
    }
    return NULL;
}

void* p2_controller_thread(void* arg) {
    (void)arg;
    while (1) {
        sem_wait(&shm->sem_enemy_turn);
        
        if (check_game_over_safe()) { p2_wake_all(); break; }

        pthread_mutex_lock(&p2_mutex_local);
        for(int i=0; i<4; i++) p2_local_priority_request[i] = -1;
        pthread_mutex_unlock(&p2_mutex_local);

        sem_post(&p2_sem_tracker_start); sem_wait(&p2_sem_tracker_done);
        for(int i=0; i<4; i++) sem_post(&p2_sem_ghost_start[i]);
        for(int i=0; i<4; i++) sem_wait(&p2_sem_ghost_done[i]);

        int best_req = -1;
        pthread_mutex_lock(&p2_mutex_local);
        for(int i=0; i<4; i++) { if(p2_local_priority_request[i] != -1) { best_req = p2_local_priority_request[i]; break; } }
        pthread_mutex_unlock(&p2_mutex_local);

        if(best_req != -1) {
            pthread_mutex_lock(&shm->mutex_mailboxes);
            shm->pending_enemy_priority = best_req; shm->enemy_priority_request_active = 1;
            pthread_mutex_unlock(&shm->mutex_mailboxes);
        }
        sem_post(&shm->sem_turn_finished);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 2) return 1;
    strncpy(case_path, argv[1], sizeof(case_path)-1);
    int fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (fd < 0) { perror("P2 SHM"); exit(1); }
    shm = mmap(NULL, sizeof(SharedMemory), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);

    pthread_t ctrl_tid, tracker_tid, col_tid, ghost_tids[4];
    int ghost_ids[4] = {0, 1, 2, 3};
    
    pthread_mutex_init(&p2_mutex_ghosts, NULL); pthread_mutex_init(&p2_mutex_local, NULL);
    sem_init(&p2_sem_tracker_start, 0, 0); sem_init(&p2_sem_tracker_done, 0, 0);
    for(int i=0; i<4; i++) { sem_init(&p2_sem_ghost_start[i], 0, 0); sem_init(&p2_sem_ghost_done[i], 0, 0); }

    pthread_create(&ctrl_tid, NULL, p2_controller_thread, NULL);
    pthread_create(&tracker_tid, NULL, p2_tracker_thread, NULL);
    pthread_create(&col_tid, NULL, p2_collision_thread, NULL);
    for(int i=0; i<4; i++) pthread_create(&ghost_tids[i], NULL, p2_ghost_thread, &ghost_ids[i]);

    pthread_join(ctrl_tid, NULL); pthread_join(tracker_tid, NULL); pthread_join(col_tid, NULL);
    for(int i=0; i<4; i++) pthread_join(ghost_tids[i], NULL);
    
    pthread_mutex_destroy(&p2_mutex_ghosts); pthread_mutex_destroy(&p2_mutex_local);
    sem_destroy(&p2_sem_tracker_start); sem_destroy(&p2_sem_tracker_done);
    for(int i=0; i<4; i++) { sem_destroy(&p2_sem_ghost_start[i]); sem_destroy(&p2_sem_ghost_done[i]); }
    struct rusage ru_p2;
    getrusage(RUSAGE_SELF, &ru_p2);
    shm->p2_rss_kb = ru_p2.ru_maxrss;
    munmap(shm, sizeof(SharedMemory));
    
    return 0;
}