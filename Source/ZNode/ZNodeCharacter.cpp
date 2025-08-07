#include "ZNodeCharacter.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Components/PrimitiveComponent.h"
#include "Kismet/KismetSystemLibrary.h"

/* =========================================================
 *                       CONSTRUTOR
 * ========================================================= */
AZNodeCharacter::AZNodeCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	/* ---- SPRING ARM ---- */
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 800.f;
	CameraBoom->SetRelativeRotation(FRotator(-60.f, 0.f, 0.f));  // ângulo ISO
	CameraBoom->bUsePawnControlRotation = true;                  // gira com mouse
	CameraBoom->bInheritPitch = false;                           // não eleva
	CameraBoom->bDoCollisionTest = false;                        // não encolhe (usamos fade)

	/* ---- CÂMERA ---- */
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	/* ---- MOVIMENTO ---- */
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
}

/* =========================================================
 *                        BEGIN PLAY
 * ========================================================= */
void AZNodeCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Yaw inicial de 45° para vista isométrica
	if (AController* C = GetController())
		C->SetControlRotation(FRotator(0.f, 45.f, 0.f));

	// Adiciona Mapping Context
	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (auto* SubSys =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			SubSys->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

/* =========================================================
 *                           TICK
 * ========================================================= */
void AZNodeCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateObstacleFade();

	// Enquanto mira, gira pawn para o alvo
	if (bIsAiming)
	{
		const FVector Target = GetAimTargetPoint();
		FRotator Desired = (Target - GetActorLocation()).Rotation();
		Desired.Pitch = 0.f; Desired.Roll = 0.f;

		SetActorRotation(FMath::RInterpTo(GetActorRotation(), Desired, DeltaTime, 15.f));
	}
}

/* =========================================================
 *                 BINDING DE INPUTS
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

/* --------------------------- MOVIMENTO --------------------------- */
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

/* ----------------------------- LOOK ------------------------------ */
void AZNodeCharacter::Look(const FInputActionValue& Value)
{
	if (bIsAiming) return;                    // câmera fica fixa enquanto mira
	AddControllerYawInput(Value.Get<FVector2D>().X);
}

/* ----------------------------- ZOOM ------------------------------ */
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

/* ------------------------- START / STOP AIM ---------------------- */
void AZNodeCharacter::StartAim()
{
	bIsAiming = true;

	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		PC->bShowMouseCursor = true;
		PC->CurrentMouseCursor = EMouseCursor::Crosshairs;
		PC->SetIgnoreLookInput(true);          // segurança extra
	}
}
void AZNodeCharacter::StopAim()
{
	bIsAiming = false;

	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		PC->bShowMouseCursor = false;
		PC->CurrentMouseCursor = EMouseCursor::Default;
		PC->SetIgnoreLookInput(false);
	}
}

/* ------------------------------ FIRE ----------------------------- */
void AZNodeCharacter::Fire()
{
	if (!bIsAiming) return;  // só dispara se mirando

	/* 1) ponto inicial = socket "head" ou olhos */
	FVector Muzzle = GetActorLocation() + FVector(0.f, 0.f, BaseEyeHeight);
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		if (MeshComp->DoesSocketExist(TEXT("head")))
			Muzzle = MeshComp->GetSocketLocation(TEXT("head"));
	}

	/* 2) ponto alvo */
	const FVector Target = GetAimTargetPoint();

	/* 3) line trace visível (vermelho = hit, verde = miss) */
	FHitResult Hit;
	UKismetSystemLibrary::LineTraceSingle(
		this,
		Muzzle,
		Target,
		UEngineTypes::ConvertToTraceType(ECC_Visibility),
		false,
		TArray<AActor*>(),
		EDrawDebugTrace::ForDuration,   // desenha por 1.5 s
		Hit,
		true,
		FLinearColor::Red,
		FLinearColor::Green,
		1.5f);

	// TODO: dano ou projétil real aqui
}

/* ------------------- CONVERTE CURSOR → 3-D ----------------------- */
FVector AZNodeCharacter::GetAimTargetPoint() const
{
	APlayerController* PC = Cast<APlayerController>(Controller);
	if (!PC) return GetActorLocation();

	/* 1) De-projecta posição do mouse */
	FVector CamPos, CamDir;
	PC->DeprojectMousePositionToWorld(CamPos, CamDir);

	/* 2) Raycast a 1 000 000 uu */
	const float MaxDist = 1'000'000.f;
	FHitResult Hit;
	FCollisionQueryParams P(SCENE_QUERY_STAT(CursorTrace), true, this);

	GetWorld()->LineTraceSingleByChannel(
		Hit,
		CamPos,
		CamPos + CamDir * MaxDist,
		ECC_Visibility,
		P);

	return Hit.bBlockingHit ? Hit.ImpactPoint              // ponto exato do clique
		: CamPos + CamDir * MaxDist;   // nada atingido → longe
}

/* ------------------- F A D E   D E   O B S T Á C U L O ----------- */
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
			C->SetVisibility(false, true);   // oculta
			Current.Add(C);
		}
	}
	for (auto Old : FadedComponents)
		if (Old.IsValid() && !Current.Contains(Old))
			Old->SetVisibility(true, true);  // revela

	FadedComponents = Current.Array();
}
