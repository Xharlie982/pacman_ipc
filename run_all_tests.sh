#!/bin/bash
# run_all_tests.sh — Ejecuta los 15 tests y guarda métricas en .csv
# Uso interactivo: ./run_all_tests.sh
# Uso automático:  ./run_all_tests.sh < /dev/null   (o: make run_tests_auto)
# Los tests SDL2 (2, 4, 15) usan SDL_VIDEODRIVER=offscreen para evitar ventanas.

CSV_OUTPUT="resultados_tests.csv"
TMP_GAME=$(mktemp)
CURRENT_TEST=0

echo "test_num,test_desc,caso,sim_time_ms,wall_clock_s,cpu_user_s,cpu_kernel_s,p0_rss_kb,p1_rss_kb,p2_rss_kb,p3_rss_kb,max_rss_proceso,max_rss_kb,mem_total_kb,ctx_vol,ctx_invol,vol_ratio_pct,ops_actual,ops_esperadas,integridad_pct,coord_cost,kernel_per_ctx,throughput_ops_s,render_errors,ratio_parallelismo" > "$CSV_OUTPUT"

log()     { echo "$@"; }
sep()     { log ""; log "================================================================="; log "  TEST $1 — $2"; log "================================================================="; }
compile() {
    echo -n "  Compilando test$1... "
    if make "test$1" > /dev/null 2>&1; then echo "OK"; else echo "ERROR — abortando test$1"; return 1; fi
}

parse_and_csv() {
    local t="$1" tdesc="$2" caso="$3"
    local out; out=$(cat "$TMP_GAME")

    local sim_time wall_clock cpu_user cpu_kernel
    local p0_rss p1_rss p2_rss p3_rss
    local max_rss_proceso max_rss_kb mem_total
    local vol_ratio_pct coord_cost kernel_per_ctx throughput_ops_s
    local render_err ratio

    sim_time=$(echo        "$out" | grep "Tiempo Interno de Simulacion"  | awk -F': ' '{print $2}' | grep -oP '[\d.]+' | head -1)
    wall_clock=$(echo      "$out" | grep "Tiempo Real Total"             | awk -F': ' '{print $2}' | grep -oP '[\d.]+' | head -1)
    cpu_user=$(echo        "$out" | grep "CPU Modo Usuario"              | awk -F': ' '{print $2}' | grep -oP '[\d.]+' | head -1)
    cpu_kernel=$(echo      "$out" | grep "CPU Modo Kernel"               | awk -F': ' '{print $2}' | grep -oP '[\d.]+' | head -1)
    p0_rss=$(echo          "$out" | grep "RAM Pico P0"                   | awk -F': ' '{print $2}' | grep -oP '\d+'    | head -1)
    p1_rss=$(echo          "$out" | grep "RAM Pico P1"                   | awk -F': ' '{print $2}' | grep -oP '\d+'    | head -1)
    p2_rss=$(echo          "$out" | grep "RAM Pico P2"                   | awk -F': ' '{print $2}' | grep -oP '\d+'    | head -1)
    p3_rss=$(echo          "$out" | grep "RAM Pico P3"                   | awk -F': ' '{print $2}' | grep -oP '\d+'    | head -1)
    max_rss_proceso=$(echo "$out" | grep "Proceso Mayor Consumo RAM"     | awk -F': ' '{print $2}' | awk -F' -- ' '{print $1}' | sed 's/[[:space:]]*$//')
    max_rss_kb=$(echo      "$out" | grep "Proceso Mayor Consumo RAM"     | awk -F' -- ' '{print $2}' | grep -oP '\d+' | head -1)
    mem_total=$(echo       "$out" | grep "Estimacion Total Memoria"      | awk -F': ' '{print $2}' | grep -oP '\d+'    | head -1)
    vol_ratio_pct=$(echo   "$out" | grep "Fraccion Coordinacion"         | awk -F': ' '{print $2}' | grep -oP '[\d.]+' | head -1)
    coord_cost=$(echo      "$out" | grep "Costo Coordinacion"            | awk -F': ' '{print $2}' | grep -oP '[\d.]+' | head -1)
    kernel_per_ctx=$(echo  "$out" | grep "Tiempo Kernel por"             | awk -F': ' '{print $2}' | grep -oP '[\d.]+' | head -1)
    throughput_ops_s=$(echo "$out"| grep "Operaciones por Segundo"       | awk -F': ' '{print $2}' | grep -oP '[\d.]+' | head -1)
    render_err=$(echo      "$out" | grep "Fallos de Renderizado"         | awk -F': ' '{print $2}' | grep -oP '\d+'    | head -1)
    ratio=$(echo           "$out" | grep "Ratio de Paralelismo"          | awk -F': ' '{print $2}' | grep -oP '[\d.]+' | head -1)

    local ctx_vol ctx_invol ops_act ops_exp integ_pct

    # Todos los tests usan etiquetas unificadas (RESULTADO ACUMULADO)
    ctx_vol=$(echo   "$out" | grep "Contexto Voluntarios"   | grep -v "Invol" | grep -v "Fraccion" | awk -F': ' '{print $2}' | tr -d ' ')
    ctx_invol=$(echo "$out" | grep "Contexto Involuntarios"                   | awk -F': ' '{print $2}' | tr -d ' ')
    ops_act=$(echo   "$out" | grep "Integridad de Datos"    | grep -oP '\d+(?= /)'   | head -1)
    ops_exp=$(echo   "$out" | grep "Integridad de Datos"    | grep -oP '(?<= / )\d+' | head -1)
    integ_pct=$(echo "$out" | grep "Integridad de Datos"    | grep -oP '[\d.]+(?=%)' | head -1)

    echo "$t,\"$tdesc\",$caso,$sim_time,$wall_clock,$cpu_user,$cpu_kernel,$p0_rss,$p1_rss,$p2_rss,$p3_rss,\"$max_rss_proceso\",$max_rss_kb,$mem_total,$ctx_vol,$ctx_invol,$vol_ratio_pct,$ops_act,$ops_exp,$integ_pct,$coord_cost,$kernel_per_ctx,$throughput_ops_s,$render_err,$ratio" >> "$CSV_OUTPUT"
}

run() {
    log "  → $1"
    case "$CURRENT_TEST" in
        2|4|15) SDL_VIDEODRIVER=offscreen ./scheduler_process "cases/$1" 2>/dev/tty | tee "$TMP_GAME" ;;
        *)      ./scheduler_process "cases/$1" 2>/dev/null | tee "$TMP_GAME" ;;
    esac
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
desc[12]="ANSI sync HEADLESS"
desc[13]="ANSI DISABLE_SYNC HEADLESS"
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
log "Tests completados."
log "  CSV : $CSV_OUTPUT"

if [ -t 0 ]; then
    echo ""
    read -rp "¿Ejecutar IPC benchmark ahora? (s/N): " yn
    if [[ "$yn" =~ ^[Ss]$ ]]; then
        sep "IPC" "Benchmark IPC — mmap vs pipes vs archivo"
        echo -n "  Compilando benchmark... "
        make ipc_benchmark > /dev/null 2>&1 && echo "OK" || echo "ERROR"
        ./ipc_benchmark
    else
        log ""
        log "  Para ejecutar IPC benchmark manualmente:"
        log "     make ipc_benchmark && ./ipc_benchmark"
    fi
fi
