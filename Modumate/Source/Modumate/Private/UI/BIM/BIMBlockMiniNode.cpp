// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/BIM/BIMBlockMiniNode.h"
#include "UI/BIM/BIMDesigner.h"
#include "UI/Custom/ModumateButton.h"
#include "UI/Custom/ModumateTextBlock.h"

UBIMBlockMiniNode::UBIMBlockMiniNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UBIMBlockMiniNode::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!MainButton)
	{
		return false;
	}

	MainButton->OnReleased.AddDynamic(this, &UBIMBlockMiniNode::OnMainButtonReleased);

	return true;
}

void UBIMBlockMiniNode::NativeConstruct()
{
	Super::NativeConstruct();
}

void UBIMBlockMiniNode::BuildMiniNode(class UBIMDesigner* OuterBIMDesigner, const FBIMEditorNodeIDType& NewParentID, int32 NumberOfNodes)
{
	ParentBIMDesigner = OuterBIMDesigner;
	ParentID = NewParentID;
	TitleText->SetText(FText::AsNumber(NumberOfNodes));
}

void UBIMBlockMiniNode::OnMainButtonReleased()
{
	ParentBIMDesigner->SetNodeAsSelected(ParentID);
}
