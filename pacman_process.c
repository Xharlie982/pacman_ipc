#include "shared.h"

SharedMemory *shm;
char case_path[256];

Command p1_buffer[BUFFER_SIZE];
int p1_head = 0, p1_tail = 0, p1_count = 0;
pthread_mutex_t p1_mutex_buffer;
pthread_cond_t p1_cond_not_empty, p1_cond_not_full;
int p1_eof_reached = 0;
Command p1_local_cmd = { .type = CMD_NONE, .value = 0 };
sem_t p1_sem_publisher_start, p1_sem_publisher_done;

int check_game_over_safe() {
    pthread_mutex_lock(&shm->mutex_game_state);
    int go = shm->game_over;
    pthread_mutex_unlock(&shm->mutex_game_state);
    return go;
}

void p1_wake_all() {
    pthread_cond_broadcast(&p1_cond_not_empty); pthread_cond_broadcast(&p1_cond_not_full);
    sem_post(&p1_sem_publisher_start); sem_post(&p1_sem_publisher_done);
}

void* p1_movement_reader(void* arg) {
    (void)arg;
    char filepath[300]; snprintf(filepath, sizeof(filepath), "%s/pacman_moves.txt", case_path);
    FILE *file = fopen(filepath, "r");
    if (!file) {
        p1_eof_reached = 1;
        pthread_cond_broadcast(&p1_cond_not_empty);
        pthread_cond_broadcast(&p1_cond_not_full);
        return NULL;
    }

    char line[64];
    while (fgets(line, sizeof(line), file)) {
        if (check_game_over_safe()) break;
        if (line[0] == '\n' || line[0] == '\r') continue;
        Command cmd = parse_line(line);
        if (cmd.type == CMD_EOF) continue;

        pthread_mutex_lock(&p1_mutex_buffer);
        while (p1_count == BUFFER_SIZE && !check_game_over_safe()) pthread_cond_wait(&p1_cond_not_full, &p1_mutex_buffer);
        if (check_game_over_safe()) { pthread_mutex_unlock(&p1_mutex_buffer); break; }

        p1_buffer[p1_tail] = cmd; p1_tail = (p1_tail + 1) % BUFFER_SIZE; p1_count++;
        pthread_cond_signal(&p1_cond_not_empty);
        pthread_mutex_unlock(&p1_mutex_buffer);
    }
    fclose(file);
    pthread_mutex_lock(&p1_mutex_buffer); p1_eof_reached = 1; pthread_mutex_unlock(&p1_mutex_buffer);
    pthread_cond_broadcast(&p1_cond_not_empty);
    pthread_cond_broadcast(&p1_cond_not_full);
    return NULL;
}

void* p1_movement_executor(void* arg) {
    (void)arg;
    while (1) {
        sem_wait(&shm->sem_pacman_turn);
        if (check_game_over_safe()) { p1_wake_all(); break; }

        pthread_mutex_lock(&p1_mutex_buffer);
        
        while (p1_count == 0 && !p1_eof_reached) {
            if (check_game_over_safe()) { break; }
            pthread_cond_wait(&p1_cond_not_empty, &p1_mutex_buffer);
        }
        
        if (check_game_over_safe()) { 
            pthread_mutex_unlock(&p1_mutex_buffer); 
            p1_wake_all(); 
            break; 
        }

        if (p1_count > 0) {
            Command cmd = p1_buffer[p1_head]; p1_head = (p1_head + 1) % BUFFER_SIZE; p1_count--;
            pthread_cond_signal(&p1_cond_not_full);
            
            if (cmd.type != CMD_SET_PRIORITY) {
                p1_local_cmd = cmd;
            }
            pthread_mutex_unlock(&p1_mutex_buffer);

            if (cmd.type == CMD_SET_PRIORITY) {
                pthread_mutex_lock(&shm->mutex_mailboxes);
                shm->pending_pacman_priority = cmd.value; shm->pacman_priority_request_active = 1;
                pthread_mutex_unlock(&shm->mutex_mailboxes);

                pthread_mutex_lock(&shm->mutex_pacman_state);
                append_history(shm->pacman_history, cmd, 0);
                __sync_fetch_and_add(&shm->expected_stress_ops, STRESS_OPS);
                for(volatile int k=0; k<STRESS_OPS; k++) { shm->stress_counter++; }
                pthread_mutex_unlock(&shm->mutex_pacman_state);

                sem_post(&shm->sem_turn_finished);
            } else {
                sem_post(&p1_sem_publisher_start); sem_wait(&p1_sem_publisher_done);
                if (check_game_over_safe()) { p1_wake_all(); break; }
                sem_post(&shm->sem_turn_finished);
            }
        } else {
            pthread_mutex_unlock(&p1_mutex_buffer);
            pthread_mutex_lock(&shm->mutex_game_state); shm->pacman_eof = 1; pthread_mutex_unlock(&shm->mutex_game_state);
            sem_post(&shm->sem_turn_finished);
        }
    }
    return NULL;
}

void* p1_pacman_publisher(void* arg) {
    (void)arg;
    while(1) {
        sem_wait(&p1_sem_publisher_start);
        if (check_game_over_safe()) { p1_wake_all(); break; }

        pthread_mutex_lock(&p1_mutex_buffer);
        Command cmd_to_exec = p1_local_cmd;
        pthread_mutex_unlock(&p1_mutex_buffer);

        pthread_mutex_lock(&shm->mutex_pacman_state);
        int old_x = shm->pacman_x; int old_y = shm->pacman_y;
        int new_x = old_x, new_y = old_y;

        apply_movement(&new_x, &new_y, cmd_to_exec.type, shm->map_grid, shm->map_width, shm->map_height);
        
        int wall_hit = (new_x == old_x && new_y == old_y);
        append_history(shm->pacman_history, cmd_to_exec, wall_hit);

        if (!wall_hit) {
#ifdef ENABLE_POWER_PELLETS
            if (shm->power_pellets[new_y][new_x] == 1) {
                shm->pacman_score += 50;
                shm->power_pellets[new_y][new_x] = 0; 
                shm->dots_eaten[new_y][new_x] = 1; 
                
                pthread_mutex_lock(&shm->mutex_game_state);
                shm->power_ticks_left = 30;
                shm->ghosts_eaten_combo = 0;
                for(int i=0; i<4; i++) {
                    if (shm->ghost_dead_timer[i] == 0) shm->ghost_is_scared[i] = 1;
                }
                pthread_mutex_unlock(&shm->mutex_game_state);
            } 
            else 
#endif
            if (shm->dots_eaten[new_y][new_x] == 0) {
                shm->pacman_score += 10;
                shm->dots_eaten[new_y][new_x] = 1;
            }
        }

        __sync_fetch_and_add(&shm->expected_stress_ops, STRESS_OPS);
        for(volatile int k=0; k<STRESS_OPS; k++) {
            shm->stress_counter++;
        }

        shm->pacman_old_x = old_x; shm->pacman_old_y = old_y;
        shm->pacman_x = new_x; shm->pacman_y = new_y;
        pthread_mutex_unlock(&shm->mutex_pacman_state);

        sem_post(&p1_sem_publisher_done);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 2) return 1;
    strncpy(case_path, argv[1], sizeof(case_path)-1);
    int fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (fd < 0) { perror("P1 SHM"); exit(1); }
    shm = mmap(NULL, sizeof(SharedMemory), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    
    close(fd);

    pthread_t r_tid, e_tid, p_tid;
    pthread_mutex_init(&p1_mutex_buffer, NULL);
    pthread_cond_init(&p1_cond_not_empty, NULL); pthread_cond_init(&p1_cond_not_full, NULL);
    sem_init(&p1_sem_publisher_start, 0, 0); sem_init(&p1_sem_publisher_done, 0, 0);

    pthread_create(&r_tid, NULL, p1_movement_reader, NULL);
    pthread_create(&e_tid, NULL, p1_movement_executor, NULL);
    pthread_create(&p_tid, NULL, p1_pacman_publisher, NULL);

    pthread_join(r_tid, NULL); pthread_join(e_tid, NULL); pthread_join(p_tid, NULL);
    
    pthread_mutex_destroy(&p1_mutex_buffer);
    pthread_cond_destroy(&p1_cond_not_empty); 
    pthread_cond_destroy(&p1_cond_not_full);
    sem_destroy(&p1_sem_publisher_start);
    sem_destroy(&p1_sem_publisher_done);
    struct rusage ru_p1;
    getrusage(RUSAGE_SELF, &ru_p1);
    shm->p1_rss_kb = ru_p1.ru_maxrss;
    munmap(shm, sizeof(SharedMemory));
    
    return 0;
}