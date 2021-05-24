#include "Drafting/Schedules/FFESchedule.h"

#include "Drafting/Schedules/IconElement.h"
#include "DocumentManagement/ModumateDocument.h"
#include "Drafting/ModumateDraftingTags.h"
#include "Objects/ModumateObjectInstance.h"
#include "UnrealClasses/ThumbnailCacheManager.h"


#define LOCTEXT_NAMESPACE "ModumateFFESchedule"

FFFESchedule::FFFESchedule(const UModumateDocument *doc, IModumateDraftingDraw *drawingInterface)
{
	Title = MakeShareable(new FDraftingText(
		LOCTEXT("ffesummaryschedule_title", "FF&E Schedule: Assembly Summaries"),
		ModumateUnitParams::FFontSize::FloorplanInches(TitleHeight),
		DefaultColor,
		FontType::Bold));
	Children.Add(Title);

	// TODO: this code may be temporary with a different accessor from Document
	TMap<FString, int32> CodeNameToCount;
	TMap<FString, FBIMAssemblySpec> CodeNameToAssembly;

	auto ffes = doc->GetObjectsOfType(EObjectType::OTFurniture);
	ensureAlways(ffes.Num() > 0);

	for (auto& ffe : ffes)
	{
		FString codeName = ffe->GetAssembly().CodeName;
		if (!CodeNameToCount.Contains(codeName))
		{
			CodeNameToCount.Add(codeName, 1);
			CodeNameToAssembly.Add(codeName, ffe->GetAssembly());
		}
		else
		{
			CodeNameToCount[codeName]++;
		}
	}

	for (auto& pair : CodeNameToCount)
	{
		TSharedPtr<FIconElement> iconElement = MakeShareable(new FIconElement());

		FString codeName = pair.Key;
		auto assembly = CodeNameToAssembly[codeName];

		iconElement->DefaultColumnWidth = 1.0f;
		// id tag
		TSharedPtr<FDraftingText> tagText = iconElement->MakeDraftingText(FText::FromString(codeName));
		tagText->InitializeBounds(drawingInterface);
		TSharedPtr<FFilledRoundedRectTag> idTag = MakeShareable(new FFilledRoundedRectTag(tagText));

		iconElement->Information->Children.Add(idTag);

		// Name
		FText name = FText::FromString(assembly.DisplayName);
		iconElement->Information->Children.Add(iconElement->MakeDraftingText(name));

		// Count
		int32 count = pair.Value;

		FText countFormat = LOCTEXT("ffeschedule_count", "Count: {0}");
		FText countText = FText::Format(countFormat, FText::AsNumber(count));
		iconElement->Information->Children.Add(iconElement->MakeDraftingText(countText));

		// Icon thumbnail
		FName key = UThumbnailCacheManager::GetThumbnailKeyForAssembly(assembly);
		FString path = UThumbnailCacheManager::GetThumbnailCachePathForKey(key);
		FModumateUnitCoord2D imageSize = FModumateUnitCoord2D(ModumateUnitParams::FXCoord::FloorplanInches(1.0f), ModumateUnitParams::FYCoord::FloorplanInches(1.0f));

		iconElement->Icon->Children.Add(MakeShareable(new FImagePrimitive(path, imageSize)));
		iconElement->Icon->Dimensions = imageSize;

		ffeIconElements.Add(iconElement);
	}

	HorizontalAlignment = DraftingAlignment::Right;
	VerticalAlignment = DraftingAlignment::Bottom;
}


#undef LOCTEXT_NAMESPACE
