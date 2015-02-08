// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UE4ProgIntro.h"
#include "UE4ProgIntroCharacter.h"
#include "BatteryPickup.h"
#include "UE4ProgIntroGameMode.h"
#include "Kismet/GameplayStatics.h"

//////////////////////////////////////////////////////////////////////////
// AUE4ProgIntroCharacter

AUE4ProgIntroCharacter::AUE4ProgIntroCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

	// Set base power level
	PowerLevel = 2000.0f;
	// Set the dependence on the speed of the power level
	SpeedFactor = 0.75f;
	BaseSpeed = 10.0f;

	// Create our battery collection volume
	CollectionSphere = ObjectInitializer.CreateDefaultSubobject<USphereComponent>(this, TEXT("CollectionSphere"));
	CollectionSphere->AttachTo(RootComponent);
	CollectionSphere->SetSphereRadius(200.0f);

	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f); // ...at this rotation rate
	GetCharacterMovement()->JumpZVelocity = 600.f;
	GetCharacterMovement()->AirControl = 0.2f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = ObjectInitializer.CreateDefaultSubobject<USpringArmComponent>(this, TEXT("CameraBoom"));
	CameraBoom->AttachTo(RootComponent);
	CameraBoom->TargetArmLength = 300.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = ObjectInitializer.CreateDefaultSubobject<UCameraComponent>(this, TEXT("FollowCamera"));
	FollowCamera->AttachTo(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named MyCharacter (to avoid direct content references in C++)
}

//////////////////////////////////////////////////////////////////////////
// Input

void AUE4ProgIntroCharacter::SetupPlayerInputComponent(class UInputComponent* InputComponent)
{
	// Set up gameplay key bindings
	check(InputComponent);
	InputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	InputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	InputComponent->BindAction("CollectPickups", IE_Pressed, this, &AUE4ProgIntroCharacter::CollectBatteries);

	InputComponent->BindAxis("MoveForward", this, &AUE4ProgIntroCharacter::MoveForward);
	InputComponent->BindAxis("MoveRight", this, &AUE4ProgIntroCharacter::MoveRight);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	InputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	InputComponent->BindAxis("TurnRate", this, &AUE4ProgIntroCharacter::TurnAtRate);
	InputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	InputComponent->BindAxis("LookUpRate", this, &AUE4ProgIntroCharacter::LookUpAtRate);

	// handle touch devices
	InputComponent->BindTouch(IE_Pressed, this, &AUE4ProgIntroCharacter::TouchStarted);
	InputComponent->BindTouch(IE_Released, this, &AUE4ProgIntroCharacter::TouchStopped);
}


void AUE4ProgIntroCharacter::TouchStarted(ETouchIndex::Type FingerIndex, FVector Location)
{
	// jump, but only on the first touch
	if (FingerIndex == ETouchIndex::Touch1)
	{
		Jump();
	}
}

void AUE4ProgIntroCharacter::TouchStopped(ETouchIndex::Type FingerIndex, FVector Location)
{
	if (FingerIndex == ETouchIndex::Touch1)
	{
		StopJumping();
	}
}

void AUE4ProgIntroCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void AUE4ProgIntroCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

void AUE4ProgIntroCharacter::MoveForward(float Value)
{
	if ((Controller != NULL) && (Value != 0.0f))
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		AddMovementInput(Direction, Value);
	}
}

void AUE4ProgIntroCharacter::MoveRight(float Value)
{
	if ( (Controller != NULL) && (Value != 0.0f) )
	{
		// find out which way is right
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
	
		// get right vector 
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		// add movement in that direction
		AddMovementInput(Direction, Value);
	}
}

void AUE4ProgIntroCharacter::CollectBatteries()
{
	float BatteryPower = 0.0f;

	AUE4ProgIntroGameMode* MyGameMode = Cast < AUE4ProgIntroGameMode>(UGameplayStatics::GetGameMode(this));


	// if we are currently playing
	if (MyGameMode->GetCurrentState() == EUE4ProgIntroPlayerState::EPlaying)
	{
		// Get all overlapping Actors and store them in a CollectedActors array
		TArray<AActor*> CollectedActors;
		CollectionSphere->GetOverlappingActors(CollectedActors);

		// For each Actor collected
		for (int32 iCollected = 0; iCollected < CollectedActors.Num(); ++iCollected)
		{
			// Cast the collected Actor to ABatteryPickup
			ABatteryPickup* const TestBattery = Cast<ABatteryPickup>(CollectedActors[iCollected]);

			// If the cast is successful, and the battery is valid and active
			if (TestBattery && !TestBattery->IsPendingKill() && TestBattery->bIsActive) {
				// Store its battery power for adding to the character's power
				BatteryPower += TestBattery->PowerLevel;
				// Deactivate the battery
				TestBattery->bIsActive = false;
				// Call the battery's OnPickedUp function
				TestBattery->OnPickedUp();
			}
		}

		if (BatteryPower > 0.f) {
			PowerUp(BatteryPower);
		}
	}
}

void AUE4ProgIntroCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	GetCharacterMovement()->MaxWalkSpeed = SpeedFactor * PowerLevel + BaseSpeed;
}