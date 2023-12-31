// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

// From Layers and Line Types at https://docs.google.com/spreadsheets/d/1re5Qm-58Tm5WEHsnbmAxFiCux8i4mxa3-xqHB5yz19M/#gid=642729209 .
// The layer table in draftingServer/Dwg/JsonToDwg.cpp (Layers[]) must be in corresponding order.
// Also preview color table dwgColors in DraftingHUDDraw.cpp.
enum class FModumateLayerType
{
	kDefault,
	kSeparatorCutStructuralLayer,
	kSeparatorCutOuterSurface,
	kSeparatorCutMinorLayer,
	kSeparatorBeyondSurfaceEdges,
	kSeparatorBeyondModuleEdges,
	kOpeningSystemCutLine,
	kOpeningSystemBeyond,
	kOpeningSystemBehind,
	kOpeningSystemOperatorLine,
	kSeparatorCutTrim,
	kCabinetCutCarcass,
	kCabinetCutAttachment,
	kCabinetBeyond,
	kCabinetBehind,
	kCabinetBeyondBlockedByCountertop,
	kCountertopCut,
	kCountertopBeyond,
	kFfeOutline,
	kFfeInteriorEdges,
	kBeamColumnCut,
	kBeamColumnBeyond,
	kMullionCut,
	kMullionBeyond,
	kSystemPanelCut,
	kSystemPanelBeyond,
	kFinishCut,
	kFinishBeyond,
	kDebug1,
	kDebug2,
	kSeparatorCutEndCaps,
	kDimensionMassing,
	kDimensionFraming,
	kDimensionOpening,
	KDimensionReference,
	kTerrainCut,
	kTerrainBeyond,
	kPartPointCut,
	kPartEdgeCut,
	kPartFaceCut,
	kPartPointBeyond,
	kPartEdgeBeyond,
	kPartFaceBeyond,
	kFinalLayerType = kPartFaceBeyond
};
