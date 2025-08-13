#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ZombieDummy.generated.h"

class UHealthComponent;

UCLASS()
class ZNODE_API AZombieDummy : public ACharacter
{
    GENERATED_BODY()
public:
    AZombieDummy();

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UHealthComponent* HealthComponent; // aparece no Details
};
