// JsonHelpers.h
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/**
 * JSON utility functions for REST handlers.
 * Provides consistent serialization of UE types to JSON.
 */
namespace JsonHelpers
{
    /** Convert FVector to JSON object {x, y, z} */
    TSharedPtr<FJsonObject> VectorToJson(const FVector& Vector);

    /** Convert FRotator to JSON object {pitch, yaw, roll} */
    TSharedPtr<FJsonObject> RotatorToJson(const FRotator& Rotator);

    /** Convert FTransform to JSON object {location, rotation, scale} */
    TSharedPtr<FJsonObject> TransformToJson(const FTransform& Transform);

    /** Parse FVector from JSON object */
    bool JsonToVector(const TSharedPtr<FJsonObject>& Json, FVector& OutVector);

    /** Parse FRotator from JSON object */
    bool JsonToRotator(const TSharedPtr<FJsonObject>& Json, FRotator& OutRotator);

    /** Parse FTransform from JSON (location, rotation, scale fields) */
    bool JsonToTransform(const TSharedPtr<FJsonObject>& Json, FTransform& OutTransform);

    /** Get required string field or return error response */
    bool GetRequiredString(const TSharedPtr<FJsonObject>& Json, const FString& FieldName, FString& OutValue, FString& OutError);

    /** Get optional string field with default */
    FString GetOptionalString(const TSharedPtr<FJsonObject>& Json, const FString& FieldName, const FString& Default = TEXT(""));

    /** Get optional int field with default */
    int32 GetOptionalInt(const TSharedPtr<FJsonObject>& Json, const FString& FieldName, int32 Default = 0);

    /** Get optional bool field with default */
    bool GetOptionalBool(const TSharedPtr<FJsonObject>& Json, const FString& FieldName, bool Default = false);

    /** Get optional double field with default */
    double GetOptionalDouble(const TSharedPtr<FJsonObject>& Json, const FString& FieldName, double Default = 0.0);

    /** Create error response with suggestions (for smart errors) */
    TSharedPtr<FJsonObject> CreateErrorDetails(
        const FString& SearchedValue,
        const FString& Suggestion,
        const TArray<FString>& Similar = TArray<FString>()
    );

    /** Find similar strings using Levenshtein distance */
    TArray<FString> FindSimilarStrings(const FString& Input, const TArray<FString>& Candidates, int32 MaxResults = 3, int32 MaxDistance = 5);
}
