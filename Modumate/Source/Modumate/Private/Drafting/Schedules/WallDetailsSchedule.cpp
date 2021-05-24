#include "Drafting/Schedules/WallDetailsSchedule.h"

#include "ModumateCore/ModumateDimensionStatics.h"
#include "DocumentManagement/ModumateDocument.h"
#include "Drafting/ModumateDraftingElements.h"
#include "Drafting/ModumateDraftingTags.h"
#include "BIMKernel/AssemblySpec/BIMAssemblySpec.h"
#include "Drafting/Schedules/ScheduleGrid.h"
#include "UnrealClasses/ThumbnailCacheManager.h"

#define LOCTEXT_NAMESPACE "ModumateWallDetailsSchedule"

FWallDetailsSchedule::FWallDetailsSchedule(const UModumateDocument *doc, UWorld *World, IModumateDraftingDraw *drawingInterface)
{
	Title = MakeShareable(new FDraftingText(
		LOCTEXT("title", "Wall Schedule: Assembly Details"),
		ModumateUnitParams::FFontSize::FloorplanInches(TitleHeight),
		DefaultColor,
		FontType::Bold));

	Children.Add(Title);

	ColumnHeaders = {
		LOCTEXT("id", "ID"),
		LOCTEXT("function", "Function"),
		LOCTEXT("category", "Category"),
		LOCTEXT("thickness", "Thickness"),
		LOCTEXT("pattern", "Pattern"),
		LOCTEXT("modules", "Module(s)"),
		LOCTEXT("gap", "Gap"),
		LOCTEXT("comments", "Comments")
	};

	/*
	SectionHeaders = {
		LOCTEXT("walltypeschedule_interior", "Interior"),
		LOCTEXT("walltypeschedule_exterior", "Exterior")
	};
	//*/


	TArray<FBIMAssemblySpec> assemblies;
	doc->GetPresetCollection().GetProjectAssembliesForObjectType(EObjectType::OTWallSegment,assemblies);
	ensureAlways(assemblies.Num() > 0);

	static const FText InchesFormat = FText::FromString(TEXT("{0}\""));

	for (int32 i = 0; i < assemblies.Num(); i++)
	{
		FBIMAssemblySpec assembly = assemblies[i];

		// Icon thumbnail
		FName key = UThumbnailCacheManager::GetThumbnailKeyForAssembly(assembly);
		FString path = UThumbnailCacheManager::GetThumbnailCachePathForKey(key);
		FModumateUnitCoord2D imageSize = FModumateUnitCoord2D(ModumateUnitParams::FXCoord::FloorplanInches(1.0f), ModumateUnitParams::FYCoord::FloorplanInches(1.0f));
		iconData.Add(MakeShareable(new FImagePrimitive(path, imageSize)));

		TSharedPtr<FScheduleGrid> assemblyGrid = MakeShareable(new FScheduleGrid());
		assemblyGrid->InitializeColumns(ColumnHeaders, false);

		TArray<TSharedPtr<FDraftingComposite>> topRow;

		// Materials
		TSharedPtr<FMaterialTagSequence> materialLayers = MakeShareable(new FMaterialTagSequence(assembly));
		materialLayers->InitializeBounds(drawingInterface);
		TSharedPtr<FFilledRoundedRectTag> layerTag = MakeShareable(new FFilledRoundedRectTag(materialLayers));
		layerTag->InitializeBounds(drawingInterface);
		topRow.Add(layerTag);

		assemblyGrid->RowHeight = layerTag->Dimensions.Y.AsFloorplanInches() + 2.0f * Margin;

		// Function contains custom name
		topRow.Add(MakeDraftingText(FText::FromString(assembly.DisplayName)));

		// Category columns is blank
		// column widths are implicitly set by subsequent rows
		topRow.Add(MakeShareable(new FDraftingComposite()));

		FText thickness = FText::FromString(UModumateDimensionStatics::DecimalToFractionString_DEPRECATED(materialLayers->TotalThickness));
		topRow.Add(MakeDraftingText(FText::Format(InchesFormat, thickness)));

		// Pattern, Module(s), Gap, and Comments columns are blank
		topRow.Add(MakeShareable(new FDraftingComposite()));
		topRow.Add(MakeShareable(new FDraftingComposite()));
		topRow.Add(MakeShareable(new FDraftingComposite()));

		topRow.Add(MakeDraftingText(FText::FromString(assembly.DisplayName)));

		assemblyGrid->MakeRow(topRow);

		// headers were not set through the InitializeColumns
		TArray<TSharedPtr<FDraftingComposite>> headerRow;
		for (auto header : ColumnHeaders)
		{
			//headerRow.Add(detailsSchedule->MakeDraftingText(header));
			headerRow.Add(MakeShareable(new FDraftingText(
				header,
				DefaultFontSize,
				DefaultColor,
				FontType::Bold,
				DefaultAlignment)));
		}
		assemblyGrid->MakeRow(headerRow);

		// rows are associated with each wall layer
		//*
		for (int32 j = 0; j < assembly.Layers.Num(); j++)
		{
			TArray<TSharedPtr<FDraftingComposite>> row;
			auto layer = assembly.Layers[j];

			// ID
			TSharedPtr<FMaterialTag> materialTag = MakeShareable(new FMaterialTag());
			materialTag->Material = FText::FromString(layer.CodeName);
			materialTag->Sequence = FText::FromString(layer.PresetSequence);

			FModumateUnitValue thicknessUnits = FModumateUnitValue::WorldCentimeters(layer.ThicknessCentimeters);
			FString imperial = UModumateDimensionStatics::DecimalToFractionString_DEPRECATED(thicknessUnits.AsWorldInches());
			auto imperialList = UModumateDimensionStatics::DecimalToFraction_DEPRECATED(thicknessUnits.AsWorldInches());
			materialTag->ThicknessFraction = imperialList;

			materialTag->InitializeBounds(drawingInterface);
			row.Add(materialTag);

			// Function
#if 0 // TODO: refactor for new subcategories
			if (layer.SubcategoryDisplayNames.Num() > 0)
			{
				row.Add(MakeDraftingText(FText::FromString(layer.SubcategoryDisplayNames[0])));
			}
			else
			{
				row.Add(MakeShareable(new FDraftingComposite()));
			}

			// Category
			if (layer.SubcategoryDisplayNames.Num() > 1)
			{
				row.Add(MakeDraftingText(FText::FromString(layer.SubcategoryDisplayNames[1])));
			}
			else
			{
				row.Add(MakeShareable(new FDraftingComposite()));
			}
#endif

			// Thickness
			row.Add(MakeDraftingText(FText::Format(InchesFormat, FText::FromString(imperial))));

#if 0 //TODO: refactor for new patterns && comments

			// Pattern
			row.Add(MakeDraftingText(layer.Pattern.DisplayName));

			// Module(s)
			FText moduleDisplayName;
			// TODO: support multiple modules
			if (layer.Modules.Num() > 0)
			{
				row.Add(MakeDraftingText(layer.Modules[0].DisplayName));
			}
			else
			{
				row.Add(MakeShareable(new FDraftingComposite()));
			}

			// Gap
			row.Add(MakeDraftingText(layer.Gap.DisplayName));

			// Comments
			TSharedPtr<FDraftingComposite> whiteSpace = MakeShareable(new FDraftingComposite());
			auto commentsText = MakeDraftingText(FText::FromString(layer.Comments));
			whiteSpace->Children.Add(commentsText);

			float commentsWidth = FMath::Max(commentsText->GetDimensions(drawingInterface).X.AsFloorplanInches(), CommentsWidth);
			whiteSpace->Dimensions.X = FXCoord::FloorplanInches(commentsWidth);
			float rowHeight = layerTag->Dimensions.Y.AsFloorplanInches() + 2.0f * Margin;
			whiteSpace->Dimensions.Y = FYCoord::FloorplanInches(rowHeight);

			row.Add(whiteSpace);
#endif
			assemblyGrid->MakeRow(row);
		}

		assemblyData.Add(assemblyGrid);
	}

	HorizontalAlignment = DraftingAlignment::Right;
	VerticalAlignment = DraftingAlignment::Bottom;
}

EDrawError FWallDetailsSchedule::InitializeBounds(IModumateDraftingDraw *drawingInterface)
{
	EDrawError error = EDrawError::ErrorNone;

	Title->InitializeBounds(drawingInterface);
	Title->MoveYTo(Title->Dimensions.Y * -1.0f);

	Dimensions = Title->Dimensions;
	Dimensions.Y += ModumateUnitParams::FYCoord::FloorplanInches(TitleMargin);

	if (assemblyData.Num() != iconData.Num())
	{
		return EDrawError::ErrorBadParam;
	}

	for (int32 i = 0; i < assemblyData.Num(); i++)
	{
		auto grid = assemblyData[i];
		auto icon = iconData[i];
		if (!grid->IsEmpty())
		{
			error = grid->InitializeBounds(drawingInterface);
			icon->MoveYTo((Dimensions.Y + icon->Dimensions.Y) * -1.0f);
			ModumateUnitParams::FXCoord iconX = icon->Dimensions.X + ModumateUnitParams::FXCoord::FloorplanInches(Margin * 2.0f);
			grid->SetLocalPosition(iconX, Dimensions.Y * -1.0f);

			Dimensions.X = grid->Dimensions.X + iconX > Dimensions.X ? grid->Dimensions.X + iconX : Dimensions.X;
			Dimensions.Y += grid->Dimensions.Y > icon->Dimensions.Y ? grid->Dimensions.Y : icon->Dimensions.Y;

			Children.Add(icon);
			Children.Add(grid);
		}

		if (i != assemblyData.Num() - 1)
		{
			Dimensions.Y += ModumateUnitParams::FYCoord::FloorplanInches(ScheduleMargin);
		}
	}

	return error;
}

#undef LOCTEXT_NAMESPACE
