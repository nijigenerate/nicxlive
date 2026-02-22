#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 ]]; then
  echo "Usage: $0 <nicxlive-queue.log> <nijilive-queue.log> [vo_csv] [range_threshold]" >&2
  echo "  vo_csv default: 1947,2044" >&2
  echo "  range_threshold default: 1000.0" >&2
  exit 2
fi

LOG_NICX="$1"
LOG_NIJI="$2"
VO_CSV="${3:-1947,2044}"
RANGE_THRESHOLD="${4:-1000.0}"

analyze_log() {
  local label="$1"
  local log_path="$2"
  awk -v vos="$VO_CSV" -v th="$RANGE_THRESHOLD" -v label="$label" '
    BEGIN {
      nvo = split(vos, voList, ",");
      for (i = 1; i <= nvo; ++i) {
        wanted[voList[i]] = 1;
      }
      exitCode = 0;
    }
    /vo=[0-9]+/ && /d0=\(/ {
      if (!match($0, /vo=[0-9]+/)) next;
      vo = substr($0, RSTART + 3, RLENGTH - 3);
      if (!(vo in wanted)) next;

      if (!match($0, /d0=\([-0-9.eE+]+,[-0-9.eE+]+\)/)) next;
      d0 = substr($0, RSTART + 4, RLENGTH - 5);
      split(d0, xy, ",");
      x = xy[1] + 0.0;
      y = xy[2] + 0.0;

      if (!(vo in seen)) {
        seen[vo] = 1;
        minX[vo] = maxX[vo] = x;
        minY[vo] = maxY[vo] = y;
        firstX[vo] = x;
        firstY[vo] = y;
      }
      if (x < minX[vo]) minX[vo] = x;
      if (x > maxX[vo]) maxX[vo] = x;
      if (y < minY[vo]) minY[vo] = y;
      if (y > maxY[vo]) maxY[vo] = y;
      count[vo]++;
    }
    END {
      printf("[%s]\n", label);
      for (i = 1; i <= nvo; ++i) {
        vo = voList[i];
        if (!(vo in seen)) {
          printf("  VO %s: MISSING\n", vo);
          exitCode = 3;
          continue;
        }
        rangeX = maxX[vo] - minX[vo];
        rangeY = maxY[vo] - minY[vo];
        printf("  VO %s: samples=%d first=(%.6f,%.6f) range=(%.6f,%.6f)\n",
               vo, count[vo], firstX[vo], firstY[vo], rangeX, rangeY);
        if (rangeX > th || rangeY > th) {
          printf("  VO %s: RANGE_EXCEEDED threshold=%.6f\n", vo, th);
          exitCode = 4;
        }
      }
      exit exitCode;
    }
  ' "$log_path"
}

analyze_log "nicxlive" "$LOG_NICX"
analyze_log "nijilive" "$LOG_NIJI"

echo "[ok] shared deform consistency check finished"
