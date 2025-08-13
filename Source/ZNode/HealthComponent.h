#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDeathSignature, AActor*, OwnerActor);

UCLASS(ClassGroup = (Combat), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class ZNODE_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHealthComponent();

	/* --------- Config --------- */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health", meta = (ClampMin = "1"))
	float MaxHealth = 100.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Health")
	float CurrentHealth = 0.f;

	/** Se true, personagens entram em ragdoll ao morrer */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	bool bAutoRagdollOnDeath = true;

	/** Se true, dar hit em osso da cabeça mata instantaneamente */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health|Headshot")
	bool bHeadshotInstantKill = false;

	/** Quais ossos contam como “cabeça” (case-insensitive). Se vazio, usaremos heurística por nome contendo "head". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health|Headshot")
	TArray<FName> HeadshotBones;

	/** Raio (uu) para fallback por proximidade do socket "head" quando BoneName vier vazio/inesperado */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health|Headshot", meta = (ClampMin = "0"))
	float HeadshotProximityRadius = 30.f;

	/** Liga logs de debug deste componente (o que chegou, qual osso, se matou, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health|Debug")
	bool bDebugDamage = true;

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnDeathSignature OnDeath;

	/* --------- API --------- */
	UFUNCTION(BlueprintCallable, Category = "Health")
	bool IsDead() const { return CurrentHealth <= 0.f; }

	UFUNCTION(BlueprintCallable, Category = "Health")
	void Heal(float Amount);

	/** Mata imediatamente (útil p/ testes/cheats) */
	UFUNCTION(BlueprintCallable, Category = "Health")
	void Kill();

protected:
	virtual void BeginPlay() override;

	/* Dano genérico */
	UFUNCTION()
	void HandleAnyDamage(AActor* DamagedActor, float Damage,
		const class UDamageType* DamageType,
		class AController* InstigatedBy, AActor* DamageCauser);

	/* Dano pontual – tem BoneName/HitLocation/HitComponent */
	UFUNCTION()
	void HandlePointDamage(AActor* DamagedActor, float Damage, class AController* InstigatedBy,
		FVector HitLocation, class UPrimitiveComponent* HitComponent,
		FName BoneName, FVector ShotFromDirection,
		const class UDamageType* DamageType, AActor* DamageCauser);

private:
	void ApplyDamage(float Damage);
	void Die();

	/** heurística por nome (case-insensitive) + lista HeadshotBones */
	bool NameIsHeadLike(const FName& Bone) const;

	/** Evita contar duas vezes quando a engine dispara AnyDamage além de PointDamage */
	bool bBlockNextAnyDamage = false;
};
