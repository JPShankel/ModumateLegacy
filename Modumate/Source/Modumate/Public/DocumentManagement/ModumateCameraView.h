// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModumateCameraView.generated.h"

USTRUCT(BlueprintType)
struct FModumateCameraView
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CameraView")
	FVector Position;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CameraView")
	FQuat Rotation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CameraView")
	float FOV;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CameraView")
	float AspectRatio;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CameraView")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CameraView")
	FDateTime TimeOfDay;
};
