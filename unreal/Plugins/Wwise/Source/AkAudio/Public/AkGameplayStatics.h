#pragma once

#include "CoreMinimal.h"
#include "AkGameplayStatics.generated.h"

UENUM(BlueprintType)
enum class EAkCallbackType : uint8
{
    EndOfEvent = 0
};

UCLASS(BlueprintType)
class AKAUDIO_API UAkAudioEvent : public UObject
{
    GENERATED_BODY()
};

UCLASS(BlueprintType)
class AKAUDIO_API UAkCallbackInfo : public UObject
{
    GENERATED_BODY()
};

DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnAkPostEventCallback, EAkCallbackType, CallbackType, UAkCallbackInfo*, CallbackInfo);
