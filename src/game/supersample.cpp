// EoN's frame buffers: the supersampled set or the plain one.
//
// 0x5E270 (the mode in al) chooses the present parameters CreateDevice and its frame end switch use: al set, 2x
// horizontal supersampling (MultiSampleType 0x1121) with the game's own 1280x480 back buffer 0x35B754, front buffer
// 0x35BC70 and depth buffer 0x35BCA4; al clear, no multisampling (0x11) with the 640x480 set 0x35B9C4, 0x35B97C,
// 0x35BC10. The init sets the mode at 0x5FF89 (0x35BBF4 = 1), a level's objects may ask for the other through
// 0x5FE90 (0x35BF58), and the frame end (0x60646) switches when the two differ.
//
// The runtime makes the device's buffers 640x480 whatever the title asked, and draws in its own resolution; the
// game's post effects in the supersampled mode address the back buffer as 1280 wide (the weapon carousel copied the
// last frame squeezed into the left half, every other frame). The plain mode is the game's own way to draw 640 wide,
// so every switch takes it.
#include "core/log.h"
#include "xdk/hle.h"

X86_EXTERN_FUNC(__imp__sub_0005E270);
X86_FUNC(sub_0005E270) {
    static bool told = false;
    if (!told && (ctx.eax & 0xFF)) LOGI("eon: the frame buffers without supersampling (the game asked for it)");
    told = true;
    ctx.eax &= ~0xFFu;  // al: the plain 640x480 set
    __imp__sub_0005E270(ctx, base);
}
