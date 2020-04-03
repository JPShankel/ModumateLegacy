#include "DraftingSchedule.h"

#include "ModumateDraftingElements.h"
#include "ModumateUnits.h"
#include "Modumate.h"

#define LOCTEXT_NAMESPACE "ModumateSchedule"

using namespace Modumate::Units;
using namespace Modumate::PDF;

namespace Modumate {

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
			Units::FAngle::Degrees(0.0f),
			Units::FWidth::FloorplanInches(DefaultColumnWidth)));
	}
}

#undef LOCTEXT_NAMESPACE
