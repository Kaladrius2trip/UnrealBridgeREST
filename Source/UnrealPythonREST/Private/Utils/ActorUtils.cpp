// ActorUtils.cpp
#include "Utils/ActorUtils.h"
#include "Utils/JsonHelpers.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Components/PrimitiveComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace ActorUtils
{

UWorld* GetEditorWorld()
{
	if (GEditor)
	{
		return GEditor->GetEditorWorldContext().World();
	}
	return nullptr;
}

AActor* FindActorByLabel(const FString& Label)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return nullptr;
	}

	// First pass: try to find by actor label (display name)
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor && Actor->GetActorLabel() == Label)
		{
			return Actor;
		}
	}

	// Second pass: try to find by actor name (internal unique name)
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor && Actor->GetName() == Label)
		{
			return Actor;
		}
	}

	return nullptr;
}

TArray<FString> GetAllActorLabels()
{
	TArray<FString> Labels;

	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return Labels;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor)
		{
			FString Label = Actor->GetActorLabel();
			if (!Label.IsEmpty())
			{
				Labels.Add(Label);
			}
		}
	}

	return Labels;
}

AActor* SpawnActorFromClass(const FString& ClassPath, const FTransform& Transform, FString& OutError)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		OutError = TEXT("No editor world available");
		return nullptr;
	}

	// Load the class
	UClass* ActorClass = LoadClass<AActor>(nullptr, *ClassPath);
	if (!ActorClass)
	{
		OutError = FString::Printf(TEXT("Class not found: %s"), *ClassPath);
		return nullptr;
	}

	// Spawn parameters
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	// Spawn the actor
	AActor* SpawnedActor = World->SpawnActor<AActor>(ActorClass, Transform, SpawnParams);
	if (!SpawnedActor)
	{
		OutError = FString::Printf(TEXT("Failed to spawn actor of class: %s"), *ClassPath);
		return nullptr;
	}

	return SpawnedActor;
}

bool RaycastToSurface(const FVector& StartLocation, float MaxDistance, FVector& OutHitLocation, FVector& OutHitNormal)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return false;
	}

	FVector EndLocation = StartLocation - FVector(0, 0, MaxDistance);

	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.bTraceComplex = true;

	bool bHit = World->LineTraceSingleByChannel(
		HitResult,
		StartLocation,
		EndLocation,
		ECollisionChannel::ECC_Visibility,
		QueryParams
	);

	if (bHit)
	{
		OutHitLocation = HitResult.Location;
		OutHitNormal = HitResult.Normal;
		return true;
	}

	return false;
}

TSharedPtr<FJsonObject> ActorToDetailedJson(AActor* Actor)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();

	if (!Actor)
	{
		return Json;
	}

	// Basic info
	Json->SetStringField(TEXT("label"), Actor->GetActorLabel());
	Json->SetStringField(TEXT("name"), Actor->GetName());
	Json->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
	Json->SetStringField(TEXT("path"), Actor->GetPathName());

	// Transform (all in centimeters/degrees)
	Json->SetObjectField(TEXT("location"), JsonHelpers::VectorToJson(Actor->GetActorLocation()));
	Json->SetObjectField(TEXT("rotation"), JsonHelpers::RotatorToJson(Actor->GetActorRotation()));
	Json->SetObjectField(TEXT("scale"), JsonHelpers::VectorToJson(Actor->GetActorScale3D()));

	// Bounds
	FBox Bounds = Actor->GetComponentsBoundingBox();
	if (Bounds.IsValid)
	{
		TSharedPtr<FJsonObject> BoundsJson = MakeShared<FJsonObject>();
		BoundsJson->SetObjectField(TEXT("min"), JsonHelpers::VectorToJson(Bounds.Min));
		BoundsJson->SetObjectField(TEXT("max"), JsonHelpers::VectorToJson(Bounds.Max));
		BoundsJson->SetObjectField(TEXT("center"), JsonHelpers::VectorToJson(Bounds.GetCenter()));
		BoundsJson->SetObjectField(TEXT("extent"), JsonHelpers::VectorToJson(Bounds.GetExtent()));
		Json->SetObjectField(TEXT("bounds"), BoundsJson);
	}

	// Tags
	if (Actor->Tags.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> TagsArray;
		for (const FName& Tag : Actor->Tags)
		{
			TagsArray.Add(MakeShared<FJsonValueString>(Tag.ToString()));
		}
		Json->SetArrayField(TEXT("tags"), TagsArray);
	}

	// Mobility
	USceneComponent* RootComponent = Actor->GetRootComponent();
	if (RootComponent)
	{
		FString Mobility;
		switch (RootComponent->Mobility)
		{
		case EComponentMobility::Static:
			Mobility = TEXT("static");
			break;
		case EComponentMobility::Stationary:
			Mobility = TEXT("stationary");
			break;
		case EComponentMobility::Movable:
			Mobility = TEXT("movable");
			break;
		}
		Json->SetStringField(TEXT("mobility"), Mobility);
	}

	// Visibility
	Json->SetBoolField(TEXT("hidden"), Actor->IsHidden());
	Json->SetBoolField(TEXT("editor_only"), Actor->IsEditorOnly());

	// Parent (if attached)
	AActor* Parent = Actor->GetAttachParentActor();
	if (Parent)
	{
		Json->SetStringField(TEXT("parent_label"), Parent->GetActorLabel());
	}

	return Json;
}

} // namespace ActorUtils
