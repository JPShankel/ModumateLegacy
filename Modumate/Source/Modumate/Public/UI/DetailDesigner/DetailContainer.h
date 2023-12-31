// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"

#include "Components/VerticalBox.h"
#include "UI/EditModelPlayerHUD.h"

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
	class UModumateCheckBox* IsTypicalCheckbox;

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
	TSubclassOf<class UDetailDesignerLayerData> ParticipantRiggedDataClass;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TSubclassOf<UUserWidget> ParticipantSeparatorLineClass;

	UFUNCTION()
	void ClearEditor();

	UFUNCTION()
	bool BuildEditor(const FGuid& InDetailPresetID, const TSet<int32>& InEdgeIDs);

protected:
	FGuid DetailPresetID;
	uint32 DetailConditionHash;
	TSet<int32> EdgeIDs;
	int32 OrientationIdx = INDEX_NONE;

	template <typename UserWidgetT = UUserWidget>
	UserWidgetT* GetOrCreateParticipantWidget(int32 EntryIdx, AEditModelPlayerHUD* PlayerHUD, TSubclassOf<UserWidgetT> WidgetClass)
	{
		UserWidgetT* participantWidget = Cast<UserWidgetT>(ParticipantsList->GetChildAt(EntryIdx));
		if ((participantWidget == nullptr) || !participantWidget->IsA(WidgetClass))
		{
			ClearParticipantEntries(EntryIdx);
			participantWidget = PlayerHUD->GetOrCreateWidgetInstance<UserWidgetT>(WidgetClass);
			ParticipantsList->AddChildToVerticalBox(participantWidget);
		}
		return participantWidget;
	}

	void ClearParticipantEntries(int32 StartIndex = 0);

	UFUNCTION()
	void OnPressedCancel();

	UFUNCTION()
	void OnPressedIsTypical(bool bIsChecked);

	UFUNCTION()
	void OnPresetNameEdited(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void OnLayerExtensionChanged(int32 ParticipantIndex, int32 LayerIndex, FVector2D NewExtensions);
};
