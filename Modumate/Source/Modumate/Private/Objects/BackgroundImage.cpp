// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "Objects/BackgroundImage.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "DocumentManagement/ModumateDocument.h"
#include "ToolsAndAdjustments/Handles/AdjustBackgroundImageExtentsHandle.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UI/ToolTray/ToolTrayBlockProperties.h"
#include "UI/Properties/InstPropWidgetScale.h"
#include "Kismet/KismetRenderingLibrary.h"

// Incoming image sized to min 20'.
const FModumateUnitValue AMOIBackgroundImage::DefaultSize(FModumateUnitValue::WorldInches(20 * 12.0f));
const FColor AMOIBackgroundImage::UnselectedColor(120, 120, 120);
const FColor AMOIBackgroundImage::SelectedColor(250, 250, 250);


AMOIBackgroundImage::AMOIBackgroundImage()
{
	CachedPoints.SetNumZeroed(4);
	Super::BaseColor = UnselectedColor;
	Super::SelectedColor = SelectedColor;
}

void AMOIBackgroundImage::PostCreateObject(bool bNewObject)
{
	Super::PostCreateObject(bNewObject);

	auto* gameMode = GetWorld()->GetAuthGameMode<AEditModelGameMode>();
	auto* material = DynamicMeshActor->MasterPBRMaterial;

	MaterialData.EngineMaterial = DynamicMeshActor->MasterPBRMaterial;
}

void AMOIBackgroundImage::SetupDynamicGeometry()
{
	UpdateVisuals();
	UpdateImageData();
	UpdateCachedGeometryData();
	CreateActorMesh(DynamicMeshActor->Mesh);
}

void AMOIBackgroundImage::SetupAdjustmentHandles(AEditModelPlayerController* controller)
{ }

void AMOIBackgroundImage::GetUpdatedVisuals(bool& bOutVisible, bool& bOutCollisionEnabled)
{
	Super::GetUpdatedVisuals(bOutVisible, bOutCollisionEnabled);
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

	vertices = CachedPoints;
	normals = { CachedPlane, CachedPlane, CachedPlane, CachedPlane };
	uv0 = { {0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f } };
	tangents.Init(FProcMeshTangent((vertices[1] - vertices[0]).GetSafeNormal(), false), 4);
	// Two triangles each side:
	triangles = { 0, 1, 2,  0, 2, 3,  0, 2, 1,  0, 3, 2};

	Mesh->CreateMeshSection_LinearColor(0, vertices, triangles, normals, uv0, vertexColors, tangents, true);
}
