// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BIMKernel/Core/BIMKey.h"

#include "ModumateSimpleMesh.generated.h"


USTRUCT(BlueprintType)
struct MODUMATE_API FSimplePolygon
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	TArray<FVector2D> Points;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	TArray<int32> Triangles;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FBox2D Extents = FBox2D(ForceInitToZero);

	void Reset();
	void UpdateExtents();
	bool ValidateSimple(class FFeedbackContext* InWarn = nullptr) const;
};

UCLASS(BlueprintType)
class MODUMATE_API USimpleMeshData : public UObject
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	TArray<FSimplePolygon> Polygons;

	bool SetSourcePath(const FString &FullFilePath);
	FString GetSourceFullPath() const;

protected:
	UPROPERTY(VisibleAnywhere)
	FString SourceRelPath;

	UPROPERTY(VisibleAnywhere)
	FString SourceAbsPath;
};

USTRUCT()
struct MODUMATE_API FSimpleMeshRef
{
	GENERATED_USTRUCT_BODY()

	FGuid Key;

	FSoftObjectPath AssetPath;
	TWeakObjectPtr<USimpleMeshData> Asset = nullptr;

	FGuid UniqueKey() const { return Key; }
};
