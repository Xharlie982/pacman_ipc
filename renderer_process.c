#include "shared.h"

// =========================================================================
// INTERRUPTOR MANUAL DE SALIDA GRÁFICA (Test 1B)
// Por defecto está comentado para usar la TERMINAL ANSI (Test 1).
// Quítale el "//" a la línea inferior (o pasa -DMODO_GRAFICO_SDL) para SDL2.
// =========================================================================
//#define MODO_GRAFICO_SDL 

#ifdef MODO_GRAFICO_SDL
    #include <SDL2/SDL.h>
    #include <SDL2/SDL_image.h>
    #include <SDL2/SDL_ttf.h>
    #define TILE_SIZE 32
    #define UI_TOP 64
    #define UI_BOTTOM 64 

void render_text_ttf(SDL_Renderer* renderer, TTF_Font* font, const char* text, int x, int y, SDL_Color color, int align_right) {
    if (!font || !text) return;
    SDL_Surface* surface = TTF_RenderText_Solid(font, text, color);
    if (surface) {
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        int final_x = align_right ? x - surface->w : x; 
        SDL_Rect rect = { final_x, y, surface->w, surface->h };
        SDL_RenderCopy(renderer, texture, NULL, &rect);
        SDL_DestroyTexture(texture);
        SDL_FreeSurface(surface);
    }
}
#endif

SharedMemory *shm;

static inline void output_text(const char *buf) {
#ifdef USE_SYSCALL_WRITE
    print_consola(buf);
#else
    printf("%s", buf); fflush(stdout);
#endif
}

int main() {
    int fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (fd < 0) { perror("P3 SHM"); exit(1); }
    shm = mmap(NULL, sizeof(SharedMemory), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);

#ifdef HEADLESS
    sem_post(&shm->sem_ready_done);
    while (1) {
        sem_wait(&shm->sem_renderer_turn);
        pthread_mutex_lock(&shm->mutex_game_state);
        int over = shm->game_over;
        pthread_mutex_unlock(&shm->mutex_game_state);
        sem_post(&shm->sem_renderer_done);
        if (over) break;
    }
    munmap(shm, sizeof(SharedMemory));
    return 0;
#endif

#ifdef MODO_GRAFICO_SDL
    SDL_Init(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_PNG);
    if(TTF_Init() == -1) { printf("Error inicializando TTF: %s\n", TTF_GetError()); }

    int win_width = shm->map_width * TILE_SIZE;
    int win_height = (shm->map_height * TILE_SIZE) + UI_TOP + UI_BOTTOM;
    
    SDL_Window* window = SDL_CreateWindow("Pac-Man POSIX GUI - Arcade Perfect", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, win_width, win_height, SDL_WINDOW_RESIZABLE);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_RenderSetLogicalSize(renderer, win_width, win_height);

    SDL_Texture* tex_pac_r = IMG_LoadTexture(renderer, "assets/pacman_r.png");
    SDL_Texture* tex_pac_l = IMG_LoadTexture(renderer, "assets/pacman_l.png");
    SDL_Texture* tex_pac_u = IMG_LoadTexture(renderer, "assets/pacman_u.png");
    SDL_Texture* tex_pac_d = IMG_LoadTexture(renderer, "assets/pacman_d.png");
    SDL_Texture* tex_blinky = IMG_LoadTexture(renderer, "assets/blinky.png");
    SDL_Texture* tex_pinky  = IMG_LoadTexture(renderer, "assets/pinky.png");
    SDL_Texture* tex_inky   = IMG_LoadTexture(renderer, "assets/inky.png");
    SDL_Texture* tex_clyde  = IMG_LoadTexture(renderer, "assets/clyde.png");
    SDL_Texture* tex_dot    = IMG_LoadTexture(renderer, "assets/dot.png");
    SDL_Texture* tex_scared = IMG_LoadTexture(renderer, "assets/blue_ghost.png");

    TTF_Font* font_large = TTF_OpenFont("assets/font.ttf", 20); 
    TTF_Font* font_small = TTF_OpenFont("assets/font.ttf", 16); 

    SDL_Event e;
    int pacman_direction = 0; 
    
    Uint32 ready_start_time = SDL_GetTicks();
    while (SDL_GetTicks() - ready_start_time < 500) {
        while (SDL_PollEvent(&e)) { 
            if (e.type == SDL_QUIT) {
                pthread_mutex_lock(&shm->mutex_game_state);
                shm->game_over = 1; 
                pthread_mutex_unlock(&shm->mutex_game_state);
                sem_post(&shm->sem_ready_done); 
                goto CLEANUP;
            } 
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); 
        SDL_RenderClear(renderer);

        pthread_mutex_lock(&shm->mutex_pacman_state);
        int init_px = shm->pacman_init_x, init_py = shm->pacman_init_y;
        int local_dots[20][20]; memcpy(local_dots, shm->dots_eaten, sizeof(local_dots));
#ifdef ENABLE_POWER_PELLETS
        int local_power[20][20]; memcpy(local_power, shm->power_pellets, sizeof(local_power));
#endif
        pthread_mutex_unlock(&shm->mutex_pacman_state);

        pthread_mutex_lock(&shm->mutex_game_state);
        int local_tombstones[20][20]; memcpy(local_tombstones, shm->tombstones, sizeof(local_tombstones));
        pthread_mutex_unlock(&shm->mutex_game_state);

        pthread_mutex_lock(&shm->mutex_ghost_state);
        int init_gx[4], init_gy[4];
        for(int i=0; i<4; i++) { init_gx[i] = shm->ghost_init_x[i]; init_gy[i] = shm->ghost_init_y[i]; }
        pthread_mutex_unlock(&shm->mutex_ghost_state);

        for(int y = 0; y < shm->map_height; y++) {
            for(int x = 0; x < shm->map_width; x++) {
                SDL_Rect rect = { x * TILE_SIZE, y * TILE_SIZE + UI_TOP, TILE_SIZE, TILE_SIZE };
                if (shm->map_grid[y][x] == 'X') {
                    if (shm->is_caso4) SDL_SetRenderDrawColor(renderer, 69, 179, 169, 255);
                    else SDL_SetRenderDrawColor(renderer, 25, 25, 166, 255);
                    
                    SDL_RenderFillRect(renderer, &rect);
                    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                    SDL_Rect inner = { x * TILE_SIZE + 4, y * TILE_SIZE + UI_TOP + 4, TILE_SIZE - 8, TILE_SIZE - 8 };
                    SDL_RenderFillRect(renderer, &inner);
                } 
#ifdef ENABLE_POWER_PELLETS
                else if (local_power[y][x]) {
                    SDL_SetRenderDrawColor(renderer, 255, 215, 0, 255);
                    SDL_Rect power = { x * TILE_SIZE + (TILE_SIZE/2 - 6), y * TILE_SIZE + UI_TOP + (TILE_SIZE/2 - 6), 12, 12 };
                    SDL_RenderFillRect(renderer, &power);
                }
#endif
                else if (local_dots[y][x] == 0 && shm->map_grid[y][x] == 'O') {
                    SDL_RenderCopy(renderer, tex_dot, NULL, &rect);
                }
            }
        }

        for(int i=0; i<4; i++) {
            if (init_gx[i] >= 0 && init_gy[i] >= 0) {
                SDL_Rect grect = { init_gx[i] * TILE_SIZE, init_gy[i] * TILE_SIZE + UI_TOP, TILE_SIZE, TILE_SIZE };
                if (i == 0) SDL_RenderCopy(renderer, tex_blinky, NULL, &grect);
                else if (i == 1) SDL_RenderCopy(renderer, tex_pinky, NULL, &grect);
                else if (i == 2) SDL_RenderCopy(renderer, tex_inky, NULL, &grect);
                else SDL_RenderCopy(renderer, tex_clyde, NULL, &grect);
            }
        }
        
        SDL_Rect prect = { init_px * TILE_SIZE, init_py * TILE_SIZE + UI_TOP, TILE_SIZE, TILE_SIZE };
        SDL_RenderCopy(renderer, tex_pac_r, NULL, &prect);

        if (font_small) {
            SDL_Color white = {255, 255, 255, 255};
            render_text_ttf(renderer, font_small, "SCORE", 16, 10, white, 0);
            render_text_ttf(renderer, font_small, "0000", 16, 34, white, 0);
            render_text_ttf(renderer, font_small, "HIGH SCORE", win_width - 16, 10, white, 1);
            render_text_ttf(renderer, font_small, "6700", win_width - 16, 34, white, 1);
        }

        if (font_large) {
            SDL_Color yellow = {255, 255, 0, 255};
            render_text_ttf(renderer, font_large, "READY!", win_width - 16, win_height - 40, yellow, 1); 
        }

        for(int i = 0; i < 3; i++) {
            SDL_Rect life_rect = { 16 + (i * 32), win_height - 40, 24, 24 };
            SDL_RenderCopy(renderer, tex_pac_l, NULL, &life_rect);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(20);
    }
    sem_post(&shm->sem_ready_done); 
    output_text("\033[?1049h\033[2J");     
    
    char display_init[20][20]; 
    memcpy(display_init, shm->map_grid, sizeof(display_init));
    
    pthread_mutex_lock(&shm->mutex_pacman_state);
    int init_px = shm->pacman_init_x, init_py = shm->pacman_init_y;
    int local_dots[20][20]; memcpy(local_dots, shm->dots_eaten, sizeof(local_dots));
#ifdef ENABLE_POWER_PELLETS
    int local_power[20][20]; memcpy(local_power, shm->power_pellets, sizeof(local_power));
#endif
    pthread_mutex_unlock(&shm->mutex_pacman_state);

    pthread_mutex_lock(&shm->mutex_game_state);
    int local_tombstones[20][20]; memcpy(local_tombstones, shm->tombstones, sizeof(local_tombstones));
    pthread_mutex_unlock(&shm->mutex_game_state);

    pthread_mutex_lock(&shm->mutex_ghost_state);
    char gc_init[4] = {'A', 'B', 'C', 'D'};
    int init_gx[4], init_gy[4];
    for(int i = 0; i < 4; i++) { 
        init_gx[i] = shm->ghost_init_x[i]; init_gy[i] = shm->ghost_init_y[i]; 
    }
    pthread_mutex_unlock(&shm->mutex_ghost_state);

    if(init_px >= 0 && init_py >= 0) display_init[init_py][init_px] = 'P';
    for(int i = 0; i < 4; i++) {
        if(init_gx[i] >= 0 && init_gy[i] >= 0) display_init[init_gy[i]][init_gx[i]] = gc_init[i];
    }
    
    char init_buffer[65536]; int ipos = 0;
    ipos += snprintf(init_buffer + ipos, sizeof(init_buffer) - ipos, "\033[H%s=== PAC-MAN POSIX CONCURRENT ===%s\n\n", COLOR_INFO, COLOR_RESET);
    for(int y = 0; y < shm->map_height; y++) {
        for(int x = 0; x < shm->map_width; x++) {
            char c = display_init[y][x];
            switch(c) {
                case 'P': ipos += snprintf(init_buffer + ipos, sizeof(init_buffer) - ipos, "%s%c%s", COLOR_PACMAN, c, COLOR_RESET); break;
                case 'A': ipos += snprintf(init_buffer + ipos, sizeof(init_buffer) - ipos, "%s%c%s", COLOR_BLINKY, c, COLOR_RESET); break;
                case 'B': ipos += snprintf(init_buffer + ipos, sizeof(init_buffer) - ipos, "%s%c%s", COLOR_PINKY,  c, COLOR_RESET); break;
                case 'C': ipos += snprintf(init_buffer + ipos, sizeof(init_buffer) - ipos, "%s%c%s", COLOR_INKY,   c, COLOR_RESET); break;
                case 'D': ipos += snprintf(init_buffer + ipos, sizeof(init_buffer) - ipos, "%s%c%s", COLOR_CLYDE,  c, COLOR_RESET); break;
                case 'X': ipos += snprintf(init_buffer + ipos, sizeof(init_buffer) - ipos, "%s█%s", shm->is_caso4 ? COLOR_WALL_CASO4 : COLOR_WALL, COLOR_RESET); break;
                case 'O': 
#ifdef ENABLE_POWER_PELLETS
                    if (local_power[y][x]) {
                        ipos += snprintf(init_buffer + ipos, sizeof(init_buffer) - ipos, "%s●%s", COLOR_POWER, COLOR_RESET); 
                        break;
                    }
#endif
                    if (local_tombstones[y][x]) ipos += snprintf(init_buffer + ipos, sizeof(init_buffer) - ipos, "\033[38;2;139;69;19m+\033[0m");
                    else if (local_dots[y][x] == 0) ipos += snprintf(init_buffer + ipos, sizeof(init_buffer) - ipos, "%s.%s",  COLOR_PATH,   COLOR_RESET); 
                    else ipos += snprintf(init_buffer + ipos, sizeof(init_buffer) - ipos, " ");
                    break;
                default:  ipos += snprintf(init_buffer + ipos, sizeof(init_buffer) - ipos, "%c", c); break;
            }
        }
        ipos += snprintf(init_buffer + ipos, sizeof(init_buffer) - ipos, "\n");
    }
    ipos += snprintf(init_buffer + ipos, sizeof(init_buffer) - ipos, "\n⏱️  Tick: 000 | 🏆 Score: 0000 | ❤️  Lives: 3\033[K\n");
    ipos += snprintf(init_buffer + ipos, sizeof(init_buffer) - ipos, "⚡ P0: -- | ⚡ P1: -- | ⚡ P2: --\033[K\n");
    ipos += snprintf(init_buffer + ipos, sizeof(init_buffer) - ipos, "📌 Status: %sREADY!%s\033[K\n", COLOR_PACMAN, COLOR_RESET);
    ipos += snprintf(init_buffer + ipos, sizeof(init_buffer) - ipos, "\n--- Movement History ---\n%sPac-Man (P):%s\n%sBlinky  (A):%s\n%sPinky   (B):%s\n%sInky    (C):%s\n%sClyde   (D):%s\n", COLOR_PACMAN, COLOR_RESET, COLOR_BLINKY, COLOR_RESET, COLOR_PINKY, COLOR_RESET, COLOR_INKY, COLOR_RESET, COLOR_CLYDE, COLOR_RESET);
    
    if (shm->is_caso4) {
        ipos += snprintf(init_buffer + ipos, sizeof(init_buffer) - ipos, "\n--- Ghost Status ---\n%sBlinky  (A):%s Active                    \n%sPinky   (B):%s Active                    \n%sInky    (C):%s Active                    \n%sClyde   (D):%s Active                    \n", COLOR_BLINKY, COLOR_RESET, COLOR_PINKY, COLOR_RESET, COLOR_INKY, COLOR_RESET, COLOR_CLYDE, COLOR_RESET);
        ipos += snprintf(init_buffer + ipos, sizeof(init_buffer) - ipos, "\n--- Kill Feed ---\n1st 💀: --\n2nd 💀: --\n3rd 💀: --\n4th 💀: --\n                                                                      \n");
    } else {
        ipos += snprintf(init_buffer + ipos, sizeof(init_buffer) - ipos, "\n--- Lost Lives ---\n1st 💔: --\n2nd 💔: --\n3rd 💔: --\n                                                                      \n");
    }

    output_text(init_buffer);
    
    struct timespec initial_delay = { .tv_sec = 0, .tv_nsec = TICK_DELAY_MS * 1000000L };
    nanosleep(&initial_delay, NULL);

    sem_post(&shm->sem_ready_done); 
#endif
    
    int over = 0;
    while (1) {
        sem_wait(&shm->sem_renderer_turn);
        
        pthread_mutex_lock(&shm->mutex_game_state);
        over = shm->game_over;
        int current_lives = shm->pacman_lives;
        int is_dying = shm->just_died; 
        
#ifndef MODO_GRAFICO_SDL
        KillEntry local_feed[4];
        memcpy(local_feed, shm->kill_feed, sizeof(local_feed));
        int local_feed_count = shm->kill_feed_count;
#endif
        
#ifdef ENABLE_POWER_PELLETS
        int is_dead[4]; for(int i=0; i<4; i++) is_dead[i] = (shm->ghost_dead_timer[i] > 0);
        int is_scared[4]; for(int i=0; i<4; i++) is_scared[i] = shm->ghost_is_scared[i];
#else
        int is_dead[4] = {0,0,0,0};
        int is_scared[4] = {0,0,0,0};
#endif
        pthread_mutex_unlock(&shm->mutex_game_state);
        
#ifdef MODO_GRAFICO_SDL
        while (SDL_PollEvent(&e)) { 
            if (e.type == SDL_QUIT) {
                pthread_mutex_lock(&shm->mutex_game_state);
                shm->game_over = 1; 
                pthread_mutex_unlock(&shm->mutex_game_state);
                sem_post(&shm->sem_renderer_done); 
                goto CLEANUP;
            } 
        }
        
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); 
        SDL_RenderClear(renderer);
        
        pthread_mutex_lock(&shm->mutex_pacman_state);
        int px = shm->pacman_x, py = shm->pacman_y, score = shm->pacman_score;
        int px_old = shm->pacman_old_x, py_old = shm->pacman_old_y;
        int local_dots[20][20]; memcpy(local_dots, shm->dots_eaten, sizeof(local_dots));
#ifdef ENABLE_POWER_PELLETS
        int local_power[20][20]; memcpy(local_power, shm->power_pellets, sizeof(local_power));
#endif
        pthread_mutex_unlock(&shm->mutex_pacman_state);

        pthread_mutex_lock(&shm->mutex_game_state);
        int local_tombstones[20][20]; memcpy(local_tombstones, shm->tombstones, sizeof(local_tombstones));
        pthread_mutex_unlock(&shm->mutex_game_state);

        pthread_mutex_lock(&shm->mutex_ghost_state);
        int gx[4], gy[4];
        for(int i = 0; i < 4; i++) { gx[i] = shm->ghost_x[i]; gy[i] = shm->ghost_y[i]; }
        pthread_mutex_unlock(&shm->mutex_ghost_state);

        if (px > px_old) pacman_direction = 0;
        else if (px < px_old) pacman_direction = 1;
        else if (py < py_old) pacman_direction = 2;
        else if (py > py_old) pacman_direction = 3;
        
        for(int y = 0; y < shm->map_height; y++) {
            for(int x = 0; x < shm->map_width; x++) {
                SDL_Rect rect = { x * TILE_SIZE, y * TILE_SIZE + UI_TOP, TILE_SIZE, TILE_SIZE };
                if (shm->map_grid[y][x] == 'X') {
                    if (shm->is_caso4) SDL_SetRenderDrawColor(renderer, 69, 179, 169, 255);
                    else SDL_SetRenderDrawColor(renderer, 25, 25, 166, 255);
                    SDL_RenderFillRect(renderer, &rect);
                    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                    SDL_Rect inner = { x * TILE_SIZE + 4, y * TILE_SIZE + UI_TOP + 4, TILE_SIZE - 8, TILE_SIZE - 8 };
                    SDL_RenderFillRect(renderer, &inner);
                } 
#ifdef ENABLE_POWER_PELLETS
                else if (local_power[y][x]) {
                    SDL_SetRenderDrawColor(renderer, 255, 215, 0, 255);
                    SDL_Rect power = { x * TILE_SIZE + (TILE_SIZE/2 - 6), y * TILE_SIZE + UI_TOP + (TILE_SIZE/2 - 6), 12, 12 };
                    SDL_RenderFillRect(renderer, &power);
                }
#endif
                else if (local_dots[y][x] == 0 && shm->map_grid[y][x] == 'O') {
                    SDL_RenderCopy(renderer, tex_dot, NULL, &rect);
                }
            }
        }

        if (!is_dying && !over) {
            for(int i=0; i<4; i++) {
                if (gx[i] >= 0 && gy[i] >= 0) {
                    if (is_dead[i]) continue; 
                    
                    SDL_Rect grect = { gx[i] * TILE_SIZE, gy[i] * TILE_SIZE + UI_TOP, TILE_SIZE, TILE_SIZE };
                    if (is_scared[i]) {
                        SDL_RenderCopy(renderer, tex_scared, NULL, &grect);
                    } 
                    else {
                        if (i == 0) SDL_RenderCopy(renderer, tex_blinky, NULL, &grect);
                        else if (i == 1) SDL_RenderCopy(renderer, tex_pinky, NULL, &grect);
                        else if (i == 2) SDL_RenderCopy(renderer, tex_inky, NULL, &grect);
                        else SDL_RenderCopy(renderer, tex_clyde, NULL, &grect);
                    }
                }
            }
            if (px >= 0 && py >= 0) {
                SDL_Rect prect = { px * TILE_SIZE, py * TILE_SIZE + UI_TOP, TILE_SIZE, TILE_SIZE };
                if (pacman_direction == 0) SDL_RenderCopy(renderer, tex_pac_r, NULL, &prect);
                else if (pacman_direction == 1) SDL_RenderCopy(renderer, tex_pac_l, NULL, &prect);
                else if (pacman_direction == 2) SDL_RenderCopy(renderer, tex_pac_u, NULL, &prect);
                else SDL_RenderCopy(renderer, tex_pac_d, NULL, &prect);
            }
        }

        if (font_small) {
            SDL_Color white = {255, 255, 255, 255};
            char score_str[32]; sprintf(score_str, "%04d", score);
            render_text_ttf(renderer, font_small, "SCORE", 16, 10, white, 0);
            render_text_ttf(renderer, font_small, score_str, 16, 34, white, 0);
            render_text_ttf(renderer, font_small, "HIGH SCORE", win_width - 16, 10, white, 1);
            render_text_ttf(renderer, font_small, "6700", win_width - 16, 34, white, 1);
        }

        for(int i = 0; i < current_lives; i++) {
            SDL_Rect life_rect = { 16 + (i * 32), win_height - 40, 24, 24 };
            SDL_RenderCopy(renderer, tex_pac_l, NULL, &life_rect);
        }
        
        SDL_RenderPresent(renderer);

        if (is_dying) {
            SDL_Delay(250); 
            for (int frame = 0; frame < 60; frame++) {
                while (SDL_PollEvent(&e)) { if (e.type == SDL_QUIT) goto CLEANUP; }
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                SDL_RenderClear(renderer);
                
                for(int y = 0; y < shm->map_height; y++) {
                    for(int x = 0; x < shm->map_width; x++) {
                        SDL_Rect rect = { x * TILE_SIZE, y * TILE_SIZE + UI_TOP, TILE_SIZE, TILE_SIZE };
                        if (shm->map_grid[y][x] == 'X') {
                            if (shm->is_caso4) SDL_SetRenderDrawColor(renderer, 69, 179, 169, 255);
                            else SDL_SetRenderDrawColor(renderer, 25, 25, 166, 255);
                            SDL_RenderFillRect(renderer, &rect);
                            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                            SDL_Rect inner = { x * TILE_SIZE + 4, y * TILE_SIZE + UI_TOP + 4, TILE_SIZE - 8, TILE_SIZE - 8 };
                            SDL_RenderFillRect(renderer, &inner);
                        } else if (local_dots[y][x] == 0 && shm->map_grid[y][x] == 'O') {
                             SDL_RenderCopy(renderer, tex_dot, NULL, &rect);
                        }
                    }
                }
                
                if (font_small) {
                    SDL_Color white = {255, 255, 255, 255};
                    char score_str[32]; sprintf(score_str, "%04d", score);
                    render_text_ttf(renderer, font_small, "SCORE", 16, 10, white, 0);
                    render_text_ttf(renderer, font_small, score_str, 16, 34, white, 0);
                }
                for(int i = 0; i < current_lives; i++) {
                    SDL_Rect life_rect = { 16 + (i * 32), win_height - 40, 24, 24 };
                    SDL_RenderCopy(renderer, tex_pac_l, NULL, &life_rect);
                }

                if (px >= 0 && py >= 0) {
                    int shrink = frame / 2; 
                    if (shrink < TILE_SIZE / 2) {
                        SDL_Rect prect = { px * TILE_SIZE + shrink, py * TILE_SIZE + UI_TOP + shrink, TILE_SIZE - (shrink * 2), TILE_SIZE - (shrink * 2) };
                        double angle = frame * 25.0; 
                        SDL_RenderCopyEx(renderer, tex_pac_u, NULL, &prect, angle, NULL, SDL_FLIP_NONE);
                    }
                }
                SDL_RenderPresent(renderer);
                SDL_Delay(15);
            }

            if (current_lives > 0) {
                Uint32 respawn_start = SDL_GetTicks();
                while (SDL_GetTicks() - respawn_start < 500) {
                    while (SDL_PollEvent(&e)) { if (e.type == SDL_QUIT) goto CLEANUP; }
                    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                    SDL_RenderClear(renderer);

                    for(int y = 0; y < shm->map_height; y++) {
                        for(int x = 0; x < shm->map_width; x++) {
                            SDL_Rect rect = { x * TILE_SIZE, y * TILE_SIZE + UI_TOP, TILE_SIZE, TILE_SIZE };
                            if (shm->map_grid[y][x] == 'X') {
                                if (shm->is_caso4) SDL_SetRenderDrawColor(renderer, 69, 179, 169, 255);
                                else SDL_SetRenderDrawColor(renderer, 25, 25, 166, 255);
                                SDL_RenderFillRect(renderer, &rect);
                                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                                SDL_Rect inner = { x * TILE_SIZE + 4, y * TILE_SIZE + UI_TOP + 4, TILE_SIZE - 8, TILE_SIZE - 8 };
                                SDL_RenderFillRect(renderer, &inner);
                            } else if (local_dots[y][x] == 0 && shm->map_grid[y][x] == 'O') {
                                SDL_RenderCopy(renderer, tex_dot, NULL, &rect);
                            }
                        }
                    }

                    pthread_mutex_lock(&shm->mutex_pacman_state);
                    int spawn_px = shm->pacman_init_x; int spawn_py = shm->pacman_init_y;
                    pthread_mutex_unlock(&shm->mutex_pacman_state);
                    
                    pthread_mutex_lock(&shm->mutex_ghost_state);
                    int spawn_gx[4], spawn_gy[4];
                    for(int i=0; i<4; i++) { spawn_gx[i] = shm->ghost_init_x[i]; spawn_gy[i] = shm->ghost_init_y[i]; }
                    pthread_mutex_unlock(&shm->mutex_ghost_state);

                    for(int i=0; i<4; i++) {
                        if (spawn_gx[i] >= 0 && spawn_gy[i] >= 0) {
                            SDL_Rect grect = { spawn_gx[i] * TILE_SIZE, spawn_gy[i] * TILE_SIZE + UI_TOP, TILE_SIZE, TILE_SIZE };
                            if (i == 0) SDL_RenderCopy(renderer, tex_blinky, NULL, &grect);
                            else if (i == 1) SDL_RenderCopy(renderer, tex_pinky, NULL, &grect);
                            else if (i == 2) SDL_RenderCopy(renderer, tex_inky, NULL, &grect);
                            else SDL_RenderCopy(renderer, tex_clyde, NULL, &grect);
                        }
                    }
                    
                    SDL_Rect prect = { spawn_px * TILE_SIZE, spawn_py * TILE_SIZE + UI_TOP, TILE_SIZE, TILE_SIZE };
                    SDL_RenderCopy(renderer, tex_pac_r, NULL, &prect);

                    if (font_small) {
                        SDL_Color white = {255, 255, 255, 255};
                        char score_str[32]; sprintf(score_str, "%04d", score);
                        render_text_ttf(renderer, font_small, "SCORE", 16, 10, white, 0);
                        render_text_ttf(renderer, font_small, score_str, 16, 34, white, 0);
                        render_text_ttf(renderer, font_small, "HIGH SCORE", win_width - 16, 10, white, 1);
                        render_text_ttf(renderer, font_small, "6700", win_width - 16, 34, white, 1);
                    }
                    for(int i = 0; i < current_lives; i++) {
                        SDL_Rect life_rect = { 16 + (i * 32), win_height - 40, 24, 24 };
                        SDL_RenderCopy(renderer, tex_pac_l, NULL, &life_rect);
                    }

                    if (font_large) {
                        SDL_Color yellow = {255, 255, 0, 255};
                        render_text_ttf(renderer, font_large, "READY!", win_width - 16, win_height - 40, yellow, 1);
                    }

                    SDL_RenderPresent(renderer);
                    SDL_Delay(30);
                }
            }
        }

        if (!over && !is_dying) {
            struct timespec delay = { .tv_sec = 0, .tv_nsec = TICK_DELAY_MS * 1000000L };
            nanosleep(&delay, NULL);
        }
#else
        // MODO ANSI CONSOLA
        char display[20][20]; 
        memcpy(display, shm->map_grid, sizeof(display));
        
        pthread_mutex_lock(&shm->mutex_pacman_state);
        int px = shm->pacman_x, py = shm->pacman_y, score = shm->pacman_score;
        char p_hist[8192]; strncpy(p_hist, shm->pacman_history, 8191); p_hist[8191] = '\0';
        int local_dots[20][20]; memcpy(local_dots, shm->dots_eaten, sizeof(local_dots));
#ifdef ENABLE_POWER_PELLETS
        int local_power[20][20]; memcpy(local_power, shm->power_pellets, sizeof(local_power));
#endif
        pthread_mutex_unlock(&shm->mutex_pacman_state);
        
        pthread_mutex_lock(&shm->mutex_game_state);
        int local_tombstones[20][20]; memcpy(local_tombstones, shm->tombstones, sizeof(local_tombstones));
        int tick = shm->global_tick;
        pthread_mutex_unlock(&shm->mutex_game_state);

        pthread_mutex_lock(&shm->mutex_mailboxes);
        int p0_prio = shm->p0_priority, p1_prio = shm->pacman_priority, p2_prio = shm->enemy_priority;
        pthread_mutex_unlock(&shm->mutex_mailboxes);

        pthread_mutex_lock(&shm->mutex_ghost_state);
        char gc[4] = {'A', 'B', 'C', 'D'};
        int gx[4], gy[4]; char g_hist[4][8192];
        for(int i = 0; i < 4; i++) { 
            gx[i] = shm->ghost_x[i]; gy[i] = shm->ghost_y[i]; 
            strncpy(g_hist[i], shm->ghost_history[i], 8191); g_hist[i][8191] = '\0';
        }
        pthread_mutex_unlock(&shm->mutex_ghost_state);

        if(px >= 0 && py >= 0) display[py][px] = 'P';
        for(int i = 0; i < 4; i++) {
            if(gx[i] >= 0 && gy[i] >= 0 && !is_dead[i]) {
                if (gx[i] == px && gy[i] == py) display[gy[i]][gx[i]] = '!';
                else display[gy[i]][gx[i]] = gc[i];
            }
        }
        
        char frame_buffer[65536]; int pos = 0;
        pos += snprintf(frame_buffer + pos, sizeof(frame_buffer) - pos, "\033[H%s=== PAC-MAN POSIX CONCURRENT ===%s\n\n", COLOR_INFO, COLOR_RESET);
        for(int y = 0; y < shm->map_height; y++) {
            for(int x = 0; x < shm->map_width; x++) {
                char c = display[y][x];
                switch(c) {
                    case 'P': pos += snprintf(frame_buffer + pos, sizeof(frame_buffer) - pos, "%s%c%s", COLOR_PACMAN, c, COLOR_RESET); break;
                    case 'A': pos += snprintf(frame_buffer + pos, sizeof(frame_buffer) - pos, "%s%c%s", is_scared[0] ? COLOR_SCARED : COLOR_BLINKY, c, COLOR_RESET); break;
                    case 'B': pos += snprintf(frame_buffer + pos, sizeof(frame_buffer) - pos, "%s%c%s", is_scared[1] ? COLOR_SCARED : COLOR_PINKY, c, COLOR_RESET); break;
                    case 'C': pos += snprintf(frame_buffer + pos, sizeof(frame_buffer) - pos, "%s%c%s", is_scared[2] ? COLOR_SCARED : COLOR_INKY, c, COLOR_RESET); break;
                    case 'D': pos += snprintf(frame_buffer + pos, sizeof(frame_buffer) - pos, "%s%c%s", is_scared[3] ? COLOR_SCARED : COLOR_CLYDE, c, COLOR_RESET); break;
                    case 'X': pos += snprintf(frame_buffer + pos, sizeof(frame_buffer) - pos, "%s█%s", shm->is_caso4 ? COLOR_WALL_CASO4 : COLOR_WALL, COLOR_RESET); break;
                    case 'O': 
#ifdef ENABLE_POWER_PELLETS
                        if (local_power[y][x]) {
                            pos += snprintf(frame_buffer + pos, sizeof(frame_buffer) - pos, "%s●%s", COLOR_POWER, COLOR_RESET); 
                            break;
                        }
#endif
                        if (local_tombstones[y][x]) pos += snprintf(frame_buffer + pos, sizeof(frame_buffer) - pos, "\033[38;2;139;69;19m+\033[0m");
                        else if (local_dots[y][x] == 0) pos += snprintf(frame_buffer + pos, sizeof(frame_buffer) - pos, "%s.%s",  COLOR_PATH,   COLOR_RESET); 
                        else pos += snprintf(frame_buffer + pos, sizeof(frame_buffer) - pos, " ");
                        break;
                    case '!': pos += snprintf(frame_buffer + pos, sizeof(frame_buffer) - pos, "%s!%s",  COLOR_COLLISION, COLOR_RESET); break;
                    default:  pos += snprintf(frame_buffer + pos, sizeof(frame_buffer) - pos, "%c", c); break;
                }
            }
            pos += snprintf(frame_buffer + pos, sizeof(frame_buffer) - pos, "\n");
        }
        
        pos += snprintf(frame_buffer + pos, sizeof(frame_buffer) - pos, "\n⏱️  Tick: %03d | 🏆 Score: %04d | ❤️  Lives: %d\033[K\n", tick, score, current_lives);
        pos += snprintf(frame_buffer + pos, sizeof(frame_buffer) - pos, "⚡ P0: %02d | ⚡ P1: %02d | ⚡ P2: %02d\033[K\n", p0_prio, p1_prio, p2_prio);
        
        if (over) pos += snprintf(frame_buffer + pos, sizeof(frame_buffer) - pos, "📌 Status: %sFINISHED%s\033[K\n", COLOR_ERROR, COLOR_RESET);
        else if (is_dying) pos += snprintf(frame_buffer + pos, sizeof(frame_buffer) - pos, "📌 Status: %sRESPAWNING...%s\033[K\n", COLOR_BLINKY, COLOR_RESET);
        else pos += snprintf(frame_buffer + pos, sizeof(frame_buffer) - pos, "📌 Status: %sRUNNING%s\033[K\n", COLOR_SUCCESS, COLOR_RESET);

        pos += snprintf(frame_buffer + pos, sizeof(frame_buffer) - pos, "\n--- Movement History ---\n");
        pos += snprintf(frame_buffer + pos, sizeof(frame_buffer) - pos, "%sPac-Man (P):%s %s\033[K\n", COLOR_PACMAN, COLOR_RESET, p_hist);
        pos += snprintf(frame_buffer + pos, sizeof(frame_buffer) - pos, "%sBlinky  (A):%s %s\033[K\n", COLOR_BLINKY, COLOR_RESET, g_hist[0]);
        pos += snprintf(frame_buffer + pos, sizeof(frame_buffer) - pos, "%sPinky   (B):%s %s\033[K\n", COLOR_PINKY,  COLOR_RESET, g_hist[1]);
        pos += snprintf(frame_buffer + pos, sizeof(frame_buffer) - pos, "%sInky    (C):%s %s\033[K\n", COLOR_INKY,   COLOR_RESET, g_hist[2]);
        pos += snprintf(frame_buffer + pos, sizeof(frame_buffer) - pos, "%sClyde   (D):%s %s\033[K\n", COLOR_CLYDE,  COLOR_RESET, g_hist[3]);
        
        if (shm->is_caso4) {
            pos += snprintf(frame_buffer + pos, sizeof(frame_buffer) - pos, "\n--- Ghost Status ---\n");
            char const *n_fant[] = {"Blinky (A)", "Pinky  (B)", "Inky   (C)", "Clyde  (D)"};
            char const *c_fant[] = {COLOR_BLINKY, COLOR_PINKY, COLOR_INKY, COLOR_CLYDE};
            for(int i=0; i<4; i++) {
#ifdef ENABLE_POWER_PELLETS
                if (is_dead[i]) {
                    pos += snprintf(frame_buffer + pos, sizeof(frame_buffer) - pos, "%s%s%s: %s[Respawning in %02d tick(s)]%s\033[K\n", c_fant[i], n_fant[i], COLOR_RESET, COLOR_DEAD, shm->ghost_dead_timer[i], COLOR_RESET);
                } else {
#endif
                    pos += snprintf(frame_buffer + pos, sizeof(frame_buffer) - pos, "%s%s%s: Active                    \033[K\n", c_fant[i], n_fant[i], COLOR_RESET);
#ifdef ENABLE_POWER_PELLETS
                }
#endif
            }
        }

        if (shm->is_caso4) pos += snprintf(frame_buffer + pos, sizeof(frame_buffer) - pos, "\n--- Kill Feed ---\n");
        else pos += snprintf(frame_buffer + pos, sizeof(frame_buffer) - pos, "\n--- Lost Lives ---\n");
        
        int max_lines = shm->is_caso4 ? 4 : 3;
        char const *ordinals[] = {"1st", "2nd", "3rd", "4th"};
        for (int i = 0; i < max_lines; i++) {
            if (i < local_feed_count && local_feed[i].active) {
                pos += snprintf(frame_buffer + pos, sizeof(frame_buffer) - pos, "%s %s: %s (Tick %d)\033[K\n", ordinals[i], shm->is_caso4 ? "💀" : "💔", local_feed[i].name, local_feed[i].tick);
            } else {
                pos += snprintf(frame_buffer + pos, sizeof(frame_buffer) - pos, "%s %s: --\033[K\n", ordinals[i], shm->is_caso4 ? "💀" : "💔");
            }
        }
        
        pos += snprintf(frame_buffer + pos, sizeof(frame_buffer) - pos, "\033[K\n");

        output_text(frame_buffer);
        
        if (is_dying && current_lives > 0) {
            struct timespec death_delay = { .tv_sec = 0, .tv_nsec = TICK_DELAY_MS * 1000000L };
            nanosleep(&death_delay, NULL);
            
            char respawn_buffer[65536]; int rpos = 0;
            char display_respawn[20][20]; memcpy(display_respawn, shm->map_grid, sizeof(display_respawn));
            
            pthread_mutex_lock(&shm->mutex_pacman_state);
            int r_px = shm->pacman_init_x, r_py = shm->pacman_init_y;
            pthread_mutex_unlock(&shm->mutex_pacman_state);
            
            pthread_mutex_lock(&shm->mutex_ghost_state);
            int r_gx[4], r_gy[4];
            for(int i = 0; i < 4; i++) { r_gx[i] = shm->ghost_init_x[i]; r_gy[i] = shm->ghost_init_y[i]; }
            pthread_mutex_unlock(&shm->mutex_ghost_state);

            if(r_px >= 0 && r_py >= 0) display_respawn[r_py][r_px] = 'P';
            for(int i = 0; i < 4; i++) {
                if(r_gx[i] >= 0 && r_gy[i] >= 0) display_respawn[r_gy[i]][r_gx[i]] = gc[i];
            }

            rpos += snprintf(respawn_buffer + rpos, sizeof(respawn_buffer) - rpos, "\033[H%s=== PAC-MAN POSIX CONCURRENT ===%s\n\n", COLOR_INFO, COLOR_RESET);
            for(int y = 0; y < shm->map_height; y++) {
                for(int x = 0; x < shm->map_width; x++) {
                    char c = display_respawn[y][x];
                    switch(c) {
                        case 'P': rpos += snprintf(respawn_buffer + rpos, sizeof(respawn_buffer) - rpos, "%s%c%s", COLOR_PACMAN, c, COLOR_RESET); break;
                        case 'A': rpos += snprintf(respawn_buffer + rpos, sizeof(respawn_buffer) - rpos, "%s%c%s", COLOR_BLINKY, c, COLOR_RESET); break;
                        case 'B': rpos += snprintf(respawn_buffer + rpos, sizeof(respawn_buffer) - rpos, "%s%c%s", COLOR_PINKY,  c, COLOR_RESET); break;
                        case 'C': rpos += snprintf(respawn_buffer + rpos, sizeof(respawn_buffer) - rpos, "%s%c%s", COLOR_INKY,   c, COLOR_RESET); break;
                        case 'D': rpos += snprintf(respawn_buffer + rpos, sizeof(respawn_buffer) - rpos, "%s%c%s", COLOR_CLYDE,  c, COLOR_RESET); break;
                        case 'X': rpos += snprintf(respawn_buffer + rpos, sizeof(respawn_buffer) - rpos, "%s█%s", shm->is_caso4 ? COLOR_WALL_CASO4 : COLOR_WALL, COLOR_RESET); break;
                        case 'O': 
#ifdef ENABLE_POWER_PELLETS
                            if (local_power[y][x]) {
                                rpos += snprintf(respawn_buffer + rpos, sizeof(respawn_buffer) - rpos, "%s●%s", COLOR_POWER, COLOR_RESET); 
                                break;
                            }
#endif
                            if (local_tombstones[y][x]) rpos += snprintf(respawn_buffer + rpos, sizeof(respawn_buffer) - rpos, "\033[38;2;139;69;19m+\033[0m");
                            else if (local_dots[y][x] == 0) rpos += snprintf(respawn_buffer + rpos, sizeof(respawn_buffer) - rpos, "%s.%s",  COLOR_PATH,   COLOR_RESET); 
                            else rpos += snprintf(respawn_buffer + rpos, sizeof(respawn_buffer) - rpos, " ");
                            break;
                        default:  rpos += snprintf(respawn_buffer + rpos, sizeof(respawn_buffer) - rpos, "%c", c); break;
                    }
                }
                rpos += snprintf(respawn_buffer + rpos, sizeof(respawn_buffer) - rpos, "\n");
            }
            rpos += snprintf(respawn_buffer + rpos, sizeof(respawn_buffer) - rpos, "\n⏱️  Tick: %03d | 🏆 Score: %04d | ❤️  Lives: %d\033[K\n", tick, score, current_lives);
            rpos += snprintf(respawn_buffer + rpos, sizeof(respawn_buffer) - rpos, "⚡ P0: %02d | ⚡ P1: %02d | ⚡ P2: %02d\033[K\n", p0_prio, p1_prio, p2_prio);
            rpos += snprintf(respawn_buffer + rpos, sizeof(respawn_buffer) - rpos, "📌 Status: %sRESPAWN%s\033[K\n", COLOR_PACMAN, COLOR_RESET);
            
            rpos += snprintf(respawn_buffer + rpos, sizeof(respawn_buffer) - rpos, "\n--- Movement History ---\n%sPac-Man (P):%s %s\033[K\n%sBlinky  (A):%s %s\033[K\n%sPinky   (B):%s %s\033[K\n%sInky    (C):%s %s\033[K\n%sClyde   (D):%s %s\033[K\n", COLOR_PACMAN, COLOR_RESET, p_hist, COLOR_BLINKY, COLOR_RESET, g_hist[0], COLOR_PINKY, COLOR_RESET, g_hist[1], COLOR_INKY, COLOR_RESET, g_hist[2], COLOR_CLYDE, COLOR_RESET, g_hist[3]);

            if (shm->is_caso4) {
                rpos += snprintf(respawn_buffer + rpos, sizeof(respawn_buffer) - rpos, "\n--- Ghost Status ---\n");
                char const *n_fant[] = {"Blinky (A)", "Pinky  (B)", "Inky   (C)", "Clyde  (D)"};
                char const *c_fant[] = {COLOR_BLINKY, COLOR_PINKY, COLOR_INKY, COLOR_CLYDE};
                for(int i=0; i<4; i++) {
#ifdef ENABLE_POWER_PELLETS
                    if (is_dead[i]) {
                        // LA LIMPIEZA ANSI APLICADA A RESPAWN BUFFER
                        rpos += snprintf(respawn_buffer + rpos, sizeof(respawn_buffer) - rpos, "%s%s%s: %s[Respawning in %02d tick(s)]%s\033[K\n", c_fant[i], n_fant[i], COLOR_RESET, COLOR_DEAD, shm->ghost_dead_timer[i], COLOR_RESET);
                    } else {
#endif
                        rpos += snprintf(respawn_buffer + rpos, sizeof(respawn_buffer) - rpos, "%s%s%s: Active                    \033[K\n", c_fant[i], n_fant[i], COLOR_RESET);
#ifdef ENABLE_POWER_PELLETS
                    }
#endif
                }
            }
            
            if (shm->is_caso4) rpos += snprintf(respawn_buffer + rpos, sizeof(respawn_buffer) - rpos, "\n--- Kill Feed ---\n");
            else rpos += snprintf(respawn_buffer + rpos, sizeof(respawn_buffer) - rpos, "\n--- Lost Lives ---\n");
            
            for (int i = 0; i < max_lines; i++) {
                if (i < local_feed_count && local_feed[i].active) {
                    rpos += snprintf(respawn_buffer + rpos, sizeof(respawn_buffer) - rpos, "%s %s: %s (Tick %d)\033[K\n", ordinals[i], shm->is_caso4 ? "💀" : "💔", local_feed[i].name, local_feed[i].tick);
                } else {
                    rpos += snprintf(respawn_buffer + rpos, sizeof(respawn_buffer) - rpos, "%s %s: --\033[K\n", ordinals[i], shm->is_caso4 ? "💀" : "💔");
                }
            }
            
            output_text(respawn_buffer);
            
            struct timespec respawn_delay = { .tv_sec = 0, .tv_nsec = TICK_DELAY_MS * 1000000L };
            nanosleep(&respawn_delay, NULL);
        }

        if (!over && !is_dying) {
            struct timespec delay = { .tv_sec = 0, .tv_nsec = TICK_DELAY_MS * 1000000L };
            nanosleep(&delay, NULL);
        }
#endif
        
        sem_post(&shm->sem_renderer_done);
        if (over) break; 
    }
    
#ifdef MODO_GRAFICO_SDL
    int quit_gui = 0;
    
    // Capturamos el estado de salud de Pac-Man para saber cómo terminó el juego
    pthread_mutex_lock(&shm->mutex_game_state);
    int final_lives = shm->pacman_lives;
    pthread_mutex_unlock(&shm->mutex_game_state);

    while (!quit_gui) {
        while (SDL_PollEvent(&e)) { if (e.type == SDL_QUIT) quit_gui = 1; }
        
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); 
        SDL_RenderClear(renderer);

        pthread_mutex_lock(&shm->mutex_pacman_state);
        int final_score = shm->pacman_score;
        int local_dots[20][20]; memcpy(local_dots, shm->dots_eaten, sizeof(local_dots));
        pthread_mutex_unlock(&shm->mutex_pacman_state);

        for(int y = 0; y < shm->map_height; y++) {
            for(int x = 0; x < shm->map_width; x++) {
                SDL_Rect rect = { x * TILE_SIZE, y * TILE_SIZE + UI_TOP, TILE_SIZE, TILE_SIZE };
                if (shm->map_grid[y][x] == 'X') {
                    // EL FIX: Mantener el color cian/teal del Caso 4
                    if (shm->is_caso4) SDL_SetRenderDrawColor(renderer, 69, 179, 169, 255);
                    else SDL_SetRenderDrawColor(renderer, 25, 25, 166, 255);
                    
                    SDL_RenderFillRect(renderer, &rect);
                    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                    SDL_Rect inner = { x * TILE_SIZE + 4, y * TILE_SIZE + UI_TOP + 4, TILE_SIZE - 8, TILE_SIZE - 8 };
                    SDL_RenderFillRect(renderer, &inner);
                } else if (local_dots[y][x] == 0 && shm->map_grid[y][x] == 'O') {
                    SDL_RenderCopy(renderer, tex_dot, NULL, &rect);
                }
            }
        }

        if (font_small) {
            SDL_Color white = {255, 255, 255, 255};
            char score_str[32]; sprintf(score_str, "%04d", final_score);
            render_text_ttf(renderer, font_small, "SCORE", 16, 10, white, 0);
            render_text_ttf(renderer, font_small, score_str, 16, 34, white, 0);
            render_text_ttf(renderer, font_small, "HIGH SCORE", win_width - 16, 10, white, 1);
            render_text_ttf(renderer, font_small, "6700", win_width - 16, 34, white, 1);
        }

        // EL FIX: GAME OVER vs TIME OUT
        if (font_large) {
            SDL_Color red = {255, 0, 0, 255};
            if (final_lives <= 0) {
                render_text_ttf(renderer, font_large, "GAME OVER", win_width - 16, win_height - 40, red, 1);
            } else {
                render_text_ttf(renderer, font_large, "TIME OUT", win_width - 16, win_height - 40, red, 1);
            }
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(50); 
    }

CLEANUP:
    SDL_DestroyTexture(tex_pac_r); SDL_DestroyTexture(tex_pac_l); 
    SDL_DestroyTexture(tex_pac_u); SDL_DestroyTexture(tex_pac_d);
    SDL_DestroyTexture(tex_blinky); SDL_DestroyTexture(tex_pinky);
    SDL_DestroyTexture(tex_inky); SDL_DestroyTexture(tex_clyde);
    SDL_DestroyTexture(tex_dot);
    SDL_DestroyTexture(tex_scared);
    if (font_large) TTF_CloseFont(font_large);
    if (font_small) TTF_CloseFont(font_small);
    
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
#else
    int wait_cnt = 0;
    while (shm->sim_time_ms == 0.0 && wait_cnt++ < 100) usleep(1000);
    
    char finish_buf[1024];
    int fpos = 0;
    fpos += snprintf(finish_buf + fpos, sizeof(finish_buf) - fpos,
        "\n%s=====================================================================%s\n"
        "%s🏆 PARTIDA FINALIZADA CON ÉXITO%s\n"
        "%s=====================================================================%s\n"
        "⏰  Tiempo neto de simulación          : %s%.2f ms%s\n"
        "🔥  Operaciones concurrentes procesadas : %s%lld%s\n"
        "%s=====================================================================%s\n\n"
        "👉 Presiona %s[M]%s y ENTER para ver la PANTALLA OFICIAL DE 8 MÉTRICAS.\n"
        "👉 O presiona %s[ENTER]%s directamente para salir a tu terminal WSL...\n",
        COLOR_INFO, COLOR_RESET, COLOR_PACMAN, COLOR_RESET, COLOR_INFO, COLOR_RESET,
        COLOR_POWER, shm->sim_time_ms, COLOR_RESET,
        COLOR_SUCCESS, (long long)shm->stress_counter, COLOR_RESET,
        COLOR_INFO, COLOR_RESET,
        COLOR_PACMAN, COLOR_RESET, COLOR_BLINKY, COLOR_RESET);
    output_text(finish_buf);
    
#ifndef AUTOMATED_BENCHMARK
    if (isatty(STDIN_FILENO)) {
        char ch = getchar();
        if (ch == 'M' || ch == 'm') {
            if (ch != '\n') { int c; while((c = getchar()) != '\n' && c != EOF); }
            char metrics_buf[2048];
            snprintf(metrics_buf, sizeof(metrics_buf),
                "\033[2J\033[H\n"
                "%s=====================================================================%s\n"
                "%s   PANTALLA OFICIAL DE 8 MÉTRICAS DE RENDIMIENTO (RIGOR CIENTÍFICO)  %s\n"
                "%s=====================================================================%s\n\n"
                "  %s1. Tiempo Interno de Simulación%s : %.2f ms\n"
                "  %s2. Tiempo Real Total (Wall Clock)%s : %.4f s\n"
                "  %s3. Tiempo CPU en Modo Usuario%s   : %.4f s\n"
                "  %s4. Tiempo CPU en Modo Kernel%s    : %.4f s\n"
                "  %s5. Consumo Máximo RAM (RSS)%s     : %ld KB\n"
                "  %s6. Cambios Contexto Voluntarios%s : %ld\n"
                "  %s7. Cambios Contexto Involuntarios%s : %ld\n"
                "  %s8. Integridad de Datos (Ops)%s    : %lld / %lld\n\n"
                "%s=====================================================================%s\n"
                "Presiona ENTER para salir definitivamente y volver a la terminal WSL...\n",
                COLOR_INFO, COLOR_RESET, COLOR_PACMAN, COLOR_RESET, COLOR_INFO, COLOR_RESET,
                COLOR_POWER, COLOR_RESET, shm->sim_time_ms,
                COLOR_POWER, COLOR_RESET, shm->wall_clock_s,
                COLOR_POWER, COLOR_RESET, shm->user_cpu_s,
                COLOR_POWER, COLOR_RESET, shm->sys_cpu_s,
                COLOR_POWER, COLOR_RESET, shm->max_rss_kb,
                COLOR_POWER, COLOR_RESET, shm->vol_context_switches,
                COLOR_POWER, COLOR_RESET, shm->invol_context_switches,
                COLOR_SUCCESS, COLOR_RESET, (long long)shm->stress_counter, shm->expected_stress_ops,
                COLOR_INFO, COLOR_RESET);
            output_text(metrics_buf);
            getchar();
        }
    }
#endif
    
    output_text("\033[?1049l"); 
#endif

    munmap(shm, sizeof(SharedMemory));
    return 0;
}