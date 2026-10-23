# 007: Everything or Nothing recomp - progress log

Newest first. Each entry: date, what changed, what was learned, what's next.

## Status
- Milestone: 0 - set up
- XDK: 5659, D3D8 QFE 4. default.xbe 84 of 250 functions, 7 of 13 data; driving.xbe 74 of 250, 5 of 13
- Push buffers: not known yet (BeginPush/EndPush are among the misses)
- SDK: fork main 2fbeb57
- Next: Ghidra projects for both xbes, then the D3D8 QFE 4 functions the game calls, starting with those boot needs
  (CreateDevice's neighbours, Clear, Swap, SetRenderTarget, SetStreamSource, SetIndices)

## Log

### 2026-10-23 - skeleton: the SDK template, two xbes, the first scans

The SDK's games template with EoN names; the SDK (hk21x/XboxXBERecomp main, 2fbeb57) as the sdk submodule. The disc
(title 45410042) has two executables, default.xbe (on-foot levels, `action/`) and driving.xbe (driving levels,
`driving/`), so two targets: EoN and EoNDriving (`GameInfo::images` lists both for the relaunch on
XLaunchNewImage). config/eon.toml and config/driving.toml are the hand configs over the two scans.

Scan with sdk/xdk/5659: default.xbe 84 of 250 functions, 7 of 13 data; driving.xbe 74 of 250, 5 of 13. The library
headers say why: every library is 5659 QFE 1 as in PGR2 except D3D8, which is QFE 4, and nearly all D3DDevice
functions miss (13 found only by their constants). No XNet/XOnline library is linked (the WSA/XNet misses are not in
the image); movies are XMV (Bink misses likewise). The DirectSound misses (CreateSoundBuffer among them) need a look.
