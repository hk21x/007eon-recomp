# 007: Everything or Nothing recomp - findings

What the work has established, by topic. PROGRESS.md has the dated log with the evidence for each item. Each item
is marked **[console]** (how the Xbox, its XDK or the runtime behaves: the fix is in the SDK) or **[game]** (this
game's own code and data, with its addresses).

## The image

- **[game]** Two executables on the disc: default.xbe (on-foot) and driving.xbe (driving levels), title 45410042,
  each switching to the other with XLaunchNewImage.
- **[console]** XDK 5659 with D3D8 QFE 4 (the other libraries QFE 1): the D3D8 bodies differ from PGR2's QFE 1, so
  5659's signatures miss them.
- **[console]** D3D8 QFE 4 CDevice: fields below +0x780 as QFE 1, +0x780 to +0x7A0 moved by 4, from about +0x900
  moved by 0x10 (config/eon.toml [layout] cites each). D3D__Streams moved to the end of D3D's data (0x27D148).
- **[game]** The frame buffers are the game's own (BufferSurfaces and DepthStencilSurface in the present
  parameters), back buffer 1280x480 with 2x horizontal supersampling; no auto depth buffer.
- **[game]** Every frame is recorded into one of two push buffers (0x35B970) and run at frame end; the front end also
  runs buffers recorded earlier, with fixups, every frame.
- **[game]** The .viv archives (BIGF) are mounted only when XGetDiskSectorSize("D:\\") is 2048 (0x5BD00 sets mode 2,
  the gate is 0x21110); otherwise the game looks for loose files the disc does not have.
- **[console]** 5659's IDirectSoundBuffer_PlayEx signature also matches StopEx's thunk (same bytes, other callee).

