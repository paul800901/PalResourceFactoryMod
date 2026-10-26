#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "AkGameplayStatics.h"
#include "AkComponent.generated.h"

UCLASS(Blueprintable, ClassGroup=Audio, meta=(BlueprintSpawnableComponent))
class AKAUDIO_API UAkComponent : public USceneComponent
{
    GENERATED_BODY()

public:
    UAkComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get())
        : Super(ObjectInitializer)
    {
    }

protected:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AkAudio")
    bool bUseReverbVolumes = false;
};
