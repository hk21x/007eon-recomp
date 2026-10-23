# 007: Everything or Nothing recomp - progress log

Newest first. Each entry: date, what changed, what was learned, what's next.

## Status
- Milestone: 1 - boots headless: device, audio, the archives, the logo movies (THX, MGM) and on into the front end's
  loading; 120 s without a trap, about 60 frames a second. Every frame after the movies is black.
- XDK: 5659, D3D8 QFE 4. default.xbe 84 of 250 functions by signature, 50 more by hand in config/eon.toml (matched to
  PGR2's QFE 1 bodies, or by .XTLID export id); 6 D3D globals and 28 CDevice layout offsets for QFE 4 there too
- Push buffers: YES, both kinds. Every frame is recorded (BeginPushBuffer 0x26F040 .. EndPushBuffer 0x26F0B0) and run
  (RunPushBuffer 0x26ED10) at once, which the runtime's API level drawing covers; and after the movies the game runs
  buffers recorded earlier with fixups, again and again (2 to 26 KB each), which the runtime does not replay
- SDK: branch `eon` of the fork (local, not pushed): 13fe175 title-made frame buffers, 255d367 the disc's 2048 byte
  sectors; both change shared code, so PGR1 and PGR2 smoke must pass before they go to the fork's main
- Next: why the front end is black: what draws, and how much of it is in the replayed push buffers
- driving.xbe: not built or run yet

## Log

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
