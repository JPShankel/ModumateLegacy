// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModumateCameraView.generated.h"

USTRUCT(BlueprintType)
struct FModumateCameraView
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CameraView")
	FVector Position = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CameraView")
	FQuat Rotation = FQuat::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CameraView")
	float FOV = 50.f; // Default Modumate camera fov

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CameraView")
	float AspectRatio = 1.778f; // Default UE4 camera aspect ratio

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CameraView")
	FString Name = FString(TEXT("NewCameraView"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CameraView")
	FDateTime TimeOfDay = FDateTime::Today();
};
