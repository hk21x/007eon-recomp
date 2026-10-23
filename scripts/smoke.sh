#!/usr/bin/env bash
# shared: sdk/scripts/smoke.sh (this game's values in scripts/game.env)
exec "$(dirname "$0")/../sdk/scripts/smoke.sh" "$@"
