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
- SDK: the fork's main at 697c30b (2026-10-25: eon merged after EoN, PGR1 and PGR2 smoke OK); task branch `eon`
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

### 2026-10-25 - Thermovision and the driving radar on Windows

Harry (Windows, 3440x1440): Thermovision "bugged out" (the screen looked frozen, tinted blue) and the driving radar
was stretched wide with its right side cut. Capture 180345: the effect renders the thermal view into the frame
(83A00000), shrinks it into 8316CA00 (4925, 4926), blends it into the accumulation buffer 8316C000 (4927: blend
CONSTANTALPHA / INVCONSTANTALPHA, 0x8003 0x8004) and colours that through a 256x1 ramp (4928). D3D12 and D3D11 mapped
0x8003 to the blend factor, which scales colour by the blend colour's RGB (0 here), so nothing blended in and the
buffer kept the scene from before: a still picture. The reference renderer had it right (alpha), so headless never
showed it. sdk: BlendFactor() puts the blend colour's alpha in all four channels for constant alpha blends. The log
ends with a normal close, so the "crash" was likely that still picture. Radar: its map is drawn through the HUD's
matrix program with a rotation (it turns with the car); OrthoConstants wanted x from x alone, so it was not squeezed
to 16:9 while its frame was. Now any 2D matrix (no depth, w 1, y flipped) counts. Smoke OK. Not seen headless (the
radar needs a drive this launch data does not reach; Thermovision a level the saves here do not have): Harry's test.

### 2026-10-25 - 30 minute soak with today's changes

scripts/soak.sh 30 (the level, Quit, New Game, Ground Zero, over and over) with the sharp 2D textures, the Quit Game
item, the menu mouse and the keyboard pictures: no traps, frames to the end (104940), 32 level loads, resident
memory flat (106 MB at 5 minutes, 105 MB at 30; data 607 -> 608 MB).

### 2026-10-25 - mouse in the driving missions' menus

driving.xbe has the same menu engine: its package input function is 0x13C2D0 (default.xbe's 0x131610; the package's
mask +0x20 and focus +0x84 the same, set-focus 0x1353F0). src/driving/menu_input.cpp marks a menu there too.
Headless (Train Chase, START): "a menu takes the input" with the pause screen up. The pause screens' tabs change with
the d-pad's left and right, which the wheel does not play (the arrow keys do).

### 2026-10-25 - SDK eon branch merged into the fork's main

Fork main had two new commits (PGR2's W-buffer clipping fix; a Quit game button at the settings window's top right).
Merged into eon (697c30b): the settings window keeps main's top-right button (asks first), now through app::Quit; the
footer button from earlier today goes. Smoke with 697c30b: EoN OK, PGR2 OK (its worktree, put back to 6e84311 after),
PGR1 OK (/home/harry/smoke-pgr1, put back to 2fbeb57). Fork main fast-forwarded to 697c30b; EoN's pointer moved.

### 2026-10-25 - sharp text and HUD, mouse in the menus

Sharp 2D (sdk 94a9acf): GameInfo::sharpen_2d and Settings::sharp_2d. A texture a 2D draw samples (pretransformed or
a screen program; swizzled, up to 512, not P8) is copied and enlarged on a background thread: alpha that is mostly
fully in or out (text, outlines) as a mask (Catmull-Rom, then thresholded at half with an edge one output pixel
wide), else interpolated; colour Catmull-Rom weighted by alpha; DXT decoded on the CPU. Text (A8) and pictures up to
128 four times, larger ones twice (a 256 picture at four times would be 4 MB of memory and 5 of video memory). Then
it is the texture's replacement (dropped when its bytes change; a texture dropped twice is left alone).
XBE_SHARPEN_DUMP=folder writes each as a TGA; the reference renderer runs it too. Headless: the menus' fonts (A8
256x128, 256x256), flags, logo, the HUD's weapon atlas and menu pictures made in about 0.2 s each.

Mouse in the menus: sdk input::TakeWheel and MouseButtons (raw input's wheel). The menus' engine gives each package
the input through 0x131610 (the package, the port); one taking port 1 with a button in focus (+0x84) marks a menu
in use (src/game/menu_input.cpp). GameInfo::pad (src/common/menu_mouse.cpp): in a menu the mouse buttons stop playing
their bindings, left is A, right B, each wheel notch a d-pad press. Headless: three notches down and a click choose
Quit Game; "out of the menus" from the level's load through play. Smoke OK.

### 2026-10-25 - Quit Game: main menu and settings window

sdk: app::Quit (closing the window, headless a shutdown) and a Quit game button in the settings window's footer
(asked twice). Main menu: the menus are EA FEng packages, resident and parsed by 0x138900; frontend_spmp.fng has a
fourth item this release never shows, Online (button 0x3BD5E, labels 0x3BD60/0x3BD5F, help 0x40978), whose focus
response hands the focus back to Options without Xbox Live. src/game/quit_item.cpp parses a copy with that item made
Quit Game: new string hashes for its labels and help (answered by a hook on the string lookup 0x142F60, in the game's
language told from its Single Player), its button's responses as the other items' with a message of its own (response
type 2 goes through 0x1305A0, hooked: app::Quit). Headless: the menu shows Quit Game under Options, it highlights,
and A on it closes the game ("Quit Game chosen in the main menu").

### 2026-10-25 - keyboard and mouse prompts, field of view

Keyboard prompts: sdk (input): UsingKeyboard, what controller 1 was last played with (a bound key or mouse button,
the mouse moved 16 counts while looking, or no controller plugged in: the keyboard; a controller's button or a stick
pushed half way: the controller), and ControlKeys (a control's bound keys, the settings' or the defaults); the
harness's KEYS token. src/common/buttons.cpp keeps each atlas it finds (its disc bytes, decoded) and draws it again at
its next bind whenever the look changes (controller or keyboard, a key bound anew, modern_buttons), the last two
drawings cached: on the keyboard every button's picture is its key on a cap (SPACE, CTRL, ESC, TAB, letters; arrows
as heads) or a mouse with the button lit, the d-pads as arrow-key clusters with the directions lit, the left stick as
WASD, the right stick as a mouse. The game names the sticks ANALOGPAD and ANALOGPADRIGHT (icons 8 and 9 from
0x117310); which atlas picture each is was not seen in a run (the tutorial step showing it was not reached), so the
left one is taken as the left stick. The game's LT rectangle ends at row 21 in the menus (93..115, 2..21) and at 25
in the tutorial, so the trigger plates now sit above row 22. Headless MI6 Training with KEYS: "E or Q: Punch left or
right" drawn with key caps (log: "drawn as the keys and mouse").

fov_percent (50 to 150, src/game/fov.cpp): 0x61080 makes every view's projection from the vertical field of view in
radians (1 over tan of half of it); its callers make the culling planes from its results, so the angle is scaled
there. Headless at 130%: 78 degrees for the game's 60, the training room wider. Smoke OK; driving runs (its P8 atlas
drawn). Next: Harry's Windows test of the sharp icons, the keyboard prompts and the FOV; the sticks' pictures when a
prompt shows one.

### 2026-10-25 - sharp button pictures

The prompts' atlas is 128x128, so on a 1440p screen each icon was a blurry 20 texels enlarged three times. sdk eb577a0:
gpu::SetReplacement, a title's own picture for a texture's memory: D3D12 and D3D11 upload it (with a box filtered mip
chain, sampled across it) and draw it instead of the guest bytes while those stay as they were when it was set (checked
once a frame; memory given to something else drops it). src/common/buttons.cpp now draws every icon as shapes and a
stroke font (src/common/strokes.cpp, A-Z 0-9 and a few signs, for the keyboard pictures next): the face buttons,
triggers, bumpers, Menu and View as before, and now the seven d-pads (the arms the red marks meant drawn white), the
sticks and the menus' arrows too. Drawn twice: at 128 into guest memory as before (DXT3 or P8), and at 512 for the
renderers. Offline test of both atlases (the disc's DXT3 and driving's P8 dump) at 512: clean edges, no old art left
around the boxes (cleared before enlarging). Headless MI6 Training: the 1x atlas patched in the menu and again in the
level (82085080, 82BD1D00), "Y or X: Punch" drawn with the new icons. Next: Harry's look on Windows (the log names the
512 picture once: "drawn from the title's 512x512 picture"); keyboard and mouse pictures.

### 2026-10-25 - mouse aiming straight into the game's look input

The mouse played the right stick, through the game's stick processing. The game reads controller 1's sticks in
0x1BC90 (via 0x5D210, the pad block 0x35B4A8, 0x90 a port): scaled to -1..1, squared when the response curve option
0x2FD334 is on, pushed out on the diagonals, clamped; 0x1C330 maps them to actions (0x32E20 is the menus' direction
bits). sdk aaef875: input::TakeMouse (the movement since the last call as a rate in deflections, sensitivity and
invert applied) and GameInfo::mouse_direct (the runtime leaves the stick alone). src/game/mouse.cpp adds the rate to
the right stick after 0x1BC90: no dead zone offset, no curve, no clamp, so the turn follows the mouse in proportion and
a fast move turns past the stick's full rate. Headless (harness MOUSER, half a deflection): the camera turns as with
the stick, half as far. Smoke OK. A camera-angle hunt by memory dumps (MI6 Training, the stick held) found bone
rotations, not the camera; the input route is what is used.

### 2026-10-25 - the driving HUD on ultrawide

Harry's 2.39:1 screenshot: the 3D widened, the "Missiles" text in proportion, the radar and the weapon picture
stretched and out at the screen's edges. driving.xbe lays its HUD out for 16:9 itself on a widescreen display
(0xDA0A0 and others: the frame object's +0x4C narrows sizes by 0.75), so its hud_aspect is 16:9 after all (the 4:3
of the last entry undone). Its text draws through the pass-through program (squeezed); the radar, pictures and boxes
through a program that multiplies v0 by c0..c3 (dp4 x4, then the XDK's screen tail) with an orthographic matrix
there (c0 = 1/320 0 0 -1, c1 = 0 -1/240 0 1, c3 = 0 0 0 1), which the squeeze did not know. sdk 597cc47:
MatrixProgram (four dp4 of v0 with consecutive constants at a program's start) gets the squeeze code, switched on per
draw only when those constants are an orthographic 2D matrix (OrthoConstants), so 3D draws with the same program
shape stay. Headless at 2.39:1: the objective box 225..414 squeezed to 250..389 with its text; small render targets
(64x64 to 256x128) unsqueezed. Smoke OK.

### 2026-10-25 - the driving missions' button pictures

Harry's captures (112758..112915): the driving prompts ("Y Get In Vehicle") draw from the same button atlas, but
driving.xbe's copy is P8 (128x128 swizzled indices on stage 3, its palette bound with SetPalette), so the DXT3 patch
did not touch it. sdk fd520de: texture_bound gets the stage's palette and runs again from SetPalette.
src/common/buttons.cpp: a P8 128x128 texture decoded through its palette is the atlas when each 16x16 cell's mean
colour (premultiplied) is within 12 of the disc's (the DXT3 and P8 copies agree within 4; the look, not the bytes,
since the copy only binds in the car, which the headless run does not reach); the same icons are drawn and each
changed texel takes the palette's nearest colour (cleared ones a clear entry). The START and BACK boxes grew past
their art: the P8 copy has a faint halo (alpha 12) there. Tested standalone: Harry's captured atlas quantised back
to P8 with its own 253 colours, patched once and left alone the second time, clean. Smoke OK.

### 2026-10-25 - driving: widescreen and ultrawide; 60 by default

driving.xbe's views are 4:3 whatever widescreen flag its launch data carries (the launch data differs by one bit,
byte 1216 0x80, between a 4:3 and a widescreen run of default.xbe; the driving frame is the same with either), so on
a wider screen they stretched. A view's projection is built in 0x2111E0 (field of view, aspect, near, far): a square
projection, x times +0x140, y times +0x144 and the aspect, then the culling planes (left and right from the half
angle's tangent times +0x178 and +0x17C). Giving it a wider aspect only narrowed the height (tried: the view zoomed
in, still stretched). src/driving/widescreen.cpp: x divided by k (the frame's aspect over 4:3) after, +0x178 and
+0x17C k times larger while it runs, so the sides show more and are not culled. Headless at 16:9 and 2.39:1: Bond in
proportion, more of the level at the sides. The driving 2D is laid out for 4:3: game_info.cpp (built into each
image's executable) gives driving.xbe hud_aspect 4:3, so its HUD and text keep that shape in the centre ("NEW
OBJECTIVE" checked at 2.39:1). driving_60fps is on by default (Harry: works). The driving button pictures: the HUD
atlas (0x82917880 in Harry's captures, P8 on stage 3) only loads in the car, which a blind headless walk does not
reach (Train Chase starts on foot); waiting for a capture with a driving prompt on screen.

### 2026-10-25 - driving at 60: the drawing task

Harry's log: the setting reached driving.xbe (interval 0 asked for) and every frame still took 33 ms. The drawing task
0x72410 draws (0x14EBF0) only when the blank callback's second count 0x35FBB8 (bumped every other blank, 0x15CD3F)
differs from the one it drew at (0x326F30): 30 a second whatever the ticks. src/driving/sixty.cpp now makes a blank
gone by look like a new count to it. Headless the driving mission now draws about 57 a second paced (55 unpaced):
the 33 ms a frame of the entry below was this limit, not the machine. Ticks keep to the blanks (3104 at 3253 after
loading). Smoke OK.

### 2026-10-25 - driving at 60 frames a second (setting, experimental)

driving.xbe is time based: it simulates in ticks of 1/60 s (0x3165F8 = 60, 0x3165FC = 1/60, the tick count 0x316600
advanced in 0x11CE0 by each tick 0x7A6B0, found with a gdb watchpoint), run by the scheduler 0x15F4D0 for the vertical
blanks counted at 0x35FBB4 (the blank callback 0x15CD30; the runtime's blank thread is real time). It presents every
second blank (0x214CB0(2) from the device setup 0x14D290), and the scheduler waits for two blanks and runs exactly two
ticks (0x15F5B3, last + 2) unless its object's +0x1C or +0x1E is set; then it runs a tick for each blank gone by and
draws once after (the swap is under the task queue 0x15DA60 the scheduler runs last: gdb at SwapFrame). An earlier
reading (forced interval 1 runs at double speed) compared frame numbers, not wall time: at the same wall time both
runs draw the same scene. src/driving/sixty.cpp (setting driving_60fps, off by default; EON_DRIVING60 headless):
0x214CB0 asked for 2 passes 1, and +0x1E is set while 0x15F4D0 runs. Headless it still drew 30 a second (the drawing task's
own limit, found next); the ticks keep to the blanks (3026 ticks at 3180 blanks after loading). Smoke OK.

### 2026-10-25 - every button picture modern

src/common/buttons.cpp redraws the whole prompt atlas (recognised by all of its bytes, FNV-1a 09076852FE367BA4): it
is decoded, each icon drawn again inside its own artwork's bounds (measured as the atlas's opaque components: the
rectangles the game draws with hold them, e.g. Y u 66.5..89.5 v 2.5..24.5 around its oval 69..86, 3..24), and the
blocks drawn on encoded again (the two texels furthest apart as end colours, each texel the nearest of four). A B X
Y: bold coloured letters on a dark disc; LT RT: labelled triggers where the arrow boxes were; LB RB: bumpers; START:
the Menu button's three lines; BACK: the View button's two squares (the words cleared: the icons hold the dot and
the word). D-pads and sticks as they were. EON_MODERN_BUTTONS=0 or 1 headless. driving.xbe's HUD does not bind this
atlas (its prompts are P8 textures): left as they are. Smoke OK.

### 2026-10-25 - LB and RB button pictures

The button prompts (\i<ACTION> in the strings) draw from one 128x128 DXT3 atlas: B X A Y, the trigger arrows,
the d-pads, START, BACK, the sticks, and WHITE and BLACK as small white and black ovals (16x16 cells at 32,64 and
48,64, the ovals in their columns 0 to 11, rows 0 to 13). src/common/buttons.cpp (setting modern_buttons, on by
default): when the atlas is bound (sdk 5d7f3eb GameInfo::texture_bound, from SetTexture, the texture marked dirty
after), recognised by the two cells' original bytes (FNV-1a D26D60581BC4099B), the cells are rewritten as LB and
RB bumpers inside the ovals' texels: white 3x7 letters on a dark rounded body with a grey edge, colours a DXT block
holds exactly. Headless: the atlas patched in the menus' and the level's copies (decoded from a memory dump);
SDL maps RB to BLACK and LB to WHITE, as the pictures say. Smoke OK.

### 2026-10-25 - driving.xbe: the first driving mission, headless; the palettes

Harry reached Train Chase on Windows: default.xbe hands over to driving.xbe (HalReturnToFirmware 2, launch data)
and it plays at 30 frames a second (present interval 2). His captures (080419..080450): the light glows patterned
discs, a black and white panel at the top left (the radar), the weapon icon noise. Headless now too: his save
(.local/harrysave2), Load Game, Train Chase (.local/drive_go.txt) writes hdd/launch.bin (kept as
.local/launch_trainchase.bin, the driving boot deletes it), then .local/drive.sh builds and runs driving.xbe on the
same disk. The glows are one draw sampling a P8 texture (format 0xB) on stage 3 alone, and most of the driving HUD
samples P8 on stage 3: driving.xbe's SetPalette (0x29F060, pushes 0x1B20 + stage * 0x40 itself, a QFE 4 body the
scan missed) was not replaced, so no palette reached the renderers. config/driving.toml names it; the reference
renderer now decodes P8 with the stage's palette (sdk 34cbe6b). Headless: the glows are small lights. default.xbe
sets no palettes. Next: Harry's test of the driving HUD.

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
