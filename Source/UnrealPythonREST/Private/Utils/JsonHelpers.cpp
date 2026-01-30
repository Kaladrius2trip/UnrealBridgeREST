// JsonHelpers.cpp
#include "Utils/JsonHelpers.h"
#include "Dom/JsonValue.h"

namespace JsonHelpers
{

TSharedPtr<FJsonObject> VectorToJson(const FVector& Vector)
{
    TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetNumberField(TEXT("x"), Vector.X);
    Json->SetNumberField(TEXT("y"), Vector.Y);
    Json->SetNumberField(TEXT("z"), Vector.Z);
    return Json;
}

TSharedPtr<FJsonObject> RotatorToJson(const FRotator& Rotator)
{
    TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetNumberField(TEXT("pitch"), Rotator.Pitch);
    Json->SetNumberField(TEXT("yaw"), Rotator.Yaw);
    Json->SetNumberField(TEXT("roll"), Rotator.Roll);
    return Json;
}

TSharedPtr<FJsonObject> TransformToJson(const FTransform& Transform)
{
    TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetObjectField(TEXT("location"), VectorToJson(Transform.GetLocation()));
    Json->SetObjectField(TEXT("rotation"), RotatorToJson(Transform.Rotator()));
    Json->SetObjectField(TEXT("scale"), VectorToJson(Transform.GetScale3D()));
    return Json;
}

bool JsonToVector(const TSharedPtr<FJsonObject>& Json, FVector& OutVector)
{
    if (!Json.IsValid()) return false;

    OutVector.X = Json->GetNumberField(TEXT("x"));
    OutVector.Y = Json->GetNumberField(TEXT("y"));
    OutVector.Z = Json->GetNumberField(TEXT("z"));
    return true;
}

bool JsonToRotator(const TSharedPtr<FJsonObject>& Json, FRotator& OutRotator)
{
    if (!Json.IsValid()) return false;

    OutRotator.Pitch = Json->GetNumberField(TEXT("pitch"));
    OutRotator.Yaw = Json->GetNumberField(TEXT("yaw"));
    OutRotator.Roll = Json->GetNumberField(TEXT("roll"));
    return true;
}

bool JsonToTransform(const TSharedPtr<FJsonObject>& Json, FTransform& OutTransform)
{
    if (!Json.IsValid()) return false;

    FVector Location = FVector::ZeroVector;
    FRotator Rotation = FRotator::ZeroRotator;
    FVector Scale = FVector::OneVector;

    if (Json->HasField(TEXT("location")))
    {
        JsonToVector(Json->GetObjectField(TEXT("location")), Location);
    }
    if (Json->HasField(TEXT("rotation")))
    {
        JsonToRotator(Json->GetObjectField(TEXT("rotation")), Rotation);
    }
    if (Json->HasField(TEXT("scale")))
    {
        JsonToVector(Json->GetObjectField(TEXT("scale")), Scale);
    }

    OutTransform.SetLocation(Location);
    OutTransform.SetRotation(Rotation.Quaternion());
    OutTransform.SetScale3D(Scale);
    return true;
}

bool GetRequiredString(const TSharedPtr<FJsonObject>& Json, const FString& FieldName, FString& OutValue, FString& OutError)
{
    if (!Json.IsValid())
    {
        OutError = TEXT("Request body must be valid JSON");
        return false;
    }

    if (!Json->TryGetStringField(FieldName, OutValue) || OutValue.IsEmpty())
    {
        OutError = FString::Printf(TEXT("Missing required field: %s"), *FieldName);
        return false;
    }

    return true;
}

FString GetOptionalString(const TSharedPtr<FJsonObject>& Json, const FString& FieldName, const FString& Default)
{
    if (!Json.IsValid()) return Default;

    FString Value;
    if (Json->TryGetStringField(FieldName, Value))
    {
        return Value;
    }
    return Default;
}

int32 GetOptionalInt(const TSharedPtr<FJsonObject>& Json, const FString& FieldName, int32 Default)
{
    if (!Json.IsValid()) return Default;

    if (Json->HasField(FieldName))
    {
        return static_cast<int32>(Json->GetNumberField(FieldName));
    }
    return Default;
}

bool GetOptionalBool(const TSharedPtr<FJsonObject>& Json, const FString& FieldName, bool Default)
{
    if (!Json.IsValid()) return Default;

    if (Json->HasField(FieldName))
    {
        return Json->GetBoolField(FieldName);
    }
    return Default;
}

double GetOptionalDouble(const TSharedPtr<FJsonObject>& Json, const FString& FieldName, double Default)
{
    if (!Json.IsValid()) return Default;

    if (Json->HasField(FieldName))
    {
        return Json->GetNumberField(FieldName);
    }
    return Default;
}

TSharedPtr<FJsonObject> CreateErrorDetails(const FString& SearchedValue, const FString& Suggestion, const TArray<FString>& Similar)
{
    TSharedPtr<FJsonObject> Details = MakeShared<FJsonObject>();
    Details->SetStringField(TEXT("searched_value"), SearchedValue);
    Details->SetStringField(TEXT("suggestion"), Suggestion);

    if (Similar.Num() > 0)
    {
        TArray<TSharedPtr<FJsonValue>> SimilarArray;
        for (const FString& S : Similar)
        {
            SimilarArray.Add(MakeShared<FJsonValueString>(S));
        }
        Details->SetArrayField(TEXT("similar"), SimilarArray);
    }

    return Details;
}

// Simple Levenshtein distance implementation
static int32 LevenshteinDistance(const FString& A, const FString& B)
{
    const int32 LenA = A.Len();
    const int32 LenB = B.Len();

    if (LenA == 0) return LenB;
    if (LenB == 0) return LenA;

    TArray<int32> Prev;
    TArray<int32> Curr;
    Prev.SetNum(LenB + 1);
    Curr.SetNum(LenB + 1);

    for (int32 i = 0; i <= LenB; ++i)
    {
        Prev[i] = i;
    }

    for (int32 i = 1; i <= LenA; ++i)
    {
        Curr[0] = i;
        for (int32 j = 1; j <= LenB; ++j)
        {
            int32 Cost = (A[i - 1] == B[j - 1]) ? 0 : 1;
            Curr[j] = FMath::Min3(
                Prev[j] + 1,      // deletion
                Curr[j - 1] + 1,  // insertion
                Prev[j - 1] + Cost // substitution
            );
        }
        Swap(Prev, Curr);
    }

    return Prev[LenB];
}

TArray<FString> FindSimilarStrings(const FString& Input, const TArray<FString>& Candidates, int32 MaxResults, int32 MaxDistance)
{
    TArray<TPair<int32, FString>> Scored;

    FString InputLower = Input.ToLower();
    for (const FString& Candidate : Candidates)
    {
        FString CandidateLower = Candidate.ToLower();
        int32 Distance = LevenshteinDistance(InputLower, CandidateLower);

        if (Distance <= MaxDistance)
        {
            Scored.Add(TPair<int32, FString>(Distance, Candidate));
        }
    }

    // Sort by distance
    Scored.Sort([](const TPair<int32, FString>& A, const TPair<int32, FString>& B)
    {
        return A.Key < B.Key;
    });

    TArray<FString> Results;
    for (int32 i = 0; i < FMath::Min(MaxResults, Scored.Num()); ++i)
    {
        Results.Add(Scored[i].Value);
    }

    return Results;
}

} // namespace JsonHelpers
