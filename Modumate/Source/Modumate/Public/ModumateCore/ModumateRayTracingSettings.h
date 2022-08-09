// Copyright 2022 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Engine/PostProcessVolume.h"

#include "ModumateRayTracingSettings.generated.h"


UCLASS(config=Engine)
class MODUMATE_API UModumateRayTracingSettings : public UObject
{
	GENERATED_UCLASS_BODY()
public:
	UPROPERTY(Config)
	bool bRayTracingEnabled;
	/*
		Ray Tracing Ambient Occlusion
	*/
	UPROPERTY(Config)
	TArray<bool> bRayTracingAOEnabled;
	UPROPERTY(Config)
	TArray<uint32> RTAOSamplesPerPixel; //1
	UPROPERTY(Config)
	TArray<uint32> RTAOIntensity; //1
	UPROPERTY(Config)
	TArray<uint32> RTAORadius; //200

	/*
		Ray Tracing Global Illumination
	*/
	UPROPERTY(Config)
	TArray<uint8> RTGIType; //bruteforce is high accuracy and very expensive
	UPROPERTY(Config)
	TArray<int32> RayTracingGIMaxBounces; //1
	UPROPERTY(Config)
	TArray<int32> RayTracingGISamplesPerPixel; //4

	/*
		Ray Tracing Reflections
	*/
	UPROPERTY(Config)
	TArray<bool> bRayTracingReflectionsEnabled;
	UPROPERTY(Config)
	TArray<float> RayTracingReflectionsMaxRoughness; //0.6f
	UPROPERTY(Config)
	TArray<int32> RayTracingReflectionsMaxBounces; //1
	//increase this to reduce noise at the cost of performance
	UPROPERTY(Config)
	TArray<int32> RayTracingReflectionsSamplesPerPixel; //1
	/*
	Hard shadows are cheapest other than disabling them
	Area shadows are more expensive but look better
	*/
	UPROPERTY(Config)
	TArray<uint8> RayTracingReflectionsShadows;
	UPROPERTY(Config)
	TArray<bool> RayTracingReflectionsTranslucency; //true

	/*
		Ray Tracing Translucency
	*/
	UPROPERTY(Config)
	TArray<bool> bRayTracingTranslucencyEnabled;
	UPROPERTY(Config)
	TArray<float> RayTracingTranslucencyMaxRoughness; //0.6f
	UPROPERTY(Config)
	TArray<int32> RayTracingTranslucencyRefractionRays; //3
	UPROPERTY(Config)
	TArray<int32> RayTracingTranslucencySamplesPerPixel; //1
	UPROPERTY(Config)
	TArray<uint8> RayTracingTranslucencyShadows;
	UPROPERTY(Config)
	TArray<bool> RayTracingTranslucencyRefraction; //true
	UPROPERTY(Config)
	TArray<float> RTExposure;
	UPROPERTY(Config)
	float RTCubemapIntensity = 0.3f;
	UPROPERTY(Config)
	float DefaultCubemapIntensity = 0.3f;
	UPROPERTY(Config)
	float RTToe= 0.4f;
	UPROPERTY(Config)
	float DefaultToe = 0.55f;
	UPROPERTY(Config)
	float DefaultExposure = 1.0f;

	bool ApplyRayTraceQualitySettings(APostProcessVolume* ppv, uint8 QualitySetting);
	void SetRayTracingEnabled(APostProcessVolume* ppv, bool bIsRayTracingEnabled);
	void SetExposure(APostProcessVolume* ppv, uint8 ExposureIndex);
	void Init();
	//utility function to find all actors of a class and return an array of them
	template<typename T> void FindAllActorsOfClass(UWorld* World, TArray<T*>& Out)
	{
		if (World == nullptr)
		{
			return;
		}
		for (TActorIterator<T> It(World); It; ++It)
		{
			Out.Add(*It);
		}
	}
};
