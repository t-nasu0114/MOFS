#!/usr/bin/env bash
#
# MOFS バッファキャッシュ スループット計測スクリプト (fio)
#
# 配置: test/os/linux/benchmark/ （Linux + FUSE + fio 依存の手動ベンチマーク）
#
# 本スクリプトは次を一括実行する:
#   1. クリーンビルド（CMake configure + build）
#   2. イメージの mkfs
#   3. FUSE マウント
#   4. fio 計測パターン A〜H
#   5. アンマウント（終了時 trap で確実に実施）
#
# 使い方:
#   RUN_LABEL=cached test/os/linux/benchmark/run_throughput_benchmarks.sh
#
# 環境変数:
#   MOFS_MOUNT              - マウントポイント（既定: /home/t-nasu/work/mnt）
#   MOFS_IMAGE              - バッキングイメージ（既定: /home/t-nasu/work/test/test.img）
#   MOFS_IMAGE_MB           - イメージサイズ MiB（既定: 64）
#   BUILD_DIR               - ビルドディレクトリ名（既定: build）
#   MOFS_ENABLE_BUFFER_CACHE - CMake オプション ON/OFF
#                              RUN_LABEL=cached なら既定 ON、uncached なら既定 OFF
#   RUN_LABEL               - 結果サブディレクトリ名（例: cached, uncached, clock）
#   FIO_RUNTIME             - 各パターンの計測秒数（既定: 30）
#   FIO_WARMUP              - ランプ秒数（fio --ramp_time）。0 でスキップ（既定: 5）
#   RUN_SIZE_SWEEP          - 1 でサイズスイープ（パターン C 派生）も実行（既定: 0）
#   CLEAN_BENCH             - 1 で計測前にベンチ用ファイルを削除（既定: 1）
#   SKIP_CLEAN_BUILD        - 1 でクリーンビルドをスキップ（既定: 0）
#   SKIP_FORMAT             - 1 で mkfs をスキップ（既定: 0）
#   SKIP_BENCHMARK          - 1 で fio 計測のみスキップ（ビルド+マウント確認用）
#
# 出力:
#   test/os/linux/benchmark/results/<RUN_LABEL>_<timestamp>/
#     pattern_*.txt   - 各 fio 生ログ
#     summary.txt     - BW/IOPS 抜粋 + ビルド/マウント情報
#
# MOFS 制約:
#   1 ファイルあたり最大 4 MiB（MOFS_MAX_FILE_DATA_BLOCKS × blk_size）。
#   fio の --size は末尾 EIO を避けるため 4080k（1020 × 4KiB）を使用する。

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../../.." && pwd)"

MOFS_MOUNT="${MOFS_MOUNT:-/home/t-nasu/work/mnt}"
MOFS_IMAGE="${MOFS_IMAGE:-/home/t-nasu/work/test/test.img}"
MOFS_IMAGE_MB="${MOFS_IMAGE_MB:-64}"
BUILD_DIR="${BUILD_DIR:-build}"
RUN_LABEL="${RUN_LABEL:-run}"
case "${RUN_LABEL}" in
    uncached) MOFS_ENABLE_BUFFER_CACHE="${MOFS_ENABLE_BUFFER_CACHE:-OFF}" ;;
    cached)   MOFS_ENABLE_BUFFER_CACHE="${MOFS_ENABLE_BUFFER_CACHE:-ON}" ;;
    *)        MOFS_ENABLE_BUFFER_CACHE="${MOFS_ENABLE_BUFFER_CACHE:-ON}" ;;
esac
# MOFS 最大ファイルサイズに近いが、末尾ブロック EIO を避けるサイズ
FIO_MAX_FILE_SIZE="${FIO_MAX_FILE_SIZE:-4080k}"
# bs=1m では 4080k は 1MiB 整数倍でないため 3072k（3 × 1MiB）を使う
FIO_G_1M_FILE_SIZE="${FIO_G_1M_FILE_SIZE:-3072k}"
FIO_RUNTIME="${FIO_RUNTIME:-30}"
FIO_WARMUP="${FIO_WARMUP:-5}"
RUN_SIZE_SWEEP="${RUN_SIZE_SWEEP:-0}"
CLEAN_BENCH="${CLEAN_BENCH:-1}"
SKIP_CLEAN_BUILD="${SKIP_CLEAN_BUILD:-0}"
SKIP_FORMAT="${SKIP_FORMAT:-0}"
SKIP_BENCHMARK="${SKIP_BENCHMARK:-0}"

TIMESTAMP="$(date +%Y%m%d_%H%M%S)"
OUTPUT_DIR="${SCRIPT_DIR}/results/${RUN_LABEL}_${TIMESTAMP}"

MOFS_PID=""
MOFS_STARTED_BY_SCRIPT=0

# fio 共通設定（全パターンで固定）
FIO_COMMON=(
    --ioengine=psync
    --iodepth=1
    --direct=0
    --time_based=1
    --runtime="${FIO_RUNTIME}"
    --group_reporting=1
    --randrepeat=1
    --randseed=42
)

usage() {
    sed -n '2,38p' "$0" | sed 's/^# \?//'
    echo
    echo "Example:"
    echo "  RUN_LABEL=uncached $0"
    echo "  RUN_LABEL=cached $0"
}

log_step() {
    echo "==> $*" >&2
}

cleanup() {
    local exit_code=$?

    if mountpoint -q "${MOFS_MOUNT}" 2>/dev/null; then
        log_step "unmounting ${MOFS_MOUNT} ..."
        fusermount -u "${MOFS_MOUNT}" 2>/dev/null || umount "${MOFS_MOUNT}" 2>/dev/null || true
    fi

    if [[ "${MOFS_STARTED_BY_SCRIPT}" == "1" && -n "${MOFS_PID}" ]]; then
        if kill -0 "${MOFS_PID}" 2>/dev/null; then
            log_step "stopping mofs (pid ${MOFS_PID}) ..."
            kill "${MOFS_PID}" 2>/dev/null || true
            wait "${MOFS_PID}" 2>/dev/null || true
        fi
    fi

    return "${exit_code}"
}

trap cleanup EXIT INT TERM

require_commands() {
    local cmd
    for cmd in cmake fio fusermount mountpoint dd; do
        if ! command -v "${cmd}" >/dev/null 2>&1; then
            echo "error: required command not found: ${cmd}" >&2
            exit 1
        fi
    done
}

ensure_not_mounted() {
    if mountpoint -q "${MOFS_MOUNT}" 2>/dev/null; then
        log_step "${MOFS_MOUNT} is already mounted; unmounting first ..."
        fusermount -u "${MOFS_MOUNT}" 2>/dev/null || umount "${MOFS_MOUNT}"
    fi
}

clean_build() {
    local build_path="${REPO_ROOT}/${BUILD_DIR}"

    if [[ "${SKIP_CLEAN_BUILD}" == "1" ]]; then
        log_step "SKIP_CLEAN_BUILD=1: skipping clean build"
        if [[ ! -d "${build_path}" ]]; then
            echo "error: build directory not found: ${build_path}" >&2
            exit 1
        fi
        return 0
    fi

    log_step "clean build (MOFS_ENABLE_BUFFER_CACHE=${MOFS_ENABLE_BUFFER_CACHE}) ..."
    rm -rf "${build_path}"
    cmake -S "${REPO_ROOT}" -B "${build_path}" \
        -DCMAKE_BUILD_TYPE=Debug \
        -DMOFS_ENABLE_BUFFER_CACHE="${MOFS_ENABLE_BUFFER_CACHE}"
    cmake --build "${build_path}" -j
}

format_image() {
    local mkfs_bin="${REPO_ROOT}/${BUILD_DIR}/src/os/linux/tools/mkfs/mkfs.mofs"

    if [[ "${SKIP_FORMAT}" == "1" ]]; then
        log_step "SKIP_FORMAT=1: skipping mkfs"
        if [[ ! -f "${MOFS_IMAGE}" ]]; then
            echo "error: image not found: ${MOFS_IMAGE}" >&2
            exit 1
        fi
        return 0
    fi

    log_step "preparing image ${MOFS_IMAGE} (${MOFS_IMAGE_MB} MiB) ..."
    mkdir -p "$(dirname "${MOFS_IMAGE}")"
    dd if=/dev/zero of="${MOFS_IMAGE}" bs=1M count="${MOFS_IMAGE_MB}" status=none

    log_step "formatting image ..."
    "${mkfs_bin}" "${MOFS_IMAGE}"
}

start_mofs() {
    local fuse_bin="${REPO_ROOT}/${BUILD_DIR}/src/os/linux/tools/fuse/mofs"
    local mofs_log="${OUTPUT_DIR}/mofs.log"

    mkdir -p "${MOFS_MOUNT}" "${OUTPUT_DIR}"

    ensure_not_mounted

    log_step "starting mofs FUSE (background) ..."
    "${fuse_bin}" "${MOFS_IMAGE}" "${MOFS_MOUNT}" >>"${mofs_log}" 2>&1 &
    MOFS_PID=$!
    MOFS_STARTED_BY_SCRIPT=1

    local i
    for i in $(seq 1 60); do
        if mountpoint -q "${MOFS_MOUNT}"; then
            log_step "mounted at ${MOFS_MOUNT} (pid ${MOFS_PID})"
            return 0
        fi
        if ! kill -0 "${MOFS_PID}" 2>/dev/null; then
            echo "error: mofs exited before mount succeeded" >&2
            if [[ -f "${mofs_log}" ]]; then
                echo "error: see ${mofs_log}:" >&2
                tail -20 "${mofs_log}" >&2 || true
            fi
            wait "${MOFS_PID}" 2>/dev/null || true
            MOFS_PID=""
            exit 1
        fi
        sleep 0.5
    done

    echo "error: mount timed out: ${MOFS_MOUNT}" >&2
    if [[ -f "${mofs_log}" ]]; then
        echo "error: see ${mofs_log}:" >&2
        tail -20 "${mofs_log}" >&2 || true
    fi
    exit 1
}

log_header() {
    local tag="$1"
    local intent="$2"
    {
        echo "=== ${tag} ==="
        echo "intent: ${intent}"
        echo "run_label: ${RUN_LABEL}"
        echo "mount: ${MOFS_MOUNT}"
        echo "runtime: ${FIO_RUNTIME}s  warmup: ${FIO_WARMUP}s"
        echo "timestamp: $(date -Iseconds)"
        echo "---"
    } >> "${OUTPUT_DIR}/summary.txt"
}

# 単一ファイルベンチ (.bin) を削除。パターン H 前など、容量確保が必要なときだけ呼ぶ。
# キャッシュ ON 時は削除前に sync して dirty ブロックをフラッシュする。
sync_benchmark_mount() {
    sync "${MOFS_MOUNT}" 2>/dev/null || sync
}

# マウントと fio_bench ディレクトリを確認・復旧する（C 終了後に消えることがある）
ensure_benchmark_ready() {
    if ! mountpoint -q "${MOFS_MOUNT}" 2>/dev/null; then
        echo "error: benchmark mount lost: ${MOFS_MOUNT}" >&2
        return 1
    fi
    if [[ -e "${BENCH_DIR}" && ! -d "${BENCH_DIR}" ]]; then
        echo "warning: ${BENCH_DIR} is not a directory; removing ..." >&2
        rm -f "${BENCH_DIR}"
    fi
    mkdir -p "${BENCH_DIR}"
}

ensure_many_small_dir() {
    ensure_benchmark_ready
    if [[ -e "${MANY_DIR}" && ! -d "${MANY_DIR}" ]]; then
        echo "warning: ${MANY_DIR} is not a directory; removing ..." >&2
        rm -f "${MANY_DIR}"
    fi
    mkdir -p "${MANY_DIR}"
}

clean_bench_bin_files() {
    sync_benchmark_mount
    if [[ -d "${BENCH_DIR}" ]]; then
        rm -f "${BENCH_DIR}"/*.bin 2>/dev/null || true
    fi
}

# パターン H 前: ベンチツリーを作り直す（many_small の mkdir EIO 回避）
refresh_bench_tree() {
    sync_benchmark_mount
    if mountpoint -q "${MOFS_MOUNT}" 2>/dev/null; then
        rm -rf "${BENCH_DIR}" 2>/dev/null || true
    fi
    ensure_many_small_dir
}

# 書き込み / mixed 系: ディレクトリ復旧 + 旧 .bin 削除（E/F/G 前）
prepare_write_pattern() {
    local filename="${1:-}"
    ensure_benchmark_ready
    clean_bench_bin_files
    if [[ -n "${filename}" ]]; then
        rm -f "${BENCH_DIR}/${filename}"
    fi
}

# 読み取り系ベンチ用: 計測前にファイルを 1 回書き込みで作成・填充する。
# fio 3.36 には --file_write が無いため、rw=write の別ジョブで代用する。
prepare_fio_files() {
    local tag="$1"
    local logfile="${OUTPUT_DIR}/${tag}.txt"
    shift

    ensure_benchmark_ready

    local fio_args=("$@")
    local arg
    local has_name=0
    local target_dir="" target_file=""
    for arg in "${fio_args[@]}"; do
        if [[ "${arg}" == --name=* ]]; then
            has_name=1
        elif [[ "${arg}" == --directory=* ]]; then
            target_dir="${arg#--directory=}"
        elif [[ "${arg}" == --filename=* ]]; then
            target_file="${arg#--filename=}"
        fi
    done
    if [[ "${has_name}" -eq 0 ]]; then
        fio_args=(--name="${tag}" "${fio_args[@]}")
    fi

    # 対象ファイルのみ削除（パターン A→B 間で *.bin 一括削除しない）
    if [[ -n "${target_dir}" && -n "${target_file}" ]]; then
        rm -f "${target_dir}/${target_file}"
    fi

    echo "[${tag}] preparing test file(s) ..." >&2
    {
        echo "# ${tag} (setup)"
        echo "# setup: fio ${fio_args[*]}"
        echo
    } > "${logfile}"

    if ! fio \
        --ioengine=psync \
        --iodepth=1 \
        --direct=0 \
        --group_reporting=1 \
        "${fio_args[@]}" >> "${logfile}" 2>&1; then
        echo "error: fio setup failed for ${tag} (see ${logfile})" >&2
        return 1
    fi

    echo >> "${logfile}"
    echo "--- benchmark ---" >> "${logfile}"
}

run_fio() {
    local tag="$1"
    local intent="$2"
    local logfile="${OUTPUT_DIR}/${tag}.txt"
    shift 2

    ensure_benchmark_ready

    log_header "${tag}" "${intent}"

    if [[ -f "${logfile}" ]]; then
        {
            echo "# ${tag}"
            echo "# ${intent}"
            echo "# command: fio $*"
            echo
        } >> "${logfile}"
    else
        {
            echo "# ${tag}"
            echo "# ${intent}"
            echo "# command: fio $*"
            echo
        } > "${logfile}"
    fi

    local fio_args=("${FIO_COMMON[@]}")
    if [[ "${FIO_WARMUP}" -gt 0 ]]; then
        fio_args+=(--ramp_time="${FIO_WARMUP}")
    fi

    echo "[${tag}] measuring ${FIO_RUNTIME}s (ramp ${FIO_WARMUP}s) ..." >&2
    # MOFS 最大ファイル末尾で fio が EIO を返すことがあるが、大部分の計測は有効
    fio "${fio_args[@]}" "$@" >> "${logfile}" 2>&1 || true

    if grep -q 'func=io_u error' "${logfile}"; then
        echo "warning: ${tag} had fio I/O errors (see ${logfile})" >&2
    fi

    grep -E '^(  (read|write):|   READ:|  WRITE:)' "${logfile}" >> "${OUTPUT_DIR}/summary.txt" || true
    echo >> "${OUTPUT_DIR}/summary.txt"
}

prepare_bench_dirs() {
    ensure_benchmark_ready
}

clean_bench_files() {
    if [[ "${CLEAN_BENCH}" != "1" ]]; then
        return 0
    fi
    echo "cleaning bench files under ${BENCH_DIR} ..." >&2
    ensure_benchmark_ready
    clean_bench_bin_files
}

# ---------------------------------------------------------------------------
# パターン A: シーケンシャル read（cold / one-pass 相当）
#
# 意図:
#   ワーキングセット >> キャッシュ（64 × 4KiB = 256KiB）の one-pass 読み。
#   scan 耐性・read-ahead の効果を見る。置換アルゴリズム単体の差は小さめ。
# ---------------------------------------------------------------------------
pattern_a_seq_read_cold() {
    prepare_fio_files "pattern_a_seq_read_cold" \
        --directory="${BENCH_DIR}" \
        --rw=write \
        --bs=4k \
        --size="${FIO_MAX_FILE_SIZE}" \
        --filename=bench_a_4m.bin
    run_fio "pattern_a_seq_read_cold" \
        "Sequential read, ~4MiB file (≈16× cache), bs=4k. Scan / read-ahead evaluation." \
        --name=pattern_a \
        --directory="${BENCH_DIR}" \
        --rw=read \
        --bs=4k \
        --size="${FIO_MAX_FILE_SIZE}" \
        --filename=bench_a_4m.bin
}

# ---------------------------------------------------------------------------
# パターン B: シーケンシャル read（hot / キャッシュに収まる）
#
# 意図:
#   128KiB（キャッシュの約半分）をループ再読。ほぼ全ヒットの上限性能。
#   アルゴリズム間差は小さい参照点。
# ---------------------------------------------------------------------------
pattern_b_seq_read_hot() {
    prepare_fio_files "pattern_b_seq_read_hot" \
        --directory="${BENCH_DIR}" \
        --rw=write \
        --bs=4k \
        --size=128k \
        --filename=bench_b_128k.bin
    run_fio "pattern_b_seq_read_hot" \
        "Sequential read, 128KiB loop, bs=4k. Upper bound with cache hits." \
        --name=pattern_b \
        --directory="${BENCH_DIR}" \
        --rw=read \
        --bs=4k \
        --size=128k \
        --filename=bench_b_128k.bin
}

# ---------------------------------------------------------------------------
# パターン C: シーケンシャル read（中程度プレッシャー）★置換比較の本命
#
# 意図:
#   512KiB（キャッシュの約 2 倍）をループ再読。
#   追い出しが発生し LRU / Clock / 2Q / メタデータ予約の差が出やすい。
# ---------------------------------------------------------------------------
pattern_c_seq_read_pressure() {
    prepare_fio_files "pattern_c_seq_read_pressure" \
        --directory="${BENCH_DIR}" \
        --rw=write \
        --bs=4k \
        --size=512k \
        --filename=bench_c_512k.bin
    run_fio "pattern_c_seq_read_pressure" \
        "Sequential read, 512KiB loop, bs=4k. Primary cache replacement comparison." \
        --name=pattern_c \
        --directory="${BENCH_DIR}" \
        --rw=read \
        --bs=4k \
        --size=512k \
        --filename=bench_c_512k.bin
}

# ---------------------------------------------------------------------------
# パターン C 派生: サイズスイープ（RUN_SIZE_SWEEP=1 時）
#
# 意図:
#   ファイルサイズだけ変え、キャッシュ境界（256KiB 前後）での性能変化を見る。
# ---------------------------------------------------------------------------
pattern_c_sweep_size() {
    local size
    for size in 128k 256k 512k 1m "${FIO_MAX_FILE_SIZE}"; do
        prepare_fio_files "pattern_c_sweep_${size}" \
            --directory="${BENCH_DIR}" \
            --rw=write \
            --bs=4k \
            --size="${size}" \
            --filename="bench_sweep_${size}.bin"
        run_fio "pattern_c_sweep_${size}" \
            "Sequential read size sweep (${size}), bs=4k. Cache boundary profiling." \
            --name="pattern_c_sweep_${size}" \
            --directory="${BENCH_DIR}" \
            --rw=read \
            --bs=4k \
            --size="${size}" \
            --filename="bench_sweep_${size}.bin"
    done
}

# ---------------------------------------------------------------------------
# パターン D: ランダム read（ファイル内）
#
# 意図:
#   4MiB 内の 4KiB ランダム read。局所性と部分ヒット。
# ---------------------------------------------------------------------------
pattern_d_randread() {
    prepare_fio_files "pattern_d_randread" \
        --directory="${BENCH_DIR}" \
        --rw=write \
        --bs=4k \
        --size="${FIO_MAX_FILE_SIZE}" \
        --filename=bench_d_4m.bin
    run_fio "pattern_d_randread" \
        "Random read within ~4MiB file, bs=4k." \
        --name=pattern_d \
        --directory="${BENCH_DIR}" \
        --rw=randread \
        --bs=4k \
        --size="${FIO_MAX_FILE_SIZE}" \
        --filename=bench_d_4m.bin \
        --randrepeat=1 \
        --randseed=42
}

# ---------------------------------------------------------------------------
# パターン E: シーケンシャル write
#
# 意図:
#   write-back + dirty 追い出し。flush / バッチ書き出しの効果もここで見る。
# ---------------------------------------------------------------------------
pattern_e_seq_write() {
    prepare_write_pattern bench_e_4m.bin
    run_fio "pattern_e_seq_write" \
        "Sequential write, ~4MiB, bs=4k. Write-back and dirty eviction." \
        --name=pattern_e \
        --directory="${BENCH_DIR}" \
        --rw=write \
        --bs=4k \
        --size="${FIO_MAX_FILE_SIZE}" \
        --filename=bench_e_4m.bin \
        --create_on_open=1
}

# ---------------------------------------------------------------------------
# パターン F: ランダム write
#
# 意図:
#   dirty ブロックが散在し、追い出し時 flush が多発しやすい条件。
# ---------------------------------------------------------------------------
pattern_f_randwrite() {
    prepare_write_pattern bench_f_4m.bin
    run_fio "pattern_f_randwrite" \
        "Random write within ~4MiB file, bs=4k. Scattered dirty blocks." \
        --name=pattern_f \
        --directory="${BENCH_DIR}" \
        --rw=randwrite \
        --bs=4k \
        --size="${FIO_MAX_FILE_SIZE}" \
        --filename=bench_f_4m.bin \
        --create_on_open=1
}

# ---------------------------------------------------------------------------
# パターン G: 混合 rw（既存ベンチ相当 + MOFS ブロック粒度版）
#
# 意図:
#   総合スループット。1MiB 版は従来ログとの比較用、4k 版は MOFS 論理ブロック一致。
# ---------------------------------------------------------------------------
pattern_g_mixed_rw_1m() {
    prepare_write_pattern bench_g_1m.bin
    prepare_fio_files "pattern_g_mixed_rw_1m" \
        --directory="${BENCH_DIR}" \
        --rw=write \
        --bs=1m \
        --size="${FIO_G_1M_FILE_SIZE}" \
        --filename=bench_g_1m.bin \
        --create_on_open=1
    run_fio "pattern_g_mixed_rw_1m" \
        "Mixed rw, 3MiB loop, bs=1M. Comparable to prior cached/uncached logs." \
        --name=pattern_g_1m \
        --directory="${BENCH_DIR}" \
        --rw=rw \
        --bs=1m \
        --size="${FIO_G_1M_FILE_SIZE}" \
        --filename=bench_g_1m.bin
}

pattern_g_mixed_rw_4k() {
    prepare_write_pattern bench_g_4k.bin
    prepare_fio_files "pattern_g_mixed_rw_4k" \
        --directory="${BENCH_DIR}" \
        --rw=write \
        --bs=4k \
        --size="${FIO_MAX_FILE_SIZE}" \
        --filename=bench_g_4k.bin \
        --create_on_open=1
    run_fio "pattern_g_mixed_rw_4k" \
        "Mixed rw, ~4MiB loop, bs=4k. Matches MOFS logical block size." \
        --name=pattern_g_4k \
        --directory="${BENCH_DIR}" \
        --rw=rw \
        --bs=4k \
        --size="${FIO_MAX_FILE_SIZE}" \
        --filename=bench_g_4k.bin
}

# ---------------------------------------------------------------------------
# パターン H: 多ファイル・小ファイル random read（メタデータ寄り）
#
# 意図:
#   32 ファイル × 16KiB の randread。path/inode/bitmap 参照が増え、
#   メタデータスロット予約などの効果が出やすい。
#   （128 ファイルは 64MiB イメージ + fio openfiles 制約で不安定なため 32 に縮小）
# ---------------------------------------------------------------------------
pattern_h_many_small_randread() {
    # MOFS ファイル名上限は MOFS_FILENAME_LEN-1 (=27) 文字。--name=h で fio 自動生成名 (h.0.N) を使う。
    # --filename を指定すると nrfiles が無効になるため指定しない。
    ensure_many_small_dir
    prepare_fio_files "pattern_h_many_small_randread" \
        --name=h \
        --directory="${MANY_DIR}" \
        --rw=write \
        --bs=4k \
        --nrfiles=32 \
        --filesize=16k \
        --openfiles=16
    run_fio "pattern_h_many_small_randread" \
        "32 files × 16KiB randread, bs=4k. Metadata-heavy proxy workload." \
        --name=h \
        --directory="${MANY_DIR}" \
        --rw=randread \
        --bs=4k \
        --nrfiles=32 \
        --filesize=16k \
        --openfiles=16 \
        --randrepeat=1 \
        --randseed=42
}

run_benchmarks() {
    BENCH_DIR="${MOFS_MOUNT}/fio_bench"
    MANY_DIR="${BENCH_DIR}/many_small"

    echo "MOFS throughput benchmarks" >&2
    echo "  repo:       ${REPO_ROOT}" >&2
    echo "  mount:      ${MOFS_MOUNT}" >&2
    echo "  image:      ${MOFS_IMAGE}" >&2
    echo "  label:      ${RUN_LABEL}" >&2
    echo "  output:     ${OUTPUT_DIR}" >&2
    echo "  runtime:    ${FIO_RUNTIME}s per pattern" >&2
    echo "  size_sweep: ${RUN_SIZE_SWEEP}" >&2
    echo "  cache:      ${MOFS_ENABLE_BUFFER_CACHE}" >&2
    echo >&2

    mkdir -p "${OUTPUT_DIR}"
    : > "${OUTPUT_DIR}/summary.txt"
    {
        echo "MOFS fio throughput benchmark summary"
        echo "run_label=${RUN_LABEL}"
        echo "repo=${REPO_ROOT}"
        echo "build_dir=${BUILD_DIR}"
        echo "MOFS_ENABLE_BUFFER_CACHE=${MOFS_ENABLE_BUFFER_CACHE}"
        echo "mount=${MOFS_MOUNT}"
        echo "image=${MOFS_IMAGE}"
        echo "runtime=${FIO_RUNTIME}s warmup=${FIO_WARMUP}s"
        echo "started=$(date -Iseconds)"
        echo
    } >> "${OUTPUT_DIR}/summary.txt"

    prepare_bench_dirs
    clean_bench_files

    pattern_a_seq_read_cold
    pattern_b_seq_read_hot
    pattern_c_seq_read_pressure

    if [[ "${RUN_SIZE_SWEEP}" == "1" ]]; then
        pattern_c_sweep_size
    fi

    # 2 つ目以降の 4080k ファイル前: ディレクトリ復旧 + 旧 .bin 削除
    echo "preparing for pattern D+ (refresh bench dir) ..." >&2
    ensure_benchmark_ready
    clean_bench_bin_files

    pattern_d_randread
    pattern_e_seq_write
    pattern_f_randwrite
    pattern_g_mixed_rw_1m
    pattern_g_mixed_rw_4k

    # パターン H 用にベンチツリーを作り直す（64MiB イメージの容量確保）
    echo "freeing space for pattern H ..." >&2
    refresh_bench_tree

    pattern_h_many_small_randread

    {
        echo "finished=$(date -Iseconds)"
        echo "results_dir=${OUTPUT_DIR}"
    } >> "${OUTPUT_DIR}/summary.txt"

    echo >&2
    echo "Benchmarks done. Results: ${OUTPUT_DIR}" >&2
    echo "Summary: ${OUTPUT_DIR}/summary.txt" >&2
}

main() {
    if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
        usage
        exit 0
    fi

    require_commands
    cd "${REPO_ROOT}"

    clean_build
    format_image
    start_mofs

    if [[ "${SKIP_BENCHMARK}" != "1" ]]; then
        run_benchmarks
    else
        log_step "SKIP_BENCHMARK=1: mount only, skipping fio"
    fi

    # unmount + stop mofs via EXIT trap
}

main "$@"
