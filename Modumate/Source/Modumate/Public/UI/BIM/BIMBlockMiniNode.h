// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BIMKernel/Presets/BIMPresetEditorNode.h"
#include "BIMBlockMiniNode.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UBIMBlockMiniNode : public UUserWidget
{
	GENERATED_BODY()

public:
	UBIMBlockMiniNode(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:

	virtual void NativeConstruct() override;

	UPROPERTY()
	class UBIMDesigner* ParentBIMDesigner;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButton* MainButton;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlock* TitleText;

	// Y-offset from the top for drawing connection between this node and its parent node
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EstimateSize")
	float AttachmentOffset = 16.f;

	FBIMEditorNodeIDType ParentID;

	void BuildMiniNode(class UBIMDesigner* OuterBIMDesigner, const FBIMEditorNodeIDType& NewParentID, int32 NumberOfNodes);

	UFUNCTION()
	void OnMainButtonReleased();
};
