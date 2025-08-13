#include "ZombieDummy.h"
#include "HealthComponent.h"
#include "Components/SkeletalMeshComponent.h"

AZombieDummy::AZombieDummy()
{
    PrimaryActorTick.bCanEverTick = false;

    HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));

    // Recomendações de colisão pro tiro pegar no Mesh
    if (USkeletalMeshComponent* MeshComp = GetMesh())
    {
        MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        MeshComp->SetCollisionResponseToAllChannels(ECR_Ignore);
        MeshComp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    }
}
