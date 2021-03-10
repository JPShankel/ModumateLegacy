// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"

#include "DetailContainer.generated.h"


UCLASS(BlueprintType)
class MODUMATE_API UDetailDesignerContainer : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlock* MainTitle;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget* SubTitle;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonCancel;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UDetailDesignerPresetName* PresetName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UVerticalBox* ParticipantsList;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TSubclassOf<class UDetailDesignerAssemblyTitle> ParticipantTitleClass;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TSubclassOf<UUserWidget> ParticipantColumnTitlesClass;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TSubclassOf<class UDetailDesignerLayerData> ParticipantLayerDataClass;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TSubclassOf<UUserWidget> ParticipantSeparatorLineClass;

	UFUNCTION()
	void ClearEditor();

	UFUNCTION()
	void BuildEditor(const FGuid& InDetailPresetID, const TSet<int32>& InEdgeIDs);

protected:
	FGuid DetailPresetID;
	TSet<int32> EdgeIDs;
	int32 OrientationIdx = INDEX_NONE;

	UFUNCTION()
	void OnPressedCancel();

	UFUNCTION()
	void OnPresetNameEdited(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void OnLayerExtensionChanged(int32 ParticipantIndex, int32 LayerIndex, FVector2D NewExtensions);
};
