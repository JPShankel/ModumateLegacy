// Copyright 2022 Modumate, Inc. All Rights Reserved.

#include "ModumateCore/ModumateRayTracingSettings.h"

#include "Engine/PostProcessVolume.h"
#include "Kismet/GameplayStatics.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UnrealClasses/CompoundMeshActor.h"

UModumateRayTracingSettings::UModumateRayTracingSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Init();
}
void UModumateRayTracingSettings::Init()
{
	if (bRayTracingAOEnabled.Num() < 5)
	{
		bRayTracingAOEnabled.Init(false, 5);
	}
	if (RTAOSamplesPerPixel.Num() < 5)
	{
		RTAOSamplesPerPixel.Init(0, 5);
	}
	if (RTAOIntensity.Num() < 5)
	{
		RTAOIntensity.Init(0, 5);
	}
	if (RTAORadius.Num() < 5)
	{
		RTAORadius.Init(0, 5);
	}
	if (RTGIType.Num() < 5)
	{
		RTGIType.Init(0, 5);
	}
	if (RayTracingGIMaxBounces.Num() < 5)
	{
		RayTracingGIMaxBounces.Init(0, 5);
	}
	if (RayTracingGISamplesPerPixel.Num() < 5)
	{
		RayTracingGISamplesPerPixel.Init(0, 5);
	}
	if (bRayTracingReflectionsEnabled.Num() < 5)
	{
		bRayTracingReflectionsEnabled.Init(false, 5);
	}
	if (RayTracingReflectionsMaxRoughness.Num() < 5)
	{
		RayTracingReflectionsMaxRoughness.Init(0.0f, 5);
	}
	if (RayTracingReflectionsMaxBounces.Num() < 5)
	{
		RayTracingReflectionsMaxBounces.Init(0, 5);
	}
	if (RayTracingReflectionsSamplesPerPixel.Num() < 5)
	{
		RayTracingReflectionsSamplesPerPixel.Init(0, 5);
	}
	if (RayTracingReflectionsShadows.Num() < 5)
	{
		RayTracingReflectionsShadows.Init(0, 5);
	}
	if (RayTracingReflectionsTranslucency.Num() < 5)
	{
		RayTracingReflectionsTranslucency.Init(false, 5);
	}
	if (bRayTracingTranslucencyEnabled.Num() < 5)
	{
		bRayTracingTranslucencyEnabled.Init(false, 5);
	}
	if (RayTracingTranslucencyMaxRoughness.Num() < 5)
	{
		RayTracingTranslucencyMaxRoughness.Init(0, 5);
	}
	if (RayTracingTranslucencyRefractionRays.Num() < 5)
	{
		RayTracingTranslucencyRefractionRays.Init(0, 5);
	}
	if (RayTracingTranslucencySamplesPerPixel.Num() < 5)
	{
		RayTracingTranslucencySamplesPerPixel.Init(0, 5);
	}
	if (RayTracingTranslucencyShadows.Num() < 5)
	{
		RayTracingTranslucencyShadows.Init(0, 5);
	}
	if (RayTracingTranslucencyRefraction.Num() < 5)
	{
		RayTracingTranslucencyRefraction.Init(false, 5);
	}
	if (RTExposure.Num() < 9)
	{
		RTExposure.Init(1.0f, 9);
	}
}
extern FAutoConsoleVariableRef CVarForceAllRayTracingEffects;
extern FAutoConsoleVariableRef CVarRayTracingOcclusion;
extern FAutoConsoleVariableRef CVarEnableRayTracingMaterials;
extern FAutoConsoleVariableRef CVarRayTracingGlobalIlluminationFireflySuppression;
void UModumateRayTracingSettings::SetRayTracingEnabled(APostProcessVolume* ppv, bool bIsRayTracingEnabled)
{
	if (ppv == nullptr)
	{
		return;
	}
	bRayTracingEnabled = bIsRayTracingEnabled;
	ppv->Settings.bOverride_AmbientCubemapIntensity = true;
	ppv->Settings.bOverride_AutoExposureMinBrightness = true;
	ppv->Settings.bOverride_AutoExposureMaxBrightness = true;
	ppv->Settings.bOverride_FilmToe = true;
	ppv->Settings.bOverride_WhiteTint = true;
	if (bRayTracingEnabled)
	{
		ppv->Settings.AmbientCubemapIntensity = RTCubemapIntensity;
		ppv->Settings.FilmToe = RTToe;
	}
	else {
		ppv->Settings.AmbientCubemapIntensity = DefaultCubemapIntensity;
		ppv->Settings.FilmToe = DefaultToe;
		ppv->Settings.AutoExposureMaxBrightness = DefaultExposure;
		ppv->Settings.AutoExposureMinBrightness = DefaultExposure;
		ApplyRayTraceQualitySettings(ppv, 0);
	}

	
	auto enableRTShadowsCVAR = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RayTracing.Shadows"));
	if (enableRTShadowsCVAR == nullptr)
	{
		return;
	}
	enableRTShadowsCVAR->Set(bIsRayTracingEnabled ? 1 : 0, EConsoleVariableFlags::ECVF_SetByGameSetting);
	
	auto enableRTMaterialsCVAR = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RayTracing.EnableMaterials"));
	if (enableRTMaterialsCVAR == nullptr)
	{
		return;
	}
	enableRTMaterialsCVAR->Set(bIsRayTracingEnabled, EConsoleVariableFlags::ECVF_SetByGameSetting);

	
	auto enableFireflySuppression = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RayTracing.GlobalIllumination.FireflySuppression"));
	if (enableFireflySuppression == nullptr)
	{
		return;
	}
	enableFireflySuppression->Set(bIsRayTracingEnabled, EConsoleVariableFlags::ECVF_SetByGameSetting);

	
	auto bForceAllRTOff = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RayTracing.ForceAllRayTracingEffects"));
	if (bForceAllRTOff != nullptr)
	{
		if (bIsRayTracingEnabled)
		{
			//if rt is on send -1 which will revert to using our defined RT settings
			bForceAllRTOff->Set(-1, EConsoleVariableFlags::ECVF_SetByGameSetting);
		}
		else {
			//if rt is off force disable all RT effects
			bForceAllRTOff->Set(0, EConsoleVariableFlags::ECVF_SetByGameSetting);
		}
	}
	TArray<ACompoundMeshActor*> outActors;
	FindAllActorsOfClass(ppv->GetWorld(), outActors);
	
	for (ACompoundMeshActor* actor : outActors)
	{
		actor->RayTracingEnabled_OnToggled();
	}
}
void UModumateRayTracingSettings::SetExposure(APostProcessVolume* ppv, uint8 ExposureIndex)
{
	auto bRTEnabledCVAR = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RayTracing.Shadows"));
	if (ppv == nullptr || bRTEnabledCVAR == nullptr ||  bRTEnabledCVAR->GetInt() == 0)
	{
		return;
	}
	ppv->Settings.bOverride_AutoExposureMinBrightness = true;
	ppv->Settings.bOverride_AutoExposureMaxBrightness = true;
	ppv->Settings.AutoExposureMinBrightness = RTExposure[ExposureIndex];
	ppv->Settings.AutoExposureMaxBrightness = RTExposure[ExposureIndex];
}
bool UModumateRayTracingSettings::ApplyRayTraceQualitySettings(APostProcessVolume* ppv, uint8 QualitySetting)
{	
	auto bRTEnabledCVAR = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RayTracing.Shadows"));
	if (bRTEnabledCVAR == nullptr)
	{
		return false;
	}
	if (bRTEnabledCVAR->GetInt() == 1)
	{
		bRayTracingEnabled = true;
	}
	else
	{
		bRayTracingEnabled = false;
	}
	if (ppv == nullptr)
	{
		return false;
	}
	//quality should be between 0-4
	if (QualitySetting > 4)
	{
		return false;
	}
	/* Ambient Occlusion - mild impact*/
	ppv->Settings.bOverride_RayTracingAO = true;
	ppv->Settings.bOverride_RayTracingAOSamplesPerPixel = true;
	ppv->Settings.bOverride_RayTracingAOIntensity = true;
	ppv->Settings.bOverride_RayTracingAORadius = true;

	ppv->Settings.RayTracingAO = bRayTracingEnabled;
	ppv->Settings.RayTracingAOSamplesPerPixel = RTAOSamplesPerPixel[QualitySetting];
	ppv->Settings.RayTracingAOIntensity = RTAOIntensity[QualitySetting];
	ppv->Settings.RayTracingAORadius = RTAORadius[QualitySetting];

	/* Global Illumination - large performance impact*/
	ppv->Settings.bOverride_RayTracingGIMaxBounces = true;
	ppv->Settings.bOverride_RayTracingGISamplesPerPixel = true;
	ppv->Settings.bOverride_RayTracingGI = true;
	if (bRayTracingEnabled)
	{
		switch (RTGIType[QualitySetting])
		{
		case 1:
			ppv->Settings.RayTracingGIType = ERayTracingGlobalIlluminationType::FinalGather;
			break;
		case 2:
			ppv->Settings.RayTracingGIType = ERayTracingGlobalIlluminationType::BruteForce;
			break;
		default:
			ppv->Settings.RayTracingGIType = ERayTracingGlobalIlluminationType::Disabled;
			break;
		}
	}
	else {
		ppv->Settings.RayTracingGIType = ERayTracingGlobalIlluminationType::Disabled;
	}
	
		
	ppv->Settings.RayTracingGIMaxBounces = RayTracingGIMaxBounces[QualitySetting];
	ppv->Settings.RayTracingGISamplesPerPixel = RayTracingGISamplesPerPixel[QualitySetting];

	/* Reflections - looks great, heavy fps impact*/
	ppv->Settings.bOverride_RayTracingReflectionsShadows = true;
	switch (RayTracingReflectionsShadows[QualitySetting])
	{
	case 1:
		ppv->Settings.RayTracingReflectionsShadows = EReflectedAndRefractedRayTracedShadows::Hard_shadows;
		break;
	case 2:
		ppv->Settings.RayTracingReflectionsShadows = EReflectedAndRefractedRayTracedShadows::Area_shadows;
		break;
	default:
		ppv->Settings.RayTracingReflectionsShadows = EReflectedAndRefractedRayTracedShadows::Disabled;
		break;
	}

	ppv->Settings.bOverride_ReflectionsType = true;
	if (bRayTracingEnabled && bRayTracingReflectionsEnabled[QualitySetting])
		ppv->Settings.ReflectionsType = EReflectionsType::RayTracing;
	else
		ppv->Settings.ReflectionsType = EReflectionsType::ScreenSpace;

	ppv->Settings.bOverride_RayTracingReflectionsMaxBounces = true;
	ppv->Settings.bOverride_RayTracingReflectionsMaxRoughness = true;
	ppv->Settings.bOverride_RayTracingReflectionsSamplesPerPixel = true;
	ppv->Settings.bOverride_RayTracingReflectionsTranslucency = true;

	ppv->Settings.RayTracingReflectionsMaxBounces = RayTracingReflectionsMaxBounces[QualitySetting];
	ppv->Settings.RayTracingReflectionsMaxRoughness = RayTracingReflectionsMaxRoughness[QualitySetting];
	ppv->Settings.RayTracingReflectionsSamplesPerPixel = RayTracingReflectionsSamplesPerPixel[QualitySetting];
	ppv->Settings.RayTracingReflectionsTranslucency = RayTracingReflectionsTranslucency[QualitySetting];

	/*Translucency*/
	ppv->Settings.bOverride_TranslucencyType = true;
	ppv->Settings.bOverride_RayTracingTranslucencyShadows = true;
	switch (RayTracingTranslucencyShadows[QualitySetting])
	{
	case 1:
		ppv->Settings.RayTracingTranslucencyShadows = EReflectedAndRefractedRayTracedShadows::Hard_shadows;
		break;
	case 2:
		ppv->Settings.RayTracingTranslucencyShadows = EReflectedAndRefractedRayTracedShadows::Area_shadows;
		break;
	default:
		ppv->Settings.RayTracingTranslucencyShadows = EReflectedAndRefractedRayTracedShadows::Disabled;
		break;
	}
	if (bRayTracingEnabled && bRayTracingTranslucencyEnabled[QualitySetting])
	{
		ppv->Settings.TranslucencyType = ETranslucencyType::RayTracing;
	}
	else
	{
		ppv->Settings.TranslucencyType = ETranslucencyType::Raster;
	}
	ppv->Settings.bOverride_RayTracingTranslucencyMaxRoughness = true;
	ppv->Settings.bOverride_RayTracingTranslucencyRefractionRays = true;
	ppv->Settings.bOverride_RayTracingTranslucencySamplesPerPixel = true;
	ppv->Settings.bOverride_RayTracingTranslucencyRefraction = true;

	ppv->Settings.RayTracingTranslucencyMaxRoughness = RayTracingTranslucencyMaxRoughness[QualitySetting];
	ppv->Settings.RayTracingTranslucencyRefractionRays = RayTracingTranslucencyRefractionRays[QualitySetting];
	ppv->Settings.RayTracingTranslucencySamplesPerPixel = RayTracingTranslucencySamplesPerPixel[QualitySetting];
	ppv->Settings.RayTracingTranslucencyRefraction = RayTracingTranslucencyRefraction[QualitySetting];

	return true;
}

