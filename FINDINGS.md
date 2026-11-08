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
  it on PC (a null 0x3722B0 read in 0x23E420). Its 13 entry points are serialised (src/driving/library_lock.cpp). A plain wait there can deadlock against what the holder needs: the periodic update 0x23F4E0 skips when busy.
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
- **[game]** Damage from a hit: 0x167810 (thiscall: the victim; the event on the stack, vtable 0x2C341C, made by
  0xA1D90), every character's (enemies' vtable +0x90 0x190E80, the base's 0x2DDD80; props and glass have others). The
  event: +0x20 the attacker (the bullet's owner), +0x14 the gun, +0x2C the damage type (the gun's +0x410), +0x30 the
  bone's hash ("m_head" at [0x356808], made at start by 0x4CAD0), +0x70 the base damage, +0x40/+0x50/+0x60 the point,
  direction and normal, +0x78 flags (0x80 from behind). The health is a float at the victim's +0x164 (its most
  +0x160); it answers whether any damage was dealt; 0x8F180 calls the death (+0xA0) after it at 0 or less. Measured:
  a training target 75, 42.5, 10, -22.5 from three SIG 552 rounds.
- **[game]** Wall cover: his action's kind 0x18 and the camera mode class 0x2DCF7C (USA 0x2DC4A4), which stay while he
  locks on there (traced by a research agent, then measured). Each frame 0x14B450 sets Bond +0xD70 bit 0x400 at an
  edge (the action's +0x1B4 & 0x30 the side, within 26.2 of it). Fire there (0x152EE0) calls 0x148B70 (fastcall,
  Bond), which builds the lean out, state 0x3C8 (0x1B5D90, vtable 0x2E192C, steps 0x1B60E0): D70 | 8 and 2 or 4 by
  side; out, its step 3 aims and fires, and goes back when its animation ends with Bond +0x106C (a float, seconds)
  at 0 or less; it fires again only once the gun's +0x328 has 0x80000 (a shot's end). As he comes out it sets Bond's
  fire bit +0x1550 & 0x10, which 0x149400 (fastcall, Bond; folded into its caller 0x149B60 until listed in
  [functions]) takes to fire the gun (vtable +0x178). In the training's step 2 fire does nothing (it waits for the
  lock); the lock leans him out at the target, and can take him out of cover (0x175020, state 0x3C1).
- **[game]** The cover camera's update 0x1AECD0 (thiscall; object vtable 0x2E173C): the right stick's speeds +0xD8
  (across) and +0xD4 add into offsets from the wall's direction, +0xE0 (degrees, within 80) and +0xDC (within 60),
  unless locked on (0x148EF0), +0xD04 or +0x10F3; with the lock they ease back to 0 and it looks toward the target.
- **[game]** Bond's weapons (traced by a research agent; the switch measured): the inventory [Bond +0x274] holds
  12 byte entries at +0 (the item first), their count +4, the item held +0x10, a second +0x14, the selection +0x18
  (0x1A8C80). A weapon has +0x328 bit 0x100 (0x1A8D10; gadgets the rest, 0x80). The weapon cycle 0x152070 (fastcall,
  Bond; CYCLEWEAPONSFORWARD, action bit 0x40000, on release) goes in array order (0x1A9240, the mask 0x100), past an
  item its vtable +0x12C (inventory +0x1C bit 0) disables and one sharing the held one's +0x294 (another mode); it
  selects (0x1AA2B0 on the inventory), keeps the item at Bond +0x1474 (0x25E40) and the time at +0x146C ([0x3210B0]),
  and switches 0.6 s later or on fire: 0x14D6F0 (thiscall on Bond: item, 0, 0) builds state 0x3CB with the animation,
  and refuses while the gun is busy. Not while Bond +0x1550 has 0x400000 or state 0x3C5 runs.
- **[game]** Bond's camera: the camera manager [0x3210C0] +4, 0x240 bytes a camera, by the player index at +0x1130
  (0x8D960 copies its +0xC0 position, +0xD0 pitch/yaw/roll in degrees, +0x100 field of view).

- **[game]** The camera in play (mode class 0x2E26BC) orbits Bond in 0x1D1C70 (thiscall, the seconds on the stack):
  his position +0x70, the camera's +0x80, the height angle +0xD0, the view's heading +0x94 (turning right lowers it).
  The right stick (squared across, through 0xEE690/0xEE6A0 and the camera sensitivity options 0x1F970/0x1F9E0) sets
  turn speeds +0xCC (heading) and +0xC8 (height) that ease toward it, so a turn runs on after the stick lets go. The
  stick is ignored in states 0x3CF and 0x3D0, in 0x3BD for 2 s from +0x1B0, while aiming (0x148EF0), and with Bond's
  +0xD04 or +0x10F3. The swing behind Bond: a timer +0xC0, +0xC4 holds it, +0xB8 the angle left, +0xBC the recentre
  button's. The camera info 0x8D960's +0x30 is the field of view as a factor of the game's (1 unless a zoom).
- **[game]** Bond's movement state +0x14AC: 0x3B9 running (0x1BE950: he turns to where he goes), 0x3BA strafing
  (0x1BE9A0, vtable 0x2E17D0: he faces his lock target and steps eight ways), 0x3BC, 0x3C0 others. 0x1BC740 chooses
  as he starts to move (thiscall: the machine, +0x14 Bond, +0x1C its stack; 0x99970 the memory, 0x99F10 the push with
  0x104): in the firing stance (action kinds [+0x11B0] +0x1AC 1, 0x12, 0x13) strafing needs a lock target (vtable
  +0x170, 0x1510F0: +0x127C while +0x1274), else running unless the action's vtable +0xA4 allows strafing. The
  strafe's check 0x1B2C40 keeps it each update only with a target and aiming (vtable +0x14C, 0x14B010) and picks the
  step from the stick against the action's +0x17C. Built while free aim aims, with those two answered yes inside the
  check alone (Bond himself as the target), the state is 0x3BA and the check says it steps, but on screen he does not
  move (Harry, 2026-11-06): the strafe most likely moves him about the target's position, here his own.
- **[game]** Bond's action (+0x11B0, class 0x2DE018 in free play) vtable +0xA4 (0x16BAE0: Bond's 0x14B600, his camera
  in the scope's view, 0x8DDD0/0x8DBB0) is the strafe without a lock: 0x1BC740's other branch, 0x1B2C40's other
  condition, and in 0x171500 (his facing, by action kind) it leaves his facing and runs 0x16F630, the right stick
  moving aim angles at +0x1C0/+0x1C4 (relative to +0x1190 while +0x10F3). Answered yes while free aim aims
  (headless, free play, CLICKR with a direction): state 0x3BA and his facing kept, but one short step a press (5 to
  10 units), not a walk; on Windows (Harry) he could not move at all with it.
- **[game]** Strafing with no lock, as built here: the strafe's step comes from his action's +0x17C (the way he goes)
  against his facing (0x163A30: his +0x64 in radians). 0x171500 sets +0x17C from +0x10B8 (0x15A170: the left stick's
  angle on the camera's smoothed heading +0x10B4) except while he aims (vtable +0x14C), when it returns first. Built
  while free aim aims, with the check's target and aiming answered and +0x17C set from +0x10B8: the step animation
  starts and he sidesteps (headless: 120 units a second, facing kept), as a real lock's strafe does. Traced in branch
  strafe-b against a real lock: the real one had action kind 0x12 and +0x17C following +0x10B8; ours kind 0 and
  +0x17C left where it started.
- **[game]** A gun (vtable 0x2DF788, built by 0x17FE30): its clip +0x3E8, the clip's size +0x3E4, its ammo type +0x3D8
  (the owner's count of it: 0x197170 on the owner's +0x278), rounds waiting to go in +0x3F0. There is no reload
  action in the button map (global\xbbtnmap.csv); a reload ends in 0x17F130 (an event with the gun's vtable +0x15C
  check): the waiting rounds, at most what the owner carries, become the clip. Putting a gun away (0x180670, 0x180810)
  moves the clip into the waiting rounds; equipping (vtable +0x110, 0x181100) fills them to the clip's size if none
  wait. What starts a reload when the clip runs out (Bond's weapon animation, not found yet) is the step left for a
  reload key.

## Language

- **[game]** A disc's languages come from its files; the tables at 0x2C3D38 name swe spa jap ita ger fre eng dut. Europe
  disc: ENG, DUT, ITA, SPA, SWE; Fr/De disc: FRE, GER. 0xC18C0 (thiscall, name; 0 the first listed) sets it, 0xC11A0 is
  the list's count, 0xC11C0 an entry, 0xC0800 gets the UI object. 0x1C9D0 (cdecl, name out) says whether to show
  fe_language_select.fng; with two languages the console's language picks instead. Boot (0x23F10) tries the saved language
  first. The list's first reading is the disc's; US disc: ENG alone. Later readings can name more: set to GER on the US
  disc, a later call found it and STRGER.VIV and movies\ger were missing. driving.xbe's list names all 8 on any disc
  (ENG DUT FRE GER ITA JAP SPA SWE), so it cannot say what the disc has; it sets its language from the launch data (+0x44C,
  "ENG"; 0x203E70 returns the data's +0x4C). USA: EON_VA in src/game/language.cpp.
- **[port]** The setting replaces the name at default.xbe's two entries, so it beats the saved language and skips the
  screen; the first reading decides for the whole run. Text follows it. Measured speech stays on the disc's chatter\eng
  under Italian at the tutorial; chatter\ger on the Fr/De disc. Not proved: that default.xbe writes the set language to the
  launch data (the name there was ENG in every run kept; the run set to Italian did not reach a driving mission headless).


## Menu packages

- **[game]** Response kind 0x200 opens a package by path: 0x1302B0 looks for it open, else asks the loader (first virtual of the
  FEng manager's +0x10C object, 0x121680, thiscall path, two outs, ret 12). The string lookup 0x142F60 runs every frame for every
  text object, so a string's words can change while it shows. Script tracks: a type, a key size, then at byte 6 the field (7 the
  position), a key count, keys of a time and the data; the first key (time -1) is the value, later keys are moves from it.
- **[game]** frontend_options.fng's third row (button 0x1BD72): its starting script ends in an event that disables it, as well as
  its focus check. frontend_options_audio.fng: the code finds the value rows by object name and writes the volumes into them.
- **[port]** Holding a direction 10 frames in a pad script can move twice (the menu's repeat); 3 frames moves once. A first boot, with
  no save, plays the training level and never shows the main menu.
- **[game]** Pause menu: pm_bg.fng behind tabs pm_objectives, pm_game, pm_options. A tab is a button whose left and right open the next package. Audio
  Settings' package does not load in a mission (the loader answers 0). PkHd holds only so many name bytes: pm_options.fng's has 16.
- **[port]** Volume rows play the volume's sample on each step (0x90A98729, 0xE2819A4A, 0x88ECEFDA): the copies use the ordinary click.

