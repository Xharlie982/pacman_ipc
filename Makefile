CC = gcc
CFLAGS = -Wall -Wextra -pthread -g
LDFLAGS = -lrt -pthread -lSDL2 -lSDL2_image -lSDL2_ttf

ALL = scheduler_process pacman_process enemy_process renderer_process

all: $(ALL)

scheduler_process: scheduler_process.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

pacman_process: pacman_process.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

enemy_process: enemy_process.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

renderer_process: renderer_process.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(ALL)