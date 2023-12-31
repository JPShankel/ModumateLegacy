#include "Drafting/Schedules/DoorSummarySchedule.h"

#include "Drafting/Schedules/IconElement.h"
#include "DocumentManagement/ModumateDocument.h"
#include "Drafting/ModumateDraftingTags.h"
#include "Objects/ModumateObjectInstance.h"
#include "UnrealClasses/ThumbnailCacheManager.h"


#define LOCTEXT_NAMESPACE "ModumateDoorSummarySchedule"

FDoorSummarySchedule::FDoorSummarySchedule(const UModumateDocument *doc, IModumateDraftingDraw *drawingInterface)
{
	Title = MakeShareable(new FDraftingText(
		LOCTEXT("title", "Door Schedule: Assembly Summaries"),
		ModumateUnitParams::FFontSize::FloorplanInches(TitleHeight),
		DefaultColor,
		FontType::Bold));
	Children.Add(Title);

	// Door Summary Schedules
	// maps database key to a pair of the count of the assembly and the assembly to properly populate the summary list
	auto doors = doc->GetObjectsOfType(EObjectType::OTDoor);
	ensureAlways(doors.Num() > 0);

	TMap<FGuid, TPair<int32, FBIMAssemblySpec>> doorsByKey;
	for (auto& door : doors)
	{
		if (!doorsByKey.Contains(door->GetAssembly().UniqueKey()))
		{
			doorsByKey.Add(door->GetAssembly().UniqueKey(), TPair < int32, FBIMAssemblySpec > (1, door->GetAssembly()));
		}
		else
		{
			doorsByKey[door->GetAssembly().UniqueKey()].Key++;
		}
	}

	for (auto& kvp : doorsByKey)
	{
		int32 count = kvp.Value.Key;
		FBIMAssemblySpec assembly = kvp.Value.Value;

		TSharedPtr<FIconElement> iconElement = MakeShareable(new FIconElement());

		// TODO: use new object properties, rather than assembly properties or meta values
		//FString idText = assembly.GetProperty(BIM::Parameters::Code).AsString() + assembly.GetProperty(BIM::Parameters::Tag);
		const static FString idText(TEXT("I AM ERROR"));
		auto tagText = MakeDraftingText(FText::FromString(idText));
		tagText->InitializeBounds(drawingInterface);
		TSharedPtr<FFilledRoundedRectTag> idTag = MakeShareable(new FFilledRoundedRectTag(tagText));

		iconElement->Information->Children.Add(idTag);

		// Name
		FText name = FText::FromString(assembly.DisplayName);
		iconElement->Information->Children.Add(MakeDraftingText(name));

		// Count
		FText countFormat = LOCTEXT("count", "Count: {0}");
		FText countText = FText::Format(countFormat, FText::AsNumber(count));
		iconElement->Information->Children.Add(MakeDraftingText(countText));

		// Comments
		FText commentFormat = LOCTEXT("comments", "Comments: {0}");
		FText commentText = FText::Format(commentFormat, FText::FromString(assembly.Comments));
		iconElement->Information->Children.Add(MakeDraftingText(commentText));

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
