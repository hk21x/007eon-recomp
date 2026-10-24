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
- SDK: branch `eon` of the fork (local, not pushed): 13fe175 title-made frame buffers, 255d367 the disc's 2048 byte
  sectors, fab5f4a Walk inside a push buffer being recorded, 6bded69 RunPushBuffer replay; both change shared code, so PGR1 and PGR2 smoke must pass before they go to the fork's main
- Next: the garbage strip on the loading screen; the movies (overlay); driving.xbe; then the SDK changes through
  PGR1 and PGR2 smoke so they can go to the fork's main
- driving.xbe: not built or run yet

## Log

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
