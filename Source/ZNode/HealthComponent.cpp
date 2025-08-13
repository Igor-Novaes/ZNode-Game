#include "HealthComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"

UHealthComponent::UHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// Default: considerar "head" como cabeça se a lista ficar vazia
	HeadshotBones.AddUnique(FName(TEXT("head")));
}

void UHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	CurrentHealth = MaxHealth;

	if (AActor* Owner = GetOwner())
	{
		Owner->OnTakePointDamage.AddDynamic(this, &UHealthComponent::HandlePointDamage);
		Owner->OnTakeAnyDamage.AddDynamic(this, &UHealthComponent::HandleAnyDamage);
	}

	if (bDebugDamage)
	{
		UE_LOG(LogTemp, Log, TEXT("[HealthComponent] '%s' Init: Max=%.1f, HeadshotIK=%d, Bones=%d, Radius=%.1f"),
			*GetOwner()->GetName(), MaxHealth, bHeadshotInstantKill ? 1 : 0, HeadshotBones.Num(), HeadshotProximityRadius);
	}
}

void UHealthComponent::Heal(float Amount)
{
	if (IsDead() || Amount <= 0.f) return;
	CurrentHealth = FMath::Clamp(CurrentHealth + Amount, 0.f, MaxHealth);
}

void UHealthComponent::Kill()
{
	if (IsDead()) return;
	CurrentHealth = 0.f;
	Die();
}

bool UHealthComponent::NameIsHeadLike(const FName& Bone) const
{
	// Se a lista estiver preenchida, use-a (case-insensitive)
	if (HeadshotBones.Num() > 0)
	{
		for (const FName& H : HeadshotBones)
		{
			if (Bone.IsEqual(H, ENameCase::IgnoreCase)) return true;
		}
	}

	// Heurística: nome contém "head"
	const FString S = Bone.ToString().ToLower();
	return S.Contains(TEXT("head")) || S.Equals(TEXT("head"));
}

void UHealthComponent::HandlePointDamage(AActor* DamagedActor, float Damage, AController* InstigatedBy,
	FVector HitLocation, UPrimitiveComponent* HitComponent,
	FName BoneName, FVector ShotFromDirection,
	const UDamageType* DamageType, AActor* DamageCauser)
{
	if (IsDead()) return;

	// Detecta headshot: por nome OU por proximidade do socket "head" quando BoneName vier vazio
	bool bIsHeadshot = false;

	// 1) Nome do osso
	if (!BoneName.IsNone())
	{
		bIsHeadshot = NameIsHeadLike(BoneName);
	}

	// 2) Fallback por proximidade do socket "head"
	if (!bIsHeadshot && HeadshotProximityRadius > 0.f)
	{
		if (ACharacter* Char = Cast<ACharacter>(DamagedActor))
		{
			if (USkeletalMeshComponent* Skel = Char->GetMesh())
			{
				static const FName HeadName(TEXT("head"));
				if (Skel->DoesSocketExist(HeadName))
				{
					const float DistToHead = FVector::Distance(Skel->GetSocketLocation(HeadName), HitLocation);
					if (DistToHead <= HeadshotProximityRadius)
					{
						bIsHeadshot = true;
						BoneName = HeadName;
					}
				}
			}
		}
	}

	if (bDebugDamage)
	{
		const FString BoneStr = BoneName.IsNone() ? TEXT("None") : BoneName.ToString();
		UE_LOG(LogTemp, Log, TEXT("[HealthComponent] PointDamage on '%s': Dmg=%.1f  Bone=%s  IsHeadshot=%d  IK=%d  Curr=%.1f/%s"),
			*GetOwner()->GetName(), Damage, *BoneStr, bIsHeadshot ? 1 : 0, bHeadshotInstantKill ? 1 : 0, CurrentHealth, *FString::SanitizeFloat(MaxHealth));
	}

	// Headshot Instant Kill
	if (bHeadshotInstantKill && bIsHeadshot)
	{
		Kill();
		bBlockNextAnyDamage = true; // Ignore o AnyDamage que chegar depois desse tiro
		return;
	}

	// Caso normal: aplica o dano recebido
	ApplyDamage(Damage);
	bBlockNextAnyDamage = true;
}

void UHealthComponent::HandleAnyDamage(AActor* DamagedActor, float Damage,
	const UDamageType* DamageType,
	AController* InstigatedBy, AActor* DamageCauser)
{
	if (IsDead()) return;

	// Se acabou de processar PointDamage, ignore este AnyDamage “duplicado”
	if (bBlockNextAnyDamage)
	{
		bBlockNextAnyDamage = false;
		return;
	}

	if (Damage <= 0.f) return;

	if (bDebugDamage)
	{
		UE_LOG(LogTemp, Log, TEXT("[HealthComponent] AnyDamage on '%s': Dmg=%.1f  Curr=%.1f/%s"),
			*GetOwner()->GetName(), Damage, CurrentHealth, *FString::SanitizeFloat(MaxHealth));
	}

	ApplyDamage(Damage);
}

void UHealthComponent::ApplyDamage(float Damage)
{
	CurrentHealth = FMath::Clamp(CurrentHealth - Damage, 0.f, MaxHealth);

	if (bDebugDamage)
	{
		UE_LOG(LogTemp, Log, TEXT("[HealthComponent] '%s' ApplyDamage: -%.1f => Curr=%.1f/%s"),
			*GetOwner()->GetName(), Damage, CurrentHealth, *FString::SanitizeFloat(MaxHealth));
	}

	if (CurrentHealth <= 0.f)
	{
		Die();
	}
}

void UHealthComponent::Die()
{
	if (ACharacter* Char = Cast<ACharacter>(GetOwner()))
	{
		if (bAutoRagdollOnDeath && Char->GetMesh())
		{
			Char->GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
			Char->GetMesh()->SetSimulatePhysics(true);
		}
		if (Char->GetCharacterMovement())
		{
			Char->GetCharacterMovement()->DisableMovement();
		}
	}

	if (bDebugDamage)
	{
		UE_LOG(LogTemp, Warning, TEXT("[HealthComponent] '%s' DIED"), *GetOwner()->GetName());
	}

	OnDeath.Broadcast(GetOwner());
}
