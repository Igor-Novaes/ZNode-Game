#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WeaponBase.generated.h"

class AZNodeCharacter;

UCLASS(Blueprintable)
class ZNODE_API AWeaponBase : public AActor
{
	GENERATED_BODY()

public:
	AWeaponBase();

	/* ------------------- Config ------------------- */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Damage", meta = (ClampMin = "0"))
	float BaseDamage = 25.f;

	/** Tiros por segundo (6 = 360 RPM) */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Fire", meta = (ClampMin = "0"))
	float RateOfFire = 6.f;

	/** Alcance do line trace (1e6 ≈ “infinito” na prática) */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Fire", meta = (ClampMin = "1000"))
	float TraceRange = 1000000.0f;

	/* ------------------- Munição ------------------- */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Ammo", meta = (ClampMin = "1"))
	int32 MagazineSize = 12;

	UPROPERTY(VisibleInstanceOnly, Category = "Weapon|Ammo")
	int32 AmmoInMag = 0;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Ammo", meta = (ClampMin = "0"))
	int32 ReserveAmmo = 60;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Ammo", meta = (ClampMin = "0.1"))
	float ReloadTime = 1.6f;

	/* ------------------- Debug ------------------- */
	/** Desenha linha/ponto e imprime Actor/Component/Bone/Dist no impacto */
	UPROPERTY(EditAnywhere, Category = "Weapon|Debug")
	bool bDebugTrace = true;

	/* ------------------- API ------------------- */
	/** Dispara um hitscan do Muzzle até DesiredTarget; aplica dano no que atingir */
	bool TryFire(const FVector& MuzzleWorld, const FVector& DesiredTarget, AZNodeCharacter* Shooter);

	void StartReload(AZNodeCharacter* Shooter);
	bool IsReloading() const { return bIsReloading; }

protected:
	virtual void BeginPlay() override;

private:
	bool CanFire() const;
	void FinishReload();

	/** Multiplicador por osso (head = 2.0, etc.) */
	float GetBoneMultiplier(const FName& Bone) const;

private:
	FTimerHandle TH_Reload;
	double LastFireTime = -BIG_NUMBER;
	bool bIsReloading = false;
};
