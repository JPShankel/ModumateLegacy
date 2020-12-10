// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/BIM/BIMBlockUserEnterable.h"
#include "UI/Custom/ModumateEditableTextBoxUserWidget.h"
#include "UI/Custom/ModumateEditableTextBox.h"
#include "UI/BIM/BIMDesigner.h"
#include "Components/EditableTextBox.h"

UBIMBlockUserEnterable::UBIMBlockUserEnterable(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UBIMBlockUserEnterable::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}
	if (!Text_Value)
	{
		return false;
	}
	Text_Value->ModumateEditableTextBox->OnTextCommitted.AddDynamic(this, &UBIMBlockUserEnterable::OnEditableTextBoxCommitted);

	return true;
}

void UBIMBlockUserEnterable::NativeConstruct()
{
	Super::NativeConstruct();
}

void UBIMBlockUserEnterable::OnEditableTextBoxCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	// If text is same as before, do not update BIM Designer
	if (Text.ToString() == OriginalValueString)
	{
		return;
	}

	if (CommitMethod == ETextCommit::OnEnter || CommitMethod == ETextCommit::OnUserMovedFocus)
	{
		ParentBIMDesigner->SetNodeProperty(NodeID, Scope, NameTpye, Text_Value->ModumateEditableTextBox->GetText().ToString());
	}
	else
	{
		Text_Value->ChangeText(FText::FromString(OriginalValueString));
	}
}

void UBIMBlockUserEnterable::BuildEnterableFieldFromProperty(class UBIMDesigner *OuterBIMDesigner, const FBIMEditorNodeIDType& InNodeID, const EBIMValueScope &InScope, const FBIMNameType &InNameType, const FString &InValue)
{
	ParentBIMDesigner = OuterBIMDesigner;
	NodeID = InNodeID;
	Scope = InScope;
	NameTpye = InNameType;
	OriginalValueString = InValue;
	Text_Value->ChangeText(FText::FromString(OriginalValueString));
}
