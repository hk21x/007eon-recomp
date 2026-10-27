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


## Aiming

- **[game]** Controls come from global\xbbtnmap.csv (actdat1.viv): ACTION,BUTTON,FLAG,BUTTON,REQUIREDMODE rows, two
  schemes. The action names are a table of 48-byte records in default.xbe (name, then action bits for word 0 and
  word 1, a kind, ...): MANUALAIMTOGGLE is word 0 bit 0x1000, MANUALAIMMOVE analog 9, AIMOFFSETMOVE analog 10. The
  shipped map binds no MANUALAIMTOGGLE; the right stick click is RESETCAMERA. 0x1C330 turns the pad into actions
  (0xEE5F0 sets bits at +8/+0xC of an action set, 0xEE630(word 0, word 1) tests them; the player keeps this frame's
  at +0x1230 and last frame's at +0x11F0).
- **[game]** The only reader of MANUALAIMTOGGLE is 0x153810, the scope's first person zoom: on its release, for a
  weapon that answers 0xE07689D3 (0x14B640, zoomable), as the lock action 0x4000 also does. Bound in a test copy
  (BACK), the toggle reaches the game and nothing opens with the SIG 552: there is no free aim on foot.
- **[game]** The crosshair (0x11AB20, state at +0x2C): 7, UI_HUD_CROSSHAIRITEM_MANUALAIM, only on a mounted gun
  (0x1500B0/0x1500F0: +0x11D4 set, its flags +0x504 & 0x202); 5/2 locked on (0x148EF0: +0x1275 and action 0x4000);
  1/4 a target in view (vtable +0x170), PASSIVE; 3 a lock that misses (0x1584A0).
- **[game]** A gun's shot is 0x180CB0 (from 0x181250, which takes the round with 0x180530): the gun's own forward
  (its angles at +0x60, 0xEECB0: pitch, yaw, roll in degrees to forward, right and up; forward cos p cos y,
  cos p sin y, -sin p, z up) unless its owner has an aim (vtable +0x1AC) within 90 degrees (else +0x3D0 bit
  0x40000), then the bullet (0x222BF0: owner, gun, ..., start, forward, right, up). Enemies (vtable 0x2DF1B8) aim
  in 0x193360 (the lock +0xE30, or the point +0xE20 while +0xE4C is set); Bond (vtable 0x2E33B8, the object
  0x153810 runs for) in 0x153CD0 (the camera's angles in the scope view, the lock via 0x14E480, else his aim pose
  +0x12B0), with the start 0x153BE0 and has-aim 0x1493A0 (false while +0x1550 holds 0x100, 0x800 or 0x1000).
- **[game]** Bond's camera: the camera manager [0x3210C0] +4, 0x240 bytes a camera, by the player index at +0x1130
  (0x8D960 copies its +0xC0 position, +0xD0 pitch/yaw/roll in degrees, +0x100 field of view).
