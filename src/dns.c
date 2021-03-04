#include "dns.h"
#include "dns_data.h"
#include "fieldmap.h"
#include "field_weather.h"
#include "global.h"
#include "main.h"
#include "palette.h"
#include "rtc.h"
#include "shop.h"
#include "task.h"
#include "constants/map_types.h"
#include "gba/defines.h"

EWRAM_DATA bool8 gDontFadeWhite = FALSE;
EWRAM_DATA bool8 gWindowsLitUp = FALSE;
EWRAM_DATA u8 gLastRecordedFadeCoeff = 0;
EWRAM_DATA u16 gLastRecordedFadeColour = 0;
EWRAM_DATA IgnoredPalT gIgnoredDNSPalIndices[32];

static u32 GetMinuteDifference(u32 startYear, u8 startMonth, u8 startDay, u8 startHour, u8 startMin, u32 endYear, u8 endMonth, u8 endDay, u8 endHour, u8 endMin);
static u32 GetHourDifference(u32 startYear, u8 startMonth, u8 startDay, u8 startHour, u32 endYear, u8 endMonth, u8 endDay, u8 endHour);
static u32 GetDayDifference(u32 startYear, u8 startMonth, u8 startDay, u32 endYear, u8 endMonth, u8 endDay);
static u32 GetMonthDifference(u32 startYear, u8 startMonth, u32 endYear, u8 endMonth);
static u32 GetYearDifference(u32 startYear, u32 endYear);

static bool8 IsDayTime(void)
{
	return gLocalTime.hours >= TIME_MORNING_START && gLocalTime.hours < TIME_NIGHT_START;
}

static bool8 IsOnlyDayTime(void)
{
	return gLocalTime.hours >= TIME_DAY_START && gLocalTime.hours < TIME_EVENING_START;
}

static bool8 IsNightTime(void)
{
	return gLocalTime.hours >= TIME_NIGHT_START || gLocalTime.hours < TIME_MORNING_START;
}

static bool8 IsMorning(void)
{
	return gLocalTime.hours >= TIME_MORNING_START && gLocalTime.hours < TIME_DAY_START;
}

static bool8 IsEvening(void)
{
	return gLocalTime.hours >= TIME_EVENING_START && gLocalTime.hours < TIME_NIGHT_START;
}

static bool8 IsDate1BeforeDate2(u32 y1, u32 m1, u32 d1, u32 y2, u32 m2, u32 d2)
{
	return y1 < y2 ? TRUE : (y1 == y2 ? (m1 < m2 ? TRUE : (m1 == m2 ? d1 < d2 : FALSE)) : FALSE);
}

static u32 GetMinuteDifference(u32 startYear, u8 startMonth, u8 startDay, u8 startHour, u8 startMin, u32 endYear, u8 endMonth, u8 endDay, u8 endHour, u8 endMin)
{
	if (startYear > endYear
	|| (startYear == endYear && startMonth > endMonth)
	|| (startYear == endYear && startMonth == endMonth && startDay > endDay)
	|| (startYear == endYear && startMonth == endMonth && startDay == endDay && startHour > endHour)
	|| (startYear == endYear && startMonth == endMonth && startDay == endDay && startHour == endHour && startMin > endMin))
		return 0;

	u32 days = GetDayDifference(startYear, startMonth, startDay, endYear, endMonth, endDay);

	if (days >= 0xFFFFFFFF / 24 / 60)
		return 0xFFFFFFFF; // Max minutes
	else
	{
		u32 hours = GetHourDifference(startYear, startMonth, startDay, startHour, endYear, endMonth, endDay, endHour);

		if (startMin > endMin)
			return (max(1, hours) - 1) * 60 + ((endMin + 60) - startMin);
		else
			return hours * 60 + (endMin - startMin);
	}
}

static u32 GetHourDifference(u32 startYear, u8 startMonth, u8 startDay, u8 startHour, u32 endYear, u8 endMonth, u8 endDay, u8 endHour)
{
	if (startYear > endYear
	|| (startYear == endYear && startMonth > endMonth)
	|| (startYear == endYear && startMonth == endMonth && startDay > endDay)
	|| (startYear == endYear && startMonth == endMonth && startDay == endDay && startHour > endHour))
		return 0;

	u32 days = GetDayDifference(startYear, startMonth, startDay, endYear, endMonth, endDay);

	if (days >= 0xFFFFFFFF / 24)
		return 0xFFFFFFFF; //Max hours

	if (startHour > endHour)
		return (days - 1) * 24 + ((endHour + 24) - startHour);
	else //startHour <= endHour
		return days * 24 + (endHour - startHour);
}

static u32 GetDayDifference(u32 startYear, u8 startMonth, u8 startDay, u32 endYear, u8 endMonth, u8 endDay)
{
	const u16 cumDays[] = {0,31,59,90,120,151,181,212,243,273,304,334}; //Cumulative Days by month
	const u16 leapcumDays[] = {0,31,60,91,121,152,182,213,244,274,305,335}; //Cumulative Days by month for leap year
	u32 totdays = 0;

	if (!IsDate1BeforeDate2(startYear, startMonth, startDay, endYear, endMonth, endDay))
		return 0;

	if (startYear == endYear)
	{
		if (IsLeapYear(startYear))
			return (leapcumDays[endMonth - 1] + endDay) - (leapcumDays[startMonth - 1] + startDay);
		else
			return (cumDays[endMonth-1] + endDay) - (cumDays[startMonth - 1] + startDay);
	}

	if (IsLeapYear(startYear))
		totdays = totdays + 366 - (leapcumDays[startMonth - 1] + startDay);
	else
		totdays = totdays + 365 - (cumDays[startMonth - 1] + startDay);

	u32 year = startYear + 1;
	while (year < endYear)
	{
		if (IsLeapYear(year))
			totdays = totdays + 366;
		else
			totdays = totdays + 365;

		year = year + 1;
	}

    if (IsLeapYear(endYear))
        totdays = totdays + (leapcumDays[endMonth - 1] + endDay);
    else
        totdays = totdays + (cumDays[endMonth - 1] + endDay);

    return totdays;
}

static u32 GetMonthDifference(u32 startYear, u8 startMonth, u32 endYear, u8 endMonth)
{
	if (startYear > endYear
	|| (startYear == endYear && startMonth > endMonth))
		return 0;

	else if (endMonth >= startMonth)
		return GetYearDifference(startYear, endYear) * 12 + (endMonth - startMonth);

	else
		return (max(1, GetYearDifference(startYear, endYear)) - 1) * 12 + ((endMonth + 12) - startMonth);
}

static u32 GetYearDifference(u32 startYear, u32 endYear)
{
	if (startYear >= endYear)
		return 0;

	return endYear - startYear;
}


static u16 FadeColourForDNS(struct PlttData* blend, u8 coeff, s8 r, s8 g, s8 b)
{
	return ((r + (((blend->r - r) * coeff) >> 4)) << 0)
		 | ((g + (((blend->g - g) * coeff) >> 4)) << 5)
		 | ((b + (((blend->b - b) * coeff) >> 4)) << 10);

/*
	u8 coeffMax = 128;

	return (((r * (coeffMax - coeff) + (((r * blend->r) >> 5) * coeff)) >> 8) << 0)
		 | (((g * (coeffMax - coeff) + (((g * blend->g) >> 5) * coeff)) >> 8) << 5)
		 | (((b * (coeffMax - coeff) + (((b * blend->b) >> 5) * coeff)) >> 8) << 10);
*/
}

static void BlendFadedPalette(u16 palOffset, u16 numEntries, u8 coeff, u32 blendColor)
{
	u16 i;
	u16 ignoreOffset = palOffset / 16;
	bool8 dontFadeWhite = gDontFadeWhite && !gMain.inBattle;

	for (i = 0; i < numEntries; ++i)
	{
		u16 index = i + palOffset;
		if (gPlttBufferFaded[index] == RGB_BLACK) continue; // Don't fade black

		if (gIgnoredDNSPalIndices[ignoreOffset][i]) continue; // Don't fade this index.

		// Fixes an issue with pre-battle mugshots
		if (dontFadeWhite && gPlttBufferFaded[index] == RGB_WHITE) continue;

		struct PlttData *data1 = (struct PlttData*) &gPlttBufferFaded[index];
		s8 r = data1->r;
		s8 g = data1->g;
		s8 b = data1->b;
		struct PlttData* data2 = (struct PlttData*) &blendColor;
		((u16*) PLTT)[index] = FadeColourForDNS(data2, coeff, r, g, b);
	}
}

static void BlendFadedPalettes(u32 selectedPalettes, u8 coeff, u32 color)
{
	u16 paletteOffset;

	for (paletteOffset = 256; selectedPalettes; paletteOffset += 16)
	{
		if (selectedPalettes & 1)
		{
			// Just blend everything
			BlendFadedPalette(paletteOffset, 16, coeff, color);
		}
		selectedPalettes >>= 1;
	}
}

static void BlendFadedUnfadedPalette(u16 palOffset, u16 numEntries, u8 coeff, u32 blendColor, bool8 palFadeActive)
{
	u16 i;
	u16 ignoreOffset = palOffset / 16;

	for (i = 0; i < numEntries; ++i)
	{
		u16 index = i + palOffset;
		if (gPlttBufferUnfaded[index] == RGB_BLACK) continue; //Don't fade black

		if (gIgnoredDNSPalIndices[ignoreOffset][i]) continue; //Don't fade this index.

		struct PlttData* data1 = (struct PlttData*) &gPlttBufferUnfaded[index];
		struct PlttData* data2 = (struct PlttData*) &blendColor;
		s8 r = data1->r;
		s8 g = data1->g;
		s8 b = data1->b;

		gPlttBufferUnfaded[index] = FadeColourForDNS(data2, coeff, r, g, b);

		if (!palFadeActive)
			gPlttBufferFaded[index] = FadeColourForDNS(data2, coeff, r, g, b);
	}
}

// This function gets called once when the game transitions to a new fade colour
static void FadeOverworldBackground(u32 selectedPalettes, u8 coeff, u32 color, bool8 palFadeActive)
{
	u32 i, j, row, column;

	if (IsNightTime())
	{
		if (!gWindowsLitUp)
		{
			for (i = 0; i < ARRAY_COUNT(gSpecificTilesetFades); ++i)
			{
				if ((u32) gMapHeader.mapLayout->primaryTileset == gSpecificTilesetFades[i].tilesetPointer
				||  (u32) gMapHeader.mapLayout->secondaryTileset == gSpecificTilesetFades[i].tilesetPointer)
				{
					row = gSpecificTilesetFades[i].paletteNumToFade;

					if (row == 11 && FuncIsActiveTask(Task_BuyMenu))
						continue; // Don't fade palette 11 in shop menu

					for (j = 0; gSpecificTilesetFades[i].paletteIndicesToFade[j].index != 0xFF; ++j)
					{
						column = gSpecificTilesetFades[i].paletteIndicesToFade[j].index;
						gPlttBufferUnfaded[row * 16 + column] = gSpecificTilesetFades[i].paletteIndicesToFade[j].colour;
						if (!palFadeActive)
							gPlttBufferFaded[row * 16 + column] = gSpecificTilesetFades[i].paletteIndicesToFade[j].colour;
						gIgnoredDNSPalIndices[row][column] = TRUE;
					}
				}
			}

			gWindowsLitUp = TRUE;
		}
	}
	else
	{
		if (!palFadeActive)
		{
			LoadMapTilesetPalettes(gMapHeader.mapLayout);
		}
		memset(gIgnoredDNSPalIndices, 0, sizeof(bool8) * 16 * 32); // Don't ignore colours during day
		gWindowsLitUp = FALSE;
	}

	for (u16 paletteOffset = 0; paletteOffset < 256; paletteOffset += 16) // Only background colours
	{
		if (selectedPalettes & 1)
		{
			BlendFadedUnfadedPalette(paletteOffset, 16, coeff, color, palFadeActive);
		}
		selectedPalettes >>= 1;
	}
}


void FadeDayNightPalettes()
{
	u32 palsToFade;
	bool8 inOverworld, fadePalettes;

	switch (gMapHeader.mapType) { // Save time by not calling the function
		case MAP_TYPE_TOWN: // Try to force a jump table to manually placing these values
		case MAP_TYPE_CITY:
		case MAP_TYPE_ROUTE:
		case MAP_TYPE_UNDERWATER:
		case MAP_TYPE_OCEAN_ROUTE:
		default:
			inOverworld = FuncIsActiveTask(Task_WeatherMain);
			fadePalettes = inOverworld; // || gInShop;

			if (fadePalettes)
			{
				const u8 coeff = gDNSNightFadingByTime[gLocalTime.hours][gLocalTime.minutes / 10].amount;
				const u16 colour = gDNSNightFadingByTime[gLocalTime.hours][gLocalTime.minutes / 10].colour;
				bool8 palFadeActive = gPaletteFade.active || gWeatherPtr->palProcessingState == WEATHER_PAL_STATE_SCREEN_FADING_IN;

				if (inOverworld)
					palsToFade = OW_DNS_PAL_FADE;
				else
					palsToFade = OW_DNS_PAL_FADE & ~(OBG_SHI(11)); //Used by shop

				if (gLastRecordedFadeCoeff != coeff
				||  gLastRecordedFadeColour != colour) // Only fade the background if colour should change
				{
					bool8 hardFade = gLastRecordedFadeCoeff == 0xFF; // Set to 0xFF next so check up here

					if (!palFadeActive)
						LoadMapTilesetPalettes(gMapHeader.mapLayout);

					gWindowsLitUp = FALSE;
					FadeOverworldBackground(palsToFade, coeff, colour, palFadeActive); // Load/remove the palettes to fade once during the day and night
					gLastRecordedFadeCoeff = coeff;
					gLastRecordedFadeColour = colour;

					// The weather fading needs to be reloaded when the tileset palette is reloaded
					if (!palFadeActive)
					{
						for (u8 paletteIndex = 0; paletteIndex < 13; paletteIndex++)
							ApplyWeatherGammaShiftToPal(paletteIndex);
					}

					if (hardFade) // Changed routes and part of the tileset was reloaded
						DmaCopy16(3, gPlttBufferFaded, (void *)PLTT, PLTT_SIZE / 2);
				}

				if (coeff == 0)
					break; // Don't bother fading a null fade

				palsToFade = (palsToFade & ~OW_DNS_BG_PAL_FADE) >> 16;
				BlendFadedPalettes(palsToFade, coeff, colour);
			}
			break;
        case MAP_TYPE_UNKNOWN:
		case MAP_TYPE_INDOOR: // No fading in these areas
		case MAP_TYPE_UNDERGROUND:
		case MAP_TYPE_NONE:
		case MAP_TYPE_SECRET_BASE:
			gLastRecordedFadeCoeff = 0;
			break;
	}
}
