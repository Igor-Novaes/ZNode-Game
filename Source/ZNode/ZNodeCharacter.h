#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "ZNodeCharacter.generated.h"

/* Encaminhamentos rápidos */
class UCameraComponent;
class USpringArmComponent;
class UInputMappingContext;
class UInputAction;
class UPrimitiveComponent;

/**
 * Personagem isométrico – câmera orbitável, zoom, fade de obstáculos,
 * mira em profundidade (segurando botão direito) e disparo (esq. enquanto mira).
 */
UCLASS()
class ZNODE_API AZNodeCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AZNodeCharacter();

	/* ---------- engine ---------- */
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:
	/* ---------- callbacks de input ---------- */
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void Zoom(const FInputActionValue& Value);

	void StartAim();     // botão direito pressionado
	void StopAim();      // botão direito solto
	void Fire();         // botão esquerdo enquanto mira

	/* ---------- helpers ---------- */
	FVector GetAimTargetPoint() const; // converte cursor em ponto 3-D
	void    UpdateObstacleFade();      // oculta/mostra obstáculos

private:
	/* ---------- componentes ---------- */
	UPROPERTY(VisibleAnywhere, Category = "Camera")
	USpringArmComponent* CameraBoom = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	UCameraComponent* FollowCamera = nullptr;

	/* ---------- input assets ---------- */
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
	TObjectPtr<UInputAction> AimAction = nullptr;   // botão direito

	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> FireAction = nullptr;   // botão esquerdo

	/* ---------- zoom ---------- */
	UPROPERTY(EditDefaultsOnly, Category = "Camera|Zoom", meta = (ClampMin = "100", ClampMax = "3000"))
	float MinZoom = 400.f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera|Zoom", meta = (ClampMin = "100", ClampMax = "3000"))
	float MaxZoom = 1200.f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera|Zoom", meta = (ClampMin = "1", ClampMax = "60"))
	float ZoomInterpSpeed = 10.f;

	/* ---------- mira ---------- */
	UPROPERTY(EditDefaultsOnly, Category = "Aim", meta = (ClampMin = "1000", ClampMax = "100000"))

	bool bIsAiming = false;                    // estado atual de mira

	/* ---------- obstáculo fade ---------- */
	TArray<TWeakObjectPtr<UPrimitiveComponent>> FadedComponents;
};
