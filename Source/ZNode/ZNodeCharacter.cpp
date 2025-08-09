#include "ZNodeCharacter.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Components/PrimitiveComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetMathLibrary.h"

/* =========================================================
 *                       CONSTRUTOR
 * ========================================================= */
AZNodeCharacter::AZNodeCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	/* ---- SpringArm ---- */
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 800.f;
	CameraBoom->SetRelativeRotation(FRotator(-60.f, 0.f, 0.f));
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bInheritPitch = false;
	CameraBoom->bDoCollisionTest = false;   // fade já cuida

	/* ---- Camera ---- */
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	/* ---- Movimento ---- */
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
}

/* ========================================================= */
void AZNodeCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (AController* C = GetController())
		C->SetControlRotation(FRotator(0.f, 45.f, 0.f));  // ângulo ISO

	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (auto* SubSys =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			SubSys->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

/* ========================================================= */
void AZNodeCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateObstacleFade();

	/* --- tronco & pernas --- */
	if (bIsAiming)
	{
		/* Ponto onde o cursor intercepta o mundo */
		const FVector Target = GetAimTargetPoint();

		/* Ângulo desejado = direção (Pawn → Target) */
		const float DesiredYaw = (Target - GetActorLocation()).Rotation().Yaw;

		const float PawnYaw = GetActorRotation().Yaw;
		float DeltaYaw = FMath::FindDeltaAngleDegrees(PawnYaw, DesiredYaw);  // -180..180

		AimYaw = FMath::Clamp(DeltaYaw, -YawThreshold, YawThreshold);        // alimenta AnimBP

		/* Se exceder limiar, gira as pernas */
		if (FMath::Abs(DeltaYaw) > YawThreshold)
		{
			const float Sign = FMath::Sign(DeltaYaw);
			const float Step = Sign * TurnSpeedDegPerSec * DeltaTime;
			AddActorWorldRotation(FRotator(0.f, Step, 0.f));
		}
	}
	else
	{
		AimYaw = 0.f;
	}
}

/* =========================================================
 *                 INPUT  BINDINGS
 * ========================================================= */
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
	}
}

/* ----------------------- Movimento ----------------------- */
void AZNodeCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	if (Axis.IsNearlyZero()) return;

	const FRotator YawRot(0.f, Controller->GetControlRotation().Yaw, 0.f);
	const FVector  Fwd = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
	const FVector  Rgt = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);

	AddMovementInput(Fwd, Axis.Y);
	AddMovementInput(Rgt, Axis.X);
}

/* ----------------------- Look ----------------------- */
void AZNodeCharacter::Look(const FInputActionValue& Value)
{
	if (bIsAiming) return;                        // câmera não gira enquanto mira
	AddControllerYawInput(Value.Get<FVector2D>().X);
}

/* ----------------------- Zoom ----------------------- */
void AZNodeCharacter::Zoom(const FInputActionValue& Value)
{
	const float Scroll = Value.Get<float>();
	if (FMath::IsNearlyZero(Scroll)) return;

	const float Desired = FMath::Clamp(
		CameraBoom->TargetArmLength - Scroll * 150.f,
		MinZoom, MaxZoom);

	CameraBoom->TargetArmLength = FMath::FInterpTo(
		CameraBoom->TargetArmLength,
		Desired,
		GetWorld()->GetDeltaSeconds(),
		ZoomInterpSpeed);
}

/* --------------------- Aim state --------------------- */
void AZNodeCharacter::StartAim()
{
	bIsAiming = true;

	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		PC->bShowMouseCursor = true;
		PC->CurrentMouseCursor = EMouseCursor::Crosshairs;
		// PC->SetIgnoreLookInput(true);   // ← COMENTE
	}
}
void AZNodeCharacter::StopAim()
{
	bIsAiming = false;

	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		PC->bShowMouseCursor = false;
		PC->CurrentMouseCursor = EMouseCursor::Default;
		// PC->SetIgnoreLookInput(false);  // ← COMENTE

	}
}

/* ------------------------- Fire ------------------------- */
void AZNodeCharacter::Fire()
{
	if (!bIsAiming) return;

	/* ponto inicial: cabeça (socket) ou olhos */
	FVector Muzzle = GetActorLocation() + FVector(0.f, 0.f, BaseEyeHeight);
	if (USkeletalMeshComponent* MeshComp = GetMesh())
		if (MeshComp->DoesSocketExist(TEXT("head")))
			Muzzle = MeshComp->GetSocketLocation(TEXT("head"));

	const FVector Target = GetAimTargetPoint();

	FHitResult Hit;
	UKismetSystemLibrary::LineTraceSingle(
		this, Muzzle, Target,
		UEngineTypes::ConvertToTraceType(ECC_Visibility),
		false, {}, EDrawDebugTrace::ForDuration,
		Hit, true,
		FLinearColor::Red, FLinearColor::Green, 1.5f);

	// TODO: dano / projétil real
}

/* ------ converte cursor -> ponto 3-D (sem limite visível) ------ */
FVector AZNodeCharacter::GetAimTargetPoint() const
{
	APlayerController* PC = Cast<APlayerController>(Controller);
	if (!PC) return GetActorLocation();

	FVector Pos, Dir;
	PC->DeprojectMousePositionToWorld(Pos, Dir);

	const float MaxDist = 1'000'000.f;                          // 1 km Unreal
	FHitResult Hit;
	FCollisionQueryParams P(SCENE_QUERY_STAT(CursorTrace), true, this);

	GetWorld()->LineTraceSingleByChannel(
		Hit,
		Pos,
		Pos + Dir * MaxDist,
		ECC_Visibility,
		P);

	return Hit.bBlockingHit ? Hit.ImpactPoint
		: Pos + Dir * MaxDist;
}

/* ------------------ Fade de obstáculos ------------------ */
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
