#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "ZNodeCharacter.generated.h"

/* Encaminhamentos */
class UCameraComponent;
class USpringArmComponent;
class UInputMappingContext;
class UInputAction;
class UPrimitiveComponent;

/**
 * Personagem isométrico:
 *   • Zoom + mouse-look (câmera gira só fora da mira)
 *   • Fade de obstáculos
 *   • Mira em profundidade (segura BTD) + tiro (BT E) com linha debug
 *   • Tronco gira até YawThreshold; pernas acompanham depois
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
	/* ---------- callbacks ---------- */
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void Zoom(const FInputActionValue& Value);

	void StartAim();       // BTD pressionado
	void StopAim();        // BTD solto
	void Fire();           // BTE enquanto mira

	/* ---------- helpers ---------- */
	FVector GetAimTargetPoint() const;  // ponto-alvo do cursor
	void    UpdateObstacleFade();       // oculta/mostra obstáculos

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

	/* ---------- mira & torção ---------- */
	UPROPERTY(EditDefaultsOnly, Category = "Aim", meta = (ClampMin = "10", ClampMax = "180"))
	float YawThreshold = 60.f;                // máx. do tronco (graus)

	UPROPERTY(EditDefaultsOnly, Category = "Aim", meta = (ClampMin = "10", ClampMax = "720"))
	float TurnSpeedDegPerSec = 180.f;         // velocidade das pernas

	UPROPERTY(BlueprintReadOnly, Category = "Aim", meta = (AllowPrivateAccess = "true"))
	float AimYaw = 0.f;                       // usado pela AnimBP

	bool bIsAiming = false;

	/* ---------- fade runtime ---------- */
	TArray<TWeakObjectPtr<UPrimitiveComponent>> FadedComponents;
};
