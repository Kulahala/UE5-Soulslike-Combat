// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/Controller/CharacterController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Character/MyCharacter.h"
#include "GameFramework/CharacterMovementComponent.h" 

void ACharacterController::BeginPlay()
{
	Super::BeginPlay();

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (DefaultMappingContext)
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

void ACharacterController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(InputComponent))
	{
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ACharacterController::Input_Move);
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ACharacterController::Input_Look);

		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacterController::Input_Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacterController::Input_StopJumping);

		EnhancedInputComponent->BindAction(EquipAction, ETriggerEvent::Started, this, &ACharacterController::Input_Equip);
		EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Started, this, &ACharacterController::Input_Attack);
		EnhancedInputComponent->BindAction(ArmAction, ETriggerEvent::Started, this, &ACharacterController::Input_Arm);

		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started, this, &ACharacterController::Input_SprintStart);
		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &ACharacterController::Input_SprintEnd);

		EnhancedInputComponent->BindAction(WalkAction, ETriggerEvent::Started, this, &ACharacterController::Input_WalkStart);
		EnhancedInputComponent->BindAction(WalkAction, ETriggerEvent::Completed, this, &ACharacterController::Input_WalkEnd);

		EnhancedInputComponent->BindAction(BlockAction, ETriggerEvent::Started, this, &ACharacterController::Input_BlockStart);
		EnhancedInputComponent->BindAction(BlockAction, ETriggerEvent::Completed, this, &ACharacterController::Input_BlockEnd);
	}
}

void ACharacterController::Input_Move(const FInputActionValue& Value)
{
	AMyCharacter* MyCharacter = Cast<AMyCharacter>(GetPawn());
	if (!MyCharacter) return;

	EActionState State = MyCharacter->GetActionState();
	if (State != EActionState::EAS_UnOccupied && State != EActionState::EAS_Exhausted) return;
	if (MyCharacter->GetCharacterMovement()->IsFalling()) return;

	FVector2D MovementVector = Value.Get<FVector2D>();

	const FRotator Rotation = GetControlRotation();
	const FRotator YawRotation(0, Rotation.Yaw, 0);

	// 获取标准的虚幻坐标轴（X=前，Y=右）
	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	// Y 轴对应前后 (W/S/摇杆上下)，X 轴对应左右 (A/D/摇杆左右)
	MyCharacter->AddMovementInput(ForwardDirection, MovementVector.Y); 
	MyCharacter->AddMovementInput(RightDirection, MovementVector.X);   
}

void ACharacterController::Input_Look(const FInputActionValue& Value)
{
	if (APawn* ControlledPawn = GetPawn())
	{
		FVector2D LookAxisVector = Value.Get<FVector2D>();

		ControlledPawn->AddControllerYawInput(LookAxisVector.X);
		ControlledPawn->AddControllerPitchInput(LookAxisVector.Y);
	}
}

void ACharacterController::Input_Jump()
{
	if (AMyCharacter* MyCharacter = Cast<AMyCharacter>(GetPawn()))
	{
		if (MyCharacter->GetActionState() != EActionState::EAS_UnOccupied) return;
		MyCharacter->Jump();
	}
}

void ACharacterController::Input_StopJumping()
{
	if (AMyCharacter* MyCharacter = Cast<AMyCharacter>(GetPawn()))
	{
		MyCharacter->StopJumping();
	}
}

void ACharacterController::Input_Equip()
{
	if (AMyCharacter* MyCharacter = Cast<AMyCharacter>(GetPawn()))
	{
		MyCharacter->Equip(); 
	}
}

void ACharacterController::Input_Attack()
{
	if (AMyCharacter* MyCharacter = Cast<AMyCharacter>(GetPawn()))
	{
		MyCharacter->Attack(); 
	}
}

void ACharacterController::Input_Arm()
{
	if (AMyCharacter* MyCharacter = Cast<AMyCharacter>(GetPawn()))
	{
		MyCharacter->ArmWeapon();
	}
}

void ACharacterController::Input_SprintStart()
{
	if (AMyCharacter* MyCharacter = Cast<AMyCharacter>(GetPawn()))
	{
		MyCharacter->Sprint();
	}
}

void ACharacterController::Input_SprintEnd()
{
	if (AMyCharacter* MyCharacter = Cast<AMyCharacter>(GetPawn()))
	{
		MyCharacter->StopSprinting();
	}
}

void ACharacterController::Input_WalkStart()
{
	if (AMyCharacter* MyCharacter = Cast<AMyCharacter>(GetPawn()))
	{
		MyCharacter->Walk();
	}
}

void ACharacterController::Input_WalkEnd()
{
	if (AMyCharacter* MyCharacter = Cast<AMyCharacter>(GetPawn()))
	{
		MyCharacter->StopWalking();
	}
}

void ACharacterController::Input_BlockStart()
{
	if (AMyCharacter* MyCharacter = Cast<AMyCharacter>(GetPawn()))
	{
		MyCharacter->StartBlockInput();
	}
}

void ACharacterController::Input_BlockEnd()
{
	if (AMyCharacter* MyCharacter = Cast<AMyCharacter>(GetPawn()))
	{
		MyCharacter->ReleaseBlockInput();
	}
}
