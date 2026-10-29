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
- **[game]** driving.xbe: a library at 0x23D460 to 0x240E20 (4 slots of 0x928 bytes from 0x36FE10, the current
  one in 0x3722B0) guards its entry points with a plain busy flag at 0x3722C0; the title's threads raced through
  it on PC (a null 0x3722B0 read in 0x23E420). Its 13 entry points are serialised (src/driving/library_lock.cpp).
- **[game]** 'STRM' (0x4D525453) streams, in both xbes: taking a reader's next packet (driving 0x24AC20, default
  0x7C960) walks the packet chain outside the stream's critical section; run whole inside it (src/*/stream_walk.cpp).
- **[game]** Releases: both European discs are one code (the En/Es/It/Nl/Sv disc and the Fr/De one, certificate
  version 4; a media-patched copy differs by a JMP for JGE after XapiVerifyMediaInDrive). The USA disc is another
  compile: on foot the libraries sit 0x840 lower, driving 0x40; game code and data move by varying amounts. Its
  addresses come from sdk/tools/symbols/xbe_port.py; the hooks hold both (src/common/release.h).
- **[console]** Windows: Microsoft Defender's behaviour model quarantines an exe that starts a process suspended and
  then resumes it (Behavior:Win32/DefenseEvasion.A!ml); the relaunch passes its handover event's name on the
  command line instead.
- **[console]** A vertex attribute's offset can lie far past its stream's stride (the console adds it to the
  buffer's address): EoN's third MI6 Training enemy points its four attributes 60172 to 60196 bytes in (2149
  vertices of 28 bytes). Kept as D3D input element offsets, past the input assembler's 2047, they hung NVIDIA GPUs
  (D3D11 and D3D12, the GPU stalling ahead of the draw, no shader active, Aftermath "Hung"); the runtime moves whole
  vertices' worth of offset to the buffer's start.
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

