#include "Drafting/Schedules/DraftingSchedule.h"

#include "Drafting/ModumateDraftingElements.h"
#include "ModumateCore/ModumateUnits.h"
#include "UnrealClasses/Modumate.h"

#define LOCTEXT_NAMESPACE "ModumateSchedule"

FDraftingSchedule::~FDraftingSchedule() 
{ 
	FDraftingComposite::~FDraftingComposite(); 
}

TSharedPtr<FDraftingText> FDraftingSchedule::MakeDraftingText(FText text)
{
	return MakeShareable(new FDraftingText(
		text,
		DefaultFontSize,
		DefaultColor,
		DefaultFont,
		DefaultAlignment,
		ModumateUnitParams::FAngle::Degrees(0.0f),
		ModumateUnitParams::FWidth::FloorplanInches(DefaultColumnWidth)));
}


#undef LOCTEXT_NAMESPACE
