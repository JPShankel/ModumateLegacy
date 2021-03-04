#include "CoreMinimal.h"

#include "Drafting/ModumateDraftingElements.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ModumateCore/ModumateUnits.h"

namespace Modumate
{
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateDraftingClipRectangle, "Modumate.Drafting.Drawing.ClipRectangle", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateDraftingClipRectangle::RunTest(const FString& Parameters)
	{
		bool ret = true;

		// Test 1 - Big numbers
		FVector2D minBorder = FVector2D(0.f, 0.f);
		FVector2D maxBorder = FVector2D(1741.f, 796.f);

		FVector2D lineStart = FVector2D(1110.f, 276.f);
		FVector2D lineEnd = FVector2D(7287565824.f, -2740469504.f);

		FVector2D returnLineStart, returnLineEnd;
		float tolerance = 0.02f;
		bool clipTest = UModumateFunctionLibrary::ClipLine2DToRectangle(
			lineStart, lineEnd,
			FBox2D(minBorder, maxBorder),
			returnLineStart, returnLineEnd, tolerance);

		FVector2D originalDir = (lineEnd - lineStart).GetSafeNormal();
		FVector2D newDir = (returnLineEnd - returnLineStart).GetSafeNormal();
		ret = originalDir.Equals(newDir);

		// Test 2 - Simple
		minBorder = FVector2D(0.f, 0.f);
		maxBorder = FVector2D(1741.f, 796.f);

		lineStart = FVector2D(1110.f, 276.f);
		lineEnd = FVector2D(174.f, 79.f);

		tolerance = 0.02f;
		clipTest = UModumateFunctionLibrary::ClipLine2DToRectangle(
			lineStart, lineEnd,
			FBox2D(minBorder, maxBorder),
			returnLineStart, returnLineEnd, tolerance);

		originalDir = (lineEnd - lineStart).GetSafeNormal();
		newDir = (returnLineEnd - returnLineStart).GetSafeNormal();
		ret = ret && originalDir.Equals(newDir);

		return ret;
	}
}