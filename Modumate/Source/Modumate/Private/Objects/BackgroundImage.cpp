// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "Objects/BackgroundImage.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "DocumentManagement/ModumateDocument.h"
#include "ToolsAndAdjustments/Handles/AdjustBackgroundImageExtentsHandle.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UI/ToolTray/ToolTrayBlockProperties.h"
#include "UI/Properties/InstPropWidgetScale.h"
#include "Kismet/KismetRenderingLibrary.h"

// Incoming image sized to min 20'.
const FModumateUnitValue AMOIBackgroundImage::DefaultSize(FModumateUnitValue::WorldInches(20 * 12.0f));
const FModumateUnitValue AMOIBackgroundImage::PlaneDisplacement(FModumateUnitValue::WorldCentimeters(0.5f));

const FColor AMOIBackgroundImage::UnselectedColor(230, 230, 230);
const FColor AMOIBackgroundImage::SelectedColor(141, 217, 205);


AMOIBackgroundImage::AMOIBackgroundImage()
{
	CachedPoints.SetNumZeroed(4);
	Super::BaseColor = UnselectedColor;
	Super::SelectedColor = SelectedColor;

	FWebMOIProperty prop;

	prop.name = TEXT("Scale");
	prop.type = EWebMOIPropertyType::number;
	prop.displayName = TEXT("Scale");
	prop.isEditable = true;
	prop.isVisible = true;
	WebProperties.Add(prop.name, prop);
}

void AMOIBackgroundImage::PostCreateObject(bool bNewObject)
{
	Super::PostCreateObject(bNewObject);

	auto* material = DynamicMeshActor->MasterPBRMaterial;

	MaterialData.EngineMaterial = DynamicMeshActor->MasterPBRMaterial;
}

void AMOIBackgroundImage::SetupDynamicGeometry()
{
	TryUpdateVisuals();
	UpdateImageData();
	UpdateCachedGeometryData();
	CreateActorMesh(DynamicMeshActor->Mesh);
}

void AMOIBackgroundImage::RegisterInstanceDataUI(UToolTrayBlockProperties* PropertiesUI)
{
	static const FString scalePropertyName(TEXT("Scale"));
	if (auto scaleField = PropertiesUI->RequestPropertyField<UInstPropWidgetScale>(this, scalePropertyName))
	{
		scaleField->RegisterValue(this, InstanceData.Scale);
		scaleField->ValueChangedEvent.AddDynamic(this, &AMOIBackgroundImage::OnInstPropUIChangedScale);
	}
}

void AMOIBackgroundImage::OnInstPropUIChangedScale(float NewScale)
{
	InstanceData.Scale = NewScale;
	if (Document)
	{
		auto deltaPtr = MakeShared<FMOIDelta>();
		auto& newStateData = deltaPtr->AddMutationState(this);
		auto newInstanceData = InstanceData;
		newInstanceData.Scale = NewScale;
		newStateData.CustomData.SaveStructData(newInstanceData);

		Document->ApplyDeltas({ deltaPtr }, GetWorld());
	}
}

FVector AMOIBackgroundImage::GetCorner(int32 index) const
{
	return index >= 0 && index < 4 ? CachedPoints[index] : FVector::ZeroVector;
}

int32 AMOIBackgroundImage::GetNumCorners() const
{
	return 4;
}

FVector AMOIBackgroundImage::GetNormal() const
{
	return CachedPlane;
}

FVector AMOIBackgroundImage::GetLocation() const
{
	return InstanceData.Location;
}

FQuat AMOIBackgroundImage::GetRotation() const
{
	return InstanceData.Rotation;
}

bool AMOIBackgroundImage::GetTransformedLocationState(const FTransform Transform, FMOIStateData& OutState) const
{
	OutState = GetStateData();

	FMOIBackgroundImageData moiData = InstanceData;
	moiData.Location = Transform.GetLocation();
	moiData.Rotation = Transform.GetRotation();

	return OutState.CustomData.SaveStructData(moiData);
}

void AMOIBackgroundImage::GetStructuralPointsAndLines(TArray<FStructurePoint>& outPoints, TArray<FStructureLine>& outLines, bool bForSnapping, bool bForSelection) const
{
	Super::GetStructuralPointsAndLines(outPoints, outLines, false, bForSelection);
}

void AMOIBackgroundImage::PreDestroy()
{
	CachedFilename.Empty();
}

void AMOIBackgroundImage::UpdateCachedGeometryData()
{
	FVector halfSize = FVector(InstanceData.Scale * ImageScale * ImageSize / 2.0f, 0);
	CachedPoints[0] = FVector(-halfSize.X, +halfSize.Y, 0);
	CachedPoints[1] = halfSize;
	CachedPoints[2] = FVector(+halfSize.X, -halfSize.Y, 0);
	CachedPoints[3] = -halfSize;

	for (auto& corner: CachedPoints)
	{
		corner = InstanceData.Rotation * corner + InstanceData.Location;
	}

	CachedPlane = FPlane(InstanceData.Location, InstanceData.Rotation * FVector::UpVector);
}

void AMOIBackgroundImage::UpdateImageData()
{
	if (InstanceData.SourceFilename != CachedFilename)
	{
		if (UpdateImage(InstanceData.SourceFilename))
		{
			CachedFilename = InstanceData.SourceFilename;
		}
	}
}

bool AMOIBackgroundImage::UpdateImage(const FString& ImageFile)
{
	static const FName baseColorParam(TEXT("BaseColor"));
	static const FName colorMultiplierParam(TEXT("ColorMultiplier"));
	static const FName UVScaleParam(TEXT("UVScale"));
	static const FName MrsaParam(TEXT("MRSAMultiplier"));
	static const FLinearColor MrsaValue(0.0f, 0.8f, 0.2f, 1.0f);

	UTexture2D* imageTexture = UKismetRenderingLibrary::ImportFileAsTexture2D(GetWorld(), ImageFile);
	if (!imageTexture)
	{
		return false;
	}

	FVector2D imageSize(imageTexture->GetSizeX(), imageTexture->GetSizeY());
	ImageSize = imageSize;
	ImageScale = imageSize.X < imageSize.Y ? DefaultSize.AsWorldCentimeters() / imageSize.X
		: DefaultSize.AsWorldCentimeters() / imageSize.Y;

	UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(DynamicMeshActor->Mesh->GetMaterial(0));
	if (MID == nullptr)
	{
		return false;
	}
	MID->SetTextureParameterValue(baseColorParam, imageTexture);
	MID->SetScalarParameterValue(UVScaleParam, 1.0f);
	MID->SetVectorParameterValue(MrsaParam, MrsaValue);

	return true;
}

void AMOIBackgroundImage::CreateActorMesh(UProceduralMeshComponent* Mesh)
{
	TArray<FVector> vertices;
	TArray<FVector> normals;
	TArray<int32> triangles;
	TArray<FVector2D> uv0;
	TArray<FProcMeshTangent> tangents;
	TArray<FLinearColor> vertexColors;

	// Z-fighting:
	FVector displacement = CachedPlane * PlaneDisplacement.AsWorldCentimeters();

	vertices = { CachedPoints[0] + displacement, CachedPoints[1] + displacement,
		CachedPoints[2] + displacement, CachedPoints[3] + displacement };
	normals = { CachedPlane, CachedPlane, CachedPlane, CachedPlane };
	uv0 = { {0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f } };
	tangents.Init(FProcMeshTangent((vertices[1] - vertices[0]).GetSafeNormal(), false), 4);
	// Two triangles each side:
	triangles = { 0, 1, 2,  0, 2, 3,  0, 2, 1,  0, 3, 2};

	Mesh->CreateMeshSection_LinearColor(0, vertices, triangles, normals, uv0, vertexColors, tangents, true);
}
