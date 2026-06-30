#!/bin/bash
# run_all_tests.sh — Ejecuta los 15 tests y guarda métricas en .txt y .csv
# Uso: ./run_all_tests.sh
# Nota: Los tests SDL2 (2, 4, 15) abren ventana gráfica que se cierra sola.

OUTPUT="resultados_tests.txt"
CSV_OUTPUT="resultados_tests.csv"
TMP_GAME=$(mktemp)
CURRENT_TEST=0

> "$OUTPUT"
echo "test_num,test_desc,caso,sim_time_ms,wall_clock_s,cpu_user_s,cpu_kernel_s,max_rss_kb,ctx_vol,ctx_invol,ops_actual,ops_esperadas,integridad_pct,render_errors" > "$CSV_OUTPUT"

log()     { echo "$@" | tee -a "$OUTPUT"; }
sep()     { log ""; log "================================================================="; log "  TEST $1 — $2"; log "================================================================="; }
compile() {
    echo -n "  Compilando test$1... "
    if make "test$1" > /dev/null 2>&1; then echo "OK"; else echo "ERROR — abortando test$1"; return 1; fi
}

parse_and_csv() {
    local t="$1" tdesc="$2" caso="$3"
    local out; out=$(cat "$TMP_GAME")
    local ops_act ops_exp integ_pct render_err

    if echo "$out" | grep -q "Ejecuciones completadas"; then
        # === Formato STRESS TEST ===
        local sim_time
        sim_time=$(echo  "$out" | grep "Tiempo acumulado"      | awk -F': ' '{print $2}' | grep -oP '[\d.]+' | head -1)
        ops_act=$(echo   "$out" | grep "de estrés registradas" | grep -oP '\d+(?= /)'    | head -1)
        ops_exp=$(echo   "$out" | grep "de estrés registradas" | grep -oP '(?<= / )\d+'  | head -1)
        integ_pct=$(echo "$out" | grep "de estrés registradas" | grep -oP '[\d.]+(?=%)'  | head -1)
        render_err=$(echo "$out"| grep "Fallos"                | grep -oP '\d+'           | head -1)
        echo "$t,\"$tdesc\",$caso,$sim_time,,,,,,,$ops_act,$ops_exp,$integ_pct,$render_err" >> "$CSV_OUTPUT"
    else
        # === Formato ESTÁNDAR ===
        local sim_time wall_clock cpu_user cpu_kernel max_rss ctx_vol ctx_invol
        sim_time=$(echo   "$out" | grep "Tiempo Interno"         | awk -F': ' '{print $2}' | grep -oP '[\d.]+' | head -1)
        wall_clock=$(echo "$out" | grep "Wall Clock"             | awk -F': ' '{print $2}' | grep -oP '[\d.]+' | head -1)
        cpu_user=$(echo   "$out" | grep "CPU Modo Usuario"       | awk -F': ' '{print $2}' | grep -oP '[\d.]+' | head -1)
        cpu_kernel=$(echo "$out" | grep "CPU Modo Kernel"        | awk -F': ' '{print $2}' | grep -oP '[\d.]+' | head -1)
        max_rss=$(echo    "$out" | grep "RAM.*RSS"               | awk -F': ' '{print $2}' | grep -oP '\d+'    | head -1)
        ctx_vol=$(echo    "$out" | grep "Contexto Voluntarios"   | grep -v "Invol"         | awk -F': ' '{print $2}' | tr -d ' ')
        ctx_invol=$(echo  "$out" | grep "Contexto Involuntarios" | awk -F': ' '{print $2}' | tr -d ' ')
        ops_act=$(echo    "$out" | grep "Integridad"             | grep -oP '\d+(?= /)'    | head -1)
        ops_exp=$(echo    "$out" | grep "Integridad"             | grep -oP '(?<= / )\d+'  | head -1)
        integ_pct=$(echo  "$out" | grep "Integridad"             | grep -oP '[\d.]+(?=%)'  | head -1)
        render_err=$(echo "$out" | grep "Fallos de Renderizado"  | grep -oP '\d+'          | head -1)
        echo "$t,\"$tdesc\",$caso,$sim_time,$wall_clock,$cpu_user,$cpu_kernel,$max_rss,$ctx_vol,$ctx_invol,$ops_act,$ops_exp,$integ_pct,$render_err" >> "$CSV_OUTPUT"
    fi
}

run() {
    log "  → $1"
    echo "" | ./scheduler_process "cases/$1" 2>/dev/null \
        | tee -a "$OUTPUT" | tee "$TMP_GAME"
    parse_and_csv "$CURRENT_TEST" "${desc[$CURRENT_TEST]}" "$1"
    [ -t 0 ] && read -rp "  Presiona [Enter] para continuar... " _ 2>/dev/null || true
}

STD=(caso1 caso2 caso3)
POW=(caso4)

declare -A desc
desc[1]="ANSI sync completo"
desc[2]="SDL2 sync completo"
desc[3]="ANSI DISABLE_SYNC"
desc[4]="SDL2 DISABLE_SYNC"
desc[5]="ONLY_SEMAPHORES"
desc[6]="ONLY_MUTEX"
desc[7]="GHOSTS_FIRST_PRIORITY"
desc[8]="P0_LOWEST_PRIORITY"
desc[9]="BUFFER_SIZE=1"
desc[10]="BUFFER_SIZE=20"
desc[11]="USE_SYSCALL_WRITE"
desc[12]="STRESS 100x sync"
desc[13]="STRESS 100x no-sync"
desc[14]="POWER PELLETS ANSI"
desc[15]="POWER PELLETS SDL2"

log "PAC-MAN POSIX — Ejecución completa de tests"
log "Fecha: $(date)"
log ""

for t in {1..13}; do
    sep $t "${desc[$t]}"
    compile $t || continue
    CURRENT_TEST=$t
    for c in "${STD[@]}"; do run $c; done
done

for t in 14 15; do
    sep $t "${desc[$t]}"
    compile $t || continue
    CURRENT_TEST=$t
    for c in "${POW[@]}"; do run $c; done
done

rm -f "$TMP_GAME"

log ""
log "✓ Tests completados."
log "  Log : $OUTPUT"
log "  CSV : $CSV_OUTPUT"

if [ -t 0 ]; then
    echo ""
    read -rp "¿Ejecutar IPC benchmark ahora? (s/N): " yn
    if [[ "$yn" =~ ^[Ss]$ ]]; then
        sep "IPC" "Benchmark IPC — mmap vs pipes vs archivo"
        echo -n "  Compilando benchmark... "
        make ipc_benchmark > /dev/null 2>&1 && echo "OK" || echo "ERROR"
        ./ipc_benchmark | tee -a "$OUTPUT"
    else
        log ""
        log "  Para ejecutar IPC benchmark manualmente:"
        log "     make ipc_benchmark && ./ipc_benchmark"
    fi
fi
