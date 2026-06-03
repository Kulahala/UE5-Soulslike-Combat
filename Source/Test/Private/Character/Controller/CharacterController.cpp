// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/Controller/CharacterController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Character/MyCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HUD/PauseMenuWidget.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Enemy/Enemy.h" 

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

	// 创建并缓存暂停菜单Widget
	if (IsLocalPlayerController() && PauseMenuClass)
	{
		PauseMenuWidget = CreateWidget<UPauseMenuWidget>(this, PauseMenuClass);
		if (PauseMenuWidget)
		{
			PauseMenuWidget->OnResumeDelegate.AddDynamic(this, &ACharacterController::OnResumeRequested);
		}
	}
}

void ACharacterController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(InputComponent))
	{
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ACharacterController::Input_Move);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Completed, this, &ACharacterController::Input_MoveEnd);  // [调试] 松开清零
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Canceled, this, &ACharacterController::Input_MoveEnd);  // [调试] 取消清零
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ACharacterController::Input_Look);

		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacterController::Input_Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacterController::Input_StopJumping);

		EnhancedInputComponent->BindAction(EquipAction, ETriggerEvent::Started, this, &ACharacterController::Input_Equip);
		EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Started, this, &ACharacterController::Input_AttackPressed);
		EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Completed, this, &ACharacterController::Input_AttackReleased);
		EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Canceled, this, &ACharacterController::Input_AttackReleased);

		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started, this, &ACharacterController::Input_SprintStart);
		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &ACharacterController::Input_SprintEnd);
		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Canceled, this, &ACharacterController::Input_SprintEnd);  // [调试] 防止 held 挂住

		EnhancedInputComponent->BindAction(WalkAction, ETriggerEvent::Started, this, &ACharacterController::Input_WalkStart);
		EnhancedInputComponent->BindAction(WalkAction, ETriggerEvent::Completed, this, &ACharacterController::Input_WalkEnd);
		EnhancedInputComponent->BindAction(WalkAction, ETriggerEvent::Canceled, this, &ACharacterController::Input_WalkEnd);  // [调试] 防止 held 挂住

		EnhancedInputComponent->BindAction(BlockAction, ETriggerEvent::Started, this, &ACharacterController::Input_BlockStart);
		EnhancedInputComponent->BindAction(BlockAction, ETriggerEvent::Completed, this, &ACharacterController::Input_BlockEnd);
		EnhancedInputComponent->BindAction(BlockAction, ETriggerEvent::Canceled, this, &ACharacterController::Input_BlockEnd);  // [调试] 防止 held 挂住

		EnhancedInputComponent->BindAction(LockOnAction, ETriggerEvent::Started, this, &ACharacterController::Input_LockOn);

		if (ParryAction)
		{
			EnhancedInputComponent->BindAction(ParryAction, ETriggerEvent::Started, this, &ACharacterController::Input_Parry);
		}

		if (DodgeAction)
		{
			EnhancedInputComponent->BindAction(DodgeAction, ETriggerEvent::Started, this, &ACharacterController::Input_Dodge);
		}

		if (UsePotionAction)
		{
			EnhancedInputComponent->BindAction(UsePotionAction, ETriggerEvent::Started, this, &ACharacterController::Input_UsePotion);
		}

		if (PauseAction)
		{
			EnhancedInputComponent->BindAction(PauseAction, ETriggerEvent::Started, this, &ACharacterController::Input_Pause);
		}
	}
}

void ACharacterController::Input_Move(const FInputActionValue& Value)
{
	FVector2D MovementVector = Value.Get<FVector2D>();
	DebugMoveInput = MovementVector;  // [调试] 先采样，不受 gameplay gate 限制

	AMyCharacter* MyCharacter = GetMyCharacter();
	if (!MyCharacter) return;

	// 有移动输入时启动噪音定时器
	if (MovementVector.Length() > 0.1f)
	{
		MyCharacter->StartMovementNoiseTimer();
	}

	EActionState State = MyCharacter->GetActionState();
	if (State != EActionState::EAS_UnOccupied
		&& State != EActionState::EAS_Exhausted
		&& State != EActionState::EAS_UsingPotion) return;
	if (MyCharacter->GetCharacterMovement()->IsFalling()) return;

	const FRotator Rotation = GetControlRotation();
	const FRotator YawRotation(0, Rotation.Yaw, 0);

	// 获取标准的虚幻坐标轴（X=前，Y=右）
	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	// Y 轴对应前后 (W/S/摇杆上下)，X 轴对应左右 (A/D/摇杆左右)
	MyCharacter->AddMovementInput(ForwardDirection, MovementVector.Y);
	MyCharacter->AddMovementInput(RightDirection, MovementVector.X);
}

void ACharacterController::Input_MoveEnd()
{
	DebugMoveInput = FVector2D::ZeroVector;

	// 松开移动键时停止噪音定时器
	if (AMyCharacter* MyCharacter = GetMyCharacter())
	{
		MyCharacter->StopMovementNoiseTimer();
	}
}

void ACharacterController::Input_Look(const FInputActionValue& Value)
{
	if (AMyCharacter* MyCharacter = GetMyCharacter())
	{
		if (MyCharacter->IsLockingOn()) return;
	}

	if (APawn* ControlledPawn = GetPawn())
	{
		FVector2D LookAxisVector = Value.Get<FVector2D>();

		ControlledPawn->AddControllerYawInput(LookAxisVector.X);
		ControlledPawn->AddControllerPitchInput(LookAxisVector.Y);
	}
}

void ACharacterController::Input_Jump()
{
	DebugJumpExpireTime = GetWorld()->GetTimeSeconds() + 0.15f;  // [调试]

	if (AMyCharacter* MyCharacter = GetMyCharacter())
	{
		if (MyCharacter->GetActionState() != EActionState::EAS_UnOccupied) return;
		MyCharacter->Jump();
	}
}

void ACharacterController::Input_StopJumping()
{
	if (AMyCharacter* MyCharacter = GetMyCharacter())
	{
		MyCharacter->StopJumping();
	}
}

void ACharacterController::Input_Equip()
{
	if (AMyCharacter* MyCharacter = GetMyCharacter())
	{
		MyCharacter->Equip();
	}
	DebugEquipExpireTime = GetWorld()->GetTimeSeconds() + 0.15f;  // [调试]
}

void ACharacterController::Input_AttackPressed()
{
	DebugAttackExpireTime = GetWorld()->GetTimeSeconds() + 0.15f;  // [调试]

	if (AMyCharacter* MyCharacter = GetMyCharacter())
	{
		if (MyCharacter->IsComboWindowOpen())
		{
			MyCharacter->SetComboInputReceived(true);
			return;
		}
		MyCharacter->OnAttackInputPressed();
	}
}

void ACharacterController::Input_AttackReleased()
{
	if (AMyCharacter* MyCharacter = GetMyCharacter())
	{
		MyCharacter->OnAttackInputReleased();
	}
}

void ACharacterController::Input_SprintStart()
{
	if (AMyCharacter* MyCharacter = GetMyCharacter())
	{
		MyCharacter->Sprint();
	}
	bDebugSprintHeld = true;  // [调试]
}

void ACharacterController::Input_SprintEnd()
{
	if (AMyCharacter* MyCharacter = GetMyCharacter())
	{
		MyCharacter->StopSprinting();
	}
	bDebugSprintHeld = false;  // [调试]
}

void ACharacterController::Input_WalkStart()
{
	if (AMyCharacter* MyCharacter = GetMyCharacter())
	{
		MyCharacter->Walk();
	}
	bDebugWalkHeld = true;  // [调试]
}

void ACharacterController::Input_WalkEnd()
{
	if (AMyCharacter* MyCharacter = GetMyCharacter())
	{
		MyCharacter->StopWalking();
	}
	bDebugWalkHeld = false;  // [调试]
}

void ACharacterController::Input_BlockStart()
{
	if (AMyCharacter* MyCharacter = GetMyCharacter())
	{
		MyCharacter->StartBlockInput();
	}
	bDebugBlockHeld = true;  // [调试]
}

void ACharacterController::Input_BlockEnd()
{
	if (AMyCharacter* MyCharacter = GetMyCharacter())
	{
		MyCharacter->ReleaseBlockInput();
	}
	bDebugBlockHeld = false;  // [调试]
}

void ACharacterController::Input_LockOn()
{
	DebugLockOnExpireTime = GetWorld()->GetTimeSeconds() + 0.15f;  // [调试]
	if (AMyCharacter* MyCharacter = GetMyCharacter())
	{
		MyCharacter->ToggleLockOn();
	}
}

void ACharacterController::Input_Parry()
{
	DebugParryExpireTime = GetWorld()->GetTimeSeconds() + 0.15f;  // [调试]
	if (AMyCharacter* MyCharacter = GetMyCharacter())
	{
		MyCharacter->Input_Parry();
	}
}

void ACharacterController::Input_Dodge()
{
	DebugDodgeExpireTime = GetWorld()->GetTimeSeconds() + 0.15f;
	if (AMyCharacter* MyCharacter = GetMyCharacter())
	{
		MyCharacter->Dodge();
	}
}

void ACharacterController::Input_UsePotion()
{
	DebugPotionExpireTime = GetWorld()->GetTimeSeconds() + 0.15f;
	if (AMyCharacter* MyCharacter = GetMyCharacter())
	{
		MyCharacter->UsePotion();
	}
}

void ACharacterController::Input_Pause()
{
	if (bCanPause && PauseMenuWidget)
	{
		TogglePause();
	}
}

void ACharacterController::TogglePause()
{
	bIsPaused = !bIsPaused;

	if (bIsPaused)
	{
		// 暂停时：智能锁定处理
		AMyCharacter* MyCharacter = GetMyCharacter();
		if (MyCharacter && MyCharacter->IsLockingOn())
		{
			AEnemy* LockedTarget = MyCharacter->GetLockedTarget();

			// 有效性检查：防止pendingKill目标崩溃
			if (!IsValid(LockedTarget))
			{
				MyCharacter->ClearLockOn();
			}
			else
			{
				// 检查旋转完成度
				FRotator CurrentRot = GetControlRotation();
				FVector ToTarget = LockedTarget->GetActorLocation() - MyCharacter->GetActorLocation();
				FRotator TargetRot = FRotationMatrix::MakeFromX(ToTarget).Rotator();
				TargetRot.Pitch = 0.f;

				float YawDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(CurrentRot.Yaw, TargetRot.Yaw));

				if (YawDelta >= 1.f)  // 旋转中，清除锁定
				{
					MyCharacter->ClearLockOn();
				}
				// 否则保持锁定
			}
		}

		// 暂停游戏
		UGameplayStatics::SetGamePaused(GetWorld(), true);

		// 切换到UI输入模式
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(PauseMenuWidget->TakeWidget());
		SetInputMode(InputMode);
		bShowMouseCursor = true;

		// 显示暂停菜单
		if (PauseMenuWidget)
		{
			PauseMenuWidget->AddToViewport(1);  // Z-Order高于HUD
		}
	}
	else
	{
		// 恢复游戏
		UGameplayStatics::SetGamePaused(GetWorld(), false);

		// 切换回游戏输入模式
		FInputModeGameOnly InputMode;
		SetInputMode(InputMode);
		bShowMouseCursor = false;

		// 隐藏暂停菜单
		if (PauseMenuWidget)
		{
			PauseMenuWidget->RemoveFromParent();
		}
	}
}

void ACharacterController::OnResumeRequested()
{
	if (bIsPaused)
	{
		TogglePause();
	}
}

void ACharacterController::ClearPauseIfActive()
{
	if (bIsPaused)
	{
		TogglePause();  // 统一走恢复路径
	}
}

AMyCharacter* ACharacterController::GetMyCharacter() const
{
	return Cast<AMyCharacter>(GetPawn());
}

FString ACharacterController::GetDebugInputText() const
{
	const float Now = GetWorld()->GetTimeSeconds();
	FString Result;

	if (bDebugSprintHeld) Result += TEXT("[Sprint] ");
	if (bDebugWalkHeld)   Result += TEXT("[Walk] ");
	if (bDebugBlockHeld)  Result += TEXT("[Block] ");

	if (Now < DebugAttackExpireTime) Result += TEXT("Attack ");
	if (Now < DebugJumpExpireTime)   Result += TEXT("Jump ");
	if (Now < DebugEquipExpireTime)  Result += TEXT("Equip ");
	if (Now < DebugLockOnExpireTime) Result += TEXT("LockOn ");
	if (Now < DebugParryExpireTime) Result += TEXT("Parry ");
	if (Now < DebugDodgeExpireTime) Result += TEXT("Dodge ");
	if (Now < DebugPotionExpireTime) Result += TEXT("Potion ");

	if (!DebugMoveInput.IsNearlyZero())
		Result += FString::Printf(TEXT("Move(%.1f, %.1f) "), DebugMoveInput.X, DebugMoveInput.Y);

	return Result;
}
