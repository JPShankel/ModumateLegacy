// Fill out your copyright notice in the Description page of Project Settings.

#include "UnrealClasses/HUDDrawWidget_CPP.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "Blueprint/WidgetBlueprintLibrary.h"


UHUDDrawWidget_CPP::UHUDDrawWidget_CPP(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, UserWidgetPool(*this)
{
}

void UHUDDrawWidget_CPP::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	LinesWithText.Reset();
	LinesWithTextEditable.Reset();
	LinesToDrawOnPaint.Reset();

	for (const auto &line : LinesToDraw)
	{
		if (line.Visible)
		{
			switch (line.Functionality)
			{
			case EEnterableField::NonEditableText:
				LinesWithText.Add(line);
				break;
			case EEnterableField::EditableText_ImperialUnits_UserInput:
			case EEnterableField::EditableText_ImperialUnits_NoUserInput:
				LinesWithTextEditable.Add(line);
				break;
			case EEnterableField::None:
			default:
				break;
			}

			LinesToDrawOnPaint.Add(line);
		}
	}

	LinesToDrawOnPaint.Sort();

	Super::NativeTick(MyGeometry, InDeltaTime);
}

int32 UHUDDrawWidget_CPP::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 parentMaxLayerID = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	int32 selfPaintLayerID = LayerId;

	if (LinesToDrawOnPaint.Num() > 0)
	{
		auto *playerController = GetWorld()->GetFirstPlayerController();
		float dpiInverseScale = 1.0f / UModumateFunctionLibrary::GetViewportDPIScale();
		FPaintContext selfPaintContext(AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

		for (const auto &line : LinesToDrawOnPaint)
		{
			FVector2D startScreenPos, endScreenPos;
			bool startBehindCamera, endBehindCamera;
			FVector limitedPoint1, limitedPoint2;
			UModumateFunctionLibrary::ProjectLineToCameraPlane(playerController, line.Point1, line.Point2, limitedPoint1, limitedPoint2);

			if (line.Visible &&
				UModumateFunctionLibrary::ProjectWorldToScreenBidirectional(playerController, limitedPoint1, startScreenPos, startBehindCamera) &&
				UModumateFunctionLibrary::ProjectWorldToScreenBidirectional(playerController, limitedPoint2, endScreenPos, endBehindCamera) &&
				(!startBehindCamera || !endBehindCamera))
			{
				int32 screenX, screenY;
				playerController->GetViewportSize(screenX, screenY);
				FBox2D screenBox = FBox2D(FVector2D::ZeroVector, FVector2D(screenX, screenY));
				FVector2D clippedStart, clippedEnd;
				if (!UModumateFunctionLibrary::ClipLine2DToRectangle(startScreenPos, endScreenPos, screenBox, clippedStart, clippedEnd))
				{
					continue;
				}

				FVector2D startScreenPosScaled = clippedStart * dpiInverseScale;
				FVector2D endScreenPosScaled = clippedEnd * dpiInverseScale;

				if ((line.DashLength > 0.0f) && (line.DashSpacing > 0.0f))
				{
					float totalInterval = line.DashLength + line.DashSpacing;
					FVector2D projectedDelta = endScreenPosScaled - startScreenPosScaled;
					float projectedDist = projectedDelta.Size();
					FVector2D projectedDir = (projectedDist > 0.0f) ? (projectedDelta / projectedDist) : FVector2D::ZeroVector;
					int32 numDashes = FMath::Max(1, (totalInterval > 0.0f) ? FMath::RoundToInt(projectedDist / totalInterval) : 0);

					int32 maxNumDashes = 2 * FMath::Max(screenX, screenY);
					if (!ensureAlwaysMsgf(numDashes <= maxNumDashes, TEXT("Trying to draw too many dashes: %d"), numDashes))
					{
						numDashes = maxNumDashes;
					}

					for (int32 i = 0; i < numDashes; ++i)
					{
						float dashStartLength = (i * totalInterval);
						float dashEndLength = FMath::Min(projectedDist, dashStartLength + line.DashLength);
						FVector2D dashStart = startScreenPosScaled + dashStartLength * projectedDir;
						FVector2D dashEnd = startScreenPosScaled + dashEndLength * projectedDir;

						UWidgetBlueprintLibrary::DrawLine(selfPaintContext, dashStart, dashEnd, line.Color, line.AntiAliased, line.Thickness);
						if (i > maxNumDashes)
						{
							break;
						}
					}
				}
				else
				{
					UWidgetBlueprintLibrary::DrawLine(selfPaintContext, startScreenPosScaled, endScreenPosScaled, line.Color, line.AntiAliased, line.Thickness);
				}

				if (line.DraftAestheticType > 0)
				{
					static float extendedLineOffset = 10.0f;
					FVector2D extendedStartPos, extendedEndPos;
					UModumateFunctionLibrary::ScreenLineExtend(extendedStartPos, extendedEndPos, startScreenPosScaled, endScreenPosScaled, extendedLineOffset, extendedLineOffset);

					switch (line.DraftAestheticType)
					{
					case 1:
						UWidgetBlueprintLibrary::DrawLine(selfPaintContext, startScreenPosScaled, extendedEndPos, line.Color, line.AntiAliased, line.Thickness);
						break;
					case 2:
						UWidgetBlueprintLibrary::DrawLine(selfPaintContext, extendedStartPos, extendedEndPos, line.Color, line.AntiAliased, line.Thickness);
						break;
					default:
						break;
					}
				}
			}
		}

		selfPaintLayerID = selfPaintContext.MaxLayer;
	}

	return FMath::Max(parentMaxLayerID, selfPaintLayerID);
}

void UHUDDrawWidget_CPP::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	UserWidgetPool.ReleaseAllSlateResources();
}
