// ActorUtils.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

/**
 * Utility functions for actor operations.
 * Used by ActorsHandler for spawn, find, raycast operations.
 */
namespace ActorUtils
{
	/**
	 * Find an actor by its label in the current editor world.
	 * @param Label The actor label to search for
	 * @return The actor if found, nullptr otherwise
	 */
	AActor* FindActorByLabel(const FString& Label);

	/**
	 * Get all actor labels in the current editor world.
	 * Used for smart error suggestions.
	 * @return Array of all actor labels
	 */
	TArray<FString> GetAllActorLabels();

	/**
	 * Spawn an actor from a class path.
	 * @param ClassPath Full path to the actor class (e.g., "/Game/Blueprints/BP_Tree.BP_Tree_C")
	 * @param Transform The spawn transform
	 * @param OutError Error message if spawn fails
	 * @return The spawned actor, nullptr on failure
	 */
	AActor* SpawnActorFromClass(const FString& ClassPath, const FTransform& Transform, FString& OutError);

	/**
	 * Raycast from a point downward to find a surface.
	 * @param StartLocation Starting point for the ray
	 * @param MaxDistance Maximum raycast distance (default 100000 cm = 1km)
	 * @param OutHitLocation The hit location if found
	 * @param OutHitNormal The surface normal at hit point
	 * @return True if surface was found
	 */
	bool RaycastToSurface(const FVector& StartLocation, float MaxDistance, FVector& OutHitLocation, FVector& OutHitNormal);

	/**
	 * Get the current editor world.
	 * @return The editor world, nullptr if not available
	 */
	UWorld* GetEditorWorld();

	/**
	 * Convert actor to detailed JSON representation.
	 * @param Actor The actor to convert
	 * @return JSON object with actor details
	 */
	TSharedPtr<FJsonObject> ActorToDetailedJson(AActor* Actor);
}
