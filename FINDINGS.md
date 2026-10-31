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
- **[game]** Bond's facing is his angles' yaw at +0x64 (degrees); his matrix +0x70 is rebuilt from it each update
  (0x156E50). With no lock and 0x153CD0 answering the camera's angles the bullet's forward is the camera's exactly.
  The camera mode in use is camera +8 + [camera +0x80] * 12: class 0x2E2694 the follow camera, 0x2DCF54 during the
  lock, 0x2E2624 the tutorial's cutscene.
- **[game]** Bond's camera: the camera manager [0x3210C0] +4, 0x240 bytes a camera, by the player index at +0x1130
  (0x8D960 copies its +0xC0 position, +0xD0 pitch/yaw/roll in degrees, +0x100 field of view).


## Language

- **[game]** A disc's languages come from its files; the tables at 0x2C3D38 name swe spa jap ita ger fre eng dut. Europe
  disc: ENG, DUT, ITA, SPA, SWE; Fr/De disc: FRE, GER. 0xC18C0 (thiscall, name; 0 the first listed) sets it, 0xC11A0 is
  the list's count, 0xC11C0 an entry, 0xC0800 gets the UI object. 0x1C9D0 (cdecl, name out) says whether to show
  fe_language_select.fng; with two languages the console's language picks instead. Boot (0x23F10) tries the saved language
  first. driving.xbe: set 0x1F3310, count 0x1ECAD0, entry 0x1E8810. USA: EON_VA in src/game/language.cpp, src/driving/language.cpp.
- **[port]** The setting replaces the name at both entries, so it beats the saved language and skips the screen. Text
  follows it. Measured speech stays on the disc's chatter\eng under Italian at the tutorial; chatter\ger on the Fr/De disc.
