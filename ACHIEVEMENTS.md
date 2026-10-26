# 007: Everything or Nothing achievements

The port's achievement set, shown in the settings window (F1, Achievements) with a pop-up as each unlocks; unlocks are
kept in `achievements.toml` beside the exe. Every achievement is a condition on the game's own memory, written in
[RetroAchievements](https://retroachievements.org)' syntax (`src/common/achievement_set.cpp`), so the set does not
depend on the port's code. Status: **first set**, checked headless by writing the game's records (see "Testing").

## The set (39 achievements, 318 points)

### Missions (30)
One for completing each mission (5 points; Ground Zero 3, the final mission 25), as the game marks it completed. The
MI6 Interlude has no achievement.

| Missions |
| --- |
| Ground Zero, MI6 Training, A Long Way Down, Train Chase, An Old Friend, Sand Storm, Serena St. Germaine, Vertigo, The Ruined Tower, Death of an Agent, A Show of Force, Leap of Faith, Mardi Gras Mayhem |
| The Kiss Kiss Club, Underworld, Death's Door, Battle in the Big Easy, Faded Splendor, The Machinery of Evil, The Pontchartrain Bridge, A Simple Exchange, Red Line, Ambushed, The High Road, Diavolo's Plan, The Platinum War, Dangerous Descent, Red Underground, The Final Card, Everything or Nothing |

### Medals, difficulty and Bond moments (9)
| Title | Description | Points |
| --- | --- | --- |
| Goldfinger | Earn a gold medal | 5 |
| Platinum Standard | Earn a platinum medal | 10 |
| Decorated Agent | Earn silver or better in 10 missions | 10 |
| For Services Rendered | Earn silver or better in 20 missions | 25 |
| Double-0 Status | Complete a mission on 00 Agent | 10 |
| Licence Renewed | Complete Everything or Nothing on 00 Agent | 50 |
| Bond Moment | Find 10 Bond moments | 5 |
| Style and Substance | Find 30 Bond moments | 10 |
| The Name's Bond | Find 60 Bond moments | 25 |

## Memory map

- **The profile** (the save's `GAME.DAT` image): default.xbe holds it at `0x31EC50` once a save is loaded (from +0x6C
  on; the whole file passes through the load buffer `0x652154`). driving.xbe holds it in the launch data it hands back
  to default.xbe (`0x36CAD8`, "LOADER PRESENT" at +8, the profile at +0x1F0: `0x36CCC8`). Each exe checks its own
  copy, so a driving mission's achievement comes as it ends there. +0x78 is a dword that differs from save to save and
  is never 0 in one; +0x68 in the file is the current mission's number from 1.
- **Mission k's record**: 0x24 bytes at profile + 0x84 + 0x24 k, in Mission Select's order (each mission shown reads
  its record there). Read by the accessors at 0x16A00..0x16E60:
  - +0 bit 0: completed (0x16A30; the mission's score counts only when set)
  - +0 bits 1-3: the medal (0x16A00): 1 bronze, 2 silver, 3 gold, 4 platinum; the results code (0x96050) stores a
    better one
  - +0 bits 4-5: the difficulty (0x16A10): 0 Operative, 1 Agent, 2 00 Agent; it scales the score
  - +1 bit 0: the platinum challenge (0x16A60)
  - +6: a 16 bit mask of the Bond moments found (0x16C00 counts it)
  - +8..+0xE: four stats summed for the score (0x16D60), weighted per mission and difficulty (0x13B10)
- **Missions 0-13** were checked against Mission Select with a save that reaches Mardi Gras Mayhem (MI6 Training, the
  MI6 Interlude and record 12 have no medal; record 12's name was not shown, Leap of Faith by the game's text). The rest
  follow the order of the game's text (`action\actdat1.viv`), to be confirmed with a later save.

## Design rules

- An achievement fires on the change that earns it (a record's completed bit going from 0 to 1, a medal or a count
  growing this frame), never on a state that loading a save would also give.
- Every condition holds only when +0x78 was already non-zero the frame before, so the frame a save is loaded awards
  nothing; with the runtime's rule that an achievement is seen unmet once first, loading a finished save awards
  nothing at all.
- The collective ones (medals in 10 and 20 missions, Bond moments) sum the records with AddSource and compare the sum
  with last frame's (SubSource of the deltas) to see it grow.
- Medal counts use the medal's bits 2 and 3 (set by silver, gold and platinum, not bronze), since a sum cannot count
  comparisons.

## Testing

Headless (harness), with Harry's save loaded and Mission Select open: nothing unlocks on loading; writing records
through the harness's `poke` file (record 13 completed with gold; record 14 completed on 00 Agent with its Bond moment
mask full) unlocked exactly Mardi Gras Mayhem, Goldfinger, The Kiss Kiss Club, Double-0 Status and Bond Moment. In
driving.xbe (Serena's launch data), writing record 13 in its copy unlocked Mardi Gras Mayhem and Goldfinger. Still to
see in play: the moment the game itself writes a record at a mission's end (on foot and driving).
