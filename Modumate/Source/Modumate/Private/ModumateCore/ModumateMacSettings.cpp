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
	ppv->Settings.bOverride_ColorGamma = true;
	ppv->Settings.bOverride_ColorGammaShadows = true;
	ppv->Settings.bOverride_ColorGammaMidtones = true;
	ppv->Settings.bOverride_ColorGammaHighlights = true;
	
	ppv->Settings.bOverride_ColorGainShadows = true;
	ppv->Settings.bOverride_FilmShoulder = true;
	if (bIsMacEnabled)
	{
		ppv->Settings.ColorGamma = FVector4(MacGlobalGamma, MacGlobalGamma, MacGlobalGamma, 1.0f);
		ppv->Settings.ColorGammaShadows = FVector4(MacShadowsGamma, MacShadowsGamma, MacShadowsGamma, 1.0f);
		ppv->Settings.ColorGammaMidtones = FVector4(MacMidtonesGamma, MacMidtonesGamma, MacMidtonesGamma, 1.0f);
		ppv->Settings.ColorGammaHighlights = FVector4(MacHighlightsGamma, MacHighlightsGamma, MacHighlightsGamma, 1.0f);
		
		ppv->Settings.ColorGainShadows = FVector4(MacShadowsGain, MacShadowsGain, MacShadowsGain, 1.0f);
		ppv->Settings.FilmShoulder = MacShoulder;
	}
	else {
		ppv->Settings.ColorGamma = FVector4(GlobalGamma, GlobalGamma, GlobalGamma, 1.0f);
		ppv->Settings.ColorGammaShadows = FVector4(ShadowsGamma, ShadowsGamma, ShadowsGamma, 1.0f);
		ppv->Settings.ColorGammaMidtones = FVector4(MidtonesGamma, MidtonesGamma, MidtonesGamma, 1.0f);
		ppv->Settings.ColorGammaHighlights = FVector4(HighlightsGamma, HighlightsGamma, HighlightsGamma, 1.0f);
		
		ppv->Settings.ColorGainShadows = FVector4(ShadowsGain, ShadowsGain, ShadowsGain, 1.0f);
		ppv->Settings.FilmShoulder = Shoulder;
	}
}
