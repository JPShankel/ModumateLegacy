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
	float MacMidtoneSaturation = 1.2f;
	UPROPERTY(Config)
	float MacHighlightsSaturation = 1.25f;
	UPROPERTY(Config)
	float MacShadowsGain = 0.9f;
	UPROPERTY(Config)
	float MacShoulder = 0.45f;

	//Non Mac values
	UPROPERTY(Config)
	float MidtoneSaturation = 1.0f;
	UPROPERTY(Config)
	float HighlightsSaturation = 1.0f;
	UPROPERTY(Config)
	float ShadowsGain = 1.0f;
	UPROPERTY(Config)
	float Shoulder = 0.26f;

	void SetMacSettingsEnabled(APostProcessVolume* ppv, bool bIsMacEnabled);
};
