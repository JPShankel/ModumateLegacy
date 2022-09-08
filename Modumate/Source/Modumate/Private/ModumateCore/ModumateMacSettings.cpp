// Copyright 2022 Modumate, Inc. All Rights Reserved.

#include "ModumateCore/ModumateMacSettings.h"

#include "Engine/PostProcessVolume.h"

UModumateMacSettings::UModumateMacSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}
void UModumateMacSettings::SetMacSettingsEnabled(APostProcessVolume* ppv, bool bIsMacEnabled)
{
	if (ppv == nullptr)
	{
		return;
	}
	ppv->Settings.bOverride_ColorSaturationMidtones = true;
	ppv->Settings.bOverride_ColorSaturationHighlights = true;
	ppv->Settings.bOverride_ColorGainShadows = true;
	ppv->Settings.bOverride_FilmShoulder = true;
	if (bIsMacEnabled)
	{
		ppv->Settings.ColorSaturationMidtones = FVector4(MacMidtoneSaturation, MacMidtoneSaturation, MacMidtoneSaturation, 1.0f);
		ppv->Settings.ColorSaturationHighlights = FVector4(MacHighlightsSaturation, MacHighlightsSaturation, MacHighlightsSaturation, 1.0f);
		ppv->Settings.ColorGainShadows = FVector4(MacShadowsGain, MacShadowsGain, MacShadowsGain, 1.0f);
		ppv->Settings.FilmShoulder = MacShoulder;
	}
	else {
		ppv->Settings.ColorSaturationMidtones = FVector4(MidtoneSaturation, MidtoneSaturation, MidtoneSaturation, 1.0f);
		ppv->Settings.ColorSaturationHighlights = FVector4(HighlightsSaturation, HighlightsSaturation, HighlightsSaturation, 1.0f);
		ppv->Settings.ColorGainShadows = FVector4(ShadowsGain, ShadowsGain, ShadowsGain, 1.0f);
		ppv->Settings.FilmShoulder = Shoulder;
	}
}
