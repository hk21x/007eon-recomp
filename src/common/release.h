// The release an executable is built for. The European discs share one code; the USA disc is another compile of the
// same game, every address elsewhere (CMakeLists.txt sets EON_USA from assets/default.xbe's SHA-1). The hooks give
// each address for both, European first: EON_VA(0x7C960, 0x7C690) a value, EON_FN(0007C960, 0007C690) the function
// to replace and EON_ORIG(...) its recompiled body. The USA addresses are the European ones ported by
// sdk/tools/symbols/xbe_port.py; the comments cite the European ones.
#pragma once

#ifndef EON_USA
#define EON_USA 0
#endif

#define EON_CAT_(a, b) a##b
#define EON_CAT(a, b) EON_CAT_(a, b)

#if EON_USA
#define EON_VA(eu, us) (us)
#define EON_FN(eu, us) EON_CAT(sub_, us)
#define EON_ORIG(eu, us) EON_CAT(__imp__sub_, us)
#else
#define EON_VA(eu, us) (eu)
#define EON_FN(eu, us) EON_CAT(sub_, eu)
#define EON_ORIG(eu, us) EON_CAT(__imp__sub_, eu)
#endif
