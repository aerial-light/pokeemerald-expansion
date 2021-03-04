#pragma once

#include "gba/gba.h"

extern bool8 gDontFadeWhite;
extern bool8 gWindowsLitUp;
extern u8 gLastRecordedFadeCoeff;
extern u16 gLastRecordedFadeColour;

typedef bool8 IgnoredPalT[16];
extern IgnoredPalT gIgnoredDNSPalIndices[];

extern void FadeDayNightPalettes();