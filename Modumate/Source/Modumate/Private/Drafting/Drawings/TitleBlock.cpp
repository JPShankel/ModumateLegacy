#include "Drafting/Drawings/TitleBlock.h"

#include "Drafting/ModumateDraftingDraw.h"
#include "Drafting/ModumateDraftingTags.h"
#include "Drafting/Schedules/ScheduleGrid.h"

#define LOCTEXT_NAMESPACE "ModumateDraftingTitleBlock"

namespace Modumate
{
	FTitleBlock::FTitleBlock(const UModumateDocument *doc, UWorld *world, SceneCaptureID captureObjID) :
		FDraftingDrawing(doc, world, captureObjID)
	{
		DrawingScale = 1.0f;
	}

	EDrawError FTitleBlock::InitializeWithData(IModumateDraftingDraw *drawingInterface, const UModumateDocument *Document)
	{
		ContentHeight = ContentMargin.Y/2.0f;

		// Sheet Index
		FText sheetIndexFormat = LOCTEXT("sheetInfo_index", "Sheet {0} of {1}");
		TSharedPtr<FDraftingText> sheetIndexText = MakeShareable(new FDraftingText(FText::Format(sheetIndexFormat, FText::AsNumber(SheetIndex), FText::AsNumber(TotalPages)),HeaderFontSize,HeaderColor,HeaderType));
		sheetIndexText->SetLocalPosition(Dimensions.X - ContentMargin.X, ContentHeight);
		sheetIndexText->HorizontalAlignment = DraftingAlignment::Right;
		sheetIndexText->InitializeBounds(drawingInterface);
		Children.Add(sheetIndexText);

		ContentHeight += sheetIndexText->Dimensions.Y + ContentMargin.Y;

		// Sheet number
		FText sheetNumberHeader = LOCTEXT("sheetInfo_number", "Sheet Number:");
		TSharedPtr<FDraftingText> sheetNumberText = MakeShareable(new FDraftingText(SheetNumber,SheetNumberFontSize,ContentColor,ContentType));
		MakeContent(drawingInterface, sheetNumberHeader, sheetNumberText);

		// Sheet name
		FText sheetNameHeader = LOCTEXT("sheetInfo_name", "Sheet Name:");
		TSharedPtr<FDraftingText> sheetNameText = MakeShareable(new FDraftingText(SheetName,SheetNameFontSize,ContentColor,ContentType));
		MakeContent(drawingInterface, sheetNameHeader, sheetNameText);

		// Sheet Date
		FText sheetDateHeader = LOCTEXT("sheetInfo_date", "Sheet Printed:");
		TSharedPtr<FDraftingText> sheetDateText = MakeShareable(new FDraftingText(SheetDate,SheetDateFontSize,ContentColor,ContentType));
		MakeContent(drawingInterface, sheetDateHeader, sheetDateText);

		MakeSeparatorLine();


		// Project info
		// Description
		FText projectDescriptionHeader = LOCTEXT("projectInfo_description", "Project Description:");
		TSharedPtr<FDraftingText> projectDescriptionText = MakeShareable(new FDraftingText(FText::FromString(Document->ProjectInfo.description), HeaderFontSize, ContentColor));
		projectDescriptionText->Child->ContainerWidth = Dimensions.X - ContentMargin.X * 2.0f;
		MakeContent(drawingInterface, projectDescriptionHeader, projectDescriptionText);

		// ID
		FText projectIDHeader = LOCTEXT("projectInfo_id", "Author's Project ID:");
		TSharedPtr<FDraftingText> projectIDText = MakeShareable(new FDraftingText(FText::FromString(Document->ProjectInfo.ID), HeaderFontSize, ContentColor, ContentType));
		MakeContent(drawingInterface, projectIDHeader, projectIDText);

		// Parcel
		FText projectParcelHeader = LOCTEXT("projectInfo_parcel", "Project Lot/Parcel#(s):");
		TSharedPtr<FDraftingText> projectParcelText = MakeShareable(new FDraftingText(FText::FromString(Document->ProjectInfo.lotNumber), HeaderFontSize, ContentColor, ContentType));
		MakeContent(drawingInterface, projectParcelHeader, projectParcelText);

		// Address
		// Special case, address is split into two fields, the second of which doesn't have header text
		TSharedPtr<FDraftingText> projectAddress2Text = MakeShareable(new FDraftingText(FText::FromString(Document->ProjectInfo.address2), HeaderFontSize, ContentColor, ContentType));
		projectAddress2Text->HorizontalAlignment = DraftingAlignment::Right;
		projectAddress2Text->SetLocalPosition(Dimensions.X - ContentMargin.X, ContentHeight);
		projectAddress2Text->InitializeBounds(drawingInterface);
		Children.Add(projectAddress2Text);
		ContentHeight += projectAddress2Text->Dimensions.Y + ModumateUnitParams::FYCoord::FloorplanInches(projectAddress2Text->RowMargin);

		FText projectAddressHeader = LOCTEXT("projectInfo_address", "Project Address:");
		TSharedPtr<FDraftingText> projectAddress1Text = MakeShareable(new FDraftingText(FText::FromString(Document->ProjectInfo.address1), HeaderFontSize, ContentColor, ContentType));
		MakeContent(drawingInterface, projectAddressHeader, projectAddress1Text);

		// Name
		FText projectNameHeader = LOCTEXT("projectInfo_name", "Project Name:");
		TSharedPtr<FDraftingText> projectNameText = MakeShareable(new FDraftingText(FText::FromString(Document->ProjectInfo.name), HeaderFontSize, ContentColor, ContentType));
		MakeContent(drawingInterface, projectNameHeader, projectNameText);

		MakeSeparatorLine();


		// Stamp region
		// TODO: eventually the ordering of the different regions of the title block (sheet info, project info, stamp, verson) will need
		// to be able to respond to different ordering requirements, in particular the stamp region will sometimes need to be at the top
		FText stampHeader = LOCTEXT("stamp", "Stamp:");
		TSharedPtr<FDraftingComposite> stampWhitespace = MakeShareable(new FDraftingComposite());
		stampWhitespace->Dimensions = FModumateUnitCoord2D(Dimensions.X, StampHeight);
		MakeContent(drawingInterface, stampHeader, stampWhitespace);

		MakeSeparatorLine();


		// Version grid
		// TODO: fit to width - currently, schedule grid width is always determined by the width of the columns
		// what is needed here is to set the total width of the schedule now, and have columns use text wrapping or whitespace
		// to fill the width precisely
		TSharedPtr<FScheduleGrid> versions = MakeShareable(new FScheduleGrid());
		//versions.Dimensions.X = Dimensions.X; // Fit to width

		TArray<FText> ColumnHeaders = {
			LOCTEXT("verion_date", "Date"),
			LOCTEXT("verion_id", "ID #"),
			LOCTEXT("verion_name", "Version Name")
		};
		versions->DefaultColumnWidth = 1.0f;
		versions->InitializeColumns(ColumnHeaders);
		for (auto& revision : Document->Revisions)
		{
			TArray<TSharedPtr<FDraftingComposite>> row;

			// Date
			row.Add(versions->MakeDraftingText(FText::AsDate(revision.DateTime, EDateTimeStyle::Medium)));

			// Number
			row.Add(versions->MakeDraftingText(FText::AsNumber(revision.Number)));

			// Name
			row.Add(versions->MakeDraftingText(FText::FromString(revision.Name)));

			versions->MakeRow(row);
		}
		versions->InitializeBounds(drawingInterface);
		versions->MoveYTo(ContentHeight + versions->Dimensions.Y);
		Children.Add(versions);

		ContentHeight += versions->Dimensions.Y + ContentMargin.Y;

		// TODO: this section has the chance to extend the size of the title block to be larger than the page
		// extra individuals may be cut off the page and that may need to be handled gracefully
		TArray<UModumateDocument::FPartyProfile> allParties;
		allParties.Add(Document->LeadArchitect);
		allParties.Add(Document->Client);
		allParties.Append(Document->SecondaryParties);
		// Party Contact Information Blocks

		MakeSeparatorLine();
		for (int32 i = allParties.Num() - 1; i >= 0; i--)
		{
			MakePersonalInformation(drawingInterface, allParties[i]);
			MakeSeparatorLine();
		}

		return EDrawError::ErrorNone;
	}

	EDrawError FTitleBlock::MakeContent(IModumateDraftingDraw *drawingInterface, FText headerText, TSharedPtr<FDraftingComposite> content)
	{
		content->InitializeBounds(drawingInterface);

		TSharedPtr<FHeaderTag> header = MakeShareable(new FHeaderTag(headerText, content));
		header->SetLocalPosition(Dimensions.X - ContentMargin.X, ContentHeight);

		header->InitializeBounds(drawingInterface);
		Children.Add(header);
		ContentHeight += header->Dimensions.Y + ContentMargin.Y;

		return EDrawError::ErrorNone;
	}

	EDrawError FTitleBlock::MakeSeparatorLine()
	{
		TSharedPtr<FDraftingLine> separatorLine = MakeShareable(new FDraftingLine(ModumateUnitParams::FLength(Dimensions.X), LineThickness, LineColor));
		separatorLine->MoveYTo(ContentHeight);
		ContentHeight += ContentMargin.Y;
		Children.Add(separatorLine);

		return EDrawError::ErrorNone;
	}

	EDrawError FTitleBlock::MakePersonalInformation(IModumateDraftingDraw *drawingInterface, UModumateDocument::FPartyProfile profile)
	{
		TSharedPtr<FDraftingComposite> personalInfo = MakeShareable(new FDraftingComposite());
		personalInfo->Children.Add(MakeShareable(new FDraftingText(FText::FromString(profile.Role), SmallFontSize, HeaderColor, HeaderType)));
		personalInfo->Children.Add(MakeShareable(new FDraftingText(FText::FromString(profile.Name), SmallFontSize, HeaderColor, HeaderType)));
		personalInfo->Children.Add(MakeShareable(new FDraftingText(FText::FromString(profile.Representative), HeaderFontSize, ContentColor, ContentType)));
		personalInfo->Children.Add(MakeShareable(new FDraftingText(FText::FromString(profile.Email), HeaderFontSize, ContentColor)));
		personalInfo->Children.Add(MakeShareable(new FDraftingText(FText::FromString(profile.Phone), HeaderFontSize, ContentColor)));

		ModumateUnitParams::FYCoord textOffset = ModumateUnitParams::FYCoord::FloorplanInches(0.0f);
		for (int i = personalInfo->Children.Num() - 1; i >= 0; i--)
		{
			auto child = personalInfo->Children[i];
			child->HorizontalAlignment = DraftingAlignment::Right;
			child->InitializeBounds(drawingInterface);

			child->MoveYTo(textOffset);
			textOffset += child->Dimensions.Y + ModumateUnitParams::FYCoord::FloorplanInches(4.0f / 64.0f);
		}

		personalInfo->SetLocalPosition(Dimensions.X - ContentMargin.X, ContentHeight);
		ContentHeight += textOffset;
		Children.Add(personalInfo);

		return EDrawError::ErrorNone;
	}
}

#undef LOCTEXT_NAMESPACE
