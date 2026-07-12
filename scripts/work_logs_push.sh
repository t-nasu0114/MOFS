#!/usr/bin/env bash
# Commit and push scratch/work_logs to its separate remote (does not touch MOFS git).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
LOG_DIR="$ROOT/scratch/work_logs"

if [[ ! -d "$LOG_DIR/.git" ]]; then
  echo "scratch/work_logs is not a git repo yet." >&2
  echo "Run: scripts/work_logs_init_remote.sh <git-remote-url>" >&2
  exit 1
fi

if ! git -C "$LOG_DIR" remote get-url origin >/dev/null 2>&1; then
  echo "No origin remote configured under scratch/work_logs." >&2
  exit 1
fi

git -C "$LOG_DIR" add -A

if git -C "$LOG_DIR" diff --cached --quiet; then
  echo "No work-log changes to push."
  exit 0
fi

STAMP="$(date +%F)"
git -C "$LOG_DIR" commit -m "work logs: ${STAMP}"
git -C "$LOG_DIR" push
echo "Pushed work logs for ${STAMP}."
