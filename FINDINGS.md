# 007: Everything or Nothing recomp - findings

What the work has established, by topic. PROGRESS.md has the dated log with the evidence for each item. Each item
is marked **[console]** (how the Xbox, its XDK or the runtime behaves: the fix is in the SDK) or **[game]** (this
game's own code and data, with its addresses).

## The image

- **[console]** LOG_FIRST writes at info level, which a release log leaves out: a diagnostic meant for a
  player's log has to be report level (LOGR) or an error.

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
- **[game]** The skinned characters are push buffers built with the models: 132 no-ops (filled by a fixup with the
  bone matrices), a constant load, then index strips (0x1800/0x1808 array elements); vertex data, program and
  textures set through the API before RunPushBuffer.
- **[game]** The vertex programs are loaded with pushed commands (0x1E9C, 0x0B00) inside the recorded frame and
  selected with SelectVertexShader(0x27D209 or 0, slot); 0x27D208 is the library's direct-input shader.
- **[game]** BLACK is wall cover (the training level's first step).
- **[game]** Character shadows: a depth-only pass into a D24S8 surface given as colour target and depth buffer
  (0x03043000, 128x128 or 256x256), then the ground patches under the character redrawn with pixel shader
  0x27B5B4 (stage 0 PROJECT2D on the depth texture, border addressing; stage 1 bump env) - about 8000 draws a
  frame in the first level.
- **[game]** Movies: XMV decodes into a YUY2 surface the game draws as an immediate quad through vertex program
  slot 0, selected before the program is loaded there (SelectVertexShader(0, 0) from 0xFA6C5).
- **[console]** A selected vertex program runs from program memory as it is at the draw, not as it was at the select.
- **[game]** 0x5E270 (mode in al) picks the frame buffers: supersampled 1280x480 (MultiSampleType 0x1121) or plain
  640x480 (0x11); src/game/supersample.cpp always takes the plain set (the carousel's frame copy expects it).
- **[game]** Without supersampling the particles (0xFA3C0 .. 0xFA4F0) are drawn with D3DRS_MULTISAMPLEMODE 4X and
  vertex constant 8 at half the buffer's size: a quarter of the pixels, stretched 2 by 2.
- **[console]** A multisample mode with more samples than the target was made with draws the target as a smaller
  one (4X on a plain 640x480 buffer: 320x240, each pixel 2 by 2); sdk State::aa_x, aa_y.

