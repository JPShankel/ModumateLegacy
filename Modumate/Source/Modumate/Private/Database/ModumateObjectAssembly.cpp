// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "Database/ModumateObjectAssembly.h"

#include "ModumateCore/ExpressionEvaluator.h"
#include "DocumentManagement/ModumateDocument.h"
#include "DocumentManagement/ModumateDocument.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "DocumentManagement/ModumateCommands.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "Database/ModumateObjectDatabase.h"
#include "Algo/Accumulate.h"
#include "Algo/Reverse.h"

using namespace Modumate;
using namespace Modumate::Units;

Modumate::Units::FUnitValue FModumateObjectAssembly::CalculateThickness() const
{
	return Modumate::Units::FUnitValue::WorldCentimeters(Algo::TransformAccumulate(
		Layers,
		[](const FModumateObjectAssemblyLayer &l)
		{
			return l.Thickness.AsWorldCentimeters();
		},
		0.0f,
		[](float lhs, float rhs)
		{
			return lhs + rhs;
		}
	));
}

Modumate::FModumateCommandParameter FModumateObjectAssembly::GetProperty(const FBIMNameType &name) const
{
	return Properties.GetProperty(EBIMValueScope::Assembly, name);
}

void FModumateObjectAssembly::SetProperty(const FBIMNameType &name, const Modumate::FModumateCommandParameter &val)
{
	Properties.SetProperty(EBIMValueScope::Assembly, name, val);
}

bool FModumateObjectAssembly::HasProperty(const FBIMNameType &name) const
{
	return Properties.HasProperty(EBIMValueScope::Assembly, name);
}

void FModumateObjectAssembly::ReverseLayers()
{
	Algo::Reverse(Layers);
}

