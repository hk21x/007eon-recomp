# 007: Everything or Nothing recomp - progress log

Newest first. Each entry: date, what changed, what was learned, what's next.

## Status
- Milestone: released 1.0.0 (2026-10-26, tag v1.0.0; the launcher built and published by .github/workflows/release.yml). Windows (Harry, D3D12, RX 7900 XT, 3440x1440): plays on foot and in the
  driving missions up to the last mission, with saves. The open issues of 2026-10-24 (the dark slab, the blue figures,
  the carousel's band, the loading screen's strip) are fixed (Harry, 2026-10-26).
- Done since: widescreen and ultrawide on foot and driving (culling widened: no side pop-in), the driving radar and
  Thermovision, mouse aim and menus, keyboard prompts, modern buttons, sharp 2D, FOV, 60 FPS driving, Quit Game,
  skip intro, the driving 6 channel mix (speech, car, music), the shader cache (no repeat stalls)
- Known issue (2026-10-27): on NVIDIA GPUs MI6 Training's hand to hand room can lose the GPU device (picture stands
  still); unsolved, see the log. Launcher end to end on Windows from an .iso: confirmed by Harry (2026-10-26). Renderers: Direct3D 12 and 11 (both
  supported in the docs). Untested: NTSC discs
- XDK: 5659, D3D8 QFE 4. default.xbe 84 of 250 functions by signature, 50 more by hand in config/eon.toml; driving.xbe
  in config/driving.toml (the shared QFE 4 layout in config/d3d8_qfe4.toml)
- Push buffers: both kinds, drawn (6bded69): recorded and run at once, and recorded ones replayed with fixups
- SDK: the fork's main at 296844d (2026-10-26: eon merged after EoN, PGR1 and PGR2 smoke and PGR1's frame check);
  task branch `eon`
- Smoke: scripts/smoke.sh: SMOKE OK (2026-10-26). Soak: 30 minutes, no traps, memory flat

## Log

### 2026-10-28 - Aftermath markers that reach the dump

Run 4 (XBE_SYNC_GPU=1 XBE_NV_MARKERS=1) froze at 72.7 s, Hung, same fingerprint, stopped at a grid strip
(3957.2965), but its dump held no markers: a marker set with size 0 keeps only its pointer, which the dump leaves
out unless the app resolves it. sdk 08a034a: the marker's 8 bytes are given, so Aftermath copies them, and a lost
device logs the last marker reached ("Aftermath's last marker: list L draw D", from GFSDK_Aftermath_GetData), so the log
alone names the draw. Smoke OK. Next: run 4 again.

### 2026-10-28 - MI6 Training's hang is not a race; a marker per draw

On the L4: XBE_RENDER_THREAD=0 froze at 70.5 s, XBE_SYNC_GPU=1 at 44.3 s, both Hung with fingerprint
0x3cbf0f26bce8a45c. With the sync the CPU waits on each list, and the GPU hung inside a single list (2521 of 6553
draws done, on a grid strip) with nothing else running: the work in one list hangs, whatever the CPU does. sdk 3c730e2:
XBE_NV_MARKERS=1 (test PC with the Aftermath DLL) sets an event marker per draw holding list << 20 | draw; the
decoder (.local/aftermath/tool) prints the last marker each GPU context reached. Smoke OK.
Next: the L4 with XBE_SYNC_GPU=1 XBE_NV_MARKERS=1, then that draw's state.

### 2026-10-28 - MI6 Training's hang: no draw kind stops it; a race?

On the L4 with XBE_SKIP_STRIDES: 28 froze (a grid strip), 32 froze with the stop at the next frame's first
operation, its depth clear (0 draws done), 64 froze; 28,32,64,80 could not be played to the room. All Hung with
fingerprint 0x3cbf0f26bce8a45c, 37 to 54 s after the start. A depth clear cannot take two seconds: the stop is where
the GPU is when the timeout fires, not the work at fault, and no kind of draw is needed for it. Left: a race between
the CPU and the GPU (memory changed while a list reads it) or something timed. sdk f971c77: XBE_SYNC_GPU=1 waits for
every list once sent; with XBE_RENDER_THREAD=0 (drawing on the title's thread) nothing overlaps. Smoke OK.
Next: the L4 with both.

### 2026-10-28 - MI6 Training's hang: not the negative base vertex; bisecting by vertex layout

XBE_REBASE_INDICES=1 on the L4: froze the same, stopped at a grid strip drawn with base 0; Hung, fingerprint
0x3cbf0f26bce8a45c in two runs (deterministic). sdk 4866ee3: XBE_SKIP_STRIDES=list leaves out draws by their
vertex stride, to bisect on the test PC: 28 (the dummy's and hologram meshes, packed normals), 32 (grid strips),
64 (character meshes). Smoke OK.

### 2026-10-28 - MI6 Training's hang: no copies this time; negative base vertices

The L4 run of 1bdcba9 (Harry's EoN save, the game's own Aftermath dump): hung at 53.6 s, device state Hung (not
PageFault), a new fingerprint, no address, shaders or markers; the stop (draw 1770 of 5998, 4 4 4 [4]) had no copies
before it: the copies were not the cause. What every report shares, on D3D11 and D3D12: the stop among indexed draws
with a negative base vertex (DrawNative passes the title's 16 bit indices with base -first: grid strips at about
-5950, character meshes at -8 to -30, one at -635). sdk 9c147bb: XBE_REBASE_INDICES=1 copies those indices to
32 bit with the lowest taken off and draws with base 0 (the headless renderer does not take that path). Smoke OK.
Next: the L4 run with it.

### 2026-10-28 - MI6 Training's page fault: always after two copies

With Aftermath initialised (result 1) the dump still has no address (Nsight: "could not determine a cause";
graphics pipeline busy, every unit idle: the front end). Both runs on the L4 stopped at the same point: DRED
operations 8 8 15 [4], two CopyTextureRegion and a barrier before a strip of 4 indices from base -1000 (42308 bytes,
stride 28) with a DXT3 128x128 texture: the faulting operation is likely one of the copies. sdk 30ff9a9: every
CopyTextureRegion noted with its resources' names, footprint or subresource and box; the report lists those just
before the stop. Smoke OK. Next: Harry's run with it.

### 2026-10-28 - MI6 Training's page fault on Harry's rented NVIDIA L4

Reproduced on a rented NVIDIA L4 (Ada, driver 596.86, Windows Server 2025) with the Aftermath Monitor: the same
dump fingerprint as the player's (10562858334710211147), Error_DMA_PageFault, no address (the Monitor's resource
tracking needs the application to initialise Aftermath). The log's report: the unfinished list stopped at draw 3909
of 4892, after two CopyTextureRegion and a barrier (DRED operations 8 8 15), a strip of 4 indices from base -1000,
stride 28, sampling texture B00A89A0 (DXT3 128x128, 8 levels). Audited: texture levels uploaded with
GetCopyableFootprints sizes, a remade texture's old resource kept in the frame's garbage, the tables cache cleared
with each ring generation. sdk dca8374: Aftermath initialised (markers, resource tracking, shader debug info,
error reporting, call stacks) when NVIDIA's GFSDK_Aftermath_Lib.x64.dll is beside the exe (test PC only; declared
here, no SDK file in the repo). Smoke OK. Next: the dump with it: the faulting address and the resource's name.

### 2026-10-28 - MI6 Training's lost device: a GPU page fault (Aftermath)

The player's NVIDIA Aftermath crash dump (RTX 3080, driver 621.01) decoded here with the Aftermath SDK's decoder
(Harry's download, kept in .local/aftermath: its licence lets the library ship only built into an application, so
neither it nor the tool is committed): Device state Error_DMA_PageFault, engine reset. Not a shader running long:
the GPU read memory it had no page for, which Windows reports as DEVICE_HUNG and AMD's GPUs may read through. The
dump has no address (page fault details need resource tracking: the Aftermath Monitor's option, or the application's).
Audited without finding the fault: allocators reset after their frame's fence, idle textures released after 600
frames into a frame's garbage, late read-back buffers replaced into the frame's garbage, the upload ring flushed past
56 of 64 MB (no constants at its end). sdk 9e5900b: every GPU resource named (upload rings and own uploads,
targets with their guest data, textures with data, size and format, MSAA twins, read-backs, shrunk and feedback
copies), so DRED and Aftermath name the resource at a fault. Smoke OK. Next: a dump with the Monitor's resource
tracking on: the faulting address and the resource.

### 2026-10-27 - MI6 Training's lost device on NVIDIA: stopped, written up (unsolved)

The player's run with XBE_SKIP_TARGET=03170A00 (no draws into the dummy's second target): hung the same (draw 2468
of 6446). Past three attempts at this freeze, so stopped here, per the working rules.

What is known. Two players (RTX 3080 driver 32.0.16.2101, RTX 4060 Ti), never Harry's RX 7900 XT: in MI6 Training's
hand to hand room, as the camera turns to the dummy, Windows resets the GPU for taking over two seconds
(DXGI_ERROR_DEVICE_HUNG, 887A0006); sound and input go on, the last picture stays. Same on D3D11 and D3D12, at 100%
and 50% render scale, with MSAA off, 4x, 8x, with the mission's 2x2 multisample mode applied or not, and with the
dummy's second target's draws left out. The debug layer with GPU based validation reports no error before the hang
(two warnings only). DRED's stop moves with timing: without validation in the room's grid strips (draw 1700 to 2800
of 5700 to 8100), with validation on one character mesh on both PCs (579 indices, 64 byte vertices from
(3.78623, -3.43547, -0.060722)). Every draw around the stops is in range (indices within their vertices).

Ruled out: a presented buffer no draw reached (no log line), CPU writes over the frame buffers (no reloads), the
descriptor ring wrapping within a frame (fixed anyway: sdk 72d8bf2; never reached in these frames), invalid API use
(debug layer), the amount of work (50% scale), the 2x2 mode (XBE_NO_SAMPLE_SCALE), the dummy's target
(XBE_SKIP_TARGET), a runaway shader loop (spreadtap's count is small and its constants zeroed per draw; vertex
programs are straight line, a0 indexing clamped).

What I think is happening: a shader both renderers compile from shaders.cpp (the same HLSL through FXC for D3D11
and D3D12) that NVIDIA's driver runs effectively forever for some input in this scene (a driver fault on a
construct of ours), as no fault of ours shows in the API or the draws. The way to see it: NVIDIA's Aftermath GPU
crash dump (Nsight Graphics' crash dump monitor on the player's PC) names the shader and the instruction the GPU
hung on; or an NVIDIA GPU on hand to bisect with XBE_DRAW_SKIP. Diagnostics left in (eon-1.0): DRED and the lost
device's report, the debug layer to the log (XBE_D3D12_DEBUG), the driver version, XBE_NO_SAMPLE_SCALE and
XBE_SKIP_TARGET.

### 2026-10-27 - MI6 Training's lost device: not the 2x2 mode (main)

The player's run with XBE_NO_SAMPLE_SCALE=1 (the frame buffer "drawn 1x1"): hung the same (draw 2360 of 6314).
So the mission's multisample mode is not the trigger. In every report the draws after the stop include character
meshes drawn into a second target, data 03170A00 (in the earlier capture also a depth target at that data): the
dummy's effect, as the camera turns to it. sdk 6d14cea: XBE_SKIP_TARGET=03170A00 leaves those draws out.
Smoke OK. Next: the player's run with it; if it still hangs, NVIDIA's Aftermath crash dump.

### 2026-10-27 - MI6 Training's lost device: both validated runs stop on one mesh

The second player's run with XBE_D3D12_DEBUG=2 (RTX 4060 Ti, MSAA 4x, aniso 16): no error before the hang either,
and it stopped on the same draw as the first player's validated run (draw 3198 here, 3222 there): an indexed list
of 579 indices, 579 vertices of 64 bytes from (3.78623, -3.43547, -0.060722), program 0x27D209, a character mesh,
followed by the stride 28 meshes. Without validation the GPU runs further ahead, so those stops wandered through the
grid strips. Checked: the generated vertex programs are straight-line (VertexProgramHlsl), a0 indexing clamped to
c[0..191]: no runaway loop in our shaders. Next: the XBE_NO_SAMPLE_SCALE run, then NVIDIA's Aftermath crash dump
(the shader and instruction the GPU hung on).

### 2026-10-27 - MI6 Training's lost device: any scale, both renderers (main)

The player: froze at 50% render scale (D3D12, hung at draw 2820 of 8114, the grid strips again; driver
32.0.16.2101) and on D3D11 at 100% (d3d11: the GPU device was lost: 887A0006). So not the amount of work and not
one renderer's code: the shaders both renderers compile from shaders.cpp, or something in what this mission draws,
on NVIDIA. The generated HLSL's only loop (spreadtap) reads constants zeroed per draw (DrawConstants c{}): no
runaway count. The one thing every freeze log shows this mission set: MultiSampleMode 2 on the frame buffers, drawn
2x2 (State::aa_x, aa_y: sample placement, the HUD's half width, point sprite sizes). sdk f392d7e:
XBE_NO_SAMPLE_SCALE=1 leaves the modes unapplied. Smoke OK. Next: the player's run with it.

### 2026-10-27 - MI6 Training's lost device: valid API use, a GPU timeout (main)

The player's run with XBE_D3D12_DEBUG=2 (debug layer with GPU based validation): no error or corruption message
before the hang, only warnings (clear values unlike the resources' and a depth only pipeline's colour output).
The removal: DXGI_ERROR_DEVICE_HUNG, Windows' timeout (TDR): the GPU took over two seconds. With validation the
stop moved to the characters' meshes (draw 3222 of 6220): where the GPU had got to, not a bad draw. Checked:
spreadtap's loop (n x n taps, n from a constant) is 4 to 6 per axis here, finite. Left: a burst of GPU work as the
camera turns, or an NVIDIA driver fault on one of the generated shaders. sdk d9761d8: the driver version in
the log. Smoke OK. Next: the player tries a 50% render scale (less work) and Direct3D 11, with this build.

### 2026-10-27 - MI6 Training's lost device: not the descriptors; the debug layer into the log (main)

The player's log with 72d8bf2: hung again (887A0006, draw 2063 of 6040, the same run of grid strips, 3 valid
indices), and no "nearly used up" line: the frame never neared its views, so the wrap fix (kept: it was a real
fault) is not this freeze. The draws around and after the stop are the room's ordinary ones (indices in range).
Left: a resource state or synchronisation fault the draws do not show. sdk b88d733: XBE_D3D12_DEBUG=1 the debug
layer, =2 with GPU based validation; its corruption, error and warning messages logged at report level each present
and in the lost device's report. It needs Windows' Graphics Tools. Smoke OK. Next: a run with XBE_D3D12_DEBUG=2 on an
NVIDIA PC to the freeze.

### 2026-10-27 - MI6 Training's lost device: the descriptor ring wrapped within a frame (main, fix to confirm)

A second player's log (RTX 4060 Ti, MSAA 8x) stops in the same run of grid quads (draw 2339 of 6555). Both stop
in frames of 5700 to 7800 draws, more as the camera turns to the enemy. AllocSrvSlots gives a frame 32768 views (4 a
draw when its table is not reused) and on running out started the ring again at 0 within the frame, over views the
frame's earlier draws, recorded and not yet run, still read; its log line was info level. The sampler heap (2048)
started again the same way across frames. sdk 72d8bf2: before a draw records anything, when fewer than 16 views
are left in the frame or 16 sampler slots, the GPU finishes everything recorded (Flush) and the views (and samplers)
start again; the fallback wrap is logged at report level. D3D11's rings discard properly, so its freeze (one player,
no log line then) is not explained by this. Smoke OK. Next: the players' logs: no lost device, and "nearly used up"
lines show the fix is reached.

### 2026-10-27 - MI6 Training's lost device: the stop is an ordinary draw (main)

The next log found the hung list (7798 draws, stopped at 2474): an indexed strip of 4 from base -5812, 4 vertices
of 32 bytes, program 0x27D209, the room's grid quads (as every frame draws thousands of). In the list behind it the
first draw, a grid quad too. So the stop is where the GPU was, not plainly the culprit: the GPU works on many draws
at once. sdk 2f20950: the report also lists the draws after the stop that stand out (another target, 300+
vertices, another primitive, a wide index range) and the 1500 after it by target; indexed draws note their index
range (base added). Smoke OK.

### 2026-10-27 - MI6 Training's lost device: matching DRED's lists (main)

The next log: hung (887A0006) at draw 2751 of a 6542 draw list, "no list of 6542 draws kept": DRED counts every
draw, the renderer's blits and the settings window's too, the notes only the title's. sdk 1116e66: blits noted
with the draws, a kept list matched when it has as many draws or up to 400 fewer (the settings window's, after the
frame's), and the kept lists' sizes logged when none matches. Smoke OK.

### 2026-10-27 - MI6 Training's lost device: the hung list was two back (main)

The next log: hung again (887A0006) at draw 1778 of a 5765 draw list; behind it a one operation list and one of 5758
draws, so the list that hung was not among the two kept. sdk 56974e9: the last six lists' draws kept. Smoke OK.

### 2026-10-27 - MI6 Training's lost device: where the GPU stopped (main)

The player's log with DRED: reason 887A0006 (DXGI_ERROR_DEVICE_HUNG), seen at present; the unfinished list had
1742 of 5709 operations done, 1739 draws done: the GPU hung at the frame's draw 1739 (of 5705). The report's last
40 draws were the frame's end, not that draw. In their two captures the frame's draws near 1739 are a long run of
small additive fans (prim 6, 3 vertices, blend 1 1, textures 82CA0180 128x128 DXT and 82939000, the direct-input
program 0x27D209): a glow or particle effect. sdk b67b216: every draw of the last two command lists kept; the
report finds the unfinished list by its draw count and logs the draws around the stop, each with its buffers, first
vertex and the textures' format and size words. Smoke OK. Next: that log.

### 2026-10-27 - MI6 Training's standing picture: the GPU device is lost (main)

The player's log from 7bfb194: "pipeline creation failed 887A0005" 18 s after the mission began: 887A0005 is
DXGI_ERROR_DEVICE_REMOVED. The second player's (RTX 4060 Ti) shows the same at 255 s. So the picture stands
because the GPU device was lost (on NVIDIA; Harry's AMD has not shown it) and nothing is drawn after; sound and
input go on. The D3D11 freeze is likely the same, unlogged. The earlier diagnostics were LOG_FIRST, which is info
level and not in a release log (only report level and errors are): the read-back and target lines never showed.
sdk 6b6cb80: D3D12 turns DRED on (auto breadcrumbs, page faults) and reports a lost device once at report level
from wherever it is first seen (a command list's close, a GPU wait, a pipeline, a present): the reason, each unfinished
command list's operations done and draws done with the operations around the stop, a page fault's address and the
allocations there, and the last 40 draws (list.number, topology, count, target, textures, shader handles); D3D11
logs a lost device at present. Smoke OK. Next: the log of a freeze with this build: the draw the GPU stopped at.

### 2026-10-27 - MI6 Training's standing picture: more diagnostics (main)

Two players' logs from 3263421 builds (RTX 3080 and RTX 4060 Ti): no "presenting ... no target drew it", so the
front presented is a drawn target and the missing final.bmp has another cause. The player: the picture stops after
the counter attack step, as the camera turns to the enemy. Suspect: the plain set's two frame buffers share their
memory one row apart, a read-back (Sync) of one writes guest memory and updates only its own signature, so the
other can look written by the CPU and be reloaded from guest memory; the reload before a draw is logged (never in
their logs), the one at present was not. sdk 7bfb194: logs that one, read-backs, and the colour targets made.
Smoke OK. Next: a log from a freeze with this build.

### 2026-10-27 - a player's report: MI6 Training's picture stands still (main, diagnostics)

A player (RTX 3080, 2560x1440 144 Hz): in MI6 Training the picture stops after the third hand to hand strike;
sound and controls go on. Also with antialiasing off from the start and with D3D11. Their logs: frames presented
at 60 a second to the end; F3's thread stacks: the main thread in the world's drawing (0x110230 from 0xF5800).
Their two D3D12 captures 4 s apart (124129, 124133): full frames (5025 and 6378 draws) into the plain set's
buffers in turn (0x35B9C4 data 03A04000, 0x35B97C data 03A04A00: 640x480, the second one row into the first, as
src/game/supersample.cpp chooses), and the camera moved between them: the game runs and draws. Neither capture
has final.bmp, which is written only when a target matches the presented front buffer: the front presented was
not a surface the frames drew into, so its picture came from guest memory. Their three memory dumps agree on the
buffer mode (0x35BBF4 = 0x35BF58 = 0, the plain set) and 0x35BF5C = 1 is the frame being recorded (0x604E0).
Headless with Harry's save in the hand to hand room: the dummy knocks Bond down before a strike counts, so the
third strike is not reached. sdk 3263421: D3D12 logs a front buffer no target drew into, with the targets at its
data (on the SDK branch eon-1.0: the release's SDK and this; eon has the pacing work besides).
Next: the player (or Harry) rebuilds main through the launcher and sends the log from a freeze.

### 2026-10-26 - release 1.0.0

Harry: the launcher's game boots from the .iso and his save works once it is at play\hdd (it had been copied a level
off). Notes renamed docs/releases/v1.0.0.md for the tag (the workflow reads docs/releases/<tag>.md), with where an
own build's saves go; the README and notes say Direct3D 11 is supported too. The release workflow had never run:
GAME_TITLE's colon (007: ...) made it invalid YAML; quoted. v1.0.0 (first put on c672d47, which published nothing)
re-tagged on c632d9a with Harry's agreement; the workflow published the release with EoNLauncher.exe. The game's SDK
is the fork's eon branch (1c5282c), public. The SDK's game template leaves GAME_TITLE unquoted too (to quote).
Next: merge eon into the fork's main after EoN, PGR2 and PGR1 smoke and PGR1's frame check; the soak's memory growth
(+25 MB an hour); achievement names after Mardi Gras Mayhem.

### 2026-10-26 - the launcher's game crashed at boot (from an .iso)

Harry's first launcher build crashed at 0.22 s: a null read at guest 4 in FUN_000145d0 (via 0x1F1BE), the object
bond3.flb should have made (FUN_00014550 from FUN_0001ebf0) still null. Reproduced headless with an .iso made from
assets (extract-xiso -c; GAME=.local/iso/eon.iso): the game opened D:\ and then looked only for loose files
(d:\ASSETS\BOND3.FLB, missing) instead of opening d:\ACTION\*.VIV as it does from a folder. Cause (sdk): a handle
opened on the mounted disc image did not mark itself cdrom, so the volume query on D:\ answered as a hard disk's,
and EoN takes a hard disk volume for a development layout of loose files. Fix: disc image handles are cdrom. Headless
from the .iso: the four VIVs opened, frames to the end of a 30 s run, no exception. Smoke (folder): SMOKE OK.
Next: Harry rebuilds through the launcher (the SDK changed, so Build again) and plays from the .iso.

### 2026-10-26 - release checks

- Repo: no game files tracked or in history (EoN and the SDK); .local-tools.log (a Linux build log committed by
  mistake) untracked and ignored.
- SDK: fork main merged into eon (the PGR2 session's launcher music work) and published as fork main (b59a851) after
  EoN, PGR2 and PGR1 smoke and PGR1's frame check (a live race).
- Launcher blockers found and fixed: it took only default.xbe from the player's copy and installed only EoN.exe, so
  driving.xbe was never recompiled nor EoNDriving.exe installed. sdk: game.json "xbes" and "exes" (by default xbe
  and exe); launcher/game.json lists both. launcher_cli on Linux: both xbes written byte for byte, both exes
  installed, the build's tools left out. launcher.bat named the launcher GameLauncher.exe (GAME_SHORT); EoN now.
  release.bat's zip left out EoNDriving.exe.
- docs/releases/v1.0.md written. Soak 30 minutes: no traps, 32 scenes, frames to the end; RSS 106 -> 116 MB (+25 MB
  an hour; the last soak was flat): small, to find later (today's hooks run per bind and per replayed draw).
Next: Harry's launcher run on Windows from the .iso (the one path not testable here), then the release.

### 2026-10-26 - sniper scope: the right bar

Harry: the left of the scope now reaches the edge, the right not. Capture 173024: the right bar is x 464..641, a
pixel past the frame, and the rule allowed half a pixel; now 2 (the radar's tiles are textured and reach well past).
Smoke OK.

### 2026-10-26 - title screen pillarboxed on ultrawide

Harry (capture 173130): "Press START" on ultrawide showed the menu movie stretched at the sides. Harry chose black
bars. The art is four 256x256 DXT tiles (first kilobyte hashed: three distinct), the movie a YUY2 quad (two frames in
turn, not 2D the squeeze takes), and the title clears nothing, so the sides kept whatever was drawn there. On this
screen the tiles come by replayed push buffers and the movie by SwitchTexture, neither of which reached
GameInfo::texture_bound: sdk now reports both (and replayed draws take the hooks' squeeze flags), and
State::blank_format turns draws of a texture format into a black clear. src/common/title_screen.cpp: while the art
has been drawn in the last 200 ms, the movie's draws are a black clear. Headless at 2.39:1: the title pillarboxed in
black; the language screen and the main menu (movie full width) as before. Smoke OK.

### 2026-10-26 - sniper scope on ultrawide

Harry: scoped in on ultrawide, the 3D view showed at both sides past the scope's black mask. His F3 capture (173024):
the mask is untextured black quads (draws 11243..11252, e.g. x 0..176 over the full height), squeezed whole since
DrawSpan (a draw not spanning the frame's width is squeezed whole, for the driving radar's tiles past the left edge).
sdk: an untextured fill ending on one edge and not past it keeps that edge (kSpanFrame), the reference renderer
likewise; the radar's tiles are textured. Headless at 2.39:1, the training level's HUD and the pause menu as before;
smoke OK. Also from Harry: the title screen ("Press START") shows the menu movie stretched at the sides (capture
173130: the movie 81D1A000 full screen, the title art's four 256x256 tiles squeezed to 16:9): a choice for Harry.

### 2026-10-26 - achievements

Harry's saves: the profile is GAME.DAT's image, at 0x652154 (load buffer, the whole file) and live at 0x31EC50 (from
+0x6C) in default.xbe, at 0x36CCC8 in driving.xbe's launch data (0x36CAD8). Mission k's record 0x24 bytes at profile
+ 0x84 + 0x24 k: found by gdb read watchpoints on Mission Select (the accessors 0x16A00..0x16E60, the results code
0x96050) and a probe on 0x16A10 tying each mission shown to its record; byte 0 bit 0 completed, bits 1-3 the medal
(1 bronze .. 4 platinum: Vertigo, The Ruined Tower and Death of an Agent bronze against their gold targets on
screen, the rest gold), bits 4-5 the difficulty; +6 the Bond moments' mask. Mission names 0-13 from Mission Select,
the rest by the game's text order (actdat1.viv). src/common/achievement_set.cpp: 39 achievements, 318 points, each on
the change that earns it and guarded by +0x78 being non-zero last frame (the frame a save loads awards nothing); each
exe checks its own copy. Headless with pokes: loading the save unlocked nothing; the poked records unlocked exactly the
expected five (default.xbe) and two (driving.xbe). ACHIEVEMENTS.md has the set and the map. Next: Harry's play, and
the sniper scope on ultrawide (an F3 capture asked for).

### 2026-10-26 - driving edge pop-in: a wider margin, from Harry's footage

Harry: still popping with the 15% margin. His recording (13:20, 16:9 in a 3440 window), scanned for one-frame jumps
in the outer columns (ffmpeg to grey 320x180, pure Python): at 69.2 s a street light's yellow pool on the grass at
the left fades out over four frames as its lamp leaves the screen, the lit ground still in view; a light whose
source is off the screen lights what is on it, so the culling has to reach well past the edges. Margin now 2.2
times the tangent (headless Serena at 16:9, draws by frame 2700: 695656 none, 706497 at 1.15, 731021 at 1.6,
750752 at 2.2; the frame right). Smoke OK. Harry, Windows (same day): fixed.

### 2026-10-26 - driving pop-in at the screen's edges: a culling margin

Harry: buildings and lights pop at the screen's edges as the car turns, at any speed, at 16:9 and (less) at 4:3;
not with driving_60fps off either. The culling frustum at 0x32A8C0 (logged headless at 16:9: sides (+-0.7505, 0,
0.6609), 82.7 degrees across (+0xBC), top and bottom (0, +-0.5749, 0.8182), near 0.17, far 250) has its sides on
the frame's edges: on a TV the overscan hid them. src/driving/widescreen.cpp now widens the side planes at every
aspect by 15% in tangent on top of the frame's widening (it was ultrawide only). Headless Serena at 16:9: 706497
draws by frame 2700 against 695656, the frame as before; smoke OK. Next: Harry's drive.

### 2026-10-26 - ND1b finishes: disc reads at the console's speed

Harry, Windows: with XBE_DISC_RATE=6 ND1b plays to its end (no stall). Now the default for EoN
(GameInfo::disc_rate = 6, sdk; XBE_DISC_RATE still overrides, 0 for instant reads). Smoke OK (4 minutes, as
before). The pop-in in driving remains (with driving_60fps off too). Next: the pop-in.

### 2026-10-26 - the ND1b stall: disc timing as the next test

Why the first fix crashed: the stream's packet splitter (0x24A420, on a read's completion) tags packets with their
reader but leaves them out of its count when their file slot is being cancelled (state 4); the cancel (0x24B1D0)
deals with them. Live uncounted packets are part of that path, and releasing them from outside left the stream
handing the loader the wrong data. The async layer copes with reads already done when their callback is registered
(0x24C910 calls it at once), so fast completion as such is handled; but the stall comes at the same place in every
run, and here disc reads complete at once where the console's drive takes milliseconds, keeping the 500 KB ring
full whenever the loader cancels and seeks. sdk: XBE_DISC_RATE=6 paces disc reads at 6 MB a second (off by
default). Headless ND1b with it: loads and drives as before; smoke OK. Next: Harry's ND1b with XBE_DISC_RATE=6.

### 2026-10-26 - the ND1b stall: the release fix reverted

Harry's run with src/driving/stream_fix.cpp: at the same point it released 3 packets of reader 1, and the main
thread faulted at once in the asset pump (0x442BA, a read at 34AEEF54) and a second thread too: the packets were
still referenced, not abandoned. Reverted (the stall is back rather than a crash). Also from Harry: the pop-in
happens with driving_60fps off as well. Next: what keeps packets live behind a reader (0x321F0, 0x32120).

### 2026-10-26 - the ND1b stall: abandoned stream packets (fix, untested on Windows)

Harry's guest memory dumps at the stall (F3 with XBE_CAPTURE_DIR), read with sdk/scripts/guestmem.py: the main
thread polls compartment 0x8205AE20 (vtable 0x2BA4D8, its loaded check 0x3EE40) whose 370 assets (vtable 0x2BA150,
0x44290) stop at the 181st: its resource entry (0x82DCC8C0, id 2588) has no buffer, its request 0x5DE (0x7000
bytes) pending at the asset manager's SushiLoader (0x82053290, the stream of data\track\stream\nd1_map.str,
blocks 0x37F5..0x3802, at 0x37FF). The loader step (0x46500, 48 times a tick from the scheduler 0x46C80) takes
packets with 0x24AC20 and gets none: its reader (0x82BCF864) has 0 bytes to read. The stream (STRM 0x82BCF480) is
paused for room (+0x48 = 2): its 500 KB ring is full, and at its read pointer sit two packets of that reader never
released (blocks 36d0 and 36df, an earlier request's first and last), behind the reader's next packet; the ring
cannot move past them. A reader never goes back, so src/driving/stream_fix.cpp (hook on 0x24AC20): when a reader
gets nothing while its stream is paused for room, its packets between the read pointer and its next one are
released with the game's own release (0x24B140, which resumes the reads). Checked against both dumps offline (finds
exactly those two and stops at the reader's next); headless ND1b 150 s releases nothing, smoke OK. How they were
abandoned is not known (the port's disc reads complete at once, so the ring is always full: likely a path the
console's slower reads rarely took). Next: Harry's ND1b to its end.

### 2026-10-26 - the ND1b stall, round two

Harry's rerun with XBE_FILES_LOG=1: stalled again, about 92 s into ND1b, right after the game probed for
actors\anims\nd1c.dat (an end sequence's animations, in misc.viv); no read stuck at a file's end, so that
suspect is out. Headless with a probe: the game's tick (0x3766BC, bumped by thread 0x252950 on each XDK
timeSetEvent callback 0x2529D0, re-armed at 0x265570) ran at 60 a second for 10 minutes of ND1b, and the waiting
threads in the F3 stacks are where they always are between ticks: no evidence there. driving.xbe's main loop
(0x160980) only ends when 0x3165F0 = 2, set by two state classes' destructors (0x78BB0, 0x78CD0, vtables 0x2BB81C
and 0x2BB820); the stalled main thread was in another state's teardown (0x77640, vtable 0x2BBB78) drawing frames.
The mission's end cannot be reached headless. sdk: F3 with XBE_CAPTURE_DIR writes the guest's memory
(scripts/guestmem.py reads it), so the stalled state can be read here. Next: Harry's dump at the stall.

### 2026-10-26 - a driving mission stalls on its loading screen (diagnosis)

Harry (Windows log, F3): New Orleans, ND1a then ND1b (launched from driving.xbe by driving.xbe); about 150 s into
ND1b the screen went black with the loading bar, sound kept going, frames kept coming (30 a second) and no relaunch
followed. driving.xbe's main (0x162280) launches the next image (0x207420: XLaunchNewImage of 0x2D3B78[0 default,
1 driving]) only after the game loop (0x160980, 0x160CA0, 0x15FD10) returns, so the stall is inside the game loop,
most likely a streamed load: the F3 stacks show the main thread drawing and every other thread idle in its wait
(0x264167 from 0x251FBF: the async file system's workers 0x2496F0, 0x23 of them). Headless: ND1b starts from
.local/launch_nd1b.bin (launch_serena.bin with ND1b at 0x428) and streams from driving\streams.viv and nd1bbr.mad;
the end of the mission cannot be reached by a pad script. Suspect: disc reads past a file's end complete with 0
bytes and success (17f1ac0, for Forza), where the console gives STATUS_END_OF_FILE; a loader retrying would wait
forever. sdk: such reads 100 times in a row are logged, and XBE_FILES_LOG=1's short reads reach release logs.
Next: Harry's rerun of ND1b with XBE_FILES_LOG=1 (and F3 at the stall).

### 2026-10-26 - hide_hud

Setting hide_hud (EON_HIDE_HUD=1 headless). Driving: the game's own "hud" option (driving.xbe's profile option
"hud", read by 0x15D070 into the byte 0x2FA279) gates the frame's HUD pass 0xDFCB0 (the radar and meters 0x1F2440
under it) and the objective and hint messages 0x1EC000 and 0x1EC1E0; src/driving/hud.cpp clears it while those run.
Headless Serena drive, frame 1800: the radar and the health bar gone, the scene the same. On foot default.xbe has no
such option: gdb on the harness (a temporary trap on pass-through program draws past 30M draws) gave the 2D's stacks,
all through 0x23210 -> 0xC15B0 -> 0xC6C20 -> 0xC15D0, a UI manager's draw: three layers (on bytes +0x100..+0x102,
element lists at +0x18). In the training level the HUD manager (vtable 0x2C3F80) has 11 elements in layer 0 (the
ammo box) and the tutorial text in another; the pause screen is another manager's, and the game switches the HUD's
layers off itself while paused. src/game/hud.cpp switches that manager's three layers off while it draws. Headless:
the level without HUD or tutorial text; the pause screen, main menu and Single Player screens unchanged. Not checked:
the cutscenes' subtitles (if they are the HUD manager's they go too). Smoke OK, wincheck OK.

### 2026-10-26 - keyboard layout for the driving missions

The driving exe had the on-foot keys (fire on the left mouse button was the gas, RT). The game's own driving map
(driving\misc.viv, the car's table): RT gas, LT brake/reverse, B handbrake, A weapon, X gadget, Y action, BLACK and
d-pad left/right next weapon, WHITE full screen map, right stick down look back and click centre, d-pad up camera,
BACK Bond sense. src/common/game_info.cpp DrivingKeys: W/S gas and brake, A/D or arrows steer, Space handbrake, the
mouse buttons (or Enter, E) weapon and gadget, F action, R next weapon, M map, C look back, V centre, 1 to 4 the
d-pad, Tab Bond sense. eon::kDriving (src/game, src/driving hooks.cpp) picks the layout. Both exes share
settings.toml: the driving exe keeps its changes as [driving_keys] (GameInfo load/save: the runtime's [keys] moved
there), and each writes the other's table back as read. Windows only (the harness has no settings file). Headless
drive identical (501636 draws at frame 2100), smoke OK, wincheck OK. Next: Harry's test; achievements; hide HUD.

### 2026-10-26 - README for the release

README rewritten in PGR2's layout with Harry's banner (docs/img/eon-pc-port.webp): supported release (both xbe
SHA-1s), the launcher and building from source, the keyboard and mouse table (from ShooterKeys), features up to date
(widescreen driving without side pop-in, the driving mix, the shader cache), status and troubleshooting.

### 2026-10-26 - SDK published to the fork's main

The eon branch (the shader cache, the 6 channel mix, the ultrawide DrawSpan and Thermovision viewport, texture
replacement and Sharpen2D, input and quit) merged with fork main (memory best fit; PGR's squeeze_beyond, which
conflicted with DrawSpan: both now flag "squeezed whole" by hud.x negative, the shift moving the pivot hud.y) as
296844d, and fork main fast-forwarded to it. Gate: EoN smoke OK, PGR2 smoke OK, PGR1 smoke OK, PGR1's repro.sh
a live race (timer running, draws climbing). Test trees put back (PGR2 6e84311, smoke-pgr1 2fbeb57, pgr1-recomp
pc-features with its harness rebuilt).

### 2026-10-26 - frame stalls: a shader cache on disk

Harry's Windows logs: past loading, driving has 1 to 5 frames in each 10 s over 25 ms, the longest 50 to 330 ms (the
menus too). Headless, AUDIT=1 over the Serena drive writes each shader when a draw first needs it: 36 at the drive's
start, then new ones 1.6, 4.4, 5, 9, 16 and 17 s in. The D3D12 and D3D11 backends compile each with D3DCompile the
first time (DXC on Linux takes 30 to 60 ms for these vertex shaders; often a vertex and pixel shader together) and
kept nothing between runs, so every session stalled at the same places. Now the compiled blobs are kept in
shadercache.bin beside the exe (tested on Linux with the D3D parts stubbed: written, read back, a cut short tail
trimmed, a foreign file started again); a run finds them and compiles only what it has never seen. The frames lines
now end with how many shaders were compiled in those 10 s, so a stall left with 0 has another cause. Smoke OK.
Harry, Windows (same day): "Much better".

### 2026-10-26 - driving audio: speech and the 6 channel mix

Harry: in the driving missions speech never plays (the music ducks for it) and the car and other sounds pop in and
out. Headless with XBE_DSOUND_LOG=2: no streams at all in driving; one looping 16 bit PCM buffer B00657B0 of 6
channels (made at 0x25782B, no mix bins) the game mixes its music and speech into itself, beside a pool of 181 ADPCM
voices it points at each sound with SetBufferData. The SDK's mixer took a voice's first two channels only (Levels
gave gains for channels 0 and 1, the render read those two), so the centre (speech), LFE and back pair (the car and
ambience as they pan behind) were dropped. Now up to 6 channels, each its voice's run of slots; with no bins a
multichannel voice plays front left, front right, centre, LFE, back left, back right (LFE left out as before), the
centre and back pair 3 dB down into left and right. Headless, the same drive: identical for the first 20 s (stereo),
from the drive on the added channels as loud as the mix (steady, about 700 Hz: the car); clipped samples 214 before,
279 after over 70 s. Also: XBE_DSOUND_LOG's summaries were info level, compiled out of release builds, so Harry's
Windows log showed none; now release level. Smoke OK. Harry, Windows (same day): fixed, and the music with it (it lost the same channels: it is in that buffer).

### 2026-10-26 - driving pop-in fixed: the visibility pass's frustum

Found by following the draws back: the world's draw list (0x14E9D0, entries queued by 0x2C680 with callback
0x2C2B0) is filled by the visibility pass 0x14C460, which walks the scene's quadtree (0x3E830 -> 0x34360) testing
each node's sphere with 0x147300 against the frustum at 0x32A8C0. Logged there: side planes (x, z) (+-0.7505,
0.6609), top and bottom (+-0.5749, 0.8182), near 0.17, far 225..300, the same at 16:9 and 2.39:1. The game makes it
for its 16:9 (widescreen flag 0x35ED54 +0x4C), so what lay past the 16:9 edges was never drawn until it came inside.
src/driving/widescreen.cpp widens the two side planes at the pass's start by the frame's aspect over the game's, in
tangent, once for each frustum the game writes (no compounding). Headless, Serena St. Germaine with throttle held:
722233 draws by frame 2700 at 2.39:1 against 695656 at 16:9 (before: the same at every aspect). The frame as before;
smoke OK.

### 2026-10-26 - driving pop-in: the terrain chunks, stopped

Continued from 2026-10-25 (Harry: carry on). More measured, all temporary and taken out:
- the global projection copy 0x36DA70 (y at +0x14) is read only by 0x2113D0 (its writer) and the per draw transform
  upload (0x2636BC via 0x220D10 / 0x219BC0): not the culling
- widening inside the build (the x multiplier +0x140 during 0x2111E0) instead of after: no change; per frame the
  draws at 4:3 and 6:1 are the same (264.2 and 264.3): the driving scene's culling ignores the view's width outright
- the screen aspect getter 0xBEBC0 (16:9 with the widescreen flag): its callers 0x2E320 and 0xDD4C0 put HUD markers
  on screen (world to screen), not culling; answering the frame's aspect changed no draw
- the scene renderer 0xF1A50's camera argument +0x10 widened: no change
- Harry's two captures (221850 without the far mountain, 221852 with it): the difference is terrain chunks drawn with
  vertex program B0102ED1 and rock texture 82EB0480 (397 and 225 vertices) and their B0102EB1 / B0102581 passes
- the camera's own field of view (camera +0x10, set by 0x14F0A0 from its controller each frame, smoothed by
  0xBF2E0) widened at 0xC9450: draws move a lot, but the value compounds over the frame's calls (the guard against it
  fails with the smoothing) and the view breaks; 0xE14D0 also fades objects by size over that field of view, so a
  wider one drops distant objects
Reading: the terrain chooses its chunks from the camera's field of view (horizontal, the game's 16:9), not from
the projection. A fix would widen that field of view for the terrain's choice only (its reader not yet found:
the chunks' draws come from the scene below 0xEDAE0) or widen the camera's and narrow the projection back, with
0xE14D0's size fade kept on the old value. Stopped here (past 3 approaches); options for Harry.

### 2026-10-25 - driving pop-in: what culls (stopped after 3 approaches)

Driving at 2.39:1 pops objects in at the 16:9 edge. Headless with .local/launch_serena.bin (throttle,
.local/drive_gas2.txt), the cumulative draws at frame 2100 (501636) were compared with one change at a time in the
0x2111E0 hook (temporary, all taken out):
- the field of view argument x1.5: 502549 (culling follows it)
- the y scale (+0x54) after the build /1.5: 502352 (culling follows it)
- the x scale (+0x40, already /k for the widening), the left/right factors +0x178/+0x17C (kept widened or not), the
  top/bottom factors +0x180/+0x184, the stored field of view +0x130, a wider aspect argument with +0x54 put back:
  no change; nor the camera object's field of view (0x81BEBE40, 0x14F0A0's view render)
So the driving culling takes the projection's vertical scale and a width of its own (the 16:9 the game lays out:
0xC9450 sets the aspect 0x2BED4C and a wider field of view when 0x35ED54 +0x4C is set), not the x scale the
widening divides. Readers of +0x54 under a gdb watch: 0x2111E0, 0x2113D0 (copies the projection to the device through
0x2191E0, to 0x36DA70, and view x projection to +0xC0), 0x211A40 (copies it out: from 0x145B30 in 0x14F0A0, 0xBEC70),
0x210CE0 (an orthographic one, not this). Not culling: 0x148F80 (tiled screenshots), 0xE14D0 (fades small far objects
by their size over the field of view), 0xEC080 (not called in these missions). Next: find which copy's y element the
cull reads (watch 0x36DA84 and the 0x211A40 copies' +0x14), then give that test the widened width. Asked Harry.

### 2026-10-25 - ultrawide pop-in: on foot fixed, driving not found

Harry: objects at the sides pop in, right at the 16:9 edge, on foot and driving. On foot: src/game/ultrawide.cpp
widened the factor 0x5AA550 only while the views are drawn (0xF6660) and after 0xF4B60; every frustum the game culls
with is built through 0x61080 with that factor as an argument (0xF5C70's planes, 0xF6170's visibility tests), so
outside those the frustums stayed 16:9. Now src/game/fov.cpp's hook on 0x61080 widens the factor it is given unless it
is the one ultrawide.cpp wrote (eon::WidenedFactor). Headless, the last mission (.local/harrysave3) at 2.39:1:
24.61M draws by frame 8700 against 23.20M without (6% more drawn, the sides). Smoke OK.
Driving: not found. With Harry's new save (.local/harrysave4: Mission Select, Right to the last mission, Serena St.
Germaine, A, A; launch data kept as .local/launch_serena.bin) a vehicle mission runs headless (drive.sh with
.local/drive_gas2.txt, throttle held). Its draw counts at frame 2700 are the same at 4:3, 16:9 and 2.39:1 (695537), so
its culling does not follow the view the hook widens (0x2111E0's planes). Tried: the field of view getter 0x211180
(only 0xEC080, not called in these missions) and keeping the widened +0x178/+0x17C (no change); both taken out. The
camera setup 0xC9450 makes the view's field of view and aspect (widescreen: 0x35ED54 +0x4C) for 0x2111E0. Next: find
what the driving scene culls with (a trace of the object loop), attempt 3.

### 2026-10-25 - Thermovision: the shrink pass's viewport

Harry: the radar is fixed; Thermovision better but doubled (capture 204137: the shrunk view sits at x 82..320 of its
320 wide corner, the accumulation shows it twice with a dark strip). His log: the shrunk texture's pitch is 5120 (the
game keeps the 1280 wide layout of its supersampled buffers, 8316CA00 the right half of 8316C000's row), so the
earlier top left fix only applied once the pitch condition went. With Harry's save (.local/harrysave3, the last
mission; Up then A on the d-pad switches Thermovision on) the effect runs headless (.local/m4.sh, .local/m4thermo.txt)
and CAP lists it: shrink into rt 83170A00 with viewport 320x240 (quad 0..320 x 0..240, reading the frame 0..640),
accumulate into 83170000 (full screen, stage 1 at 0.5..320.5 x 0.5..240.5 of the 320x240 texture), colour the frame
from 83170000. The per draw squeeze took the shrink quad for a draw not spanning the frame and squeezed it whole: now
a pass filling a viewport narrower than the frame is not squeezed (kSpanViewport). Smoke OK. The reference renderer
shows the thermal view in the top half only (it does not keep the 5120 pitch apart from the target's); the D3D
renderers' targets are not in guest memory, so that is the reference renderer's own.

### 2026-10-25 - radar and Thermovision, third look

Harry: the radar better but still flicking into an oval now and then; Thermovision unchanged (capture 200028: the
half size copy still in the accumulation buffer's top left). Radar: a rotating tile corner landing within the 2 pixel
edge allowance still stayed for that frame. Now decided per draw (SpansFrameWidth: the draw's screen x from its
vertices, pretransformed, pass-through or through its 2D matrix): only a draw reaching both edges (a full screen quad)
keeps its edge vertices, any other is squeezed whole; the shaders get it as the sign of hud.y (KeptAtEdge), the
reference renderer squeezes after the draw's vertices are known. Visible change at 2.39:1: the Mission Select header
bar starts at the 16:9 edge rather than the screen's. Thermovision: the top left reading needed the texture's pitch to
be the target's; this one's evidently is not, so a smaller linear texture over a colour target now always reads the
top left (its pitch logged once, "read as the top left of target"). Its draw's vertices were not in the capture (raw
buffers are not converted); D3D12 captures now print the guest's vertex buffers too. Headless at 2.39:1 menus fine,
26 shaders compile, smoke OK. Thermovision cannot be reached headless (Ground Zero has none; the saves stop at Train
Chase): attempt 2 of 3 on that effect.

### 2026-10-25 - Thermovision and the radar, second look

Harry: Thermovision now shows thermal colours but the top left quarter holds a sharp half size copy of the view; the
radar still stretched. Capture 193903: the effect shrinks the frame into 640x480 target 83170A00 (viewport 320x240)
and reads it back as a 320x240 linear texture (0228, with the target's 2560 byte pitch: on the console its top left).
The renderers normalised that texture's texel coordinates by 320x240, so the whole 640x480 target was spread over
the screen and the small copy landed 1:1 in the top left. Now a smaller linear texture over a colour target with the
target's pitch reads its top left, as a smaller depth texture over a depth buffer already did (the shadows). The two
buffers also overlap in memory (83170A00 is one row into 83170000): EoN draws this effect for its 1280 wide
supersampled frame buffers, where +0xA00 is the right half; the renderers keep them apart, so it does not matter.
Radar (capture 193756): its matrix is plain 2D after all (c0..c3 ortho, not rotated; the rotation is in its
vertices), so the earlier change was not it. The squeeze leaves vertices on or past the frame's left and right edges
where they are (a full screen quad's corners); the radar's map is big rotating tiles reaching far off screen left,
so some corners stayed and others moved, stretching it, and its mask cut it. Now only vertices within 2 pixels past
an edge stay (AtFrameEdge, shaders and reference renderer). Headless at 2.39:1: menus and backgrounds as before;
shaders compile (DXC); smoke OK. The radar and Thermovision themselves only on Windows.

### 2026-10-25 - Thermovision and the driving radar on Windows

Harry (Windows, 3440x1440): Thermovision "bugged out" (the screen looked frozen, tinted blue) and the driving radar
was stretched wide with its right side cut. Capture 180345: the effect renders the thermal view into the frame
(83A00000), shrinks it into 8316CA00 (4925, 4926), blends it into the accumulation buffer 8316C000 (4927: blend
CONSTANTALPHA / INVCONSTANTALPHA, 0x8003 0x8004) and colours that through a 256x1 ramp (4928). D3D12 and D3D11 mapped
0x8003 to the blend factor, which scales colour by the blend colour's RGB (0 here), so nothing blended in and the
buffer kept the scene from before: a still picture. The reference renderer had it right (alpha), so headless never
showed it. sdk: BlendFactor() puts the blend colour's alpha in all four channels for constant alpha blends. The log
ends with a normal close, so the "crash" was likely that still picture. Radar: its map is drawn through the HUD's
matrix program with a rotation (it turns with the car); OrthoConstants wanted x from x alone, so it was not squeezed
to 16:9 while its frame was. Now any 2D matrix (no depth, w 1, y flipped) counts. Smoke OK. Not seen headless (the
radar needs a drive this launch data does not reach; Thermovision a level the saves here do not have): Harry's test.

### 2026-10-25 - 30 minute soak with today's changes

scripts/soak.sh 30 (the level, Quit, New Game, Ground Zero, over and over) with the sharp 2D textures, the Quit Game
item, the menu mouse and the keyboard pictures: no traps, frames to the end (104940), 32 level loads, resident
memory flat (106 MB at 5 minutes, 105 MB at 30; data 607 -> 608 MB).

### 2026-10-25 - mouse in the driving missions' menus

driving.xbe has the same menu engine: its package input function is 0x13C2D0 (default.xbe's 0x131610; the package's
mask +0x20 and focus +0x84 the same, set-focus 0x1353F0). src/driving/menu_input.cpp marks a menu there too.
Headless (Train Chase, START): "a menu takes the input" with the pause screen up. The pause screens' tabs change with
the d-pad's left and right, which the wheel does not play (the arrow keys do).

### 2026-10-25 - SDK eon branch merged into the fork's main

Fork main had two new commits (PGR2's W-buffer clipping fix; a Quit game button at the settings window's top right).
Merged into eon (697c30b): the settings window keeps main's top-right button (asks first), now through app::Quit; the
footer button from earlier today goes. Smoke with 697c30b: EoN OK, PGR2 OK (its worktree, put back to 6e84311 after),
PGR1 OK (/home/harry/smoke-pgr1, put back to 2fbeb57). Fork main fast-forwarded to 697c30b; EoN's pointer moved.

### 2026-10-25 - sharp text and HUD, mouse in the menus

Sharp 2D (sdk 94a9acf): GameInfo::sharpen_2d and Settings::sharp_2d. A texture a 2D draw samples (pretransformed or
a screen program; swizzled, up to 512, not P8) is copied and enlarged on a background thread: alpha that is mostly
fully in or out (text, outlines) as a mask (Catmull-Rom, then thresholded at half with an edge one output pixel
wide), else interpolated; colour Catmull-Rom weighted by alpha; DXT decoded on the CPU. Text (A8) and pictures up to
128 four times, larger ones twice (a 256 picture at four times would be 4 MB of memory and 5 of video memory). Then
it is the texture's replacement (dropped when its bytes change; a texture dropped twice is left alone).
XBE_SHARPEN_DUMP=folder writes each as a TGA; the reference renderer runs it too. Headless: the menus' fonts (A8
256x128, 256x256), flags, logo, the HUD's weapon atlas and menu pictures made in about 0.2 s each.

Mouse in the menus: sdk input::TakeWheel and MouseButtons (raw input's wheel). The menus' engine gives each package
the input through 0x131610 (the package, the port); one taking port 1 with a button in focus (+0x84) marks a menu
in use (src/game/menu_input.cpp). GameInfo::pad (src/common/menu_mouse.cpp): in a menu the mouse buttons stop playing
their bindings, left is A, right B, each wheel notch a d-pad press. Headless: three notches down and a click choose
Quit Game; "out of the menus" from the level's load through play. Smoke OK.

### 2026-10-25 - Quit Game: main menu and settings window

sdk: app::Quit (closing the window, headless a shutdown) and a Quit game button in the settings window's footer
(asked twice). Main menu: the menus are EA FEng packages, resident and parsed by 0x138900; frontend_spmp.fng has a
fourth item this release never shows, Online (button 0x3BD5E, labels 0x3BD60/0x3BD5F, help 0x40978), whose focus
response hands the focus back to Options without Xbox Live. src/game/quit_item.cpp parses a copy with that item made
Quit Game: new string hashes for its labels and help (answered by a hook on the string lookup 0x142F60, in the game's
language told from its Single Player), its button's responses as the other items' with a message of its own (response
type 2 goes through 0x1305A0, hooked: app::Quit). Headless: the menu shows Quit Game under Options, it highlights,
and A on it closes the game ("Quit Game chosen in the main menu").

### 2026-10-25 - keyboard and mouse prompts, field of view

Keyboard prompts: sdk (input): UsingKeyboard, what controller 1 was last played with (a bound key or mouse button,
the mouse moved 16 counts while looking, or no controller plugged in: the keyboard; a controller's button or a stick
pushed half way: the controller), and ControlKeys (a control's bound keys, the settings' or the defaults); the
harness's KEYS token. src/common/buttons.cpp keeps each atlas it finds (its disc bytes, decoded) and draws it again at
its next bind whenever the look changes (controller or keyboard, a key bound anew, modern_buttons), the last two
drawings cached: on the keyboard every button's picture is its key on a cap (SPACE, CTRL, ESC, TAB, letters; arrows
as heads) or a mouse with the button lit, the d-pads as arrow-key clusters with the directions lit, the left stick as
WASD, the right stick as a mouse. The game names the sticks ANALOGPAD and ANALOGPADRIGHT (icons 8 and 9 from
0x117310); which atlas picture each is was not seen in a run (the tutorial step showing it was not reached), so the
left one is taken as the left stick. The game's LT rectangle ends at row 21 in the menus (93..115, 2..21) and at 25
in the tutorial, so the trigger plates now sit above row 22. Headless MI6 Training with KEYS: "E or Q: Punch left or
right" drawn with key caps (log: "drawn as the keys and mouse").

fov_percent (50 to 150, src/game/fov.cpp): 0x61080 makes every view's projection from the vertical field of view in
radians (1 over tan of half of it); its callers make the culling planes from its results, so the angle is scaled
there. Headless at 130%: 78 degrees for the game's 60, the training room wider. Smoke OK; driving runs (its P8 atlas
drawn). Next: Harry's Windows test of the sharp icons, the keyboard prompts and the FOV; the sticks' pictures when a
prompt shows one.

### 2026-10-25 - sharp button pictures

The prompts' atlas is 128x128, so on a 1440p screen each icon was a blurry 20 texels enlarged three times. sdk eb577a0:
gpu::SetReplacement, a title's own picture for a texture's memory: D3D12 and D3D11 upload it (with a box filtered mip
chain, sampled across it) and draw it instead of the guest bytes while those stay as they were when it was set (checked
once a frame; memory given to something else drops it). src/common/buttons.cpp now draws every icon as shapes and a
stroke font (src/common/strokes.cpp, A-Z 0-9 and a few signs, for the keyboard pictures next): the face buttons,
triggers, bumpers, Menu and View as before, and now the seven d-pads (the arms the red marks meant drawn white), the
sticks and the menus' arrows too. Drawn twice: at 128 into guest memory as before (DXT3 or P8), and at 512 for the
renderers. Offline test of both atlases (the disc's DXT3 and driving's P8 dump) at 512: clean edges, no old art left
around the boxes (cleared before enlarging). Headless MI6 Training: the 1x atlas patched in the menu and again in the
level (82085080, 82BD1D00), "Y or X: Punch" drawn with the new icons. Next: Harry's look on Windows (the log names the
512 picture once: "drawn from the title's 512x512 picture"); keyboard and mouse pictures.

### 2026-10-25 - mouse aiming straight into the game's look input

The mouse played the right stick, through the game's stick processing. The game reads controller 1's sticks in
0x1BC90 (via 0x5D210, the pad block 0x35B4A8, 0x90 a port): scaled to -1..1, squared when the response curve option
0x2FD334 is on, pushed out on the diagonals, clamped; 0x1C330 maps them to actions (0x32E20 is the menus' direction
bits). sdk aaef875: input::TakeMouse (the movement since the last call as a rate in deflections, sensitivity and
invert applied) and GameInfo::mouse_direct (the runtime leaves the stick alone). src/game/mouse.cpp adds the rate to
the right stick after 0x1BC90: no dead zone offset, no curve, no clamp, so the turn follows the mouse in proportion and
a fast move turns past the stick's full rate. Headless (harness MOUSER, half a deflection): the camera turns as with
the stick, half as far. Smoke OK. A camera-angle hunt by memory dumps (MI6 Training, the stick held) found bone
rotations, not the camera; the input route is what is used.

### 2026-10-25 - the driving HUD on ultrawide

Harry's 2.39:1 screenshot: the 3D widened, the "Missiles" text in proportion, the radar and the weapon picture
stretched and out at the screen's edges. driving.xbe lays its HUD out for 16:9 itself on a widescreen display
(0xDA0A0 and others: the frame object's +0x4C narrows sizes by 0.75), so its hud_aspect is 16:9 after all (the 4:3
of the last entry undone). Its text draws through the pass-through program (squeezed); the radar, pictures and boxes
through a program that multiplies v0 by c0..c3 (dp4 x4, then the XDK's screen tail) with an orthographic matrix
there (c0 = 1/320 0 0 -1, c1 = 0 -1/240 0 1, c3 = 0 0 0 1), which the squeeze did not know. sdk 597cc47:
MatrixProgram (four dp4 of v0 with consecutive constants at a program's start) gets the squeeze code, switched on per
draw only when those constants are an orthographic 2D matrix (OrthoConstants), so 3D draws with the same program
shape stay. Headless at 2.39:1: the objective box 225..414 squeezed to 250..389 with its text; small render targets
(64x64 to 256x128) unsqueezed. Smoke OK.

### 2026-10-25 - the driving missions' button pictures

Harry's captures (112758..112915): the driving prompts ("Y Get In Vehicle") draw from the same button atlas, but
driving.xbe's copy is P8 (128x128 swizzled indices on stage 3, its palette bound with SetPalette), so the DXT3 patch
did not touch it. sdk fd520de: texture_bound gets the stage's palette and runs again from SetPalette.
src/common/buttons.cpp: a P8 128x128 texture decoded through its palette is the atlas when each 16x16 cell's mean
colour (premultiplied) is within 12 of the disc's (the DXT3 and P8 copies agree within 4; the look, not the bytes,
since the copy only binds in the car, which the headless run does not reach); the same icons are drawn and each
changed texel takes the palette's nearest colour (cleared ones a clear entry). The START and BACK boxes grew past
their art: the P8 copy has a faint halo (alpha 12) there. Tested standalone: Harry's captured atlas quantised back
to P8 with its own 253 colours, patched once and left alone the second time, clean. Smoke OK.

### 2026-10-25 - driving: widescreen and ultrawide; 60 by default

driving.xbe's views are 4:3 whatever widescreen flag its launch data carries (the launch data differs by one bit,
byte 1216 0x80, between a 4:3 and a widescreen run of default.xbe; the driving frame is the same with either), so on
a wider screen they stretched. A view's projection is built in 0x2111E0 (field of view, aspect, near, far): a square
projection, x times +0x140, y times +0x144 and the aspect, then the culling planes (left and right from the half
angle's tangent times +0x178 and +0x17C). Giving it a wider aspect only narrowed the height (tried: the view zoomed
in, still stretched). src/driving/widescreen.cpp: x divided by k (the frame's aspect over 4:3) after, +0x178 and
+0x17C k times larger while it runs, so the sides show more and are not culled. Headless at 16:9 and 2.39:1: Bond in
proportion, more of the level at the sides. The driving 2D is laid out for 4:3: game_info.cpp (built into each
image's executable) gives driving.xbe hud_aspect 4:3, so its HUD and text keep that shape in the centre ("NEW
OBJECTIVE" checked at 2.39:1). driving_60fps is on by default (Harry: works). The driving button pictures: the HUD
atlas (0x82917880 in Harry's captures, P8 on stage 3) only loads in the car, which a blind headless walk does not
reach (Train Chase starts on foot); waiting for a capture with a driving prompt on screen.

### 2026-10-25 - driving at 60: the drawing task

Harry's log: the setting reached driving.xbe (interval 0 asked for) and every frame still took 33 ms. The drawing task
0x72410 draws (0x14EBF0) only when the blank callback's second count 0x35FBB8 (bumped every other blank, 0x15CD3F)
differs from the one it drew at (0x326F30): 30 a second whatever the ticks. src/driving/sixty.cpp now makes a blank
gone by look like a new count to it. Headless the driving mission now draws about 57 a second paced (55 unpaced):
the 33 ms a frame of the entry below was this limit, not the machine. Ticks keep to the blanks (3104 at 3253 after
loading). Smoke OK.

### 2026-10-25 - driving at 60 frames a second (setting, experimental)

driving.xbe is time based: it simulates in ticks of 1/60 s (0x3165F8 = 60, 0x3165FC = 1/60, the tick count 0x316600
advanced in 0x11CE0 by each tick 0x7A6B0, found with a gdb watchpoint), run by the scheduler 0x15F4D0 for the vertical
blanks counted at 0x35FBB4 (the blank callback 0x15CD30; the runtime's blank thread is real time). It presents every
second blank (0x214CB0(2) from the device setup 0x14D290), and the scheduler waits for two blanks and runs exactly two
ticks (0x15F5B3, last + 2) unless its object's +0x1C or +0x1E is set; then it runs a tick for each blank gone by and
draws once after (the swap is under the task queue 0x15DA60 the scheduler runs last: gdb at SwapFrame). An earlier
reading (forced interval 1 runs at double speed) compared frame numbers, not wall time: at the same wall time both
runs draw the same scene. src/driving/sixty.cpp (setting driving_60fps, off by default; EON_DRIVING60 headless):
0x214CB0 asked for 2 passes 1, and +0x1E is set while 0x15F4D0 runs. Headless it still drew 30 a second (the drawing task's
own limit, found next); the ticks keep to the blanks (3026 ticks at 3180 blanks after loading). Smoke OK.

### 2026-10-25 - every button picture modern

src/common/buttons.cpp redraws the whole prompt atlas (recognised by all of its bytes, FNV-1a 09076852FE367BA4): it
is decoded, each icon drawn again inside its own artwork's bounds (measured as the atlas's opaque components: the
rectangles the game draws with hold them, e.g. Y u 66.5..89.5 v 2.5..24.5 around its oval 69..86, 3..24), and the
blocks drawn on encoded again (the two texels furthest apart as end colours, each texel the nearest of four). A B X
Y: bold coloured letters on a dark disc; LT RT: labelled triggers where the arrow boxes were; LB RB: bumpers; START:
the Menu button's three lines; BACK: the View button's two squares (the words cleared: the icons hold the dot and
the word). D-pads and sticks as they were. EON_MODERN_BUTTONS=0 or 1 headless. driving.xbe's HUD does not bind this
atlas (its prompts are P8 textures): left as they are. Smoke OK.

### 2026-10-25 - LB and RB button pictures

The button prompts (\i<ACTION> in the strings) draw from one 128x128 DXT3 atlas: B X A Y, the trigger arrows,
the d-pads, START, BACK, the sticks, and WHITE and BLACK as small white and black ovals (16x16 cells at 32,64 and
48,64, the ovals in their columns 0 to 11, rows 0 to 13). src/common/buttons.cpp (setting modern_buttons, on by
default): when the atlas is bound (sdk 5d7f3eb GameInfo::texture_bound, from SetTexture, the texture marked dirty
after), recognised by the two cells' original bytes (FNV-1a D26D60581BC4099B), the cells are rewritten as LB and
RB bumpers inside the ovals' texels: white 3x7 letters on a dark rounded body with a grey edge, colours a DXT block
holds exactly. Headless: the atlas patched in the menus' and the level's copies (decoded from a memory dump);
SDL maps RB to BLACK and LB to WHITE, as the pictures say. Smoke OK.

### 2026-10-25 - driving.xbe: the first driving mission, headless; the palettes

Harry reached Train Chase on Windows: default.xbe hands over to driving.xbe (HalReturnToFirmware 2, launch data)
and it plays at 30 frames a second (present interval 2). His captures (080419..080450): the light glows patterned
discs, a black and white panel at the top left (the radar), the weapon icon noise. Headless now too: his save
(.local/harrysave2), Load Game, Train Chase (.local/drive_go.txt) writes hdd/launch.bin (kept as
.local/launch_trainchase.bin, the driving boot deletes it), then .local/drive.sh builds and runs driving.xbe on the
same disk. The glows are one draw sampling a P8 texture (format 0xB) on stage 3 alone, and most of the driving HUD
samples P8 on stage 3: driving.xbe's SetPalette (0x29F060, pushes 0x1B20 + stage * 0x40 itself, a QFE 4 body the
scan missed) was not replaced, so no palette reached the renderers. config/driving.toml names it; the reference
renderer now decodes P8 with the stage's palette (sdk 34cbe6b). Headless: the glows are small lights. default.xbe
sets no palettes. Next: Harry's test of the driving HUD.

### 2026-10-25 - the second muzzle flash, found

Harry's save (hdd.zip: E:\UDATA\45410042) loads headless: Load Game, MI6 Training (.local/m2punch.txt). His
D3D12 captures (072255..072404, an older build: XBE_CAPTURE_DRAWS=aa was not in it) show two flashes per shot: one
at the gun and a copy at half its place (179,64 against 364,129). The particle draws are immediate (pushed) quads,
batched until Walk reads them; SetRenderState_MultiSampleMode is an HLE setter that did not read pending pushed
commands first, so the last batch inside the 4X window was drawn after the mode went back, unstretched. Headless the
same: of the punch sprites (texture 0x82C99480) 6369 stretched and 6370 not. sdk 2867d1d: both multisample
setters walk pending commands first (WalkBeforeState, as SetRenderTarget does); now all three sprites are stretched.
Smoke OK. The other complex state setters do not walk first either; left as they are for PGR's sake.

### 2026-10-24 - mouse and keyboard

sdk 4e05344: mouse look for a title that offers it (GameInfo::mouse_look): raw mouse movement (WM_INPUT)
read each poll as a rate (2400 counts a second full deflection at 100%), past a 20% start so a game's stick dead zone
does not swallow slow moves, eased over a frame, added to controller 1's right stick; the cursor hidden and clipped
to the window while the game is in front and the settings window shut. Settings mouse_look, mouse_sensitivity (10 to
1000%), mouse_invert_y, in the Controls tab; mouse buttons bind as keys (mouse_left, mouse_right, mouse_middle,
mouse4, mouse5), also by clicking in the binding box; GameInfo::default_keys for a title's own layout. EoN: WASD,
mouse aims, left button fires (RT), right button locks on (LT), Space A, Ctrl B, Q and E punch (X, Y), C cover
(BLACK), F WHITE, arrows or 1 to 4 the d-pad, Esc Start, Tab Back. PGR1 and PGR2 unchanged (opt-in). Built and
wincheck'd only: the feel (sensitivity, the start past the dead zone) needs Harry. README features filled in.
Next: the button pictures (BLACK and WHITE as LB and RB).

### 2026-10-24 - Skip intro movies

The boot logos (THX, MGM, the EA logo) are named in the game's data, not its code; a missing movie is passed over
(as the EA logo was when the game asked for an NTSC file). sdk af60d2d: GameInfo::hide_file, a file reported
missing to NtCreateFile (open), NtOpenFile and NtQueryFullAttributesFile when the title says so. EoN's setting
skip_intro ("Skip intro movies", off by default; EON_SKIP_INTRO=1 headless) hides thxlogo*, mgm_* and eag* under
movies: headless, the opening gun barrel starts at 17.7 s instead of 35.5 s. Smoke OK, wincheck OK. Next: mouse
aiming, then the button pictures (BLACK and WHITE as LB and RB).

### 2026-10-24 - the cutscenes: a PAL console

The runtime reported an NTSC console, so the game asked for its movies as _ntsc and _ntscw (XBE_FILES_LOG:
eag_ntsc and in1_ntscw missing): Harry's disc (game region 4) has the EA logo and every cutscene (in1, in2a, intro,
ls1, ...) only as _pal and _palw, so they were all skipped. The suffix comes from 0x24539C: _pal when 0x35BF78 is 1
or 2, set at 0x5FF90 when XGetVideoStandard (setting 0x103, bits 8 to 15) is 3, PAL-I; with the PAL60 video flag
(0x40, setting 8's 0x00400000) the game picks 60 Hz (0x35B7BC = 60). sdk 63b9ffe: GameInfo::pal_console, for a
release whose certificate regions are rest of the world without North America, reports PAL-I with PAL60 on; EoN
opts in, PGR1 and PGR2 do not. Headless: eag_pal and in1_palw open, the opening gun barrel plays; THX and MGM stay
_ntsc (the game asks for them so, both exist). Smoke OK, wincheck OK.

### 2026-10-24 - the second mission's gun flashes and punch effects

Harry: the flashes are right in the first mission, at the top left again in the second (VR training), punch effects
too. His log: MultiSampleMode 4X applied on the back buffer's surface 0x35B9C4; his captures (231217..231240) draw
whole frames into 0x03A04000 and 0x03A04A00 in turn, the second buffer through the front buffer's surface object, so
those frames' particles missed the 2x2 and landed at half their place. On the console the frame buffers take turns
at being the back buffer: sdk 22a2d73 applies the mode on any of them. Mission 1 headless unchanged, smoke OK,
wincheck OK. Next: Harry's test in the second mission.

### 2026-10-24 - ultrawide; fork main in sync

SDK: the fork's main (PGR2's send level, a vertex range check, short read logging, a recompiler report) merged into
`eon` (53d91b1) and the fork's main fast-forwarded to it: EoN, PGR1 and PGR2 smoke OK; EoN's firing test unchanged.
Widescreen: EoN has its own 16:9 mode, on with the console's widescreen flag (0x5AA59C), which the runtime reports
for a display 3:2 or wider: a wider view, anamorphic in 640x480, shown at 16:9 (4:3 and 16:9 outputs compared
headless). Ultrawide (aspect = "screen"): src/game/ultrawide.cpp widens the view factor 0x5AA550 (4/3 at 16:9; the
projection 0x61080 divides x by it) by the frame's width over 16:9 while 0xF6660 draws the views (0xF67A0 sets the
factor and draws in one call, so a change after it came too late) and after 0xF4B60. The HUD and menus are drawn by
a pass-through vertex program (oPos = v0 plus constants), not pretransformed vertices, so the renderers' 16:9
squeeze missed them: sdk 00f769f GameInfo::squeeze_screen_programs and gpu::ScreenProgram give such programs the
same squeeze, EoN opts in. Headless at 2.39:1 (FRAME_ASPECT_X100=239): more of the level to the sides, Bond in
proportion, HUD and menu text 16:9 and centred; full screen backgrounds (the menus' picture) still reach both edges,
stretched, as PGR2's full screen passes do. 16:9 smoke OK, wincheck OK. Next: Harry's test with aspect = "screen";
mouse aiming.

### 2026-10-24 - the ghost in the sky: supersampling only

Harry (D3D12): the gun flash is right; a faint copy of the buildings showed in the night sky. His capture
(193348) ran with EON_SUPERSAMPLE=1 left set in the shell. The ghost is the depth haze (draw 9318: pretransformed
quad, pixel shader 0x27B5B4, stage 0 the depth buffer 0x83C5C000 read as LIN_B8G8R8A8 bytes, stage 1 a bump-env
lookup into the 128x256 ramp 0x82021000, its alpha the haze): with supersampling it reads the depth at twice x (the
ghost floodlight at x 232 is the one at 462, same height), the game addressing its 1280 wide buffer while the
runtime keeps it 640. With EON_SUPERSAMPLE=0 (the default) Harry sees no ghost. Nothing to fix while the plain
buffers are the default; supersampling would need the runtime to keep the 1280 wide buffers. sdk 1b5c7b9: the
headless draw log gives a draw's multisample scale (aa).

### 2026-10-24 - the gun flash at the top left: MultiSampleMode 4X

Harry (D3D12): with the frame buffers made without supersampling (b0e80c3, which fixed the carousel's flicker) the
gun's flash, its tracer and the bullet sparks land at the top left; EON_SUPERSAMPLE=1 puts them back (with other
shadow oddities). Headless shows the same: a fire burning at (590,200) with supersampling sits at (290,105) without.
Cause: around its particles (0xFA3C0 .. 0xFA4F0, called from 0xAB9B0 and 0x259B50) the game, without supersampling
(0x35BBF4 clear), sets D3DRS_MULTISAMPLEMODE to 2 (4X) and vertex constant 8 to half the back buffer's width and
height; with supersampling it only switches MultiSampleAntiAlias. 4X on a buffer made without multisampling makes
the console draw the buffer as a 320x240 one, each pixel 2 by 2 of it: the particles are drawn at a quarter of the
pixels and stretched. The runtime kept the state but drew in the buffer's own pixels. sdk 5228627: State::aa_x,
aa_y, the mode (MultiSampleMode on the back buffer, MultiSampleRenderTargetMode on another target) over the samples
the target was made with (D3DPRESENT_PARAMETERS MultiSampleType), never below 1; the D3D12 and D3D11 backends and
the reference renderer draw in pixels that size (the screen constants, scissor and viewport). Headless now: the
fire in its place, the flash, tracer and shell casings at the gun (ten frames snapped while firing). Smoke OK,
wincheck OK. Next: Harry's test on Windows; then PGR1 and PGR2 smoke for the fork's main.

### 2026-10-24 - Windows: shadows right, full speed; the carousel's flicker

Harry (D3D12): the shadow under Bond is now as faint as on the console (fe7afee). The level runs at full speed:
1 to 13 frames in 10 s over 25 ms (all of them before the strip batching and the leak fix). The weapon carousel
now copies the frame (4bf191d: a colour target on the depth buffer's memory keeps the colour target when colour is
written) and shows its desaturated ghost of the copy (draw 11803: c0 0.30 0.59 0.11, alpha 0.5), but flickers
white: draw 11534 (pixel shader 0x27B5B4) samples a 640x480 LIN_B8G8R8A8 (0x40) texture whose data is physical 0,
and Bond goes white in the capture (153029). The header is filled from something the host leaves empty; sdk now
logs such a texture once with its header address. Next: the header, from Harry's next log (the carousel's button
is not Y, WHITE or the d-pad in the tutorial headless).

### 2026-10-24 - the shadow diamond on Windows: what the capture shows

Harry's D3D12 capture (.local/win/144836, F3 with XBE_CAPTURE_DRAWS): a light, diagonally hatched diamond under
Bond, the projected shadow's ground patch. The shadow pass binds a 640x480 colour surface (0x12) and a 640x480
D24S8 surface (0x2E), both pitch 2560, on the same memory 0x03043000 (SetRenderTargetFast, headers 0x849A00 and
0x8499D0), with a 128x128 or 256x256 viewport; the ground patches then sample 0x03043000 through 256x256 (pitch
1024, header 0x63A9AC) and 128x128 (pitch 512, 0x63A9C0) D24S8 texture headers. No SetTile covers that memory
(tiles 0 and 1 are the frame buffers 0x03A04000 and 0x03C5C000, pitch 0x1400).
D3D12 keeps the pass in a 640x480 depth target (the silhouettes are in its top left in the capture's z images);
a 256x256 depth texture finds no target of its size and reads guest memory, which the GPU never wrote. The
reference renderer reads guest memory too, but there, at the first sample in the level, the memory holds noise
in both layouts (no silhouette): it is not clearly right either. Open: how the console lays the pass out for those
texture headers; what EoN's character shadows look like on a console (or xemu) decides the fix (sample the
target's top left, or rebuild the texture's layout from the target on the GPU).

### 2026-10-24 - a soak test; the memory leak

scripts/soak_pad.sh (SOAK_PAD): the level, Quit, New Game, Ground Zero, over and over; run from a separate
worktree (/home/harry/eon-soak, its own .local). 20 minutes on 3d9aaef: no traps, frames to the end (70620),
repeated quit and reload cycles - but the process grew from 510 MB to 3858 MB (about 12.5 GB an hour).

Cause: SelectProgram made a new VertexShader on every SelectVertexShader, hundreds a frame here, and the renderers
and EndPrimitive keep each shader they meet (by pointer). Measured over 3 minutes in the level: +56 MB every 10 s.
sdk 63211f1: one object per declaration, program and stream layout, handed out again. Then 101 to 103 MB over the
same 3 minutes; the movies and the level unchanged; EoN, PGR2 and PGR1 smoke OK; the fork's main 63211f1.
The 20 minute soak again on 63211f1: no traps, frames to the end (70860), 94 MB at 5 minutes, 104 MB at 20
(+39 MB an hour, against 12.5 GB an hour before). On
Windows this also stops the backends making pipelines for new shader objects every frame.

### 2026-10-24 - a smoke test; driving.xbe left for a driving mission

scripts/game.env: SMOKE_PAD=scripts/pad/explore.txt, SMOKE_MENU="900 1500" (the THX and MGM movies, one draw a
frame). scripts/smoke.sh 150: SMOKE OK (movies 1 draw a frame, level 11532, no traps).

driving.xbe through the game's own launch, three attempts, stopped here: (1) run alone it falls back to the
developer track qlab (not on the disc); (2) a new save's Mission Select has only Ground Zero (MI6 Training
locked), the unlocks are in the profile, not found; (3) the flow's state table 0x2FD358 (handler 0x1EB00 ->
0x21DE0 -> 0x21D30(1) "D:\DRIVING.XBE", launch data the state object 0x31EA60, mission from 0x35B6EC, which
0x5DD3D fills from a sorted table of 12 byte records) is too deep to drive by pokes with confidence. Next: Harry
reaching a driving mission on Windows, or the profile's unlocks found.

### 2026-10-24 - the scan's false PlayEx; SDK to the fork's main again

sdk cc6857c: xbe_symbols drops a byte match the image's .XTLID lists under another export id (never this one):
5659's PlayEx signature had taken StopEx's thunk in both xbes. PGR2, PGR1 and Forza scan unchanged; EoN's scans
lose the false PlayEx, the hand overrides are gone, the generated symbols of both xbes are unchanged.
PGR2 smoke (race 2010 draws a frame) and PGR1 smoke (2812) OK on cc6857c; a PGR2 run to the race with logging
showed it never reaches the strip batching or the program rebuild. The fork's main fast-forwarded 6bded69..cc6857c.
The loading screen's strip: game code (0xFC740 via BeginPush, 0x26F6E0) pushes text vertices into 0x83043000
early on; the same memory is later the shadow maps and the loading picture. Left open (cosmetic, loading only).

### 2026-10-24 - Windows: the shadow pass, half the draws, the movies

Harry's first Windows build (D3D12): it boots and plays; a black square with hatched edges under Bond, more
glitches with the weapon carousel up, and every frame over 25 ms in the level.

- The square: the character shadows are a depth-only pass (cw=0) into a 128x128 or 256x256 D24S8 surface given as
  both colour target and depth buffer (0x03043000), sampled as a depth texture (PROJECT2D, compared with 0 as
  xemu does) by the ground patches redrawn under each character (pixel shader 0x27B5B4, stage 1 bump env).
  sdk 63e43e1: both GPU backends bind the depth target alone when the colour target is its memory (they made a
  colour twin and could drop the depth target). Harry: the square under Bond is gone.
- Draws: about 26000 a frame in the level, 14500 of them pushed index strips (the characters). sdk 8833e6b: Walk
  keeps the strips' triangles and draws them as one list before any other command and at the end of every walk.
  57 million draws over 4100 frames before, 24 million after, the frames the same. (A first version flushed only
  at the end of a walk, after the next object's constants: spiky characters; the flush now comes before any
  other command.) About 11500 remain: 8000 of them the ground patches under the shadows, one API draw each.
- Movies: each frame is a YUY2 640x480 texture drawn as an immediate quad (0x17FC, colour 0x194C, position
  0x1518) through program slot 0; the game calls SelectVertexShader(0, 0) and only then pushes the program into
  slot 0 (1E9C, 0B00..), reusing the slot for other programs in the frame. The runtime copied the program at the
  select: the quad went through the slot's previous program, off screen. sdk 8068dbc: the slot is kept and the
  selected shader rebuilt from program memory when pushed instructions change it. THX and MGM now play; the level
  and menus unchanged.
- The loading screen's strip: a 640x480 A8R8G8B8 texture at 0x83043000 (the shadow maps' memory) drawn dimmed;
  nothing writes there before it in the harness. Open.

### 2026-10-24 - SDK published; driving.xbe named and booted on its own; the menus

SDK: PGR2 smoke (this repo's sdk at 6bded69 in a PGR2 worktree: SMOKE OK, race 2221 draws a frame) and PGR1 smoke
(/home/harry/smoke-pgr1: SMOKE OK, race 2723) passed, neither touches the push buffer paths; the fork's main
fast-forwarded 2fbeb57..6bded69 and pushed with the task branch `eon`.

driving.xbe: the same QFE 4 library, so the CDevice layout moved to config/d3d8_qfe4.toml, included by both hand
configs (default.xbe's generated symbols unchanged). Its D3D functions matched against default.xbe's names (43;
SetGammaRamp begins its D3D section; not linked there: SetVertexShaderInput(Direct), Begin/EndPushBuffer,
SetRenderTargetFast, SetVertexShaderConstant1Fast), its D3D data by the instructions using default.xbe's (every use
agrees), DSOUND by .XTLID (PlayEx again the scan's false match), SetBackBufferScale 0x2A2870 by reading it. Built
with sdk/harness/build.sh -g generated/driving and run alone: CreateDevice (640x480, two back buffers, auto depth),
audio, then the tuning table's fallback track qlab (0x286F80), whose files are not on the disc, and a crash.
The game launches it from its flow (state handler 0x1EB00 -> 0x21DE0 -> 0x21D30(1): "D:\DRIVING.XBE" from
0x2B9ACC, the state object 0x31EA60 as launch data). So it needs a driving mission reached in default.xbe.

Menus: Quit from the pause menu (Game tab, Quit, Yes) leads to the main menu (Single Player, Multiplayer,
Options, over a looping movie), New Game, Mission Select (Ground Zero; MI6 Training locked). scripts/pad/menus.txt.

### 2026-10-24 - the characters draw: RunPushBuffer replays; the tutorial plays

The 64 buffers run with fixups were not recorded with BeginPushBuffer at all: Common 00020001, built with the
models and loaded. Decoded (one-off log): 132 no-ops (0x100, where the fixups write), a constant load (0x1EA4,
0xB80), then hundreds of index strips: begin 6, array elements 0x1800 (16 bit pairs) and 0x1808 (one 32 bit),
end. The vertex data, program and textures come from the API calls before the run; the fixup batch (BeginFixup
0x26F2B0, D3DPushBuffer_SetVertexShaderConstant 0x26F1F0: records of byte count, offset + 8, commands) loads the
bone matrices into the no-op block.

sdk 6bded69: RunPushBuffer copies the fixup batch into the buffer and Walks its commands; Walk draws array elements
and draw arrays with the bound vertex data. Buffers recorded here keep their draws (state and place) and replay them
between their commands; one recorded and run in the same frame twice (EoN's two frame buffers, 0x686F10 and
0x686F40) is drawn as it records from then on.

Bond, the soldiers, the helicopter and crates now draw. BLACK takes wall cover (it was the step the tutorial waited
on, not a stuck stick); scripts/pad/explore.txt plays steps 1 to 3 (cover, lock on with LT: red target markers,
fire with RT: ammo 30 to 21). The loading screen (frame 1800) still has a garbage strip along its top.

### 2026-10-24 - the training level draws: commands pushed while recording are read

The black frames after the movies had about 11000 draws each, every one with vertex shader handle 0x27D209 (the
library's static direct-input shader, 0x27D208: SetVertexShaderInputDirect 0x26E530 copies the inputs into it). The
engine selects its programs with SelectVertexShader(0x27D209 or 0, slot 0 or 8) and loads them with pushed commands
(0x1E9C), never LoadVertexShader. Logged: no program load ever reached the runtime, every selected program was zeros.
The loads are in the frame's own recording (the first RunPushBuffer's data begins 00041E9C 00000000): Walk only read
the scratch buffer. sdk: Begin/EndPushBuffer (0x26F040, 0x26F0B0, named here) wrapped, Walk follows the push into
the buffer being recorded and back. Program loads and selects now carry 24 of 32 dwords.

With none.txt the first boot goes from the movies to the training level ("Basic combat, step 1: Press [] for wall
cover"), drawn: the level, lights, fire, smoke, HUD and text; 4 minutes (13440 frames) without a trap or new
warning. scripts/pad/explore.txt: START opens the pause menu (drawn: Objectives, Game, Options). The left stick
moves nothing visible and Bond is not drawn: the skinned meshes (0xE9D30) patch bone matrices into a fixup
(0x26F2B0 begins it, 0x26F1F0 sets constants) and run a push buffer recorded at load time with it, which the
runtime does not replay (64 RunPushBuffer logged, 2 to 26 KB). The loading screen (frame 1800) has a garbage strip
along the top.

Next: replaying recorded push buffers with their fixups.

### 2026-10-23 - first boot: QFE 4 names and layout, the game's own frame buffers, the DVD's sectors; black after the movies

Each step, then what stopped the next run:
- D3D globals unknown at CreateDevice: RenderState 0x27A9F0, DirtyFlags 0x27A7E8, DeferredTextureState 0x27A7F0,
  PresentInterval 0x27ABEC (same instructions as PGR2's SetPixelShader, SelectVertexShader, Swap), Streams 0x27D148
  (QFE 4 moved it to the end of D3D's data; the vertex setup 0x27575F reads it like PGR2's 0x1C7A9F), and
  FvfShader 0x27D320 (the device init 0x270F7B), in config/eon.toml [data].
- CDevice layout: every instruction sdk/xdk/5659/layout.toml cites in PGR2 matched in EoN by its instruction window
  (numbers masked). Fields below +0x780 did not move, +0x780..+0x7A0 moved 4, from about +0x900 on 0x10. 28 keys
  in config/eon.toml [layout], each citing EoN's instruction; BaseVertex unverified (no SetIndices linked).
- 43 D3D functions the 5659 signatures miss, matched to PGR2's named QFE 1 bodies the same way; ambiguous ones by
  link order. Without them InitDefaultState and the fences ran the console code on device state the host lacks.
  SetTile 0x26C3F0 and RunPushBuffer 0x26ED10 by reading them (PGR2 links neither).
- The game makes its own frame buffers: BufferSurfaces 0x35B754 (1280x480, 2x horizontal supersampling, MultiSample
  0x1121) and 0x35BC70 (640x480), DepthStencilSurface 0x35BCA4 (D24S8), and asks for no auto depth; the runtime made
  its own, so GetDepthStencilSurface2 returned 0 and the game read through it. sdk 13fe175: those objects become the
  device's buffers, 640x480 (the supersampled size is not kept).
- DSOUND (QFE 1 like PGR2's, but EoN links another set): 6 exports named by .XTLID id (SetEG, StopEx,
  GetCurrentPosition, FlushEx, CreateSoundBuffer, XAudioCalculatePitch). The 5659 PlayEx signature took StopEx's
  thunk (same bytes, other callee); EoN exports no PlayEx, so PlayEx = 0 here. The signature needs fixing.
- Then a null object in the game's loading: XGetDiskSectorSize("D:\\") (0x623DA, from 0x5BD00) must be 2048 for the
  game to mount ACTION\ACTDAT1.VIV, ACTDAT2.VIV, STR.VIV (setting 0x484F46C1, mode 2); the runtime said 512 for
  every volume and the game looked for loose files. sdk 255d367: the disc reports a DVD's geometry.

Now: 120 s headless without a trap. The THX and MGM movies are opened (16 s, 21 s; eag_ntsc.xmv and in1_ntscw.xmv
are not on this disc and are skipped), STRENG.VIV loads, and from 32 s the game runs push buffers recorded earlier,
with fixups, every frame (64 logged). REF frames 600 to 4200 are black (the movies are an overlay the runtime does
not show; after them the front end draws nothing visible). The renderer counts about 8000 draws a frame.

Next: what the black front end draws: the replayed push buffers first (what they hold, whether the front end is in
them), and the draw count.

### 2026-10-23 - skeleton: the SDK template, two xbes, the first scans

The SDK's games template with EoN names; the SDK (hk21x/XboxXBERecomp main, 2fbeb57) as the sdk submodule. The disc
(title 45410042) has two executables, default.xbe (on-foot levels, `action/`) and driving.xbe (driving levels,
`driving/`), so two targets: EoN and EoNDriving (`GameInfo::images` lists both for the relaunch on
XLaunchNewImage). config/eon.toml and config/driving.toml are the hand configs over the two scans.

Scan with sdk/xdk/5659: default.xbe 84 of 250 functions, 7 of 13 data; driving.xbe 74 of 250, 5 of 13. The library
headers say why: every library is 5659 QFE 1 as in PGR2 except D3D8, which is QFE 4, and nearly all D3DDevice
functions miss (13 found only by their constants). No XNet/XOnline library is linked (the WSA/XNet misses are not in
the image); movies are XMV (Bink misses likewise). The DirectSound misses (CreateSoundBuffer among them) need a look.
