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
- SDK: on the fork's main since 2026-10-24 (6bded69; PGR1 and PGR2 smoke OK), task branch `eon`: 13fe175 title-made frame buffers, 255d367 the disc's 2048 byte
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
- Menus: scripts/pad/menus.txt reaches the main menu and the Mission Select (only Ground Zero on a new save)

## Log

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
