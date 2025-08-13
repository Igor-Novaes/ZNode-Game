#include "WeaponBase.h"

#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "ZNodeCharacter.h"

AWeaponBase::AWeaponBase()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AWeaponBase::BeginPlay()
{
	Super::BeginPlay();
	AmmoInMag = MagazineSize;
}

bool AWeaponBase::CanFire() const
{
	if (bIsReloading) return false;
	if (AmmoInMag <= 0) return false;

	if (RateOfFire > 0.f)
	{
		const double Now = FPlatformTime::Seconds();
		const double TimeBetween = 1.0 / static_cast<double>(RateOfFire);
		if (Now - LastFireTime < TimeBetween) return false;
	}
	return true;
}

bool AWeaponBase::TryFire(const FVector& MuzzleWorld, const FVector& DesiredTarget, AZNodeCharacter* Shooter)
{
	if (!CanFire() || !Shooter) return false;
	UWorld* World = GetWorld();
	if (!World) return false;

	/* ---------------------------------------------------------
	   1) MultiTrace complexo (triângulos) no canal Visibility
	----------------------------------------------------------*/
	TArray<FHitResult> Hits;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(WeaponFire), /*bTraceComplex*/ true, Shooter);
	Params.bReturnPhysicalMaterial = false;
	Params.AddIgnoredActor(Shooter);
	Params.TraceTag = TEXT("WeaponFire");

	// Garante que vamos até TraceRange (caso DesiredTarget esteja mais perto)
	const FVector Dir = (DesiredTarget - MuzzleWorld).GetSafeNormal();
	const FVector End = MuzzleWorld + Dir * TraceRange;

	const ECollisionChannel Channel = ECC_Visibility;

	World->LineTraceMultiByChannel(Hits, MuzzleWorld, End, Channel, Params);

	/* ---------------------------------------------------------
	   2) Log detalhado do caminho (ordem dos acertos)
	----------------------------------------------------------*/
	if (bDebugTrace)
	{
		int32 Index = 0;
		for (const FHitResult& H : Hits)
		{
			const FString Bone = H.BoneName.IsNone() ? TEXT("None") : H.BoneName.ToString();
			const FString Comp = H.Component.IsValid() ? H.Component->GetName() : TEXT("None");
			const FString Act = H.GetActor() ? H.GetActor()->GetName() : TEXT("None");
			const float Dist = FVector::Distance(MuzzleWorld, H.ImpactPoint);

			UE_LOG(LogTemp, Warning, TEXT("[%d] Hit=%d  Act=%s  Comp=%s  Bone=%s  Dist=%.0f"),
				Index++, H.bBlockingHit ? 1 : 0, *Act, *Comp, *Bone, Dist);
		}
	}

	/* ---------------------------------------------------------
	   3) Escolhe o hit efetivo
		  - Primeiro blocking
		  - Se for Capsule de Character, tenta o SkeletalMesh do mesmo ator
	----------------------------------------------------------*/
	FHitResult Hit; bool bHit = false;

	// 3.1: primeiro blocking encontrado
	for (const FHitResult& H : Hits)
	{
		if (H.bBlockingHit)
		{
			Hit = H; bHit = true;
			break;
		}
	}

	// 3.2: furar a cápsula — trocar por SkeletalMesh do mesmo ator, se existir logo depois
	if (bHit && Hit.Component.IsValid() && Hit.Component->IsA<UCapsuleComponent>() && Hit.GetActor())
	{
		for (const FHitResult& H : Hits)
		{
			if (H.GetActor() == Hit.GetActor() &&
				H.Component.IsValid() && H.Component->IsA<USkeletalMeshComponent>() &&
				H.bBlockingHit)
			{
				Hit = H; // agora temos o mesh (e possivelmente BoneName)
				break;
			}
		}
	}

	/* ---------------------------------------------------------
	   4) Fallback: sem BoneName? Marca "head" por proximidade do socket
	----------------------------------------------------------*/
	if (bHit && Hit.BoneName.IsNone())
	{
		if (ACharacter* Char = Cast<ACharacter>(Hit.GetActor()))
		{
			if (USkeletalMeshComponent* Skel = Char->GetMesh())
			{
				static const FName HeadName(TEXT("head"));
				if (Skel->DoesSocketExist(HeadName))
				{
					const float DistToHead = FVector::Distance(Skel->GetSocketLocation(HeadName), Hit.ImpactPoint);
					// Ajuste conforme o seu esqueleto (20–35 uu é um bom ponto de partida)
					if (DistToHead <= 30.f)
					{
						Hit.BoneName = HeadName;
					}
				}
			}
		}
	}

	/* ---------------------------------------------------------
	   5) Debug visual (linha/ponto + texto no ponto final)
	----------------------------------------------------------*/
	const FVector EndPoint = bHit ? Hit.ImpactPoint : End;

	if (bDebugTrace)
	{
		DrawDebugLine(World, MuzzleWorld, EndPoint, bHit ? FColor::Red : FColor::Green, false, 1.5f, 0, 2.5f);
		DrawDebugPoint(World, EndPoint, 12.f, bHit ? FColor::Red : FColor::Green, false, 1.5f);

		const FString Bone = Hit.BoneName.IsNone() ? TEXT("None") : Hit.BoneName.ToString();
		const FString Comp = Hit.Component.IsValid() ? Hit.Component->GetName() : TEXT("None");
		const FString Act = Hit.GetActor() ? Hit.GetActor()->GetName() : TEXT("None");
		const float   Dist = FVector::Distance(MuzzleWorld, EndPoint);

		const FString Msg = FString::Printf(TEXT("Hit: %s\nComp: %s\nBone: %s\nDist: %.0f\nBlocking:%d"),
			*Act, *Comp, *Bone, Dist, bHit && Hit.bBlockingHit ? 1 : 0);

		DrawDebugString(World, EndPoint + FVector(0, 0, 18), Msg, nullptr, FColor::White, 1.5f, true);
	}

	/* ---------------------------------------------------------
	   6) Aplicar dano (BoneName pode ter sido ajustado pelo fallback)
	----------------------------------------------------------*/
	if (bHit && Hit.GetActor())
	{
		const float Mult = GetBoneMultiplier(Hit.BoneName);
		const float FinalDamage = BaseDamage * Mult;
		const FVector ShotDir = Dir; // direção do disparo

		UGameplayStatics::ApplyPointDamage(
			Hit.GetActor(), FinalDamage, ShotDir, Hit,
			Shooter->GetController(), this, UDamageType::StaticClass());
	}

	/* ---------------------------------------------------------
	   7) Consumo de munição e controle de cadência
	----------------------------------------------------------*/
	AmmoInMag = FMath::Max(AmmoInMag - 1, 0);
	LastFireTime = FPlatformTime::Seconds();

	if (AmmoInMag == 0 && ReserveAmmo > 0)
	{
		StartReload(Shooter);
	}

	return true;
}

void AWeaponBase::StartReload(AZNodeCharacter* Shooter)
{
	if (bIsReloading) return;
	if (AmmoInMag >= MagazineSize) return;
	if (ReserveAmmo <= 0) return;

	bIsReloading = true;

	FTimerDelegate Del;
	Del.BindUObject(this, &AWeaponBase::FinishReload);
	GetWorldTimerManager().SetTimer(TH_Reload, Del, ReloadTime, false);
}

void AWeaponBase::FinishReload()
{
	bIsReloading = false;

	const int32 Need = MagazineSize - AmmoInMag;
	const int32 Taken = FMath::Min(Need, ReserveAmmo);
	AmmoInMag += Taken;
	ReserveAmmo -= Taken;
}

float AWeaponBase::GetBoneMultiplier(const FName& Bone) const
{
	const FString Name = Bone.ToString().ToLower();

	if (Name.Contains(TEXT("head")) || Name.Equals(TEXT("head")))
		return 2.0f;
	if (Name.Contains(TEXT("neck")))
		return 1.5f;
	if (Name.Contains(TEXT("spine")) || Name.Contains(TEXT("pelvis")) || Name.Contains(TEXT("chest")))
		return 1.0f;
	if (Name.Contains(TEXT("upperarm")) || Name.Contains(TEXT("lowerarm")) || Name.Contains(TEXT("hand")))
		return 0.75f;
	if (Name.Contains(TEXT("thigh")) || Name.Contains(TEXT("calf")) || Name.Contains(TEXT("foot")))
		return 0.75f;

	return 1.0f;
}
