#!/usr/bin/env bash
# Prints a pad script for $1 seconds (scripts/soak.sh, SOAK_PAD): the first boot's training level, then over and
# over: a minute of play (cover, lock on and fire, moving, pause and back), Quit from the pause menu, New Game,
# Ground Zero again. Times are vertical blanks (60 a second), from scripts/pad/explore.txt and menus.txt.
set -euo pipefail
end=$(( ${1:?seconds} * 60 ))
t=2600  # the training level is up by about 2000 on a first boot
play() {  # a minute in the level from $t
    for i in 0 1 2; do
        local b=$(( t + i * 1200 ))
        echo "$b BLACK 20"; echo "$(( b + 300 )) LT 240"; echo "$(( b + 400 )) LT+RT 60"
        echo "$(( b + 700 )) LS 120"; echo "$(( b + 850 )) LSL 60"; echo "$(( b + 950 )) START 20"
        echo "$(( b + 1100 )) START 20"
    done
    t=$(( t + 3600 ))
}
again() {  # Quit (Game tab, Quit, Yes), Single Player, New Game, Ground Zero; the level is up about 6200 later
    echo "$t START 20"; echo "$(( t + 200 )) RIGHT 10"; echo "$(( t + 400 )) DOWN 10"; echo "$(( t + 600 )) DOWN 10"
    echo "$(( t + 800 )) A 10"; echo "$(( t + 1100 )) RIGHT 10"; echo "$(( t + 1300 )) A 10"
    echo "$(( t + 3600 )) A 10"; echo "$(( t + 4000 )) A 10"; echo "$(( t + 4800 )) A 10"
    t=$(( t + 7000 ))
}
echo "# soak: $1 seconds"
while [ "$t" -lt "$end" ]; do
    play
    [ "$t" -lt "$end" ] && again
done
