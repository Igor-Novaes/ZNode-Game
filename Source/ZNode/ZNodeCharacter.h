#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "ZNodeCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UInputMappingContext;
class UInputAction;
class UPrimitiveComponent;
class AWeaponBase; // <- forward declaration da arma

UCLASS()
class ZNODE_API AZNodeCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AZNodeCharacter();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:
	// Input
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void Zoom(const FInputActionValue& Value);

	void StartAim();
	void StopAim();
	void Fire();                 // atira via arma (só se estiver mirando)
	void ReloadWeapon();         // <--- DECLARADO AQUI

	// Helpers
	FVector GetAimTargetPoint() const; // cursor → mundo
	void    UpdateObstacleFade();      // oculta obstáculos

private:
	// Componentes de câmera
	UPROPERTY(VisibleAnywhere, Category = "Camera")
	USpringArmComponent* CameraBoom = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	UCameraComponent* FollowCamera = nullptr;

	// Enhanced Input
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> JumpAction = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> LookAction = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> ZoomAction = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> AimAction = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> FireAction = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> ReloadAction = nullptr; // <--- NOVO (R)

	// Zoom cfg
	UPROPERTY(EditDefaultsOnly, Category = "Camera|Zoom")
	float MinZoom = 400.f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera|Zoom")
	float MaxZoom = 1200.f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera|Zoom")
	float ZoomInterpSpeed = 10.f;

	// Mira & torção
	UPROPERTY(EditDefaultsOnly, Category = "Aim", meta = (ClampMin = "10", ClampMax = "180"))
	float YawThreshold = 60.f;

	UPROPERTY(EditDefaultsOnly, Category = "Aim", meta = (ClampMin = "10", ClampMax = "720"))
	float TurnSpeedDegPerSec = 180.f;

	UPROPERTY(BlueprintReadOnly, Category = "Aim", meta = (AllowPrivateAccess = "true"))
	float AimYaw = 0.f;

	bool bIsAiming = false;

	// Fade runtime
	TArray<TWeakObjectPtr<UPrimitiveComponent>> FadedComponents;

	// Arma
	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	TSubclassOf<AWeaponBase> DefaultWeaponClass;   // <--- NOVO: classe da arma

	UPROPERTY(VisibleInstanceOnly, Category = "Weapon")
	TObjectPtr<AWeaponBase> CurrentWeapon;         // <--- NOVO: arma atual
};
