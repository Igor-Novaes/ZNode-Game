#include "ZNodeCharacter.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Components/PrimitiveComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "WeaponBase.h" // <--- include da arma

/* ---------------- Constructor ---------------- */
AZNodeCharacter::AZNodeCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 800.f;
	CameraBoom->SetRelativeRotation(FRotator(-60.f, 0.f, 0.f));
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bInheritPitch = false;
	CameraBoom->bDoCollisionTest = false;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
}

/* ---------------- BeginPlay ---------------- */
void AZNodeCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (AController* C = GetController())
		C->SetControlRotation(FRotator(0.f, 45.f, 0.f));

	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (auto* SubSys =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			SubSys->AddMappingContext(DefaultMappingContext, 0);
		}
	}

	// ------- Spawn da arma padrão (DENTRO DE UMA FUNÇÃO MEMBRO) -------
	if (DefaultWeaponClass)
	{
		FActorSpawnParameters SP; SP.Owner = this; SP.Instigator = this;
		CurrentWeapon = GetWorld()->SpawnActor<AWeaponBase>(DefaultWeaponClass, GetActorTransform(), SP);
	}
}

/* ---------------- Tick ---------------- */
void AZNodeCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateObstacleFade();

	if (bIsAiming)
	{
		const FVector Target = GetAimTargetPoint();
		const float DesiredYaw = (Target - GetActorLocation()).Rotation().Yaw;
		const float PawnYaw = GetActorRotation().Yaw;

		float DeltaYaw = FMath::FindDeltaAngleDegrees(PawnYaw, DesiredYaw); // -180..180
		AimYaw = FMath::Clamp(DeltaYaw, -YawThreshold, YawThreshold);

		if (FMath::Abs(DeltaYaw) > YawThreshold)
		{
			const float Step = FMath::Sign(DeltaYaw) * TurnSpeedDegPerSec * DeltaTime;
			AddActorWorldRotation(FRotator(0.f, Step, 0.f));
		}
	}
	else
	{
		AimYaw = 0.f;
	}
}

/* ---------------- Setup Input ---------------- */
void AZNodeCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (auto* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EIC->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AZNodeCharacter::Move);
		EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &AZNodeCharacter::Look);
		EIC->BindAction(ZoomAction, ETriggerEvent::Triggered, this, &AZNodeCharacter::Zoom);

		EIC->BindAction(AimAction, ETriggerEvent::Started, this, &AZNodeCharacter::StartAim);
		EIC->BindAction(AimAction, ETriggerEvent::Completed, this, &AZNodeCharacter::StopAim);

		EIC->BindAction(FireAction, ETriggerEvent::Triggered, this, &AZNodeCharacter::Fire);

		// --- ESTA LINHA SÓ COMPILA SE ReloadWeapon ESTIVER DECLARADO NO .h E ReloadAction ESTIVER SETADO ---
		EIC->BindAction(ReloadAction, ETriggerEvent::Started, this, &AZNodeCharacter::ReloadWeapon);
	}
}

/* ------------ Move / Look / Zoom ------------ */
void AZNodeCharacter::Move(const FInputActionValue& V)
{
	const FVector2D Axis = V.Get<FVector2D>(); if (Axis.IsNearlyZero()) return;
	const FRotator YawRot(0.f, Controller->GetControlRotation().Yaw, 0.f);
	const FVector Fwd = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
	const FVector Rgt = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);
	AddMovementInput(Fwd, Axis.Y); AddMovementInput(Rgt, Axis.X);
}
void AZNodeCharacter::Look(const FInputActionValue& V) { if (bIsAiming) return; AddControllerYawInput(V.Get<FVector2D>().X); }
void AZNodeCharacter::Zoom(const FInputActionValue& V) {
	const float D = V.Get<float>(); if (FMath::IsNearlyZero(D)) return;
	const float T = FMath::Clamp(CameraBoom->TargetArmLength - D * 150.f, MinZoom, MaxZoom);
	CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, T,
		GetWorld()->GetDeltaSeconds(), ZoomInterpSpeed);
}

/* --------------- Aim state --------------- */
void AZNodeCharacter::StartAim()
{
	bIsAiming = true;
	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		PC->bShowMouseCursor = true;
		PC->CurrentMouseCursor = EMouseCursor::Crosshairs;
	}
}
void AZNodeCharacter::StopAim()
{
	bIsAiming = false;
	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		PC->bShowMouseCursor = false;
		PC->CurrentMouseCursor = EMouseCursor::Default;
	}
}

/* ---------------- Fire / Reload ---------------- */
void AZNodeCharacter::Fire()
{
	if (!bIsAiming || !CurrentWeapon) return;

	// Ponto inicial: cabeça (socket "head") ou olhos
	FVector Muzzle = GetActorLocation() + FVector(0.f, 0.f, BaseEyeHeight);
	if (USkeletalMeshComponent* MeshComp = GetMesh())
		if (MeshComp->DoesSocketExist(TEXT("head")))
			Muzzle = MeshComp->GetSocketLocation(TEXT("head"));

	const FVector Target = GetAimTargetPoint();

	CurrentWeapon->TryFire(Muzzle, Target, this);
}

void AZNodeCharacter::ReloadWeapon() // <--- DEFINIÇÃO EXISTE AQUI
{
	if (CurrentWeapon)
	{
		CurrentWeapon->StartReload(this);
	}
}

/* -------- cursor 3-D (alvo “infinito”) -------- */
FVector AZNodeCharacter::GetAimTargetPoint() const
{
	APlayerController* PC = Cast<APlayerController>(Controller);
	if (!PC) return GetActorLocation();

	FVector CamPos, CamDir;
	PC->DeprojectMousePositionToWorld(CamPos, CamDir);

	const float MaxDist = 1000000.0f; // sem apóstrofos
	FHitResult Hit;
	FCollisionQueryParams P(SCENE_QUERY_STAT(CursorTrace), true, this);
	GetWorld()->LineTraceSingleByChannel(Hit, CamPos, CamPos + CamDir * MaxDist, ECC_Visibility, P);

	return Hit.bBlockingHit ? Hit.ImpactPoint : CamPos + CamDir * MaxDist;
}

/* -------- esconde obstáculos -------- */
void AZNodeCharacter::UpdateObstacleFade()
{
	const FVector Cam = FollowCamera->GetComponentLocation();
	const FVector Target = GetActorLocation();

	TArray<FHitResult> Hits;
	FCollisionQueryParams P(SCENE_QUERY_STAT(ObsFade), true, this);
	GetWorld()->LineTraceMultiByChannel(Hits, Cam, Target, ECC_Visibility, P);

	TSet<TWeakObjectPtr<UPrimitiveComponent>> Current;
	for (const FHitResult& H : Hits)
	{
		if (UPrimitiveComponent* C = H.GetComponent())
		{
			if (C->GetOwner() == this) continue;
			C->SetVisibility(false, true);
			Current.Add(C);
		}
	}
	for (auto Old : FadedComponents)
		if (Old.IsValid() && !Current.Contains(Old))
			Old->SetVisibility(true, true);

	FadedComponents = Current.Array();
}
