// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BIMKernel/Presets/BIMPresetEditorNode.h"
#include "PresetCardHeader.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UPresetCardHeader : public UUserWidget
{
	GENERATED_BODY()

public:
	UPresetCardHeader(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:

	virtual void NativeConstruct() override;

	UPROPERTY()
	class AEditModelPlayerController* EMPlayerController;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UImage* IconImageSmall;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget* MainText;

	UPROPERTY()
	class UMaterialInterface* IconMaterial;

	// Builder
	void BuildAsBrowserHeader(const FGuid& InGUID, const FBIMEditorNodeIDType& NodeID);

	bool CaptureIcon(const FGuid& InGUID, const FBIMEditorNodeIDType& NodeID, bool bAsAssembly);
};
