// Copyright 2022 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Engine/PostProcessVolume.h"

#include "ModumateMacSettings.generated.h"


UCLASS(config=Engine)
class MODUMATE_API UModumateMacSettings : public UObject
{
	GENERATED_UCLASS_BODY()
public:
	//Mac Values
	UPROPERTY(Config)
	float MacGlobalGamma = 0.85f;
	UPROPERTY(Config)
	float MacShadowsGamma = 1.1f;
	UPROPERTY(Config)
	float MacMidtonesGamma = 0.8f;
	UPROPERTY(Config)
	float MacHighlightsGamma= 0.75f;
	UPROPERTY(Config)
	float MacShadowsGain = 0.9f;
	UPROPERTY(Config)
	float MacShoulder = 0.45f;

	//Non Mac values
	UPROPERTY(Config)
	float GlobalGamma = 1.0f;
	UPROPERTY(Config)
	float ShadowsGamma = 1.0f;
	UPROPERTY(Config)
	float MidtonesGamma = 1.0f;
	UPROPERTY(Config)
	float HighlightsGamma = 1.0f;
	UPROPERTY(Config)
	float ShadowsGain = 1.0f;
	UPROPERTY(Config)
	float Shoulder = 0.26f;

	void SetMacSettingsEnabled(APostProcessVolume* ppv, bool bIsMacEnabled);
};
