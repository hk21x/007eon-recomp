#!/usr/bin/env bash
# shared: sdk/scripts/recompile.sh (this game's values in scripts/game.env)
exec "$(dirname "$0")/../sdk/scripts/recompile.sh" "$@"
