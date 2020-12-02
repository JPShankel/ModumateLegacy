// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

namespace Modumate
{

	// From Layers and Line Types at https://docs.google.com/spreadsheets/d/1re5Qm-58Tm5WEHsnbmAxFiCux8i4mxa3-xqHB5yz19M/#gid=642729209 .
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
		kFinishBeyond
	};

}
