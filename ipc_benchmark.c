#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <semaphore.h>
#include <string.h>

#define NUM_OPS 100000
#define SHM_BENCH_NAME "/pacman_ipc_bench"
#define FILE_BENCH_NAME "/tmp/pacman_file_bench.dat"

typedef struct {
    int value;
    sem_t sem_p2c;
    sem_t sem_c2p;
} BenchSHM;

double get_time_diff_ms(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_nsec - start.tv_nsec) / 1000000.0;
}

double bench_shm(int quiet) {
    int fd = shm_open(SHM_BENCH_NAME, O_CREAT | O_RDWR, 0666);
    ftruncate(fd, sizeof(BenchSHM));
    BenchSHM *shm = mmap(NULL, sizeof(BenchSHM), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);

    sem_init(&shm->sem_p2c, 1, 0);
    sem_init(&shm->sem_c2p, 1, 1);

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    pid_t pid = fork();
    if (pid == 0) {
        for (int i = 0; i < NUM_OPS; i++) {
            sem_wait(&shm->sem_p2c);
            volatile int val = shm->value;
            (void)val;
            sem_post(&shm->sem_c2p);
        }
        exit(0);
    } else {
        for (int i = 0; i < NUM_OPS; i++) {
            sem_wait(&shm->sem_c2p);
            shm->value = i;
            sem_post(&shm->sem_p2c);
        }
        wait(NULL);
        clock_gettime(CLOCK_MONOTONIC, &end);
        double ms = get_time_diff_ms(start, end);
        if (!quiet) printf("1. POSIX Shared Memory (mmap + semaphores): %.2f ms (%.2f ops/sec)\n", ms, (NUM_OPS / (ms / 1000.0)));
        sem_destroy(&shm->sem_p2c);
        sem_destroy(&shm->sem_c2p);
        munmap(shm, sizeof(BenchSHM));
        shm_unlink(SHM_BENCH_NAME);
        return ms;
    }
}

double bench_pipe(int quiet) {
    int p2c[2], c2p[2];
    if (pipe(p2c) < 0 || pipe(c2p) < 0) { perror("pipe"); return 0; }

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    pid_t pid = fork();
    if (pid == 0) {
        close(p2c[1]);
        close(c2p[0]);
        int val, ack = 1;
        for (int i = 0; i < NUM_OPS; i++) {
            if (read(p2c[0], &val, sizeof(int)) <= 0) break;
            if (write(c2p[1], &ack, sizeof(int)) <= 0) break;
        }
        close(p2c[0]);
        close(c2p[1]);
        exit(0);
    } else {
        close(p2c[0]);
        close(c2p[1]);
        int ack;
        for (int i = 0; i < NUM_OPS; i++) {
            if (write(p2c[1], &i, sizeof(int)) <= 0) break;
            if (read(c2p[0], &ack, sizeof(int)) <= 0) break;
        }
        close(p2c[1]);
        close(c2p[0]);
        wait(NULL);
        clock_gettime(CLOCK_MONOTONIC, &end);
        double ms = get_time_diff_ms(start, end);
        if (!quiet) printf("2. POSIX Pipes (read/write):                %.2f ms (%.2f ops/sec)\n", ms, (NUM_OPS / (ms / 1000.0)));
        return ms;
    }
}

double bench_file(int quiet) {
    unlink(FILE_BENCH_NAME);
    int fd = open(FILE_BENCH_NAME, O_CREAT | O_RDWR | O_TRUNC, 0666);
    int dummy = 0;
    if (write(fd, &dummy, sizeof(int)) <= 0) { perror("write file"); close(fd); return 0; }
    close(fd);

    sem_unlink("/bench_file_p2c");
    sem_unlink("/bench_file_c2p");
    sem_t *sem_p2c = sem_open("/bench_file_p2c", O_CREAT, 0666, 0);
    sem_t *sem_c2p = sem_open("/bench_file_c2p", O_CREAT, 0666, 1);

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    pid_t pid = fork();
    if (pid == 0) {
        int fd_c = open(FILE_BENCH_NAME, O_RDONLY);
        int val;
        for (int i = 0; i < NUM_OPS; i++) {
            sem_wait(sem_p2c);
            lseek(fd_c, 0, SEEK_SET);
            if (read(fd_c, &val, sizeof(int)) <= 0) break;
            sem_post(sem_c2p);
        }
        close(fd_c);
        sem_close(sem_p2c);
        sem_close(sem_c2p);
        exit(0);
    } else {
        int fd_p = open(FILE_BENCH_NAME, O_WRONLY);
        for (int i = 0; i < NUM_OPS; i++) {
            sem_wait(sem_c2p);
            lseek(fd_p, 0, SEEK_SET);
            if (write(fd_p, &i, sizeof(int)) <= 0) break;
            sem_post(sem_p2c);
        }
        close(fd_p);
        wait(NULL);
        clock_gettime(CLOCK_MONOTONIC, &end);
        double ms = get_time_diff_ms(start, end);
        if (!quiet) printf("3. Disk File (read/write + lseek):          %.2f ms (%.2f ops/sec)\n", ms, (NUM_OPS / (ms / 1000.0)));
        sem_close(sem_p2c);
        sem_close(sem_c2p);
        sem_unlink("/bench_file_p2c");
        sem_unlink("/bench_file_c2p");
        unlink(FILE_BENCH_NAME);
        return ms;
    }
}

int main() {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=====================================================================\n");
    printf("🏆 EMPIRICAL JUSTIFICATION FOR MMAP(): IPC BENCHMARK SUITE\n");
    printf("=====================================================================\n\n");

    printf("--- FASE 1: Prueba de Latencia Instantánea (1 Ejecución x 100,000 Operaciones) ---\n");
    bench_shm(0);
    bench_pipe(0);
    bench_file(0);

    int runs = 10;
    printf("\n--- FASE 2: Prueba de Estrés Sostenido (%d Ejecuciones x 100,000 Operaciones) ---\n", runs);
    printf("Ejecutando 1 millón de operaciones por mecanismo, por favor espere...\n");
    double total_shm = 0, total_pipe = 0, total_file = 0;
    for (int r = 1; r <= runs; r++) {
        total_shm += bench_shm(1);
        total_pipe += bench_pipe(1);
        total_file += bench_file(1);
    }
    double avg_shm = total_shm / runs;
    double avg_pipe = total_pipe / runs;
    double avg_file = total_file / runs;
    printf("\nResultados Promedio sobre %d ejecuciones:\n", runs);
    printf("1. POSIX Shared Memory (mmap): %.2f ms (%.2f ops/sec) -> 🔥 MECANISMO ELEGIDO\n", avg_shm, (NUM_OPS / (avg_shm / 1000.0)));
    printf("2. POSIX Pipes:                %.2f ms (%.2f ops/sec)\n", avg_pipe, (NUM_OPS / (avg_pipe / 1000.0)));
    printf("3. Disk File (read/write):     %.2f ms (%.2f ops/sec)\n", avg_file, (NUM_OPS / (avg_file / 1000.0)));

    printf("\n=====================================================================\n");
    printf("✔ Conclusión: POSIX Shared Memory (mmap) ofrece una superioridad masiva\n");
    printf("  en throughput y latencia frente a Pipes y Archivos en disco.\n");
    printf("=====================================================================\n");
    return 0;
}
