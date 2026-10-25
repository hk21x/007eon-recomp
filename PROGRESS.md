# 007: Everything or Nothing recomp - progress log

Newest first. Each entry: date, what changed, what was learned, what's next.

## Status
- Milestone: 3 - headless, playable: the first boot goes from the movies to the training level, drawn with Bond,
  the soldiers and the helicopter; scripts/pad/explore.txt takes cover (BLACK), locks on (LT) and fires (RT)
  through the tutorial's first three steps. No trap in any run (up to 4 minutes).
- XDK: 5659, D3D8 QFE 4. default.xbe 84 of 250 functions by signature, 50 more by hand in config/eon.toml (matched to
  PGR2's QFE 1 bodies, or by .XTLID export id); 6 D3D globals and 28 CDevice layout offsets for QFE 4 there too
- Push buffers: YES, both kinds, now drawn (6bded69). Every frame is recorded (BeginPushBuffer 0x26F040 .. EndPushBuffer 0x26F0B0) and run
  (RunPushBuffer 0x26ED10) at once, which the runtime's API level drawing covers; and after the movies the game runs
  buffers recorded earlier with fixups, again and again (2 to 26 KB each), which the runtime does not replay
- SDK: the fork's main at 63211f1 (2026-10-24, PGR1, PGR2 and EoN smoke OK), task branch `eon`: 13fe175 title-made frame buffers, 255d367 the disc's 2048 byte
  sectors, fab5f4a Walk inside a push buffer being recorded, 6bded69 RunPushBuffer replay; both change shared code, so PGR1 and PGR2 smoke must pass before they go to the fork's main
- Windows (Harry, 2026-10-24, D3D12, RX 7900 XT): boots and plays. Fixed since: the black square under Bond
  (63e43e1), the movies (8068dbc), half the draws (8833e6b). Open: a dark slab on the ground, blue figures at
  the top left and a bright band with the weapon carousel up (perhaps the movies' cause, to be seen); frame rate
  (about 23 a second in the level before 8833e6b)
- Next: Harry's next test (F3 capture with XBE_CAPTURE_DIR and XBE_CAPTURE_DRAWS=1 for what remains); driving.xbe
  through the game's own launch; the loading screen's strip (0x83043000, the shadow maps' memory, sampled as a
  640x480 picture with nothing written there before)
- driving.xbe: named and built (config/driving.toml, 43 D3D names matched from default.xbe, the shared QFE 4
  layout in config/d3d8_qfe4.toml); run on its own it boots its device and audio, then loads the developer fallback
  track qlab (not on the disc): it needs the launch data default.xbe passes
- Smoke: scripts/smoke.sh (explore.txt; the movies as the 2D check, the training level after): SMOKE OK, 1 draw a
  frame in the movies, 11532 in the level (2026-10-24)
- Menus: scripts/pad/menus.txt reaches the main menu and the Mission Select (only Ground Zero on a new save)

## Log

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
