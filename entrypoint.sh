#!/bin/sh
set -e

APP_UID="${APP_UID:-1001}"
APP_GID="${APP_GID:-1001}"
RUN_AS="${RUN_AS:-$APP_UID:$APP_GID}"
FIX_PERMS_DIRS="${FIX_PERMS_DIRS:-/app /data}"

log() {
  echo "[entrypoint] $*"
}

if [ "$(id -u)" = "0" ] && [ -n "$FIX_PERMS_DIRS" ]; then
  for dir in $FIX_PERMS_DIRS; do
    if [ -d "$dir" ]; then
      log "Fixing permissions on $dir -> $RUN_AS"
      chown -R "$RUN_AS" "$dir" || log "WARN: cannot chown $dir"
    else
      log "Skip $dir (not a directory)"
    fi
  done
fi

if [ "$(id -u)" = "0" ]; then
  log "Starting as $RUN_AS"
  exec gosu "$RUN_AS" "$@"
fi

exec "$@"
