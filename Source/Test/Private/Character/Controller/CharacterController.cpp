// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/Controller/CharacterController.h"
#include "AIController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Character/MyCharacter.h"
#include "Combat/CombatProjectile.h"
#include "Components/CapsuleComponent.h"
#include "Engine/TargetPoint.h"
#include "NavigationSystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Game/SoulslikeGameInstance.h"
#include "Game/TestGameMode.h"
#include "Engine/World.h"
#include "HUD/BonfireMenuWidget.h"
#include "HUD/DeathOverlayWidget.h"
#include "HUD/InteractionPromptWidget.h"
#include "HUD/PauseMenuWidget.h"
#include "Blueprint/UserWidget.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet/GameplayStatics.h"
#include "Settings/TestGameUserSettings.h"
#include "Enemy/Enemy.h" 
#include "Items/ItemOwnershipComponent.h"
#include "World/CheckpointActor.h"

namespace
{
	constexpr float ProjectileDebugSpawnClearance = 30.f;
	constexpr float ProjectileDebugSelfSourceDistance = 600.f;
	constexpr float ProjectileDebugSelfSpeed = 600.f;
	constexpr float EnemyRangedProbeSpawnDistance = 850.f;
	constexpr float EnemyRangedProbeSpawnSearchExtent = 300.f;
	constexpr float EnemyRangedProbeLifetimeBuffer = 5.f;

	ATargetPoint* SpawnProjectileDebugSource(UWorld* World, const FVector& Location, const FRotator& Rotation,
		const FName TeamTag)
	{
		if (!World)
		{
			return nullptr;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ATargetPoint* Source = World->SpawnActor<ATargetPoint>(ATargetPoint::StaticClass(), Location, Rotation,
			SpawnParameters);
		if (!Source)
		{
			UE_LOG(LogTemp, Warning, TEXT("ProjectileDebugFireSelf failed to create a temporary source."));
			return nullptr;
		}

		Source->Tags.AddUnique(TeamTag);
		Source->SetActorEnableCollision(false);
		Source->SetLifeSpan(4.f);
		return Source;
	}

	FString BuildSavedItemOwnershipDebugSummary(const TArray<FTestItemInstanceRecord>& ItemRecords,
	                                           const TArray<FTestEquipmentSlotRecord>& SlotRecords,
	                                           const TArray<FTestAmmoContainerRecord>& LoadedAmmoContainers,
	                                           const TSet<FName>& ClaimedRewardIds)
	{
		FString Result = FString::Printf(TEXT("Saved item ownership: %d instance(s), %d equipped slot(s), %d loaded ammo container(s), %d claimed reward(s)."),
			ItemRecords.Num(), SlotRecords.Num(), LoadedAmmoContainers.Num(), ClaimedRewardIds.Num());

		for (const FTestItemInstanceRecord& ItemRecord : ItemRecords)
		{
			Result += FString::Printf(TEXT("\n  Item Instance=%s Definition=%s Quantity=%d Upgrade=%d"),
				*ItemRecord.InstanceId.ToString(), *ItemRecord.DefinitionId.ToString(),
				ItemRecord.Quantity, ItemRecord.UpgradeLevel);
		}

		for (const FTestEquipmentSlotRecord& SlotRecord : SlotRecords)
		{
			Result += FString::Printf(TEXT("\n  Slot=%s Instance=%s"),
			*SlotRecord.SlotId.ToString(), *SlotRecord.ItemInstanceId.ToString());
		}

		for (const FTestAmmoContainerRecord& ContainerRecord : LoadedAmmoContainers)
		{
			Result += FString::Printf(TEXT("\n  Loaded Ammo Definition=%s Quantity=%d"),
				*ContainerRecord.DefinitionId.ToString(), ContainerRecord.LoadedQuantity);
		}

		TArray<FString> SortedClaimedRewardIds;
		SortedClaimedRewardIds.Reserve(ClaimedRewardIds.Num());
		for (const FName ClaimedRewardId : ClaimedRewardIds)
		{
			SortedClaimedRewardIds.Add(ClaimedRewardId.ToString());
		}
		SortedClaimedRewardIds.Sort();
		for (const FString& ClaimedRewardId : SortedClaimedRewardIds)
		{
			Result += FString::Printf(TEXT("\n  Claimed Reward=%s"), *ClaimedRewardId);
		}

		return Result;
	}
}

void ACharacterController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalPlayerController() && FSlateApplication::IsInitialized())
	{
		ApplicationActivationChangedHandle = FSlateApplication::Get().OnApplicationActivationStateChanged()
			.AddUObject(this, &ACharacterController::HandleApplicationActivationChanged);
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (DefaultMappingContext)
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}

	RestoreGameplayInput();

	// 创建并缓存暂停菜单Widget
	if (IsLocalPlayerController() && PauseMenuClass)
	{
		PauseMenuWidget = CreateWidget<UPauseMenuWidget>(this, PauseMenuClass);
		if (PauseMenuWidget)
		{
			PauseMenuWidget->OnResumeDelegate.AddDynamic(this, &ACharacterController::OnResumeRequested);
			PauseMenuWidget->OnQuitDelegate.AddDynamic(this, &ACharacterController::OnQuitRequested);
		}
	}

	if (IsLocalPlayerController() && BonfireMenuClass)
	{
		BonfireMenuWidget = CreateWidget<UBonfireMenuWidget>(this, BonfireMenuClass);
		if (BonfireMenuWidget)
		{
			BonfireMenuWidget->OnRestRequested.AddDynamic(this, &ACharacterController::OnBonfireRestRequested);
			BonfireMenuWidget->OnLeaveRequested.AddDynamic(this, &ACharacterController::OnBonfireLeaveRequested);
			BonfireMenuWidget->OnEquipmentRequested.AddDynamic(this, &ACharacterController::OnBonfireEquipmentRequested);
			BonfireMenuWidget->OnLoadoutSelectionRequested.AddDynamic(this, &ACharacterController::OnBonfireLoadoutSelectionRequested);
		}
	}

	if (IsLocalPlayerController() && InteractionPromptClass)
	{
		InteractionPromptWidget = CreateWidget<UInteractionPromptWidget>(this, InteractionPromptClass);
	}

	if (IsLocalPlayerController() && DeathOverlayClass)
	{
		DeathOverlayWidget = CreateWidget<UDeathOverlayWidget>(this, DeathOverlayClass);
	}
}

void ACharacterController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ApplicationActivationChangedHandle.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().OnApplicationActivationStateChanged().Remove(ApplicationActivationChangedHandle);
		ApplicationActivationChangedHandle.Reset();
	}

	Super::EndPlay(EndPlayReason);
}

void ACharacterController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(InputComponent))
	{
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ACharacterController::Input_Move);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Completed, this, &ACharacterController::Input_MoveEnd);  // 松开清零移动输入缓存
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Canceled, this, &ACharacterController::Input_MoveEnd);  // 取消清零移动输入缓存
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ACharacterController::Input_Look);

		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacterController::Input_Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacterController::Input_StopJumping);

		EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &ACharacterController::Input_Interact);
		EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Started, this, &ACharacterController::Input_AttackPressed);
		EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Completed, this, &ACharacterController::Input_AttackReleased);
		EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Canceled, this, &ACharacterController::Input_AttackCanceled);

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
		if (LockTargetSwitchAction)
		{
			EnhancedInputComponent->BindAction(LockTargetSwitchAction, ETriggerEvent::Triggered, this, &ACharacterController::Input_LockTargetSwitch);
			EnhancedInputComponent->BindAction(LockTargetSwitchAction, ETriggerEvent::Completed, this, &ACharacterController::Input_LockTargetSwitchEnd);
			EnhancedInputComponent->BindAction(LockTargetSwitchAction, ETriggerEvent::Canceled, this, &ACharacterController::Input_LockTargetSwitchEnd);
		}

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
	CachedMoveInput = MovementVector;  // 先采样，不受 gameplay gate 限制

	AMyCharacter* MyCharacter = GetMyCharacter();
	if (!MyCharacter) return;
	const EActionState State = MyCharacter->GetActionState();
	if (State == EActionState::EAS_GuardBroken)
	{
		MyCharacter->StopMovementNoiseTimer();
		return;
	}

	// 有移动输入时启动噪音定时器
	if (MovementVector.Length() > 0.1f)
	{
		MyCharacter->StartMovementNoiseTimer();
	}

	if (State != EActionState::EAS_UnOccupied
		&& State != EActionState::EAS_Exhausted
		&& State != EActionState::EAS_UsingPotion
		&& State != EActionState::EAS_Aiming) return;
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
	CachedMoveInput = FVector2D::ZeroVector;

	// 松开移动键时停止噪音定时器
	if (AMyCharacter* MyCharacter = GetMyCharacter())
	{
		MyCharacter->StopMovementNoiseTimer();
	}
}

void ACharacterController::Input_Look(const FInputActionValue& Value)
{
	const FVector2D LookAxisVector = Value.Get<FVector2D>();
	if (AMyCharacter* MyCharacter = GetMyCharacter())
	{
		if (!LookAxisVector.IsNearlyZero())
		{
			MyCharacter->StopCameraRecenter();
		}

		if (MyCharacter->IsLockingOn() && !MyCharacter->IsBowAiming()) return;
	}

	if (APawn* ControlledPawn = GetPawn())
	{
		FVector2D AdjustedLookAxisVector = LookAxisVector;
		if (const UTestGameUserSettings* UserSettings = UTestGameUserSettings::GetTestGameUserSettings())
		{
			AdjustedLookAxisVector *= UserSettings->GetLookSensitivity();
			if (UserSettings->GetInvertY())
			{
				AdjustedLookAxisVector.Y *= -1.f;
			}
		}

		ControlledPawn->AddControllerYawInput(AdjustedLookAxisVector.X);
		ControlledPawn->AddControllerPitchInput(AdjustedLookAxisVector.Y);
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

void ACharacterController::Input_Interact()
{
	if (AMyCharacter* MyCharacter = GetMyCharacter())
	{
		MyCharacter->TryInteract();
	}
	DebugInteractExpireTime = GetWorld()->GetTimeSeconds() + 0.15f;  // [调试]
}

void ACharacterController::Input_AttackPressed()
{
	bSuppressNextAttackReleaseAfterFocusLoss = false;
	DebugAttackExpireTime = GetWorld()->GetTimeSeconds() + 0.15f;  // [调试]

	if (AMyCharacter* MyCharacter = GetMyCharacter())
	{
		MyCharacter->OnAttackInputPressed();
	}
}

void ACharacterController::Input_AttackReleased()
{
	if (bSuppressNextAttackReleaseAfterFocusLoss)
	{
		bSuppressNextAttackReleaseAfterFocusLoss = false;
		return;
	}

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

void ACharacterController::Input_LockTargetSwitch(const FInputActionValue& Value)
{
	if (bIsPaused || bBonfireMenuOpen)
	{
		Input_LockTargetSwitchEnd();
		return;
	}

	const float InputValue = Value.Get<float>();
	const float AbsoluteInputValue = FMath::Abs(InputValue);
	if (AbsoluteInputValue <= LockTargetSwitchRearmThreshold)
	{
		bLockTargetSwitchInputArmed = true;
		return;
	}

	if (!bLockTargetSwitchInputArmed || AbsoluteInputValue < LockTargetSwitchInputThreshold)
	{
		return;
	}

	bLockTargetSwitchInputArmed = false;
	UWorld* World = GetWorld();
	if (!World || World->GetTimeSeconds() < NextLockTargetSwitchTime)
	{
		return;
	}

	if (AMyCharacter* MyCharacter = GetMyCharacter())
	{
		// UE Mouse Wheel Axis 正值为上滚，对应当前锁定目标的屏幕左侧。
		if (MyCharacter->SwitchLockOnTarget(InputValue < 0.f))
		{
			NextLockTargetSwitchTime = World->GetTimeSeconds() + LockTargetSwitchCooldown;
		}
	}
}

void ACharacterController::Input_LockTargetSwitchEnd()
{
	bLockTargetSwitchInputArmed = true;
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
				// Bow Aim 允许刻意将准星转离保留的锁定目标，不能沿用普通锁定的归中未完成判定。
				if (!MyCharacter->IsBowAiming())
				{
					FRotator CurrentRot = GetControlRotation();
					FVector ToTarget = LockedTarget->GetActorLocation() - MyCharacter->GetActorLocation();
					FRotator TargetRot = FRotationMatrix::MakeFromX(ToTarget).Rotator();
					TargetRot.Pitch = 0.f;

					const float YawDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(CurrentRot.Yaw, TargetRot.Yaw));

					if (YawDelta >= 1.f)  // 旋转中，清除锁定
					{
						MyCharacter->ClearLockOn();
					}
				}
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

		RestoreGameplayInput();

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

void ACharacterController::Input_AttackCanceled()
{
	if (AMyCharacter* MyCharacter = GetMyCharacter())
	{
		MyCharacter->OnAttackInputCanceled();
	}
}

void ACharacterController::HandleApplicationActivationChanged(bool bIsActive)
{
	if (bIsActive || !IsLocalPlayerController())
	{
		return;
	}

	// Windows Alt+Tab 会清空 Slate 的按键状态，但无 Trigger 的 Digital Action 仍可能随后派发 Completed。
	bSuppressNextAttackReleaseAfterFocusLoss = true;
	Input_AttackCanceled();
	Input_BlockEnd();
	Input_SprintEnd();
	Input_WalkEnd();
	Input_LockTargetSwitchEnd();
	Input_MoveEnd();
	Input_StopJumping();
	UE_LOG(LogTemp, Display, TEXT("%s: cleared held gameplay input after application focus loss."), *GetName());
}

void ACharacterController::OnQuitRequested()
{
	if (ATestGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ATestGameMode>() : nullptr)
	{
		GameMode->RequestReturnToMainMenu(this);
		return;
	}

	if (USoulslikeGameInstance* GameInstance = GetGameInstance<USoulslikeGameInstance>())
	{
		GameInstance->ReturnToMainMenu();
	}
}

void ACharacterController::ClearPauseIfActive()
{
	if (bIsPaused)
	{
		TogglePause();  // 统一走恢复路径
	}
}

bool ACharacterController::OpenBonfireMenu(ACheckpointActor* Checkpoint)
{
	if (!IsLocalPlayerController() || bBonfireMenuOpen || !Checkpoint || !BonfireMenuWidget)
	{
		return false;
	}

	AMyCharacter* PlayerCharacter = GetMyCharacter();
	if (!PlayerCharacter)
	{
		return false;
	}

	ClearPauseIfActive();
	PlayerCharacter->SetBonfireServiceProtection(true);
	ActiveBonfireCheckpoint = Checkpoint;
	bBonfireMenuOpen = true;
	bCanPause = false;
	HideInteractionPrompt();
	BonfireMenuWidget->ShowServicePage();

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(BonfireMenuWidget->TakeWidget());
	SetInputMode(InputMode);
	bShowMouseCursor = true;
	BonfireMenuWidget->AddToViewport(2);
	BonfireMenuWidget->SetKeyboardFocus();
	return true;
}

void ACharacterController::CloseBonfireMenu()
{
	DismissBonfireMenu(true);
}

void ACharacterController::DismissBonfireMenu(bool bRestoreInput)
{
	if (!bBonfireMenuOpen)
	{
		return;
	}

	bBonfireMenuOpen = false;
	ActiveBonfireCheckpoint.Reset();

	if (BonfireMenuWidget)
	{
		BonfireMenuWidget->RemoveFromParent();
	}

	AMyCharacter* PlayerCharacter = GetMyCharacter();
	if (PlayerCharacter)
	{
		PlayerCharacter->SetBonfireServiceProtection(false);
	}

	if (!bRestoreInput)
	{
		return;
	}

	bCanPause = true;
	RestoreGameplayInput();
	if (PlayerCharacter)
	{
		PlayerCharacter->RefreshInteractionPrompt();
	}
}

void ACharacterController::OnBonfireRestRequested()
{
	ACheckpointActor* Checkpoint = ActiveBonfireCheckpoint.Get();
	AMyCharacter* PlayerCharacter = GetMyCharacter();
	ATestGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ATestGameMode>() : nullptr;
	if (Checkpoint && PlayerCharacter && GameMode)
	{
		GameMode->RequestRestAtCheckpoint(Checkpoint, PlayerCharacter);
	}
}

void ACharacterController::OnBonfireLeaveRequested()
{
	CloseBonfireMenu();
}

void ACharacterController::OnBonfireEquipmentRequested()
{
	if (!bBonfireMenuOpen || !BonfireMenuWidget || !GetMyCharacter())
	{
		return;
	}

	RefreshBonfireLoadoutOptions();
	BonfireMenuWidget->ShowLoadoutPage();
}

void ACharacterController::OnBonfireLoadoutSelectionRequested(EItemEquipmentSlot EquipmentSlot, FName InstanceId)
{
	if (!bBonfireMenuOpen)
	{
		return;
	}

	AMyCharacter* PlayerCharacter = GetMyCharacter();
	if (!PlayerCharacter)
	{
		return;
	}

	PlayerCharacter->TryApplyBonfireLoadoutSelection(EquipmentSlot, InstanceId);
	RefreshBonfireLoadoutOptions();
}

void ACharacterController::RefreshBonfireLoadoutOptions()
{
	if (!BonfireMenuWidget)
	{
		return;
	}

	AMyCharacter* PlayerCharacter = GetMyCharacter();
	UItemOwnershipComponent* ItemOwnership = PlayerCharacter ? PlayerCharacter->GetItemOwnershipComponent() : nullptr;
	if (!ItemOwnership)
	{
		return;
	}

	TArray<FItemLoadoutOption> MainHandOptions;
	ItemOwnership->GetLoadoutOptions(EItemEquipmentSlot::MainHand, MainHandOptions);
	BonfireMenuWidget->SetLoadoutOptions(EItemEquipmentSlot::MainHand, MainHandOptions,
		ItemOwnership->GetEquippedInstanceId(EItemEquipmentSlot::MainHand));

	TArray<FItemLoadoutOption> OffHandOptions;
	ItemOwnership->GetLoadoutOptions(EItemEquipmentSlot::OffHand, OffHandOptions);
	BonfireMenuWidget->SetLoadoutOptions(EItemEquipmentSlot::OffHand, OffHandOptions,
		ItemOwnership->GetEquippedInstanceId(EItemEquipmentSlot::OffHand));
}

void ACharacterController::ShowInteractionPrompt(const FText& PromptText)
{
	if (!InteractionPromptWidget)
	{
		return;
	}

	InteractionPromptWidget->SetPromptText(PromptText);
	if (!InteractionPromptWidget->IsInViewport())
	{
		InteractionPromptWidget->AddToViewport(1);
	}
}

void ACharacterController::HideInteractionPrompt()
{
	if (InteractionPromptWidget)
	{
		InteractionPromptWidget->RemoveFromParent();
	}
}

void ACharacterController::ShowDeathOverlay()
{
	DismissBonfireMenu(false);
	RestoreGameplayInput();
	bCanPause = false;
	ClearPauseIfActive();
	HideInteractionPrompt();

	if (!DeathOverlayWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: DeathOverlayClass is not configured."), *GetName());
		return;
	}

	if (!DeathOverlayWidget->IsInViewport())
	{
		DeathOverlayWidget->AddToViewport(2);
	}
	DeathOverlayWidget->PlayDeathOverlayIn();
}

void ACharacterController::PrepareForMapTransition()
{
	DismissBonfireMenu(false);
	ClearPauseIfActive();
	HideInteractionPrompt();
	RestoreGameplayInput();

	if (DeathOverlayWidget && DeathOverlayWidget->IsInViewport())
	{
		DeathOverlayWidget->PlayDeathOverlayOut();
	}
}

AMyCharacter* ACharacterController::GetMyCharacter() const
{
	return Cast<AMyCharacter>(GetPawn());
}

void ACharacterController::ItemDebugGrant(FName DefinitionId)
{
#if UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Warning, TEXT("ItemDebugGrant is unavailable in Shipping builds."));
#else
	AMyCharacter* PlayerCharacter = GetMyCharacter();
	if (!PlayerCharacter)
	{
		UE_LOG(LogTemp, Warning, TEXT("ItemDebugGrant failed: no AMyCharacter is possessed."));
		return;
	}

	FName InstanceId = NAME_None;
	if (PlayerCharacter->TryGrantOwnedItem(DefinitionId, InstanceId))
	{
		UE_LOG(LogTemp, Display, TEXT("ItemDebugGrant succeeded: DefinitionId='%s', InstanceId='%s'."),
			*DefinitionId.ToString(), *InstanceId.ToString());
	}
#endif
}

void ACharacterController::ItemDebugGrantQuantity(FName DefinitionId, int32 Quantity)
{
#if UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Warning, TEXT("ItemDebugGrantQuantity is unavailable in Shipping builds."));
#else
	AMyCharacter* PlayerCharacter = GetMyCharacter();
	if (!PlayerCharacter)
	{
		UE_LOG(LogTemp, Warning, TEXT("ItemDebugGrantQuantity failed: no AMyCharacter is possessed."));
		return;
	}

	FName InstanceId = NAME_None;
	if (PlayerCharacter->TryGrantOwnedItemQuantity(DefinitionId, Quantity, InstanceId))
	{
		UE_LOG(LogTemp, Display, TEXT("ItemDebugGrantQuantity succeeded: DefinitionId='%s', Quantity=%d, InstanceId='%s'."),
			*DefinitionId.ToString(), Quantity, *InstanceId.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ItemDebugGrantQuantity failed: DefinitionId='%s', Quantity=%d."),
			*DefinitionId.ToString(), Quantity);
	}
#endif
}

void ACharacterController::ItemDebugEquip(FName InstanceId)
{
#if UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Warning, TEXT("ItemDebugEquip is unavailable in Shipping builds."));
#else
	AMyCharacter* PlayerCharacter = GetMyCharacter();
	if (!PlayerCharacter)
	{
		UE_LOG(LogTemp, Warning, TEXT("ItemDebugEquip failed: no AMyCharacter is possessed."));
		return;
	}

	if (PlayerCharacter->TryEquipOwnedItem(InstanceId))
	{
		UE_LOG(LogTemp, Display, TEXT("ItemDebugEquip succeeded for InstanceId='%s'."), *InstanceId.ToString());
	}
#endif
}

void ACharacterController::ItemDebugDump()
{
#if UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Warning, TEXT("ItemDebugDump is unavailable in Shipping builds."));
#else
	if (const AMyCharacter* PlayerCharacter = GetMyCharacter())
	{
		UE_LOG(LogTemp, Display, TEXT("%s"), *PlayerCharacter->GetItemOwnershipDebugSummary());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ItemDebugDump: no AMyCharacter is possessed."));
	}

	USoulslikeGameInstance* GameInstance = GetGameInstance<USoulslikeGameInstance>();
	TArray<FTestItemInstanceRecord> SavedItemRecords;
	TArray<FTestEquipmentSlotRecord> SavedSlotRecords;
	TArray<FTestAmmoContainerRecord> SavedLoadedAmmoContainers;
	TSet<FName> SavedClaimedRewardIds;
	if (!GameInstance || !GameInstance->GetSavedItemOwnership(SavedItemRecords, SavedSlotRecords)
		|| !GameInstance->GetSavedLoadedAmmoContainers(SavedLoadedAmmoContainers)
		|| !GameInstance->GetSavedClaimedRewardIds(SavedClaimedRewardIds))
	{
		UE_LOG(LogTemp, Warning, TEXT("ItemDebugDump: no usable saved item ownership is available."));
		return;
	}

	UE_LOG(LogTemp, Display, TEXT("%s"),
		*BuildSavedItemOwnershipDebugSummary(SavedItemRecords, SavedSlotRecords, SavedLoadedAmmoContainers,
			SavedClaimedRewardIds));
#endif
}

void ACharacterController::ItemDebugFailNextClaimSave()
{
#if UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Warning, TEXT("ItemDebugFailNextClaimSave is unavailable in Shipping builds."));
#else
	USoulslikeGameInstance* GameInstance = GetGameInstance<USoulslikeGameInstance>();
	if (!GameInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("ItemDebugFailNextClaimSave failed: no SoulslikeGameInstance is available."));
		return;
	}

	if (GameInstance->ArmNextItemClaimSaveFailureForDebug())
	{
		UE_LOG(LogTemp, Display,
			TEXT("ItemDebugFailNextClaimSave armed: the next valid fixed-item claim will simulate a save failure."));
	}
#endif
}

void ACharacterController::BowDebugFailNextAmmoConsumeSave()
{
#if UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Warning, TEXT("BowDebugFailNextAmmoConsumeSave is unavailable in Shipping builds."));
#else
	USoulslikeGameInstance* GameInstance = GetGameInstance<USoulslikeGameInstance>();
	if (!GameInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("BowDebugFailNextAmmoConsumeSave failed: no SoulslikeGameInstance is available."));
		return;
	}

	if (GameInstance->ArmNextLoadedAmmoConsumeSaveFailureForDebug())
	{
		UE_LOG(LogTemp, Display,
			TEXT("BowDebugFailNextAmmoConsumeSave armed: the next valid loaded-arrow consumption will simulate a save failure."));
	}
#endif
}

void ACharacterController::BowDebugFailNextProjectilePrepare()
{
#if UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Warning, TEXT("BowDebugFailNextProjectilePrepare is unavailable in Shipping builds."));
#else
	AMyCharacter* MyCharacter = GetMyCharacter();
	if (!MyCharacter)
	{
		UE_LOG(LogTemp, Warning, TEXT("BowDebugFailNextProjectilePrepare failed: no possessed AMyCharacter is available."));
		return;
	}

	if (MyCharacter->ArmNextProjectilePrepareFailureForDebug())
	{
		UE_LOG(LogTemp, Display,
			TEXT("BowDebugFailNextProjectilePrepare armed: the next valid bow release will discard its prepared projectile before consuming ammo."));
	}
#endif
}

void ACharacterController::BowDebugFailNextAmmoRefillSave()
{
#if UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Warning, TEXT("BowDebugFailNextAmmoRefillSave is unavailable in Shipping builds."));
#else
	USoulslikeGameInstance* GameInstance = GetGameInstance<USoulslikeGameInstance>();
	if (!GameInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("BowDebugFailNextAmmoRefillSave failed: no SoulslikeGameInstance is available."));
		return;
	}

	if (GameInstance->ArmNextAmmoRefillSaveFailureForDebug())
	{
		UE_LOG(LogTemp, Display,
			TEXT("BowDebugFailNextAmmoRefillSave armed: the next rest that transfers ammo will simulate a save failure."));
	}
#endif
}

void ACharacterController::ItemDebugVerifyAmmoRefillFixture(FName DefinitionId)
{
#if UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Warning, TEXT("ItemDebugVerifyAmmoRefillFixture is unavailable in Shipping builds."));
#else
	AMyCharacter* PlayerCharacter = GetMyCharacter();
	if (!PlayerCharacter)
	{
		UE_LOG(LogTemp, Warning, TEXT("ItemDebugVerifyAmmoRefillFixture failed: no AMyCharacter is possessed."));
		return;
	}

	if (!PlayerCharacter->VerifyAmmoRefillFixture(DefinitionId))
	{
		UE_LOG(LogTemp, Warning, TEXT("ItemDebugVerifyAmmoRefillFixture failed for DefinitionId='%s'."),
			*DefinitionId.ToString());
	}
#endif
}

void ACharacterController::ProjectileDebugFire()
{
#if UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Warning, TEXT("ProjectileDebugFire is unavailable in Shipping builds."));
#else
	AMyCharacter* PlayerCharacter = GetMyCharacter();
	UWorld* World = GetWorld();
	if (!PlayerCharacter || !World)
	{
		UE_LOG(LogTemp, Warning, TEXT("ProjectileDebugFire failed: no possessed AMyCharacter or World is available."));
		return;
	}

	const FVector LaunchDirection = GetControlRotation().Vector().GetSafeNormal();
	if (LaunchDirection.IsNearlyZero())
	{
		UE_LOG(LogTemp, Warning, TEXT("ProjectileDebugFire failed: player view direction is zero."));
		return;
	}

	const UCapsuleComponent* Capsule = PlayerCharacter->GetCapsuleComponent();
	const float CapsuleHalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 96.f;
	FProjectileLaunchParams LaunchParams;
	LaunchParams.Attacker = PlayerCharacter;
	LaunchParams.EventInstigator = this;
	// 不能从第三人称相机世界位置直接生成：镜头仰角接近 90 度时可能落入玩家胶囊。
	LaunchParams.SpawnLocation = PlayerCharacter->GetActorLocation()
		+ LaunchDirection * (CapsuleHalfHeight + ProjectileDebugSpawnClearance);
	LaunchParams.LaunchDirection = LaunchDirection;

	if (ACombatProjectile* Projectile = ACombatProjectile::SpawnConfiguredProjectile(World,
		ACombatProjectile::StaticClass(), LaunchParams))
	{
		UE_LOG(LogTemp, Display, TEXT("ProjectileDebugFire launched '%s' from the player view."), *Projectile->GetName());
	}
#endif
}

void ACharacterController::ProjectileDebugFireSelf(FName SourceTeam)
{
#if UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Warning, TEXT("ProjectileDebugFireSelf is unavailable in Shipping builds."));
#else
	AMyCharacter* PlayerCharacter = GetMyCharacter();
	UWorld* World = GetWorld();
	if (!PlayerCharacter || !World)
	{
		UE_LOG(LogTemp, Warning, TEXT("ProjectileDebugFireSelf failed: no possessed AMyCharacter or World is available."));
		return;
	}

	const FString SourceTeamText = SourceTeam.ToString();
	FName TeamTag;
	if (SourceTeamText.Equals(TEXT("Enemy"), ESearchCase::IgnoreCase))
	{
		TeamTag = FName(TEXT("Enemy"));
	}
	else if (SourceTeamText.Equals(TEXT("Player"), ESearchCase::IgnoreCase))
	{
		TeamTag = FName(TEXT("Player"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ProjectileDebugFireSelf expects Enemy or Player, received '%s'."),
			*SourceTeam.ToString());
		return;
	}

	const UCapsuleComponent* Capsule = PlayerCharacter->GetCapsuleComponent();
	const float CapsuleHalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 96.f;
	FVector PlayerForward = PlayerCharacter->GetActorForwardVector().GetSafeNormal2D();
	if (PlayerForward.IsNearlyZero())
	{
		PlayerForward = GetControlRotation().Vector().GetSafeNormal2D();
	}
	if (PlayerForward.IsNearlyZero())
	{
		UE_LOG(LogTemp, Warning, TEXT("ProjectileDebugFireSelf failed: player forward direction is zero."));
		return;
	}

	const FVector TargetLocation = PlayerCharacter->GetActorLocation() + FVector(0.f, 0.f, CapsuleHalfHeight * 0.5f);
	const FVector SourceLocation = TargetLocation + PlayerForward * ProjectileDebugSelfSourceDistance;
	const FVector LaunchDirection = (TargetLocation - SourceLocation).GetSafeNormal();
	ATargetPoint* Source = SpawnProjectileDebugSource(World, SourceLocation, LaunchDirection.Rotation(), TeamTag);
	if (!Source)
	{
		return;
	}

	FProjectileLaunchParams LaunchParams;
	LaunchParams.Attacker = Source;
	LaunchParams.SpawnLocation = SourceLocation;
	LaunchParams.LaunchDirection = LaunchDirection;
	LaunchParams.bOverrideDeliveryConfig = true;
	LaunchParams.DeliveryConfigOverride.InitialSpeed = ProjectileDebugSelfSpeed;
	LaunchParams.DeliveryConfigOverride.MaxSpeed = ProjectileDebugSelfSpeed;

	if (ACombatProjectile* Projectile = ACombatProjectile::SpawnConfiguredProjectile(World,
		ACombatProjectile::StaticClass(), LaunchParams))
	{
		UE_LOG(LogTemp, Display, TEXT("ProjectileDebugFireSelf launched '%s' with source team '%s'."),
			*Projectile->GetName(), *TeamTag.ToString());
	}
#endif
}

void ACharacterController::EnemyRangedDebugProbe(float ReleaseDelay)
{
#if UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Warning, TEXT("EnemyRangedDebugProbe is unavailable in Shipping builds."));
#else
	AMyCharacter* PlayerCharacter = GetMyCharacter();
	UWorld* World = GetWorld();
	if (!PlayerCharacter || !World)
	{
		UE_LOG(LogTemp, Warning, TEXT("EnemyRangedDebugProbe failed: no possessed AMyCharacter or World is available."));
		return;
	}

	if (ReleaseDelay < 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("EnemyRangedDebugProbe failed: ReleaseDelay must be >= 0."));
		return;
	}

	FVector PlayerForward = PlayerCharacter->GetActorForwardVector().GetSafeNormal2D();
	if (PlayerForward.IsNearlyZero())
	{
		PlayerForward = GetControlRotation().Vector().GetSafeNormal2D();
	}
	if (PlayerForward.IsNearlyZero())
	{
		UE_LOG(LogTemp, Warning, TEXT("EnemyRangedDebugProbe failed: player forward direction is zero."));
		return;
	}

	const FVector DesiredSpawnLocation = PlayerCharacter->GetActorLocation()
		+ PlayerForward * EnemyRangedProbeSpawnDistance;
	UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	FNavLocation NavLocation;
	if (!NavigationSystem || !NavigationSystem->ProjectPointToNavigation(DesiredSpawnLocation, NavLocation,
		FVector(EnemyRangedProbeSpawnSearchExtent)))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("EnemyRangedDebugProbe failed: no NavMesh projection near %s. Run it in a navigable PIE area."),
			*DesiredSpawnLocation.ToString());
		return;
	}

	const FRotator SpawnRotation = (PlayerCharacter->GetActorLocation() - NavLocation.Location).Rotation();
	const FTransform ProbeTransform(SpawnRotation, NavLocation.Location);
	AEnemy* ProbeEnemy = World->SpawnActorDeferred<AEnemy>(AEnemy::StaticClass(), ProbeTransform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!ProbeEnemy)
	{
		UE_LOG(LogTemp, Warning, TEXT("EnemyRangedDebugProbe failed: could not spawn temporary AEnemy."));
		return;
	}
	ProbeEnemy->PrepareRangedDebugProbeSpawn();
	UGameplayStatics::FinishSpawningActor(ProbeEnemy, ProbeTransform);
	if (!IsValid(ProbeEnemy))
	{
		UE_LOG(LogTemp, Warning, TEXT("EnemyRangedDebugProbe failed: temporary AEnemy was invalid after FinishSpawning."));
		return;
	}

	FActorSpawnParameters ControllerSpawnParameters;
	ControllerSpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AAIController* ProbeController = World->SpawnActor<AAIController>(AAIController::StaticClass(),
		NavLocation.Location, SpawnRotation, ControllerSpawnParameters);
	if (!ProbeController)
	{
		UE_LOG(LogTemp, Warning, TEXT("EnemyRangedDebugProbe failed: could not spawn temporary AAIController."));
		ProbeEnemy->Destroy();
		return;
	}

	ProbeController->Possess(ProbeEnemy);
	if (ProbeEnemy->GetController() != ProbeController
		|| !ProbeEnemy->StartRangedDebugProbe(PlayerCharacter, ReleaseDelay, ProbeController))
	{
		UE_LOG(LogTemp, Warning, TEXT("EnemyRangedDebugProbe failed: temporary enemy could not be possessed or initialized."));
		ProbeController->Destroy();
		ProbeEnemy->Destroy();
		return;
	}

	ProbeEnemy->SetLifeSpan(ReleaseDelay + EnemyRangedProbeLifetimeBuffer);
	ProbeController->SetLifeSpan(ReleaseDelay + EnemyRangedProbeLifetimeBuffer);
	UE_LOG(LogTemp, Display, TEXT("EnemyRangedDebugProbe spawned '%s' at %s with ReleaseDelay %.2f."),
		*ProbeEnemy->GetName(), *NavLocation.Location.ToString(), ReleaseDelay);
#endif
}

void ACharacterController::RestoreGameplayInput()
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	bShowMouseCursor = false;

	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);
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
	if (Now < DebugInteractExpireTime)  Result += TEXT("Interact ");
	if (Now < DebugLockOnExpireTime) Result += TEXT("LockOn ");
	if (Now < DebugParryExpireTime) Result += TEXT("Parry ");
	if (Now < DebugDodgeExpireTime) Result += TEXT("Dodge ");
	if (Now < DebugPotionExpireTime) Result += TEXT("Potion ");

	// HUD 与 Dodge fallback 共用同一份缓存，避免维护两份移动输入状态。
	if (!CachedMoveInput.IsNearlyZero())
		Result += FString::Printf(TEXT("Move(%.1f, %.1f) "), CachedMoveInput.X, CachedMoveInput.Y);

	return Result;
}
