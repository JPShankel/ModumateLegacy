// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"

#include "DetailLayerData.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnExtensionChanged, int32, ParticipantIndex, int32, LayerIndex, FVector2D, NewExtensions);

UCLASS(BlueprintType)
class MODUMATE_API UDetailDesignerLayerData : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget* LayerName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget* LayerThickness;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateEditableTextBoxUserWidget* ExtensionFront;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateEditableTextBoxUserWidget* ExtensionBack;

	UPROPERTY()
	FOnExtensionChanged OnExtensionChanged;

	UFUNCTION()
	void PopulateLayerData(int32 InParticipantIndex, int32 InLayerIndex, const FVector2D& InExtensions);

protected:
	UPROPERTY()
	int32 DetailParticipantIndex = INDEX_NONE;

	UPROPERTY()
	int32 DetailLayerIndex = INDEX_NONE;

	UPROPERTY()
	FVector2D CurrentExtensions = FVector2D::ZeroVector;

	UFUNCTION()
	bool OnExtensionTextCommitted(int32 ExtensionIdx, const FString& String);

	UFUNCTION()
	void OnExtensionFrontTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void OnExtensionBackTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);
};
