// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/TutorialMenu/HelpMenu.h"

#include "UnrealClasses/EditModelPlayerController.h"
#include "UI/EditModelUserWidget.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "UI/LeftMenu/NCPNavigator.h"
#include "UI/TutorialMenu/HelpBlockTutorialSearch.h"
#include "UI/TutorialMenu/HelpBlockTutorialArticle.h"
#include "Serialization/Csv/CsvParser.h"
#include "BIMKernel/Core/BIMKey.h"
#include "HttpModule.h"
#include "Runtime/Online/HTTP/Public/Http.h"

UHelpMenu::UHelpMenu(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UHelpMenu::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}
	if (!ButtonClose)
	{
		return false;
	}

	ButtonClose->ModumateButton->OnReleased.AddDynamic(this, &UHelpMenu::OnButtonCloseRelease);

	return true;
}

void UHelpMenu::NativeConstruct()
{
	Super::NativeConstruct();
}

void UHelpMenu::BuildTutorialLibraryMenu()
{
	// Only need to request from online source once
	if (AllTutorialNodesByGUID.Num() > 0)
	{
		return;
	}

	auto httpRequest = FHttpModule::Get().CreateRequest();
	httpRequest->SetVerb("GET");
	httpRequest->SetURL(TutorialLibraryDataLink);
	httpRequest->OnProcessRequestComplete().BindUObject(this, &UHelpMenu::ReadTutorialCSV);
	httpRequest->ProcessRequest();
}

void UHelpMenu::ReadTutorialCSV(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!bWasSuccessful)
	{
		return;
	}

	// Get response as csv rows
	FString contentString = Response->GetContentAsString();
	FCsvParser csvParser(contentString);
	const FCsvParser::FRows& rows = csvParser.GetRows();

	// Get column types
	using TColumnRef = TPair<int32, ETutorialTaxonomyColumn>;
	TArray<TColumnRef> columnTypes;

	for (int32 column = 0; column < rows[0].Num(); ++column)
	{
		ETutorialTaxonomyColumn columnType;
		if (FindEnumValueByName(rows[0][column], columnType) && columnType != ETutorialTaxonomyColumn::None)
		{
			columnTypes.Add(TColumnRef(column, columnType));
		}
	}

	// Create tutorial node from each row
	for (int32 row = 1; row < rows.Num(); ++row)
	{
		FBIMTagPath tagPath;
		FTutorialTaxonomyNode newNode;

		for (auto& kvp : columnTypes)
		{
			FString cell = rows[row][kvp.Key];
			if (cell.IsEmpty())
			{
				continue;
			}
			switch (kvp.Value)
			{
			case ETutorialTaxonomyColumn::GUID:
			{
				FGuid cellGUID;
				if (FGuid::Parse(cell, cellGUID))
				{
					newNode.Guid = cellGUID;
				}
			}
			break;
			case ETutorialTaxonomyColumn::Include:
			{
				newNode.bInclude = !cell.IsEmpty();
			}
			break;
			case ETutorialTaxonomyColumn::CategoryPath:
			{
				if (ensureAlways(tagPath.FromString(cell) == EBIMResult::Success))
				{
					newNode.TagPath = tagPath;
				}
			}
			break;
			case ETutorialTaxonomyColumn::Title:
			{
				newNode.Title = cell;
			}
			break;
			case ETutorialTaxonomyColumn::Body:
			{
				newNode.Body = cell;
			}
			break;
			case ETutorialTaxonomyColumn::VideoURL:
			{
				newNode.VideoURL = cell;
			}
			break;
			}
		}

		// Save non-included nodes somewhere?
		if (newNode.bInclude && newNode.TagPath.Tags.Num() > 0)
		{
			AllTutorialTagPaths.Add(newNode.TagPath);
			AllTutorialNodesByGUID.Add(newNode.Guid, newNode);
		}
	}

	if (AllTutorialNodesByGUID.Num() > 0)
	{
		NCPNavigator->BuildNCPNavigator(EPresetCardType::TutorialCategory);
	}
}

EBIMResult UHelpMenu::GetTutorialNCPSubcategories(const FBIMTagPath& InNCP, TArray<FString>& OutSubcats) const
{
	for (auto& ncp : AllTutorialTagPaths)
	{
		if (ncp.Tags.Num() > InNCP.Tags.Num() && ncp.MatchesPartial(InNCP))
		{
			OutSubcats.AddUnique(ncp.Tags[InNCP.Tags.Num()]);
		}
	}
	return EBIMResult::Success;
}

EBIMResult UHelpMenu::GetTutorialsForNCP(const FBIMTagPath& InNCP, TArray<FGuid>& OutGUIDs, bool bExactMatch /*= false*/) const
{
	for (auto& curNode : AllTutorialNodesByGUID)
	{
		auto tutorialNode = curNode.Value;
		if (tutorialNode.TagPath.Tags.Num() >= InNCP.Tags.Num())
		{
			if (bExactMatch)
			{
				if (tutorialNode.TagPath.MatchesExact(InNCP))
				{
					OutGUIDs.Add(tutorialNode.Guid);
				}
			}
			else
			{
				if (tutorialNode.TagPath.MatchesPartial(InNCP))
				{
					OutGUIDs.Add(tutorialNode.Guid);
				}
			}
		}
	}
	return EBIMResult::Success;
}

void UHelpMenu::ResetMenu()
{
	HelpBlockTutorialArticleBP->ToggleWebBrowser(false);

	HelpBlockTutorialsSearchBP->SetVisibility(ESlateVisibility::Visible);
	NCPNavigator->SetVisibility(ESlateVisibility::Visible);
	HelpBlockTutorialArticleBP->SetVisibility(ESlateVisibility::Collapsed);
}

void UHelpMenu::ToArticleMenu(const FGuid& InGUID)
{
	HelpBlockTutorialArticleBP->BuildTutorialArticle(InGUID);

	HelpBlockTutorialsSearchBP->SetVisibility(ESlateVisibility::Collapsed);
	NCPNavigator->SetVisibility(ESlateVisibility::Collapsed);
	HelpBlockTutorialArticleBP->SetVisibility(ESlateVisibility::Visible);
}

void UHelpMenu::OnButtonCloseRelease()
{
	AEditModelPlayerController* controller = GetOwningPlayer<AEditModelPlayerController>();
	if (controller)
	{
		controller->EditModelUserWidget->ToggleHelpMenu(false);
	}
}
