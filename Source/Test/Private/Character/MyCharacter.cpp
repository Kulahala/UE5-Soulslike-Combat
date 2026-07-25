// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/MyCharacter.h"
#include "Combat/ComboDataAsset.h"
#include "Combat/AttackConfigDataAsset.h"
#include "Combat/CombatHitTypes.h"
#include "Combat/CombatProjectile.h"
#include "Combat/HitReactionConfigDataAsset.h"
#include "Character/Components/ObstructionRecoverySpringArmComponent.h"
#include "Character/Components/PlayerLockOnComponent.h"
#include "Character/Controller/CharacterController.h"
#include "Game/SoulslikeGameInstance.h"
#include "Game/TestGameMode.h"

#include "Camera/CameraComponent.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Items/Weapon/Weapon.h"
#include "Items/Bow/Bow.h"
#include "Items/Shield/Shield.h"
#include "Items/ItemDefinitionDataAsset.h"
#include "Items/ItemOwnershipComponent.h"
#include "Interfaces/InteractableInterface.h"
#include "NiagaraFunctionLibrary.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "HUD/PlayerHUDWidget.h"
#include "AttributeComponent/AttributeComponent.h"
#include "Enemy/Enemy.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "MotionWarpingComponent.h"
#include "Perception/AISense_Hearing.h"
#include "Components/PawnNoiseEmitterComponent.h"
#include "Utils/DebugDrawHelper.h"

namespace
{
	const FName PlayerAttackWarpTargetName(TEXT("AttackTarget"));
}

// ==================== 生命周期 ====================

AMyCharacter::AMyCharacter()
{
	// 移动设置
	GetCharacterMovement()->RotationRate = FRotator(0.f, 600.f, 0.f);

	// 受击后退
	BaseHitKnockbackDistance = 10.f;

	// 相机组件
	SpringArm = CreateDefaultSubobject<UObstructionRecoverySpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(GetRootComponent());
	SpringArm->TargetArmLength = 360.f;
	SpringArm->SocketOffset = FVector(0.f, 0.f, 90.f);
	SpringArm->bUsePawnControlRotation = true;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm);

	LockOnComponent = CreateDefaultSubobject<UPlayerLockOnComponent>(TEXT("LockOnComponent"));
	ItemOwnershipComponent = CreateDefaultSubobject<UItemOwnershipComponent>(TEXT("ItemOwnershipComponent"));

	MotionWarpingComponent = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarpingComponent"));
	NoiseEmitterComponent = CreateDefaultSubobject<UPawnNoiseEmitterComponent>(TEXT("NoiseEmitterComponent"));
}

void AMyCharacter::BeginPlay()
{
	Super::BeginPlay();
	Tags.Add(FName("Player"));

	if (GEngine && PIETargetMaxFPS > 0.f)
	{
		GEngine->SetMaxFPS(PIETargetMaxFPS);
	}

	// 校验玩家配置入口
	ensureMsgf(PlayerProfile, TEXT("PlayerProfile is not set on %s - player profile driven actions may fail"), *GetName());
	if (!PlayerProfile)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: PlayerProfile is not set. Assign it before using player action montages."), *GetName());
	}
	else
	{
		if (!PlayerProfile->GetAttackConfig())
		{
			UE_LOG(LogTemp, Warning, TEXT("%s: PlayerProfile '%s' has no AttackConfig."),
			       *GetName(), *PlayerProfile->GetName());
		}

		if (!PlayerProfile->GetActionConfig())
		{
			UE_LOG(LogTemp, Warning, TEXT("%s: PlayerProfile '%s' has no ActionConfig."),
			       *GetName(), *PlayerProfile->GetName());
		}

		if (!PlayerProfile->GetReactionConfig())
		{
			UE_LOG(LogTemp, Warning,
			       TEXT("%s: PlayerProfile '%s' has no ReactionConfig. Hit reactions and death montages will not play."),
			       *GetName(), *PlayerProfile->GetName());
		}
	}

	// 初始化缓存为当前实际值（Blueprint 可能已覆盖）
	CachedSocketOffset = SpringArm->SocketOffset;
	CachedTargetArmLength = SpringArm->TargetArmLength;
	CachedBaseCameraFOV = Camera ? Camera->FieldOfView : 90.f;
	if (UObstructionRecoverySpringArmComponent* RecoverySpringArm = Cast<UObstructionRecoverySpringArmComponent>(SpringArm))
	{
		RecoverySpringArm->ConfigureObstructionRecovery(
			CameraObstructionRecoveryInterpSpeed, CameraObstructionClearDelay);
	}
	ResetCameraObstructionRecovery();

	if (Attributes)
	{
		Attributes->OnExhausted.AddDynamic(this, &AMyCharacter::HandleExhausted);
		Attributes->EnableHealthRegen();
		Attributes->SetPotionCount(3);  // 初始3个药瓶
	}

	if (ItemOwnershipComponent)
	{
		ItemOwnershipComponent->OnLoadedAmmoQuantityChanged.AddDynamic(this, &AMyCharacter::HandleLoadedAmmoQuantityChanged);
	}

}

void AMyCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	InvalidateActiveAttackMontagePlayback();
	bBonfireServiceProtected = false;
	bFailNextProjectilePrepareForDebug = false;
	ClearCombatPresence();
	StopCameraRecenter();
	ResetCameraObstructionRecovery();
	if (ItemOwnershipComponent)
	{
		ItemOwnershipComponent->OnLoadedAmmoQuantityChanged.RemoveDynamic(this, &AMyCharacter::HandleLoadedAmmoQuantityChanged);
	}
	CancelBowAim(true, true);
	DestroyMaterializedLoadout();

	if (PlayerHUDWidget)
	{
		PlayerHUDWidget->RemoveFromParent();
		PlayerHUDWidget = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void AMyCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (IsLocallyControlled())
	{
		InitializePlayerHUD();
	}
}

void AMyCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateBowChargeCameraFOV(DeltaTime);

	ACharacterController* CC = Cast<ACharacterController>(GetController());

	// 暂停时跳过所有gameplay逻辑
	if (CC && CC->IsPaused())
	{
		return;
	}

	// [调试] 输入状态，不受 Stunning/Dead 限制
	if (CC)
	{
		const FString InputText = CC->GetDebugInputText();
		if (!InputText.IsEmpty() && FDebugDrawHelper::IsPlayerEnabled())
		{
			FDebugDrawHelper::Add(FString::Printf(TEXT("Input: %s"), *InputText), FColor::White);
		}
	}

	// 先完成有效性清理、目标交接与旋转，避免相机在本帧读取到已失效的锁定目标。
	UpdateLockOn(DeltaTime);
	UpdateLockOnCamera(DeltaTime);

	// 相机归中在动作早退前检查；锁定、瞄准与硬中断由内部 guard 立即停止。
	if (bRecenteringCamera)
	{
		UpdateCameraRecenter(DeltaTime);
	}

	if (bPotionOnCooldown)
	{
		UpdatePotionCooldownHUD();
	}

	if (!InteractableCandidates.IsEmpty() || CurrentInteractable.IsValid())
	{
		RefreshCurrentInteractable();
	}

	RefreshCombatPresenceFromEnemyEngagement();

	if (ActionState == EActionState::EAS_Stunning
		|| ActionState == EActionState::EAS_GuardBroken
		|| ActionState == EActionState::EAS_Dead) return;

	if (bIsBlocking && ShouldInterruptBlock())
	{
		InterruptBlock(!EquippedShield || ActionState == EActionState::EAS_Exhausted || ActionState == EActionState::EAS_Dead);
	}
	TryResumeBlock();
	UpdateMovementSpeed();
	ApplyLockOnRotationMode();
	DrawDebugInfo();  // [调试] 角色状态面板，放在更新之后读取本帧最终状态
}

void AMyCharacter::MarkCombatPresenceFromConfirmedHostileHit()
{
	if (bBonfireServiceProtected || ActionState == EActionState::EAS_Dead
		|| !Attributes || !Attributes->IsAlive())
	{
		return;
	}

	MarkCombatPresence();
}

void AMyCharacter::MarkCombatPresence()
{
	if (const UWorld* World = GetWorld())
	{
		LastCombatPresenceTime = World->GetTimeSeconds();
	}
}

bool AMyCharacter::IsCombatPresenceActive() const
{
	if (LastCombatPresenceTime < 0.f || bBonfireServiceProtected || ActionState == EActionState::EAS_Dead
		|| !Attributes || !Attributes->IsAlive())
	{
		return false;
	}

	const UWorld* World = GetWorld();
	return World && World->GetTimeSeconds() - LastCombatPresenceTime <= CombatPresenceExitDelay;
}

void AMyCharacter::RefreshCombatPresenceFromEnemyEngagement()
{
	if (bBonfireServiceProtected || ActionState == EActionState::EAS_Dead || !Attributes || !Attributes->IsAlive())
	{
		ClearCombatPresence();
		return;
	}

	if (ATestGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ATestGameMode>() : nullptr;
		GameMode && GameMode->IsPlayerEngagedByEnemy(this))
	{
		MarkCombatPresence();
	}
}

void AMyCharacter::ClearCombatPresence()
{
	LastCombatPresenceTime = -1.f;
}

// ==================== 战斗 ====================

bool AMyCharacter::ShouldUseSprintAttack() const
{
	return bIsSprinting &&
	       !bIsBlocking &&
	       !IsBowEquipped() &&
	       GetLastMovementInputVector().SizeSquared2D() > KINDA_SMALL_NUMBER &&
	       WeaponState != EWeaponState::EWS_Unequipped &&
	       ActionState == EActionState::EAS_UnOccupied &&
	       !GetCharacterMovement()->IsFalling();
}

bool AMyCharacter::PerformSprintAttack()
{
	UAttackConfigDataAsset* AttackConfig = GetAttackConfig();
	if (!AttackConfig)
	{
		UE_LOG(LogTemp, Warning, TEXT("AttackConfig not configured"));
		return false;
	}

	const FSpecialAttackConfig* SprintConfig = AttackConfig->FindSpecialAttack(ESpecialAttackType::SprintAttack);
	if (!SprintConfig || !SprintConfig->Montage)
	{
		UE_LOG(LogTemp, Warning, TEXT("SprintAttack not configured in AttackConfig"));
		return false;
	}

	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: PerformSprintAttack failed, AnimInstance is not available."), *GetName());
		return false;
	}

	// 1. 清理旧连招状态
	ResetCombo();

	// 2. 扣除体力并暂停恢复（支持透支）
	if (Attributes)
	{
		Attributes->UseStamina(SprintConfig->StaminaCost);
		Attributes->PauseStaminaRegen();
	}

	// 3. 设置伤害倍率和韧性伤害
	SetAttackDamageMultiplier(SprintConfig->DamageMultiplier);
	if (EquippedWeapon)
	{
		CurrentPoiseDamage = EquippedWeapon->GetBasePoiseDamage() * SprintConfig->PoiseDamageMultiplier;
	}

	// 4. 对齐攻击方向
	FVector AttackDir = GetLastMovementInputVector().GetSafeNormal2D();
	if (AttackDir.IsNearlyZero())
	{
		AttackDir = GetActorForwardVector();
	}
	FaceDirection2D(AttackDir);

	// 5. 锁住旋转模式（防止 Tick 覆盖）
	GetCharacterMovement()->bOrientRotationToMovement = false;
	bUseControllerRotationYaw = false;

	// 6. 停止冲刺
	StopSprinting();

	UpdateAttackMotionWarpTarget(SprintConfig->MotionWarping);

	// 7. 播放攻击蒙太奇（关键：必须保留 Montage_Play + Montage_SetEndDelegate 模式！）
	// 原因：当前实现依赖手动绑定 delegate，不能简化为 PlayAnimMontage()
	if (AnimInstance->Montage_Play(SprintConfig->Montage) <= 0.f)
	{
		ClearAttackMotionWarpTarget();
		RestoreRotationMode();
		if (Attributes)
		{
			Attributes->ResumeStaminaRegen();
		}
		return false;
	}
	ActionState = EActionState::EAS_Attacking;
	BeginAttackMontagePlayback(AnimInstance, SprintConfig->Montage);

	// 冲刺攻击发出噪音
	EmitNoise(AttackNoiseLoudness, AttackNoiseRange);

	return true;
}

void AMyCharacter::CancelChargeInputState()
{
	bAttackInputHeld = false;
	bIsChargingAttack = false;
	GetWorldTimerManager().ClearTimer(ChargeDecisionTimer);
}

bool AMyCharacter::CanStartChargedAttack() const
{
	return !bIsBlocking && !IsBowEquipped() &&
	       ActionState == EActionState::EAS_UnOccupied &&
	       WeaponState != EWeaponState::EWS_Unequipped &&
	       !GetCharacterMovement()->IsFalling();
}

void AMyCharacter::OnAttackInputPressed()
{
	if (IsBowAiming())
	{
		bBowChargeInputHeld = true;
		switch (ResolvePlayerActionIntent(EPlayerActionIntent::AttackPress))
		{
		case EPlayerActionIntentResolution::BufferOnce:
			bBowChargeInputBuffered = true;
			UpdateAimReticleHUD();
			return;
		case EPlayerActionIntentResolution::StartNow:
			bBowChargeInputBuffered = false;
			StartBowCharge();
			return;
		case EPlayerActionIntentResolution::EndHeld:
		case EPlayerActionIntentResolution::Reject:
		default:
			UpdateAimReticleHUD();
			return;
		}
	}

	const EPlayerActionIntentResolution Resolution = ResolvePlayerActionIntent(EPlayerActionIntent::AttackPress);
	if (Resolution == EPlayerActionIntentResolution::BufferOnce)
	{
		bComboInputReceived = true;
		return;
	}
	if (Resolution != EPlayerActionIntentResolution::StartNow)
	{
		return;
	}

	if (ShouldUseSprintAttack())
	{
		TryStartAction(EPlayerActionType::Attack);
		return;
	}
	if (!CanStartChargedAttack())
	{
		TryStartAction(EPlayerActionType::Attack);
		return;
	}

	bAttackInputHeld = true;
	AttackInputPressTime = GetWorld()->GetTimeSeconds();
	GetWorldTimerManager().SetTimer(ChargeDecisionTimer, this, &AMyCharacter::EnterChargeMode, ChargeInputThreshold, false);
}

void AMyCharacter::OnAttackInputReleased()
{
	const bool bBowInputOwned = IsBowAiming() || bBowChargeInputHeld || bBowChargeInputBuffered || bBowDrawInputHeld;
	if (bBowInputOwned)
	{
		const EPlayerActionIntentResolution Resolution = ResolvePlayerActionIntent(EPlayerActionIntent::AttackRelease);
		const bool bWasDrawingBow = bBowDrawInputHeld;
		ClearBowChargeInputState();
		bBowDrawInputHeld = false;
		if (!IsBowAiming())
		{
			return;
		}

		if (!bWasDrawingBow)
		{
			return;
		}

		if (Resolution != EPlayerActionIntentResolution::StartNow)
		{
			CancelBowCharge(false);
			return;
		}

		if (!TryStartAction(EPlayerActionType::RangedRelease))
		{
			CancelBowCharge(false);
		}
		return;
	}

	if (ResolvePlayerActionIntent(EPlayerActionIntent::AttackRelease) != EPlayerActionIntentResolution::EndHeld)
	{
		return;
	}

	bAttackInputHeld = false;
	if (GetWorldTimerManager().IsTimerActive(ChargeDecisionTimer))
	{
		GetWorldTimerManager().ClearTimer(ChargeDecisionTimer);
		if (CanStartChargedAttack() || ShouldUseSprintAttack())
		{
			Attack(); // 阈值前松开，走普攻
		}
		return;
	}
	if (bIsChargingAttack)
	{
		PerformChargedRelease(); // 蓄力中松开，释放蓄力攻击
	}
}

void AMyCharacter::OnAttackInputCanceled()
{
	if (ResolvePlayerActionIntent(EPlayerActionIntent::AttackCancel) != EPlayerActionIntentResolution::EndHeld)
	{
		return;
	}

	if (IsBowAiming())
	{
		CancelBowCharge(false);
		return;
	}

	if (bIsChargingAttack)
	{
		bAttackInputHeld = false;
		UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
		UAttackConfigDataAsset* AttackConfig = GetAttackConfig();
		if (AnimInstance && AttackConfig && AttackConfig->ChargedAttack.Montage
			&& AnimInstance->Montage_IsPlaying(AttackConfig->ChargedAttack.Montage))
		{
			// 输入取消先失效当前 token，再主动恢复；迟到的 End Delegate 不能回写后续动作。
			InvalidateActiveAttackMontagePlayback();
			AnimInstance->Montage_Stop(0.15f, AttackConfig->ChargedAttack.Montage);
			CleanupInterruptedAttack();
			return;
		}

		CleanupInterruptedAttack();
		return;
	}

	CancelChargeInputState();
}

void AMyCharacter::EnterChargeMode()
{
	if (!bAttackInputHeld) return;
	if (!CanStartChargedAttack())
	{
		CancelChargeInputState();
		return;
	}

	UAttackConfigDataAsset* AttackConfig = GetAttackConfig();
	if (!AttackConfig || !AttackConfig->ChargedAttack.Montage)
	{
		CancelChargeInputState();
		Attack(); // 无配置或无蒙太奇，回退普通攻击
		return;
	}

	ResetCombo(); // 蓄力前清空普攻 combo 计数
	bIsChargingAttack = true;
	ActionState = EActionState::EAS_Attacking;
	if (Attributes)
	{
		Attributes->PauseStaminaRegen();
	}

	static const FName ChargeSectionName(TEXT("Default"));
	PlayMontageSection(AttackConfig->ChargedAttack.Montage, ChargeSectionName);

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && AnimInstance->Montage_IsPlaying(AttackConfig->ChargedAttack.Montage))
	{
		BeginAttackMontagePlayback(AnimInstance, AttackConfig->ChargedAttack.Montage);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("%s: ChargedAttack montage could not play."), *GetName());
	RecoverFromAttackMontageEnd();
}

void AMyCharacter::PerformChargedRelease()
{
	bIsChargingAttack = false;
	UAttackConfigDataAsset* AttackConfig = GetAttackConfig();
	if (!AttackConfig || !AttackConfig->ChargedAttack.Montage)
	{
		CancelChargeInputState();
		return;
	}

	const FChargedAttackConfig& Config = AttackConfig->ChargedAttack;

	float HeldTime = GetWorld()->GetTimeSeconds() - AttackInputPressTime;
	float ChargeAlpha = 0.0f;
	if (Config.MaxChargeHoldTime > Config.MinChargeHoldTime)
	{
		ChargeAlpha = FMath::Clamp((HeldTime - Config.MinChargeHoldTime) / (Config.MaxChargeHoldTime - Config.MinChargeHoldTime), 0.f, 1.f);
	}

	float FinalDamageMultiplier = FMath::Lerp(1.f, Config.MaxDamageMultiplier, ChargeAlpha);
	float FinalPoiseMultiplier = FMath::Lerp(1.f, Config.MaxPoiseDamageMultiplier, ChargeAlpha);

	SetAttackDamageMultiplier(FinalDamageMultiplier);
	if (EquippedWeapon)
	{
		CurrentPoiseDamage = EquippedWeapon->GetBasePoiseDamage() * FinalPoiseMultiplier;
	}

	if (Attributes)
	{
		Attributes->UseStamina(Config.StaminaCost);
	}

	static const FName ChargedReleaseSectionName(TEXT("Release"));
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance)
	{
		UpdateAttackMotionWarpTarget(Config.MotionWarping);
		AnimInstance->Montage_JumpToSection(ChargedReleaseSectionName, Config.Montage);
		EmitNoise(AttackNoiseLoudness, AttackNoiseRange);
	}
}

void AMyCharacter::Attack()
{
	const EPlayerActionIntentResolution Resolution = ResolvePlayerActionIntent(EPlayerActionIntent::AttackPress);
	if (Resolution == EPlayerActionIntentResolution::BufferOnce)
	{
		bComboInputReceived = true;
		return;
	}
	if (Resolution == EPlayerActionIntentResolution::StartNow)
	{
		TryStartAction(EPlayerActionType::Attack);
	}
}

bool AMyCharacter::StartAttackAction()
{
	if (IsBowEquipped())
	{
		return false;
	}

	if (ShouldUseSprintAttack())
	{
		return PerformSprintAttack();
	}

	Super::Attack();  // 保留

	if (!CanAttack()) return false;

	if (!StartComboSegment(ComboCounter, EComboPlaybackMode::NewPlayback))
	{
		ResetCombo();
		return false;
	}

	if (bIsBlocking)
	{
		InterruptBlock(false);
	}

	return true;
}

bool AMyCharacter::StartComboSegment(int32 SegmentIndex, EComboPlaybackMode PlaybackMode)
{
	UAttackConfigDataAsset* AttackConfig = GetAttackConfig();
	if (!AttackConfig || !AttackConfig->LightAttackCombo || !AttackConfig->LightAttackCombo->ComboMontage)
	{
		UE_LOG(LogTemp, Warning, TEXT("AttackConfig or LightAttackCombo not configured"));
		return false;
	}

	const FComboSegment* Segment = AttackConfig->LightAttackCombo->GetSegment(SegmentIndex);
	if (!Segment)
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid combo segment: %d"), SegmentIndex);
		return false;
	}

	UAnimMontage* MontageToPlay = (AttackConfig && AttackConfig->LightAttackCombo)
		? AttackConfig->LightAttackCombo->ComboMontage.Get()
		: nullptr;
	if (!MontageToPlay)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: StartComboSegment - no ComboMontage available"), *GetName());
		return false;
	}

	UAnimInstance* ContinuationAnimInstance = nullptr;
	if (PlaybackMode == EComboPlaybackMode::Continuation)
	{
		ContinuationAnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
		if (!ContinuationAnimInstance)
		{
			UE_LOG(LogTemp, Warning, TEXT("%s: StartComboSegment - no AnimInstance available"), *GetName());
			return false;
		}

		if (!ContinuationAnimInstance->Montage_IsPlaying(MontageToPlay))
		{
			UE_LOG(LogTemp, Warning, TEXT("%s: StartComboSegment - continuation montage is not playing"), *GetName());
			return false;
		}

		if (!RebindActiveAttackMontageEndDelegate(ContinuationAnimInstance, MontageToPlay))
		{
			UE_LOG(LogTemp, Warning, TEXT("%s: StartComboSegment - active attack playback is unavailable for continuation."), *GetName());
			return false;
		}
	}

	if (Attributes)
	{
		Attributes->UseStamina(Segment->StaminaCost);
		Attributes->PauseStaminaRegen();
	}

	SetAttackDamageMultiplier(Segment->DamageMultiplier);
	if (EquippedWeapon)
	{
		CurrentPoiseDamage = EquippedWeapon->GetBasePoiseDamage() * Segment->PoiseDamageMultiplier;
	}

	ActionState = EActionState::EAS_Attacking;
	UpdateAttackMotionWarpTarget(Segment->MotionWarping);

	if (PlaybackMode == EComboPlaybackMode::NewPlayback)
	{
		const uint32 PreviousPlaybackId = ActiveAttackPlaybackId;
		PlayAttackMontage(Segment->SectionName);
		if (ActiveAttackMontage != MontageToPlay || ActiveAttackPlaybackId == PreviousPlaybackId)
		{
			RecoverFromAttackMontageEnd();
			return false;
		}
	}
	else
	{
		ContinuationAnimInstance->Montage_JumpToSection(Segment->SectionName, MontageToPlay);
	}

	EmitNoise(AttackNoiseLoudness, AttackNoiseRange);
	bComboInputReceived = false;

	UE_LOG(LogTemp, Log, TEXT("Attack Segment %d: %s (Damage x%.1f)"),
	       SegmentIndex, *Segment->SectionName.ToString(), Segment->DamageMultiplier);

	return true;
}

void AMyCharacter::PlayAttackMontage(const FName& SectionName)
{
	UAttackConfigDataAsset* AttackConfig = GetAttackConfig();
	UAnimMontage* MontageToPlay = (AttackConfig && AttackConfig->LightAttackCombo)
		? AttackConfig->LightAttackCombo->ComboMontage.Get()
		: nullptr;
	if (!MontageToPlay)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: PlayAttackMontage - no ComboMontage available"), *GetName());
		return;
	}

	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInstance || AnimInstance->Montage_Play(MontageToPlay) <= 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: PlayAttackMontage failed to play '%s'."),
		       *GetName(), *GetNameSafe(MontageToPlay));
		return;
	}

	AnimInstance->Montage_JumpToSection(SectionName, MontageToPlay);
	BeginAttackMontagePlayback(AnimInstance, MontageToPlay);
}

void AMyCharacter::OpenComboWindow()
{
	bComboWindowOpen = true;
	UE_LOG(LogTemp, Log, TEXT("Combo window opened for segment %d"), ComboCounter);
}

void AMyCharacter::CloseComboWindow()
{
	bComboWindowOpen = false;
	UE_LOG(LogTemp, Log, TEXT("Combo window closed"));
}

void AMyCharacter::ResetCombo()
{
	ComboCounter = 0;
	bComboWindowOpen = false;
	bComboInputReceived = false;
	bActionCancelWindowOpen = false;
	SetAttackDamageMultiplier(1.0f);
	CurrentPoiseDamage = EquippedWeapon ? EquippedWeapon->GetBasePoiseDamage() : 1.f;
	ClearAttackMotionWarpTarget();

	UE_LOG(LogTemp, Log, TEXT("Combo reset"));
}

void AMyCharacter::UpdateAttackMotionWarpTarget(const FPlayerAttackMotionWarpingConfig& Config)
{
	/*
	 * 主角攻击 Motion Warping 契约：
	 * 只在锁定目标且距离合理时写入一次固定 AttackTarget，AnimNotifyState 只消费该目标。
	 * 不在窗口内持续追踪敌人，避免轻击/冲刺/蓄力释放变成远距离吸附。
	 */
	if (!Config.bUseMotionWarping)
	{
		ClearAttackMotionWarpTarget();
		return;
	}

	if (!MotionWarpingComponent || Config.MaxWarpDistance <= 0.f)
	{
		ClearAttackMotionWarpTarget();
		return;
	}

	AEnemy* LockedTarget = GetLockedTarget();
	if (!LockedTarget || !LockedTarget->GetAttributes() || !LockedTarget->GetAttributes()->IsAlive())
	{
		ClearAttackMotionWarpTarget();
		return;
	}

	const FVector PlayerLocation = GetActorLocation();
	const FVector TargetLocation = LockedTarget->GetActorLocation();
	const FVector ToTarget = (TargetLocation - PlayerLocation).GetSafeNormal2D();
	if (ToTarget.IsNearlyZero())
	{
		ClearAttackMotionWarpTarget();
		return;
	}

	const float StopDistance = FMath::Max(0.f, Config.WarpStopDistance);
	const float CurrentDistance = FVector::Dist2D(PlayerLocation, TargetLocation);
	if (CurrentDistance <= StopDistance)
	{
		ClearAttackMotionWarpTarget();
		return;
	}

	const FVector WarpLocation = TargetLocation - ToTarget * StopDistance;
	const float WarpDistance = FVector::Dist2D(PlayerLocation, WarpLocation);
	if (WarpDistance > Config.MaxWarpDistance)
	{
		ClearAttackMotionWarpTarget();
		return;
	}

	const FRotator WarpRotation(0.f, ToTarget.Rotation().Yaw, 0.f);
	MotionWarpingComponent->AddOrUpdateWarpTargetFromTransform(
		PlayerAttackWarpTargetName,
		FTransform(WarpRotation, WarpLocation));
}

void AMyCharacter::ClearAttackMotionWarpTarget()
{
	if (!MotionWarpingComponent)
	{
		return;
	}

	MotionWarpingComponent->RemoveWarpTarget(PlayerAttackWarpTargetName);
}

bool AMyCharacter::TryConsumeComboInputAtBranchPoint()
{
	bComboWindowOpen = false;

	if (!bComboInputReceived)
	{
		return false;
	}

	bComboInputReceived = false;

	if (ActionState != EActionState::EAS_Attacking)
	{
		return false;
	}


	UAttackConfigDataAsset* AttackConfig = GetAttackConfig();
	if (!AttackConfig || !AttackConfig->LightAttackCombo)
	{
		return false;
	}

	const int32 NextComboIndex = ComboCounter + 1;
	if (NextComboIndex >= AttackConfig->LightAttackCombo->GetComboCount())
	{
		return false;
	}

	if (ShouldRecoverToExhausted_Attack())
	{
		return false;
	}

	const int32 PreviousComboIndex = ComboCounter;
	ComboCounter = NextComboIndex;
	if (StartComboSegment(ComboCounter, EComboPlaybackMode::Continuation))
	{
		return true;
	}

	ComboCounter = PreviousComboIndex;
	return false;
}

void AMyCharacter::OpenActionCancelWindow()
{
	if (bActionCancelWindowOpen)
	{
		return;
	}

	bActionCancelWindowOpen = true;

	// 攻击开始会先退出举盾，正常情况下 bIsBlocking 与 EAS_Attacking 不并存。
	if (bBlockInputHeld)
	{
		TryResumeBlock();
	}
}

void AMyCharacter::CloseActionCancelWindow()
{
	if (!bActionCancelWindowOpen)
	{
		return;
	}

	bActionCancelWindowOpen = false;
}

void AMyCharacter::Jump()
{
	if (bIsBlocking) return;
	if (ActionState == EActionState::EAS_UsingPotion) return;
	if (CanJump() && ActionState != EActionState::EAS_Exhausted)
	{
		if (Attributes)
		{
			Attributes->UseStamina(10.f);
			Attributes->ResetStaminaRegenCooldown();
		}
		Super::Jump();
	}
}

void AMyCharacter::GetHit_Implementation(const FVector& ImpactPoint, AActor* HitInstigator)
{
	if (bDodgeInvulnerable) return;  // 翻滚无敌帧
	if (bBonfireServiceProtected)
	{
		bGuardBreakRequested = false;
		ResetPendingHitContext();
		return;
	}

	if (ActionState == EActionState::EAS_UsingPotion)
	{
		InterruptPotion();
	}
	// 受击是强制中断：不能因为物理右键仍按住而在硬直结束后自动重新瞄准。
	CancelBowAim(true);

	const bool bGuardBreakFromExhaustion = ActionState == EActionState::EAS_Exhausted;
	const bool bAlreadyGuardBroken = ActionState == EActionState::EAS_GuardBroken;
	if (bGuardBreakFromExhaustion || bAlreadyGuardBroken)
	{
		// Guard Break 保留本次伤害与后退，但不允许普通受击蒙太奇覆盖专用硬直。
		PendingHitContext.bSuppressNormalHitReact = true;
		PendingHitContext.bApplyStun = false;
		bGuardBreakRequested |= bGuardBreakFromExhaustion;
	}

	Super::GetHit_Implementation(ImpactPoint, HitInstigator);

	// 受击相机晃动
	if (HitReceivedCameraShake)
	{
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			PC->ClientStartCameraShake(HitReceivedCameraShake);
		}
	}

	if (Attributes->IsAlive() && bGuardBreakRequested)
	{
		StartGuardBreak();
	}
	else if (Attributes->IsAlive() && PendingHitContext.bApplyStun)
	{
		InvalidateActiveAttackMontagePlayback();
		ResetCombo();
		CloseActionCancelWindow();
		InterruptBlock(false);
		if (bIsParrying) InterruptParry();
		ActionState = EActionState::EAS_Stunning;
		StopCameraRecenter();
		ResetCameraObstructionRecovery();
		Attributes->ResumeStaminaRegen();  // 硬直接管，恢复体力暂停

		// 硬直时停止移动噪音
		StopMovementNoiseTimer();
	}

	bGuardBreakRequested = false;
	ResetPendingHitContext();  // 最末层清理
}

void AMyCharacter::Die()
{
	InvalidateActiveAttackMontagePlayback();
	bBonfireServiceProtected = false;
	bFailNextProjectilePrepareForDebug = false;
	ClearCombatPresence();

	// 先清理暂停状态（如果死亡时正在暂停）
	if (ACharacterController* CC = Cast<ACharacterController>(GetController()))
	{
		CC->ClearPauseIfActive();
		CC->SetCanPause(false);
	}

	ResetCombo();
	InterruptBlock(true);
	ClearParryState();
	InterruptPotion();  // 死亡时中断喝药
	CancelChargeInputState();
	CancelBowAim(true, true);
	bPendingExhaustedAfterAttack = false;
	bGuardBreakRequested = false;
	GetWorldTimerManager().ClearTimer(ExhaustionTimerHandle);
	ClearLockOn();
	StopCameraRecenter();
	ResetCameraObstructionRecovery();
	ActionState = EActionState::EAS_Dead;

	// 停止移动噪音定时器
	StopMovementNoiseTimer();

	// 停止移动
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();

	// 关闭碰撞
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	PlayDeathMontage();

	BeginDeathRespawnFlow();
}

void AMyCharacter::BeginDeathRespawnFlow()
{
	if (ATestGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ATestGameMode>() : nullptr)
	{
		GameMode->HandlePlayerDeath(this);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("%s died without an active ATestGameMode; automatic respawn is unavailable."), *GetName());
	}
}

void AMyCharacter::HandleExhausted()
{
	if (bGuardBreakRequested) return;
	if (ActionState == EActionState::EAS_UsingPotion) return;
	if (ActionState == EActionState::EAS_Attacking)
	{
		bPendingExhaustedAfterAttack = true;
		bIsSprinting = false;
		return;
	}

	ResetCombo();
	InterruptBlock(true);
	ActionState = EActionState::EAS_Exhausted;

	bIsSprinting = false;

	GetWorldTimerManager().SetTimer(ExhaustionTimerHandle, this,
		&AMyCharacter::RecoverFromExhaustion, ExhaustedTime, false);
}

void AMyCharacter::RecoverFromExhaustion()
{
	if (ActionState != EActionState::EAS_Exhausted) return;

	ActionState = EActionState::EAS_UnOccupied;
	// 体力可能因"最后一击"过扣为负，先重置门卫再加，避免 bStaminaJustDepleted 卡死
	Attributes->ResetExhaustionFlag();
	Attributes->AddStamina(1.f);
}

void AMyCharacter::StartGuardBreak()
{
	if (!Attributes || !Attributes->IsAlive())
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(ExhaustionTimerHandle);
	InvalidateActiveAttackMontagePlayback();
	bPendingExhaustedAfterAttack = false;
	ResetCombo();
	CloseActionCancelWindow();
	InterruptBlock(true);
	if (bIsParrying)
	{
		InterruptParry();
	}
	CancelChargeInputState();
	CancelBowAim(true);
	StopCameraRecenter();
	ResetCameraObstructionRecovery();
	bIsSprinting = false;
	StopMovementNoiseTimer();
	ActionState = EActionState::EAS_GuardBroken;

	UAnimMontage* GuardBreakMontage = GetGuardBreakMontage();
	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!GuardBreakMontage || !AnimInstance || AnimInstance->Montage_Play(GuardBreakMontage) <= 0.f)
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("%s: Guard Break montage is unavailable; falling back to Exhausted recovery."), *GetName());
		ActionState = EActionState::EAS_Exhausted;
		EnsureExhaustionRecoveryTimer();
		return;
	}

	BindMontageEndDelegate(AnimInstance, GuardBreakMontage, &AMyCharacter::OnGuardBreakMontageEnded);
}

void AMyCharacter::OnGuardBreakMontageEnded(UAnimMontage* /*Montage*/, bool /*bInterrupted*/)
{
	RecoverFromGuardBreak();
}

void AMyCharacter::RecoverFromGuardBreak()
{
	if (ActionState != EActionState::EAS_GuardBroken)
	{
		return;
	}

	ActionState = EActionState::EAS_UnOccupied;
	if (Attributes)
	{
		Attributes->ResetExhaustionFlag();
		Attributes->AddStamina(1.f);
	}

	if (GetLastMovementInputVector().SizeSquared2D() > KINDA_SMALL_NUMBER)
	{
		StartMovementNoiseTimer();
	}
}

bool AMyCharacter::IsExhaustionTimerActive() const
{
	return GetWorldTimerManager().IsTimerActive(ExhaustionTimerHandle);
}

float AMyCharacter::TakeDamage(float DamageAmount, const struct FDamageEvent& DamageEvent,
                                class AController* EventInstigator, AActor* DamageCauser)
{
	if (bDodgeInvulnerable || bBonfireServiceProtected) return 0.f;  // 翻滚与火堆服务无敌

	if (PlayerHUDWidget)
	{
		const float FlashScale = DamageAmount > 0.f ? LastDamageFlashScale : 1.f;
		PlayerHUDWidget->SetPendingDamageFlashScale(FlashScale);
	}

	LastDamageFlashScale = 1.f;  // 推送给 HUD 后立即归位，避免下一次掉血串味
	Attributes->ReceiveDamage(DamageAmount);
	if (!Attributes->IsAlive())
	{
		Die();
	}
	return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}

bool AMyCharacter::CanAttack() const
{
	return ActionState == EActionState::EAS_UnOccupied
		&& WeaponState != EWeaponState::EWS_Unequipped
		&& !IsBowEquipped()
		&& !GetCharacterMovement()->IsFalling();
}

// ==================== 防御 ====================

bool AMyCharacter::CanStartBlock() const
{
	return ActionState == EActionState::EAS_UnOccupied
		&& IsActionStartPreflightValid(EPlayerActionType::Block, false);
}

void AMyCharacter::StartBlockInput()
{
	if (ActionState == EActionState::EAS_GuardBroken) return;

	bBlockInputHeld = true;
	bIsSprinting = false;
	const EPlayerActionIntentResolution Resolution = ResolvePlayerActionIntent(EPlayerActionIntent::BlockPress);
	if (Resolution == EPlayerActionIntentResolution::StartNow)
	{
		const EPlayerActionType TargetAction = IsBowEquipped()
			? EPlayerActionType::RangedAim
			: EPlayerActionType::Block;
		if (TryStartAction(TargetAction))
		{
			return;
		}
	}

	// 仅攻击起手 / 活跃期保留 held 输入，供同一攻击的作者化 CancelWindow 触发一次 synthetic retry。
	if (!ShouldRetainBlockInputForAttackCancel())
	{
		bBlockInputHeld = false;
	}
	if (Resolution == EPlayerActionIntentResolution::StartNow)
	{
		return;
	}
}

void AMyCharacter::ReleaseBlockInput()
{
	bBlockInputHeld = false;
	if (ResolvePlayerActionIntent(EPlayerActionIntent::BlockRelease) != EPlayerActionIntentResolution::EndHeld)
	{
		return;
	}

	if (IsBowAiming())
	{
		// 正常收起瞄准后，实际未结束的 Release Montage 仍会阻止再次放箭。
		CancelBowAim(false);
		return;
	}
	InterruptBlock(false);
}

void AMyCharacter::InterruptBlock(bool bClearHeld)
{
	bIsBlocking = false;
	if (bClearHeld) bBlockInputHeld = false;
	if (Attributes)
	{
		Attributes->SetStaminaRegenMultiplier(1.f);
	}
	StopBlockMontage(0.1f);
}

void AMyCharacter::TryResumeBlock()
{
	if (!bBlockInputHeld)
	{
		return;
	}

	if (ResolvePlayerActionIntent(EPlayerActionIntent::BlockPress) == EPlayerActionIntentResolution::StartNow)
	{
		TryStartAction(IsBowEquipped()
			? EPlayerActionType::RangedAim
			: EPlayerActionType::Block);
	}
}

FBlockResult AMyCharacter::TryBlockHit(const FCombatHitRequest& Request)
{
	FBlockResult Result;
	Result.DamageAfterBlock = Request.IncomingDamage;
	if (IsBowEquipped())
	{
		// 双手 Bow 不能通过迟到的格挡/弹反状态复用副手防御。
		return Result;
	}

	const FVector ImpactPoint = Request.HitResult.ImpactPoint;
	AActor* Attacker = Request.Attacker;
	AActor* DamageCauser = Request.DamageCauser;

	// 弹反分支优先（弹反期间不可格挡）
	if (bIsParrying)
	{
		if (!EquippedShield || !Attributes || !Attributes->IsAlive())
			return Result;

		if (bParryActive)
		{
			// 弹反方向限制（必须面对敌人）
			AActor* DirSrc = Attacker ? Attacker : DamageCauser;
			if (DirSrc)
			{
				FVector ToAttacker = (DirSrc->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
				float Dot = CalcForwardDot2D(ToAttacker);
				float CosHalf = FMath::Cos(FMath::DegreesToRadians(EquippedShield->GetBlockHalfAngleDegrees()));
				if (Dot < CosHalf) return Result; // 角度不匹配，弹反失败
			}

			if (!Request.bCanBeParried)
			{
				return Result; // 该招式不可弹反，按失败弹反处理
			}

			// 弹反成功！完全免伤 + 攻击方韧性清空
			Result.bBlocked = true;
			Result.bParried = true;
			Result.DamageAfterBlock = 0.f;
			Result.bPlayNormalHitReact = false;
			if (EquippedShield->GetParrySound())
			{
				UGameplayStatics::PlaySoundAtLocation(this, EquippedShield->GetParrySound(), ImpactPoint);
			}
			if (EquippedShield->GetParryParticle())
			{
				UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, EquippedShield->GetParryParticle(), ImpactPoint);
			}
			return Result;
		}
		// 弹反起手/收招帧——裸吃伤害
		return Result;
	}

	if (!bIsBlocking || !EquippedShield || !Attributes || !Attributes->IsAlive())
		return Result;

	AActor* DirSrc = Attacker ? Attacker : DamageCauser;
	if (!DirSrc) return Result;

	FVector ToAttacker = (DirSrc->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	float Dot = CalcForwardDot2D(ToAttacker);
	float CosHalf = FMath::Cos(FMath::DegreesToRadians(EquippedShield->GetBlockHalfAngleDegrees()));
	if (Dot < CosHalf) return Result;

	const float StaminaCost = FMath::Max(0.f,
		EquippedShield->GetBlockStaminaCost() * Request.BlockStaminaDamageMultiplier);
	const float CurrentStamina = Attributes->GetCurrentStamina();
	if (StaminaCost > 0.f && CurrentStamina <= 0.f)
	{
		return Result;
	}

	const bool bWillGuardBreak = StaminaCost > 0.f && CurrentStamina <= StaminaCost;
	if (bWillGuardBreak)
	{
		// UseStamina() 同步广播 OnExhausted，必须先标记破防才能防止旧 Exhausted 计时器抢占状态。
		bGuardBreakRequested = true;
		Attributes->UseStamina(CurrentStamina);
	}
	else
	{
		Attributes->UseStamina(StaminaCost);
	}
	if (StaminaCost > 0.f)
	{
		// 成功格挡的耐力消耗与其他动作一致：命中后重新等待自然恢复。
		Attributes->ResetStaminaRegenCooldown();
	}

	Result.bBlocked = true;
	Result.DamageAfterBlock = Request.IncomingDamage * EquippedShield->GetBlockedDamageMultiplier();
	Result.bPlayNormalHitReact = false;
	LastDamageFlashScale = EquippedShield->GetBlockedDamageMultiplier();  // 染红按减伤率缩放

	// 格挡反馈
	if (EquippedShield->GetBlockSound())
	{
		UGameplayStatics::PlaySoundAtLocation(this, EquippedShield->GetBlockSound(), ImpactPoint);
	}
	if (EquippedShield->GetBlockParticle())
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, EquippedShield->GetBlockParticle(), ImpactPoint);
	}
	return Result;
}

// ==================== 弹反 ====================

bool AMyCharacter::CanStartParry() const
{
	return ActionState == EActionState::EAS_UnOccupied
		&& IsActionStartPreflightValid(EPlayerActionType::Parry, false);
}

void AMyCharacter::Input_Parry()
{
	if (ResolvePlayerActionIntent(EPlayerActionIntent::Parry) == EPlayerActionIntentResolution::StartNow)
	{
		TryStartAction(EPlayerActionType::Parry);
	}
}

void AMyCharacter::SetParryActive(bool bActive)
{
	bParryActive = !IsBowEquipped() && bActive;
}

void AMyCharacter::SetDodgeInvulnerable(bool bInvulnerable)
{
	bDodgeInvulnerable = bInvulnerable;
}

void AMyCharacter::StartParryCooldown()
{
	const float Cooldown = EquippedShield ? EquippedShield->GetParryCooldown() : 0.4f;
	if (Cooldown <= 0.f)
	{
		bParryOnCooldown = false;
		return;
	}
	bParryOnCooldown = true;
	GetWorldTimerManager().SetTimer(
		ParryCooldownTimer, this, &AMyCharacter::ResetParryCooldown, Cooldown, false);
}

void AMyCharacter::ResetParryCooldown()
{
	bParryOnCooldown = false;
}

void AMyCharacter::InterruptParry()
{
	bIsParrying = false;
	bParryActive = false;
	StartParryCooldown();
}

void AMyCharacter::ClearParryState()
{
	bIsParrying = false;
	bParryActive = false;
	bParryOnCooldown = false;
	GetWorldTimerManager().ClearTimer(ParryCooldownTimer);
}

void AMyCharacter::OnParryMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (bInterrupted) return;  // InterruptParry() 已处理清理
	if (ActionState != EActionState::EAS_Parrying) return;

	bIsParrying = false;
	bParryActive = false;
	StartParryCooldown();
	RecoverActionStateAfterMontage(EActionState::EAS_Parrying, false);
}

// ==================== 翻滚 ====================

bool AMyCharacter::CanDodge() const
{
	return ActionState == EActionState::EAS_UnOccupied
		&& IsActionStartPreflightValid(EPlayerActionType::Dodge, false);
}

void AMyCharacter::Dodge()
{
	if (ResolvePlayerActionIntent(EPlayerActionIntent::Dodge) == EPlayerActionIntentResolution::StartNow)
	{
		TryStartAction(EPlayerActionType::Dodge);
	}
}

FVector AMyCharacter::ComputeDodgeDirection() const
{
	FVector InputDir = GetLastMovementInputVector().GetSafeNormal2D();
	if (!InputDir.IsNearlyZero()) return InputDir;

	if (const ACharacterController* PC = Cast<ACharacterController>(GetController()))
	{
		const FVector2D Cached = PC->GetCachedMoveInput();
		if (!Cached.IsNearlyZero())
		{
			const FRotator YawRot(0.f, GetControlRotation().Yaw, 0.f);
			const FVector Forward = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
			const FVector Right = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);
			const FVector World = (Forward * Cached.Y + Right * Cached.X).GetSafeNormal2D();
			if (!World.IsNearlyZero()) return World;
		}
	}

	return FVector::ZeroVector;
}

FName AMyCharacter::SelectDodgeSection(const FVector& WorldDirection) const
{
	// 无移动输入：直接后跳（不转身）
	if (WorldDirection.IsNearlyZero())
	{
		return FName("Dodge_B");
	}

	// 非锁定 + 有输入：前滚（会转身面向输入方向）
	if (!IsLockingOn())
	{
		return FName("Dodge_F");
	}

	// 锁定 + 有输入：按角色本地朝向切成 8 个 45 度扇区
	const FVector LocalDir = GetActorRotation().UnrotateVector(WorldDirection).GetSafeNormal2D();
	const float AngleDegrees = FMath::RadiansToDegrees(FMath::Atan2(LocalDir.Y, LocalDir.X));

	if (AngleDegrees >= -22.5f && AngleDegrees < 22.5f)
	{
		return FName("Dodge_F");
	}
	if (AngleDegrees >= 22.5f && AngleDegrees < 67.5f)
	{
		return FName("Dodge_FR");
	}
	if (AngleDegrees >= 67.5f && AngleDegrees < 112.5f)
	{
		return FName("Dodge_R");
	}
	if (AngleDegrees >= 112.5f && AngleDegrees < 157.5f)
	{
		return FName("Dodge_BR");
	}
	if (AngleDegrees >= -67.5f && AngleDegrees < -22.5f)
	{
		return FName("Dodge_FL");
	}
	if (AngleDegrees >= -112.5f && AngleDegrees < -67.5f)
	{
		return FName("Dodge_L");
	}
	if (AngleDegrees >= -157.5f && AngleDegrees < -112.5f)
	{
		return FName("Dodge_BL");
	}

	return FName("Dodge_B");
}

void AMyCharacter::OnDodgeMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	bDodgeInvulnerable = false;
	RestoreRotationMode();

	if (bInterrupted) return;

	if (RecoverActionStateAfterMontage(EActionState::EAS_Dodging, true) == EActionState::EAS_Exhausted)
	{
		return;
	}

	// 翻滚结束后，如果仍在移动则重启定时器
	if (GetLastMovementInputVector().SizeSquared2D() > KINDA_SMALL_NUMBER)
	{
		StartMovementNoiseTimer();
	}
}

// ==================== 装备 ====================

void AMyCharacter::Equip()
{
	TryInteract();
}

void AMyCharacter::TryInteract()
{
	if (!CanInteractWithWorld())
	{
		return;
	}

	RefreshCurrentInteractable();
	AActor* InteractableActor = CurrentInteractable.Get();
	if (!InteractableActor || !InteractableActor->Implements<UInteractableInterface>())
	{
		return;
	}

	if (IInteractableInterface::Execute_CanInteract(InteractableActor, this))
	{
		IInteractableInterface::Execute_Interact(InteractableActor, this);
	}

	RefreshCurrentInteractable();
}

bool AMyCharacter::CanInteractWithWorld() const
{
	return !bBonfireServiceProtected && ActionState == EActionState::EAS_UnOccupied
		&& Attributes
		&& Attributes->IsAlive()
		&& !GetCharacterMovement()->IsFalling();
}

void AMyCharacter::SetBonfireServiceProtection(bool bEnabled)
{
	if (bBonfireServiceProtected == bEnabled)
	{
		return;
	}

	bBonfireServiceProtected = bEnabled;
	if (!bEnabled)
	{
		return;
	}

	ClearCombatPresence();
	InvalidateActiveAttackMontagePlayback();
	ResetCombo();
	CloseActionCancelWindow();
	CancelChargeInputState();
	CancelBowAim(true);
	bPendingExhaustedAfterAttack = false;
	bIsSprinting = false;
	bIsWalking = false;
	InterruptBlock(true);
	ClearParryState();
	ClearLockOn();
	StopCameraRecenter();
	ResetCameraObstructionRecovery();
	StopMovementNoiseTimer();
	GetCharacterMovement()->StopMovementImmediately();
}

void AMyCharacter::RegisterInteractable(AActor* InteractableActor)
{
	if (!IsValid(InteractableActor) || !InteractableActor->Implements<UInteractableInterface>())
	{
		return;
	}

	InteractableCandidates.AddUnique(InteractableActor);
	RefreshCurrentInteractable();
}

void AMyCharacter::UnregisterInteractable(AActor* InteractableActor)
{
	InteractableCandidates.RemoveAll([InteractableActor](const TWeakObjectPtr<AActor>& Candidate)
	{
		return !Candidate.IsValid() || Candidate.Get() == InteractableActor;
	});
	RefreshCurrentInteractable();
}

bool AMyCharacter::RestoreItemOwnershipFromSave(const UTestSaveGame* SaveGame)
{
	if (!ItemOwnershipComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: ItemOwnershipComponent is not available."), *GetName());
		return false;
	}

	if (!ItemOwnershipComponent->RestoreFromSave(SaveGame))
	{
		DestroyMaterializedLoadout();
		return false;
	}

	MaterializeEquippedLoadout();
	UpdateBowAmmoHUD();
	RefreshBowPresentation();
	return true;
}

bool AMyCharacter::TryGrantOwnedItem(FName DefinitionId, FName& OutInstanceId)
{
	OutInstanceId = NAME_None;
	if (!ItemOwnershipComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("Grant owned item failed: ItemOwnershipComponent is not available."));
		return false;
	}

	USoulslikeGameInstance* GameInstance = GetGameInstance<USoulslikeGameInstance>();
	return ItemOwnershipComponent->TryGrantDefinition(DefinitionId, GameInstance, OutInstanceId);
}

bool AMyCharacter::TryGrantOwnedItemQuantity(FName DefinitionId, int32 Quantity, FName& OutInstanceId)
{
	OutInstanceId = NAME_None;
	if (!ItemOwnershipComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("Grant owned item quantity failed: ItemOwnershipComponent is not available."));
		return false;
	}

	USoulslikeGameInstance* GameInstance = GetGameInstance<USoulslikeGameInstance>();
	return ItemOwnershipComponent->TryGrantDefinitionQuantity(DefinitionId, Quantity, GameInstance, OutInstanceId);
}

bool AMyCharacter::TryRestockAmmoAtCheckpoint(FName GameplayMapName, FName CheckpointId)
{
	if (!ItemOwnershipComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("Restock ammo failed: ItemOwnershipComponent is not available."));
		return false;
	}

	USoulslikeGameInstance* GameInstance = GetGameInstance<USoulslikeGameInstance>();
	return ItemOwnershipComponent->TryRestockAmmoAtCheckpoint(GameplayMapName, CheckpointId, GameInstance);
}

bool AMyCharacter::VerifyAmmoRefillFixture(FName DefinitionId)
{
	if (!ItemOwnershipComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("Verify ammo refill fixture failed: ItemOwnershipComponent is not available."));
		return false;
	}

	USoulslikeGameInstance* GameInstance = GetGameInstance<USoulslikeGameInstance>();
	return ItemOwnershipComponent->VerifyAmmoRefillFixture(DefinitionId, GameInstance);
}

bool AMyCharacter::TryClaimWorldItemPickup(FName PersistentId, FName ItemDefinitionId, int32 PickupQuantity, FName& OutInstanceId,
	USoundBase*& OutPickupSound)
{
	OutInstanceId = NAME_None;
	OutPickupSound = nullptr;
	if (!ItemOwnershipComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("Claim world item failed: ItemOwnershipComponent is not available."));
		return false;
	}

	const UItemDefinitionDataAsset* Definition = ItemOwnershipComponent->GetDefinition(ItemDefinitionId);
	if (!Definition)
	{
		UE_LOG(LogTemp, Warning, TEXT("Claim world item failed: DefinitionId '%s' is not in this player's catalog."),
			*ItemDefinitionId.ToString());
		return false;
	}

	if (PickupQuantity <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Claim world item failed: PickupQuantity must be positive."));
		return false;
	}

	if (Definition->UsesAmmoContainer())
	{
		USoulslikeGameInstance* GameInstance = GetGameInstance<USoulslikeGameInstance>();
		if (!ItemOwnershipComponent->TryClaimWorldAmmoPickup(PersistentId, ItemDefinitionId, PickupQuantity,
			GameInstance, OutInstanceId))
		{
			return false;
		}

		OutPickupSound = Definition->GetPickupSound();
		UpdateBowAmmoHUD();
		RefreshBowPresentation();
		return true;
	}

	if (PickupQuantity != 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("Claim world item rejected DefinitionId '%s': non-ammo fixed pickups must use quantity 1."),
			*ItemDefinitionId.ToString());
		return false;
	}

	const EItemEquipmentSlot EquipmentSlot = Definition->GetEquipmentSlot();
	const bool bCanAutoEquip = (EquipmentSlot == EItemEquipmentSlot::MainHand
		|| EquipmentSlot == EItemEquipmentSlot::OffHand)
		&& ItemOwnershipComponent->GetEquippedInstanceId(EquipmentSlot) == NAME_None;

	Aitem* CandidateItem = nullptr;
	if (bCanAutoEquip && !PrepareMaterializedLoadoutActorFromDefinition(EquipmentSlot, Definition, CandidateItem))
	{
		// 候选表现未准备好时，不能让存档先于可见装备提交。
		return false;
	}

	USoulslikeGameInstance* GameInstance = GetGameInstance<USoulslikeGameInstance>();
	bool bAutoEquipped = false;
	if (!ItemOwnershipComponent->TryClaimWorldItem(PersistentId, ItemDefinitionId, GameInstance, bCanAutoEquip,
		OutInstanceId, bAutoEquipped))
	{
		if (CandidateItem)
		{
			CandidateItem->Destroy();
		}
		return false;
	}

	if (bAutoEquipped)
	{
		check(CandidateItem);
		if (EquipmentSlot == EItemEquipmentSlot::OffHand && DoesEquippedMainHandConsumeOffHand())
		{
			CandidateItem->Destroy();
			RefreshMaterializedOffHandForCurrentMainHand();
		}
		else
		{
			DestroyMaterializedLoadoutSlot(EquipmentSlot);
			CommitMaterializedLoadoutActor(EquipmentSlot, CandidateItem, false);
			if (EquipmentSlot == EItemEquipmentSlot::MainHand)
			{
				RefreshMaterializedOffHandForCurrentMainHand();
			}
		}
	}
	else if (CandidateItem)
	{
		// 存档中该槽位在事务开始前已占用，候选不会替换当前表现。
		CandidateItem->Destroy();
	}

	OutPickupSound = Definition->GetPickupSound();
	UpdateBowAmmoHUD();
	RefreshBowPresentation();
	return true;
}

bool AMyCharacter::TryEquipOwnedItem(FName InstanceId)
{
	if (!ItemOwnershipComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("Equip owned item failed: ItemOwnershipComponent is not available."));
		return false;
	}

	USoulslikeGameInstance* GameInstance = GetGameInstance<USoulslikeGameInstance>();
	return ItemOwnershipComponent->TryEquipInstance(InstanceId, GameInstance);
}

bool AMyCharacter::TryApplyBonfireLoadoutSelection(EItemEquipmentSlot EquipmentSlot, FName InstanceId)
{
	if (!bBonfireServiceProtected)
	{
		UE_LOG(LogTemp, Warning, TEXT("Loadout selection rejected outside Bonfire services."));
		return false;
	}

	if (!ItemOwnershipComponent || (EquipmentSlot != EItemEquipmentSlot::MainHand
		&& EquipmentSlot != EItemEquipmentSlot::OffHand))
	{
		UE_LOG(LogTemp, Warning, TEXT("Loadout selection rejected: item ownership or equipment slot is invalid."));
		return false;
	}

	USoulslikeGameInstance* GameInstance = GetGameInstance<USoulslikeGameInstance>();
	if (!GameInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("Loadout selection rejected: GameInstance is unavailable."));
		return false;
	}

	if (InstanceId == NAME_None)
	{
		if (!ItemOwnershipComponent->TryClearEquipmentSlot(EquipmentSlot, GameInstance))
		{
			return false;
		}

		DestroyMaterializedLoadoutSlot(EquipmentSlot);
		if (EquipmentSlot == EItemEquipmentSlot::MainHand)
		{
			RefreshMaterializedOffHandForCurrentMainHand();
		}
		return true;
	}

	Aitem* CandidateItem = nullptr;
	if (!PrepareMaterializedLoadoutActor(EquipmentSlot, InstanceId, CandidateItem))
	{
		return false;
	}

	if (!ItemOwnershipComponent->TryEquipInstance(InstanceId, GameInstance))
	{
		CandidateItem->Destroy();
		return false;
	}

	if (EquipmentSlot == EItemEquipmentSlot::OffHand && DoesEquippedMainHandConsumeOffHand())
	{
		CandidateItem->Destroy();
		RefreshMaterializedOffHandForCurrentMainHand();
		return true;
	}

	DestroyMaterializedLoadoutSlot(EquipmentSlot);
	CommitMaterializedLoadoutActor(EquipmentSlot, CandidateItem, true);
	if (EquipmentSlot == EItemEquipmentSlot::MainHand)
	{
		RefreshMaterializedOffHandForCurrentMainHand();
	}
	return true;
}

void AMyCharacter::MaterializeEquippedLoadout()
{
	DestroyMaterializedLoadout();

	if (!ItemOwnershipComponent)
	{
		return;
	}

	const FName MainHandInstanceId = ItemOwnershipComponent->GetEquippedInstanceId(EItemEquipmentSlot::MainHand);
	if (MainHandInstanceId != NAME_None)
	{
		Aitem* MaterializedItem = nullptr;
		if (PrepareMaterializedLoadoutActor(EItemEquipmentSlot::MainHand, MainHandInstanceId, MaterializedItem))
		{
			CommitMaterializedLoadoutActor(EItemEquipmentSlot::MainHand, MaterializedItem, false);
		}
	}

	RefreshMaterializedOffHandForCurrentMainHand();
}

bool AMyCharacter::DoesEquippedMainHandConsumeOffHand() const
{
	return IsBowEquipped();
}

void AMyCharacter::RefreshMaterializedOffHandForCurrentMainHand()
{
	if (DoesEquippedMainHandConsumeOffHand())
	{
		DestroyMaterializedLoadoutSlot(EItemEquipmentSlot::OffHand);
		return;
	}

	if (IsValid(EquippedShield) || !ItemOwnershipComponent)
	{
		return;
	}

	const FName OffHandInstanceId = ItemOwnershipComponent->GetEquippedInstanceId(EItemEquipmentSlot::OffHand);
	if (OffHandInstanceId == NAME_None)
	{
		return;
	}

	Aitem* MaterializedItem = nullptr;
	if (PrepareMaterializedLoadoutActor(EItemEquipmentSlot::OffHand, OffHandInstanceId, MaterializedItem))
	{
		// 副手由 MainHand 切换自动恢复，不额外播放第二个装备音效。
		CommitMaterializedLoadoutActor(EItemEquipmentSlot::OffHand, MaterializedItem, false);
	}
}

void AMyCharacter::DestroyMaterializedLoadout()
{
	DestroyMaterializedLoadoutSlot(EItemEquipmentSlot::OffHand);
	DestroyMaterializedLoadoutSlot(EItemEquipmentSlot::MainHand);
}

bool AMyCharacter::ResolveLoadoutDefinition(EItemEquipmentSlot EquipmentSlot, FName InstanceId,
	const UItemDefinitionDataAsset*& OutDefinition) const
{
	OutDefinition = nullptr;
	if (!ItemOwnershipComponent || InstanceId == NAME_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("Loadout resolution failed: item ownership or InstanceId is invalid."));
		return false;
	}

	const FTestItemInstanceRecord* ItemRecord = ItemOwnershipComponent->GetOwnedItemInstance(InstanceId);
	if (!ItemRecord)
	{
		UE_LOG(LogTemp, Warning, TEXT("Loadout resolution failed: instance '%s' is not owned."), *InstanceId.ToString());
		return false;
	}

	const UItemDefinitionDataAsset* Definition = ItemOwnershipComponent->GetDefinition(ItemRecord->DefinitionId);
	if (!Definition || Definition->GetEquipmentSlot() != EquipmentSlot)
	{
		UE_LOG(LogTemp, Warning, TEXT("Loadout resolution failed: instance '%s' does not match the requested equipment slot."),
			*InstanceId.ToString());
		return false;
	}

	OutDefinition = Definition;
	return true;
}

bool AMyCharacter::PrepareMaterializedLoadoutActor(EItemEquipmentSlot EquipmentSlot, FName InstanceId, Aitem*& OutItem)
{
	OutItem = nullptr;
	const UItemDefinitionDataAsset* Definition = nullptr;
	return ResolveLoadoutDefinition(EquipmentSlot, InstanceId, Definition)
		&& PrepareMaterializedLoadoutActorFromDefinition(EquipmentSlot, Definition, OutItem);
}

bool AMyCharacter::PrepareMaterializedLoadoutActorFromDefinition(EItemEquipmentSlot EquipmentSlot,
	const UItemDefinitionDataAsset* Definition, Aitem*& OutItem)
{
	OutItem = nullptr;
	if (!Definition || Definition->GetEquipmentSlot() != EquipmentSlot)
	{
		UE_LOG(LogTemp, Warning, TEXT("Loadout materialization failed: definition does not match the requested equipment slot."));
		return false;
	}

	FString FailureReason;
	if (!Definition->IsDefinitionValid(FailureReason))
	{
		UE_LOG(LogTemp, Warning, TEXT("Loadout materialization failed: definition '%s' is invalid: %s."),
			*GetNameSafe(Definition), *FailureReason);
		return false;
	}

	const UClass* RuntimeClass = Definition->GetRuntimeItemActorClass().Get();
	const bool bRuntimeClassMatchesSlot = RuntimeClass
		&& ((EquipmentSlot == EItemEquipmentSlot::MainHand && RuntimeClass->IsChildOf(AWeapon::StaticClass()))
			|| (EquipmentSlot == EItemEquipmentSlot::OffHand && RuntimeClass->IsChildOf(AShield::StaticClass())));
	if (!bRuntimeClassMatchesSlot)
	{
		UE_LOG(LogTemp, Warning, TEXT("Loadout materialization failed: definition '%s' has an incompatible runtime Actor class."),
			*GetNameSafe(Definition));
		return false;
	}

	if (!GetWorld() || !GetMesh())
	{
		UE_LOG(LogTemp, Warning, TEXT("Loadout materialization failed: world or character mesh is unavailable."));
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.Instigator = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	Aitem* SpawnedItem = GetWorld()->SpawnActor<Aitem>(Definition->GetRuntimeItemActorClass(), GetActorTransform(), SpawnParameters);
	if (!SpawnedItem)
	{
		UE_LOG(LogTemp, Warning, TEXT("Loadout materialization failed: could not spawn '%s'."),
			*GetNameSafe(Definition->GetRuntimeItemActorClass().Get()));
		return false;
	}

	SpawnedItem->SetActorHiddenInGame(true);

	bool bAttached = false;
	if (EquipmentSlot == EItemEquipmentSlot::MainHand)
	{
		if (AWeapon* Weapon = Cast<AWeapon>(SpawnedItem))
		{
			bAttached = Weapon->Equip(GetMesh(), Weapon->GetDefaultEquipSocketName(), this, this, false);
		}
	}
	else if (EquipmentSlot == EItemEquipmentSlot::OffHand)
	{
		if (AShield* Shield = Cast<AShield>(SpawnedItem))
		{
			bAttached = Shield->EquipToOffhand(GetMesh(), Shield->GetOffhandSocketName(), this, false);
		}
	}

	if (!bAttached)
	{
		UE_LOG(LogTemp, Warning, TEXT("Loadout materialization failed: '%s' could not attach for its slot."),
			*GetNameSafe(SpawnedItem));
		SpawnedItem->Destroy();
		return false;
	}

	OutItem = SpawnedItem;
	return true;
}

void AMyCharacter::CommitMaterializedLoadoutActor(EItemEquipmentSlot EquipmentSlot, Aitem* Item, bool bPlayEquipSound)
{
	if (!Item)
	{
		return;
	}

	if (EquipmentSlot == EItemEquipmentSlot::MainHand)
	{
		// PrepareMaterializedLoadoutActor 已在写盘前验证实际类型并完成附着。
		AWeapon* Weapon = static_cast<AWeapon*>(Item);

		EquippedWeapon = Weapon;
		WeaponState = Cast<ABow>(Weapon) ? EWeaponState::EWS_TwoHandEquipped : EWeaponState::EWS_OneHandEquipped;
		Item->SetActorHiddenInGame(false);
		if (bPlayEquipSound)
		{
			Weapon->PlayEquipSound();
		}
		UpdateBowAmmoHUD();
		RefreshBowPresentation();
		return;
	}

	// PrepareMaterializedLoadoutActor 已在写盘前验证实际类型并完成附着。
	AShield* Shield = static_cast<AShield*>(Item);

	EquippedShield = Shield;
	Item->SetActorHiddenInGame(false);
	if (bPlayEquipSound)
	{
		Shield->PlayEquipSound();
	}
}

void AMyCharacter::DestroyMaterializedLoadoutSlot(EItemEquipmentSlot EquipmentSlot)
{
	if (EquipmentSlot == EItemEquipmentSlot::MainHand)
	{
		InvalidateActiveAttackMontagePlayback();
		CancelBowAim(true);
		if (IsValid(EquippedWeapon))
		{
			EquippedWeapon->Destroy();
		}

		EquippedWeapon = nullptr;
		WeaponState = EWeaponState::EWS_Unequipped;
		UpdateBowAmmoHUD();
		RefreshBowPresentation();
		return;
	}

	if (EquipmentSlot == EItemEquipmentSlot::OffHand)
	{
		InterruptBlock(true);
		ClearParryState();

		if (IsValid(EquippedShield))
		{
			EquippedShield->Destroy();
		}

		EquippedShield = nullptr;
	}
}

FString AMyCharacter::GetItemOwnershipDebugSummary() const
{
	return ItemOwnershipComponent
		? ItemOwnershipComponent->BuildDebugSummary()
		: TEXT("Runtime item ownership: ItemOwnershipComponent is unavailable.");
}

int32 AMyCharacter::GetOwnedItemQuantity(FName DefinitionId) const
{
	return ItemOwnershipComponent ? ItemOwnershipComponent->GetOwnedQuantity(DefinitionId) : 0;
}

void AMyCharacter::RefreshCurrentInteractable()
{
	InteractableCandidates.RemoveAll([](const TWeakObjectPtr<AActor>& Candidate)
	{
		return !Candidate.IsValid();
	});

	AActor* BestCandidate = nullptr;
	int32 BestPriority = TNumericLimits<int32>::Lowest();
	float BestDistanceSquared = TNumericLimits<float>::Max();

	for (const TWeakObjectPtr<AActor>& Candidate : InteractableCandidates)
	{
		AActor* CandidateActor = Candidate.Get();
		if (!CandidateActor || !CandidateActor->Implements<UInteractableInterface>()
			|| !IInteractableInterface::Execute_CanInteract(CandidateActor, this))
		{
			continue;
		}

		const int32 Priority = IInteractableInterface::Execute_GetInteractionPriority(CandidateActor);
		const float DistanceSquared = FVector::DistSquared2D(GetActorLocation(), CandidateActor->GetActorLocation());
		if (!BestCandidate || Priority > BestPriority || (Priority == BestPriority && DistanceSquared < BestDistanceSquared))
		{
			BestCandidate = CandidateActor;
			BestPriority = Priority;
			BestDistanceSquared = DistanceSquared;
		}
	}

	if (CurrentInteractable.Get() == BestCandidate)
	{
		return;
	}

	CurrentInteractable = BestCandidate;
	OverLapItem = Cast<Aitem>(BestCandidate);
	UpdateInteractionPrompt();
}

void AMyCharacter::UpdateInteractionPrompt()
{
	ACharacterController* CharacterController = Cast<ACharacterController>(GetController());
	if (!CharacterController)
	{
		return;
	}

	AActor* InteractableActor = CurrentInteractable.Get();
	if (InteractableActor && InteractableActor->Implements<UInteractableInterface>())
	{
		CharacterController->ShowInteractionPrompt(IInteractableInterface::Execute_GetInteractionPrompt(InteractableActor));
	}
	else
	{
		CharacterController->HideInteractionPrompt();
	}
}

void AMyCharacter::RefreshInteractionPrompt()
{
	RefreshCurrentInteractable();
	UpdateInteractionPrompt();
}

// ==================== 药瓶 ====================

bool AMyCharacter::CanUsePotion() const
{
	return (ActionState == EActionState::EAS_UnOccupied || ActionState == EActionState::EAS_Exhausted)
		&& IsActionStartPreflightValid(EPlayerActionType::Potion, false);
}

void AMyCharacter::UsePotion()
{
	if (ResolvePlayerActionIntent(EPlayerActionIntent::Potion) == EPlayerActionIntentResolution::StartNow)
	{
		TryStartAction(EPlayerActionType::Potion);
	}
}

bool AMyCharacter::IsBowCommittedActionActive() const
{
	return IsBowAiming()
		&& (bBowDrawInputHeld || bBowDrawReady || IsValid(PreparedBowProjectile)
			|| bBowReleasePresentationPending || ActiveBowDrawMontage || ActiveBowReleaseMontage || ActiveBowLoadMontage);
}

bool AMyCharacter::ShouldRetainBlockInputForAttackCancel() const
{
	return GetCurrentPlayerActionType() == EPlayerActionType::Attack && !bActionCancelWindowOpen;
}

bool AMyCharacter::IsActionStartPreflightValid(EPlayerActionType Action, bool bRequireCommittedResource) const
{
	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;

	switch (Action)
	{
	case EPlayerActionType::Attack:
		return CanAttack() || ShouldUseSprintAttack();
	case EPlayerActionType::Dodge:
		return AnimInstance && GetDodgeMontage() && Attributes && !GetCharacterMovement()->IsFalling()
			&& (!bRequireCommittedResource || Attributes->CheckStamina(GetDodgeStaminaCost(false)));
	case EPlayerActionType::Block:
		return bBlockInputHeld && EquippedShield && !IsBowEquipped() && AnimInstance && GetBlockMontage()
			&& !GetCharacterMovement()->IsFalling();
	case EPlayerActionType::Parry:
		return EquippedShield && !IsBowEquipped() && AnimInstance && GetParryMontage() && !bParryOnCooldown
			&& Attributes && !GetCharacterMovement()->IsFalling()
			&& (!bRequireCommittedResource || Attributes->CheckStamina(EquippedShield->GetParryStaminaCost()));
	case EPlayerActionType::Potion:
	{
		const UPlayerActionConfigDataAsset* ActionConfig = GetActionConfig();
		UAnimMontage* PotionMontage = GetPotionMontage();
		return ActionConfig && Attributes && Attributes->HasPotion() && !bPotionOnCooldown
			&& Attributes->GetHealthPercent() < 1.f && !GetCharacterMovement()->IsFalling()
			&& (!PotionMontage || AnimInstance);
	}
	case EPlayerActionType::RangedAim:
	{
		ABow* Bow = GetEquippedBow();
		if (!Bow || !bBlockInputHeld || bIsBlocking || !Attributes || !Attributes->IsAlive()
			|| bBonfireServiceProtected || GetCharacterMovement()->IsFalling())
		{
			return false;
		}

		FString FailureReason;
		return Bow->HasValidProjectileConfig(FailureReason);
	}
	case EPlayerActionType::RangedRelease:
		return IsBowAiming() && bBowDrawReady && !bBowReleasePresentationPending
			&& IsValid(PreparedBowProjectile) && PreparedBowProjectile->IsPreparedForActivation();
	case EPlayerActionType::HitReact:
	case EPlayerActionType::Death:
	case EPlayerActionType::None:
	default:
		return false;
	}
}

AMyCharacter::EPlayerActionIntentResolution AMyCharacter::ResolvePlayerActionIntent(EPlayerActionIntent Intent) const
{
	const EPlayerActionType CurrentAction = GetCurrentPlayerActionType();

	switch (Intent)
	{
	case EPlayerActionIntent::AttackPress:
		if (IsBowAiming())
		{
			if (IsBowReFireGateBlocked())
			{
				return CanBufferBowChargeInput()
					? EPlayerActionIntentResolution::BufferOnce
					: EPlayerActionIntentResolution::Reject;
			}

			return !bBowDrawInputHeld && !IsValid(PreparedBowProjectile)
				? EPlayerActionIntentResolution::StartNow
				: EPlayerActionIntentResolution::Reject;
		}

		// Block 是可逆 held 状态；不能因其 ActionState 仍为 UnOccupied 而让 LMB 绕过防御。
		if (CurrentAction == EPlayerActionType::Block || bIsBlocking)
		{
			return EPlayerActionIntentResolution::Reject;
		}

		if (CurrentAction == EPlayerActionType::Attack)
		{
			return bComboWindowOpen && !bComboInputReceived
				? EPlayerActionIntentResolution::BufferOnce
				: EPlayerActionIntentResolution::Reject;
		}

		return ActionState == EActionState::EAS_UnOccupied
			&& IsActionStartPreflightValid(EPlayerActionType::Attack, false)
			? EPlayerActionIntentResolution::StartNow
			: EPlayerActionIntentResolution::Reject;

	case EPlayerActionIntent::AttackRelease:
		if (IsBowAiming() || bBowChargeInputHeld || bBowChargeInputBuffered || bBowDrawInputHeld)
		{
			if (!IsBowAiming() || !bBowDrawInputHeld || !bBowDrawReady)
			{
				return EPlayerActionIntentResolution::EndHeld;
			}

			return IsActionStartPreflightValid(EPlayerActionType::RangedRelease, false)
				? EPlayerActionIntentResolution::StartNow
				: EPlayerActionIntentResolution::EndHeld;
		}

		return (bAttackInputHeld || bIsChargingAttack)
			? EPlayerActionIntentResolution::EndHeld
			: EPlayerActionIntentResolution::Reject;

	case EPlayerActionIntent::AttackCancel:
		if (IsBowAiming())
		{
			return EPlayerActionIntentResolution::EndHeld;
		}

		return (bAttackInputHeld || bIsChargingAttack)
			? EPlayerActionIntentResolution::EndHeld
			: EPlayerActionIntentResolution::Reject;

	case EPlayerActionIntent::BlockPress:
	{
		const EPlayerActionType TargetAction = IsBowEquipped()
			? EPlayerActionType::RangedAim
			: EPlayerActionType::Block;
		if (CurrentAction == TargetAction)
		{
			return EPlayerActionIntentResolution::StartNow;
		}

		if (CurrentAction == EPlayerActionType::None)
		{
			return ActionState == EActionState::EAS_UnOccupied
				&& IsActionStartPreflightValid(TargetAction, false)
				? EPlayerActionIntentResolution::StartNow
				: EPlayerActionIntentResolution::Reject;
		}

		if (CurrentAction == EPlayerActionType::Attack && bActionCancelWindowOpen)
		{
			return IsActionStartPreflightValid(TargetAction, true)
				? EPlayerActionIntentResolution::StartNow
				: EPlayerActionIntentResolution::Reject;
		}

		return EPlayerActionIntentResolution::Reject;
	}

	case EPlayerActionIntent::BlockRelease:
		return CurrentAction == EPlayerActionType::Block || CurrentAction == EPlayerActionType::RangedAim
			? EPlayerActionIntentResolution::EndHeld
			: EPlayerActionIntentResolution::Reject;

	case EPlayerActionIntent::Dodge:
	case EPlayerActionIntent::Parry:
	case EPlayerActionIntent::Potion:
	{
		const EPlayerActionType TargetAction = Intent == EPlayerActionIntent::Dodge
			? EPlayerActionType::Dodge
			: Intent == EPlayerActionIntent::Parry
				? EPlayerActionType::Parry
				: EPlayerActionType::Potion;

		if (CurrentAction == EPlayerActionType::None)
		{
			const bool bStateAllowsStart = TargetAction == EPlayerActionType::Potion
				? (ActionState == EActionState::EAS_UnOccupied || ActionState == EActionState::EAS_Exhausted)
				: ActionState == EActionState::EAS_UnOccupied;
			return bStateAllowsStart && IsActionStartPreflightValid(TargetAction, false)
				? EPlayerActionIntentResolution::StartNow
				: EPlayerActionIntentResolution::Reject;
		}

		if (CurrentAction == EPlayerActionType::Attack)
		{
			return bActionCancelWindowOpen && IsActionStartPreflightValid(TargetAction, true)
				? EPlayerActionIntentResolution::StartNow
				: EPlayerActionIntentResolution::Reject;
		}

		if (TargetAction != EPlayerActionType::Potion && CurrentAction == EPlayerActionType::Block)
		{
			return IsActionStartPreflightValid(TargetAction, true)
				? EPlayerActionIntentResolution::StartNow
				: EPlayerActionIntentResolution::Reject;
		}

		if (TargetAction != EPlayerActionType::Potion && CurrentAction == EPlayerActionType::RangedAim
			&& !IsBowCommittedActionActive())
		{
			return IsActionStartPreflightValid(TargetAction, true)
				? EPlayerActionIntentResolution::StartNow
				: EPlayerActionIntentResolution::Reject;
		}

		return EPlayerActionIntentResolution::Reject;
	}

	default:
		return EPlayerActionIntentResolution::Reject;
	}
}

bool AMyCharacter::TryStartAction(EPlayerActionType Action)
{
	const EPlayerActionType CurrentAction = GetCurrentPlayerActionType();
	const bool bWillInterruptExistingAction = CurrentAction != EPlayerActionType::None && CurrentAction != Action;
	if (!IsActionStartPreflightValid(Action, bWillInterruptExistingAction))
	{
		return false;
	}

	if (CurrentAction == EPlayerActionType::Attack && Action != EPlayerActionType::Attack)
	{
		CleanupInterruptedAction(CurrentAction);
	}
	else if (CurrentAction == EPlayerActionType::RangedAim && Action != EPlayerActionType::RangedAim
		&& Action != EPlayerActionType::RangedRelease)
	{
		CleanupInterruptedAction(CurrentAction);
	}

	switch (Action)
	{
	case EPlayerActionType::Attack:
		return StartAttackAction();
	case EPlayerActionType::Dodge:
		return StartDodgeAction();
	case EPlayerActionType::Block:
		return bIsBlocking || StartBlockAction();
	case EPlayerActionType::Parry:
		return StartParryAction();
	case EPlayerActionType::Potion:
		return StartPotionAction();
	case EPlayerActionType::RangedAim:
		return IsBowAiming() || StartBowAimAction();
	case EPlayerActionType::RangedRelease:
		return ReleaseBowArrow();
	case EPlayerActionType::HitReact:
	case EPlayerActionType::Death:
	case EPlayerActionType::None:
	default:
		return false;
	}
}

bool AMyCharacter::StartDodgeAction()
{
	UAnimMontage* DodgeMontage = GetDodgeMontage();
	UAnimInstance* Anim = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!DodgeMontage || !Anim || !Attributes)
	{
		return false;
	}

	// Dodge 是承诺式动作；即使它从 Attack CancelWindow 或 Block 进入，也不能在结束后自动补播 held Block / Aim。
	bBlockInputHeld = false;
	ResetCombo();

	if (bIsBlocking) InterruptBlock(false);
	if (bIsParrying) InterruptParry();

	FVector DodgeDir = ComputeDodgeDirection();
	FName Section = SelectDodgeSection(DodgeDir);
	if (!IsLockingOn() && !DodgeDir.IsNearlyZero())
	{
		FaceDirection2D(DodgeDir);
	}

	SetMovementRotationMode(false, false);

	Attributes->UseStamina(GetDodgeStaminaCost(true));
	Attributes->PauseStaminaRegen();

	ActionState = EActionState::EAS_Dodging;

	PlayMontageSection(DodgeMontage, Section);

	// 翻滚发出单次噪音
	EmitNoise(DodgeNoiseLoudness, DodgeNoiseRange);

	// 停止移动噪音定时器（翻滚期间不持续发声）
	StopMovementNoiseTimer();

	BindMontageEndDelegate(Anim, DodgeMontage, &AMyCharacter::OnDodgeMontageEnded);

	return true;
}

bool AMyCharacter::StartBlockAction()
{
	UAnimMontage* BlockMontage = GetBlockMontage();
	UAnimInstance* Anim = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!BlockMontage || !Anim)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: StartBlockAction failed, BlockMontage or AnimInstance is not available."), *GetName());
		return false;
	}

	bIsBlocking = true;
	if (Attributes)
	{
		Attributes->SetStaminaRegenMultiplier(GetBlockStaminaRegenMultiplier());
	}
	PlayBlockMontage(GetBlockRaiseSection());
	return true;
}

bool AMyCharacter::StartBowAimAction()
{
	ABow* Bow = GetEquippedBow();
	if (!Bow)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: StartBowAimAction failed: ABow is not equipped."), *GetName());
		return false;
	}

	FString FailureReason;
	if (!Bow->HasValidProjectileConfig(FailureReason))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: StartBowAimAction failed: %s"), *GetName(), *FailureReason);
		return false;
	}

	// 自由瞄准必须立刻接管 ControlRotation，不能被一次失败锁定遗留的归中插值拉回。
	StopCameraRecenter();
	bIsSprinting = false;
	CancelBowCharge(false);
	CacheAimRotationState();
	ActionState = EActionState::EAS_Aiming;
	ApplyAimRotationMode();
	ResetCameraObstructionRecovery(true);
	// 当前 Aim Locomotion 已是半拉弓姿态；Bow AnimBP 用独立的物理 Aim 状态与其对齐。
	Bow->SetBowPresentationState(EBowPresentationState::EBPS_Aiming);
	PlayBowAimRaisePresentation();
	RefreshBowPresentation();
	UpdateBowAmmoHUD();
	UpdateAimReticleHUD();
	return true;
}

bool AMyCharacter::StartBowCharge()
{
	ABow* Bow = GetEquippedBow();
	if (!Bow || !ItemOwnershipComponent || bBowDrawInputHeld || IsValid(PreparedBowProjectile)
		|| IsBowReFireGateBlocked())
	{
		return false;
	}

	FString FailureReason;
	if (!Bow->HasValidProjectileConfig(FailureReason))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: Bow charge failed: %s"), *GetName(), *FailureReason);
		return false;
	}

	const FName AmmoDefinitionId = Bow->GetAmmoDefinitionId();
	if (ItemOwnershipComponent->GetLoadedAmmoQuantity(AmmoDefinitionId) <= 0)
	{
		Bow->PlayEmptyAmmoSound();
		UE_LOG(LogTemp, Display, TEXT("%s: Bow charge blocked: no loaded '%s' remaining."), *GetName(), *AmmoDefinitionId.ToString());
		return false;
	}

	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	UAnimMontage* DrawMontage = Bow->GetDrawMontage();
	UAnimMontage* ReleaseMontage = Bow->GetReleaseMontage();
	UAnimMontage* LoadMontage = Bow->GetLoadMontage();
	if (!AnimInstance || !DrawMontage || !ReleaseMontage || !LoadMontage)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("%s: Bow charge blocked: AnimInstance, DrawMontage, ReleaseMontage, and LoadMontage are required for player firing."),
			*GetName());
		return false;
	}

	FVector SpawnLocation = FVector::ZeroVector;
	FVector LaunchDirection = FVector::ForwardVector;
	if (!ResolveBowAimLaunchContext(Bow, SpawnLocation, LaunchDirection))
	{
		return false;
	}

	FProjectileLaunchParams LaunchParams;
	LaunchParams.Attacker = this;
	LaunchParams.EventInstigator = GetController();
	LaunchParams.SpawnLocation = SpawnLocation;
	LaunchParams.LaunchDirection = LaunchDirection;
	LaunchParams.bOverrideDeliveryConfig = true;
	LaunchParams.DeliveryConfigOverride = Bow->GetProjectileDeliveryConfig();

	ACombatProjectile* Projectile = ACombatProjectile::SpawnPreparedProjectile(GetWorld(), Bow->GetProjectileClass(), LaunchParams);
	if (!Projectile || !Projectile->IsPreparedForActivation())
	{
		if (Projectile)
		{
			Projectile->Destroy();
		}
		UE_LOG(LogTemp, Warning, TEXT("%s: Bow charge failed: prepared projectile is unavailable."), *GetName());
		return false;
	}

	if (ConsumeProjectilePrepareFailureForDebug())
	{
		Projectile->Destroy();
		UE_LOG(LogTemp, Warning,
			TEXT("%s: BowDebugFailNextProjectilePrepare discarded a prepared projectile before charging '%s'."),
			*GetName(), *AmmoDefinitionId.ToString());
		RefreshBowPresentation();
		return false;
	}

	if (!Bow->NockPreparedProjectile(Projectile))
	{
		RefreshBowPresentation();
		return false;
	}

	PreparedBowProjectile = Projectile;
	Projectile->GetOnImpactResolved().AddUObject(this, &AMyCharacter::HandleBowProjectileImpactResolved);
	bBowDrawInputHeld = true;
	bBowDrawReady = false;
	RefreshBowPresentation();
	ActiveBowDrawMontage = DrawMontage;
	if (AnimInstance->Montage_Play(DrawMontage) <= 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: Bow charge failed: DrawMontage could not play."), *GetName());
		CancelBowCharge(false);
		return false;
	}

	BindMontageEndDelegate(AnimInstance, DrawMontage, &AMyCharacter::OnBowDrawMontageEnded);
	Bow->SetBowPresentationState(EBowPresentationState::EBPS_Charging);
	bBowChargeFOVActive = true;
	RefreshBowPresentation();
	return true;
}

bool AMyCharacter::ReleaseBowArrow()
{
	ABow* Bow = GetEquippedBow();
	ACombatProjectile* Projectile = PreparedBowProjectile;
	if (!Bow || !ItemOwnershipComponent || !bBowDrawReady || bBowReleasePresentationPending
		|| !IsValid(Projectile) || !Projectile->IsPreparedForActivation())
	{
		CancelBowCharge(false);
		return false;
	}

	FString FailureReason;
	if (!Bow->HasValidProjectileConfig(FailureReason))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: Bow release failed: %s"), *GetName(), *FailureReason);
		CancelBowCharge(false);
		return false;
	}

	const FName AmmoDefinitionId = Bow->GetAmmoDefinitionId();
	if (ItemOwnershipComponent->GetLoadedAmmoQuantity(AmmoDefinitionId) <= 0)
	{
		Bow->PlayEmptyAmmoSound();
		UE_LOG(LogTemp, Display, TEXT("%s: Bow release blocked: no loaded '%s' remaining."), *GetName(), *AmmoDefinitionId.ToString());
		CancelBowCharge(false);
		return false;
	}

	FVector SpawnLocation = FVector::ZeroVector;
	FVector LaunchDirection = FVector::ForwardVector;
	if (!ResolveBowAimLaunchContext(Bow, SpawnLocation, LaunchDirection)
		|| !Projectile->UpdatePreparedLaunchContext(SpawnLocation, LaunchDirection))
	{
		CancelBowCharge(false);
		return false;
	}

	if (!TryStartBowReleasePresentation(Bow))
	{
		CancelBowCharge(false);
		return false;
	}

	USoulslikeGameInstance* GameInstance = GetGameInstance<USoulslikeGameInstance>();
	if (!ItemOwnershipComponent->TryConsumeLoadedAmmo(AmmoDefinitionId, 1, GameInstance))
	{
		CancelBowCharge(false);
		AbortBowReleasePresentation();
		return false;
	}

	Projectile->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Projectile->SetActorLocationAndRotation(SpawnLocation, LaunchDirection.Rotation(), false, nullptr,
		ETeleportType::TeleportPhysics);
	PreparedBowProjectile = nullptr;
	ClearBowChargeInputState();
	bBowDrawInputHeld = false;
	bBowDrawReady = false;
	bBowChargeFOVActive = false;
	Projectile->CommitPreparedLaunch();

	UpdateBowAmmoHUD();
	Bow->PlayShotSound();

	UE_LOG(LogTemp, Display, TEXT("%s: Bow released one '%s'."), *GetName(), *AmmoDefinitionId.ToString());
	return true;
}

bool AMyCharacter::ArmNextProjectilePrepareFailureForDebug()
{
#if UE_BUILD_SHIPPING
	return false;
#else
	bFailNextProjectilePrepareForDebug = true;
	return true;
#endif
}

bool AMyCharacter::StartParryAction()
{
	// 先确认蒙太奇可播放，再扣体力和进入状态（防止卡在 EAS_Parrying）
	UAnimMontage* ParryMontage = GetParryMontage();
	UAnimInstance* Anim = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!ParryMontage || !Anim || !Attributes || !EquippedShield || IsBowEquipped())
	{
		return false;
	}

	// Parry 成功启动后由自身占有输入，结束前不保留此前的防御 held 意图。
	bBlockInputHeld = false;
	if (bIsBlocking)
	{
		InterruptBlock(false);
	}

	Attributes->UseStamina(EquippedShield->GetParryStaminaCost());
	Attributes->ResetStaminaRegenCooldown();

	bIsParrying = true;
	bParryActive = false;
	ActionState = EActionState::EAS_Parrying;
	PlayMontageSection(ParryMontage, FName("Parry"));

	BindMontageEndDelegate(Anim, ParryMontage, &AMyCharacter::OnParryMontageEnded);

	return true;
}

EPlayerActionType AMyCharacter::GetCurrentPlayerActionType() const
{
	switch (ActionState)
	{
	case EActionState::EAS_Attacking:
		return EPlayerActionType::Attack;
	case EActionState::EAS_Parrying:
		return EPlayerActionType::Parry;
	case EActionState::EAS_Dodging:
		return EPlayerActionType::Dodge;
	case EActionState::EAS_UsingPotion:
		return EPlayerActionType::Potion;
	case EActionState::EAS_Aiming:
		return EPlayerActionType::RangedAim;
	case EActionState::EAS_Stunning:
	case EActionState::EAS_GuardBroken:
		return EPlayerActionType::HitReact;
	case EActionState::EAS_Dead:
		return EPlayerActionType::Death;
	case EActionState::EAS_UnOccupied:
	case EActionState::EAS_Exhausted:
		// Block 是按住输入维持的姿态，ActionState 通常仍是 UnOccupied。
		return bIsBlocking ? EPlayerActionType::Block : EPlayerActionType::None;
	default:
		return EPlayerActionType::None;
	}
}

float AMyCharacter::GetDodgeStaminaCost(bool bLogFallback) const
{
	if (const UPlayerActionConfigDataAsset* ActionConfig = GetActionConfig())
	{
		return ActionConfig->Dodge.StaminaCost;
	}

	if (bLogFallback)
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("%s: ActionConfig missing, falling back to C++ default DodgeStaminaCost = %.2f."),
		       *GetName(), DodgeStaminaCost);
	}
	return DodgeStaminaCost;
}

float AMyCharacter::GetPotionCooldown(bool bLogFallback) const
{
	if (const UPlayerActionConfigDataAsset* ActionConfig = GetActionConfig())
	{
		return ActionConfig->Potion.Cooldown;
	}

	if (bLogFallback)
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("%s: ActionConfig missing, falling back to C++ default PotionCooldown = %.2f."),
		       *GetName(), PotionCooldown);
	}
	return PotionCooldown;
}

float AMyCharacter::GetPotionFallbackHealPercent() const
{
	if (const UPlayerActionConfigDataAsset* ActionConfig = GetActionConfig())
	{
		return ActionConfig->Potion.HealPercent;
	}

	UE_LOG(LogTemp, Warning,
	       TEXT("%s: ActionConfig missing, falling back to hard-coded potion heal percent = 0.50."),
	       *GetName());
	return 0.5f;
}

float AMyCharacter::GetBlockStaminaRegenMultiplier() const
{
	if (const UPlayerActionConfigDataAsset* ActionConfig = GetActionConfig())
	{
		return FMath::Max(0.f, ActionConfig->Block.StaminaRegenMultiplier);
	}

	UE_LOG(LogTemp, Warning,
	       TEXT("%s: ActionConfig missing, falling back to normal block stamina regen multiplier = 1.00."),
	       *GetName());
	return 1.f;
}

FName AMyCharacter::GetBlockRaiseSection() const
{
	if (const UPlayerActionConfigDataAsset* ActionConfig = GetActionConfig())
	{
		return ActionConfig->Block.BlockRaiseSection;
	}

	UE_LOG(LogTemp, Warning,
	       TEXT("%s: ActionConfig missing, falling back to hard-coded BlockRaise section."),
	       *GetName());
	return FName("BlockRaise");
}

void AMyCharacter::CleanupInterruptedAction(EPlayerActionType InterruptedAction)
{
	CloseActionCancelWindow();

	switch (InterruptedAction)
	{
	case EPlayerActionType::Attack:
		CleanupInterruptedAttack();
		break;
	case EPlayerActionType::Block:
		InterruptBlock(false);
		break;
	case EPlayerActionType::Parry:
		InterruptParry();
		RecoverActionStateAfterMontage(EActionState::EAS_Parrying, false);
		break;
	case EPlayerActionType::Dodge:
		bDodgeInvulnerable = false;
		RestoreRotationMode();
		RecoverActionStateAfterMontage(EActionState::EAS_Dodging, true);
		break;
	case EPlayerActionType::Potion:
		InterruptPotion();
		break;
	case EPlayerActionType::RangedAim:
		// 翻滚、弹反等更高优先级动作打断瞄准后，要求玩家重新按住右键才可再次瞄准。
		CancelBowAim(true);
		break;
	case EPlayerActionType::HitReact:
	case EPlayerActionType::Death:
	case EPlayerActionType::RangedRelease:
	case EPlayerActionType::None:
	default:
		break;
	}
}

bool AMyCharacter::StartPotionAction()
{
	const UPlayerActionConfigDataAsset* ActionConfig = GetActionConfig();
	if (!ActionConfig)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: UsePotion failed, PlayerProfile->ActionConfig is not configured."), *GetName());
		return false;
	}

	UAnimMontage* PotionMontage = GetPotionMontage();
	if (PotionMontage && (!GetMesh() || !GetMesh()->GetAnimInstance()))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: UsePotion failed, PotionMontage is configured but AnimInstance is not available."), *GetName());
		return false;
	}

	if (Attributes->UsePotion())
	{
		// Potion 成功消费后不自动恢复此前按住的 Block / Aim；玩家需松开再按。
		bBlockInputHeld = false;
		if (bIsSprinting) StopSprinting();

		if (PotionMontage)
		{
			ActionState = EActionState::EAS_UsingPotion;
			PlayPotionMontage();
			EmitNoise(PotionNoiseLoudness, PotionNoiseRange);
		}
		else
		{
			HealFromPotion(GetPotionFallbackHealPercent());
			if (ActionConfig->Potion.FallbackHealSound)
			{
				UGameplayStatics::PlaySoundAtLocation(this, ActionConfig->Potion.FallbackHealSound.Get(), GetActorLocation());
			}
			StartPotionCooldown();
		}
		return true;
	}

	return false;
}

void AMyCharacter::PlayPotionMontage()
{
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	UAnimMontage* PotionMontage = GetPotionMontage();
	if (AnimInstance && PotionMontage)
	{
		AnimInstance->Montage_Play(PotionMontage);
		BindMontageEndDelegate(AnimInstance, PotionMontage, &AMyCharacter::OnPotionMontageEnded);
	}
}

void AMyCharacter::OnPotionMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (bInterrupted) return;  // InterruptPotion() 已处理
	if (ActionState != EActionState::EAS_UsingPotion) return;

	StartPotionCooldown();
	RecoverActionStateAfterMontage(EActionState::EAS_UsingPotion, false);
}

void AMyCharacter::HealFromPotion(float Percent)
{
	// 状态守卫：防止蒙太奇被打断后，残留的AnimNotify仍然触发恢复
	// 例外：没有蒙太奇时（fallback路径），允许在任何状态下恢复
	const UPlayerActionConfigDataAsset* ActionConfig = GetActionConfig();
	if (!ActionConfig)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: HealFromPotion skipped, PlayerProfile->ActionConfig is not configured."), *GetName());
		return;
	}

	const bool bUsesPotionMontage = ActionConfig->Potion.Montage != nullptr;
	if (bUsesPotionMontage && ActionState != EActionState::EAS_UsingPotion)
	{
		return;
	}

	if (Attributes)
	{
		Attributes->AddHealth(Attributes->GetMaxHealth() * Percent);
	}
}

void AMyCharacter::InterruptPotion()
{
	const bool bWasUsingPotion = ActionState == EActionState::EAS_UsingPotion;
	bool bWasPotionMontagePlaying = false;
	UAnimMontage* PotionMontage = GetPotionMontage();

	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
		AnimInstance && PotionMontage && AnimInstance->Montage_IsPlaying(PotionMontage))
	{
		bWasPotionMontagePlaying = true;
		AnimInstance->Montage_Stop(0.1f, PotionMontage);
	}

	if (!bWasUsingPotion && !bWasPotionMontagePlaying)
	{
		return;
	}

	ActionState = EActionState::EAS_UnOccupied;
	StartPotionCooldown();
}

void AMyCharacter::StartPotionCooldown()
{
	const float Cooldown = GetPotionCooldown(true);
	if (Cooldown <= 0.f)
	{
		bPotionOnCooldown = false;
		UpdatePotionCooldownHUD();
		return;
	}
	bPotionOnCooldown = true;
	GetWorldTimerManager().SetTimer(
		PotionCooldownTimer, this, &AMyCharacter::ResetPotionCooldown, Cooldown, false);
	UpdatePotionCooldownHUD();
}

void AMyCharacter::ResetPotionCooldown()
{
	bPotionOnCooldown = false;
	UpdatePotionCooldownHUD();
}

ABow* AMyCharacter::GetEquippedBow() const
{
	return Cast<ABow>(EquippedWeapon);
}

bool AMyCharacter::IsBowEquipped() const
{
	return IsValid(GetEquippedBow());
}

bool AMyCharacter::IsBowAiming() const
{
	return ActionState == EActionState::EAS_Aiming && IsBowEquipped();
}

bool AMyCharacter::CanStartBowAim() const
{
	return ActionState == EActionState::EAS_UnOccupied
		&& IsActionStartPreflightValid(EPlayerActionType::RangedAim, false);
}

bool AMyCharacter::IsBowReFireGateBlocked() const
{
	if (bBowReleasePresentationPending || ActiveBowReleaseMontage || ActiveBowLoadMontage)
	{
		return true;
	}

	const ABow* Bow = GetEquippedBow();
	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!Bow || !AnimInstance)
	{
		return false;
	}

	UAnimMontage* ReleaseMontage = Bow->GetReleaseMontage();
	UAnimMontage* LoadMontage = Bow->GetLoadMontage();
	return (ReleaseMontage && AnimInstance->Montage_IsPlaying(ReleaseMontage))
		|| (LoadMontage && AnimInstance->Montage_IsPlaying(LoadMontage));
}

bool AMyCharacter::CanBufferBowChargeInput() const
{
	return IsBowAiming()
		&& bBlockInputHeld
		&& bBowReleasePresentationPending
		&& (ActiveBowReleaseMontage || ActiveBowLoadMontage)
		&& Attributes
		&& Attributes->IsAlive();
}

void AMyCharacter::ClearBowChargeInputState()
{
	bBowChargeInputHeld = false;
	bBowChargeInputBuffered = false;
}

void AMyCharacter::TryConsumeBufferedBowCharge(ABow* Bow)
{
	const bool bShouldStartDraw = bBowChargeInputBuffered
		&& bBowChargeInputHeld
		&& Bow
		&& Bow == GetEquippedBow()
		&& IsBowAiming()
		&& bBlockInputHeld
		&& ItemOwnershipComponent
		&& Attributes
		&& Attributes->IsAlive()
		&& ItemOwnershipComponent->GetLoadedAmmoQuantity(Bow->GetAmmoDefinitionId()) > 0;
	bBowChargeInputBuffered = false;
	if (bShouldStartDraw)
	{
		StartBowCharge();
	}
}

void AMyCharacter::CancelBowAim(bool bClearBlockHeld, bool bImmediateChargeFOVReset)
{
	const bool bWasBowAiming = IsBowAiming();
	CancelBowCharge(bImmediateChargeFOVReset);
	if (bClearBlockHeld)
	{
		bBlockInputHeld = false;
	}

	if (ActionState == EActionState::EAS_Aiming)
	{
		ActionState = EActionState::EAS_UnOccupied;
	}

	RestoreAimRotationMode();
	if (bWasBowAiming)
	{
		ResetCameraObstructionRecovery(true);
	}
	if (ABow* Bow = GetEquippedBow())
	{
		Bow->SetBowPresentationState(EBowPresentationState::EBPS_Relaxed);
	}

	bBowReleasePresentationPending = false;
	StopBowPresentationMontages();
	RefreshBowPresentation();
	UpdateAimReticleHUD();
}

void AMyCharacter::CancelBowCharge(bool bImmediateFOVReset)
{
	UAnimMontage* DrawMontageToStop = ActiveBowDrawMontage;
	ActiveBowDrawMontage = nullptr;
	ClearBowChargeInputState();
	bBowDrawInputHeld = false;
	bBowDrawReady = false;
	bBowChargeFOVActive = false;

	if (IsValid(PreparedBowProjectile))
	{
		PreparedBowProjectile->GetOnImpactResolved().RemoveAll(this);
		PreparedBowProjectile->Destroy();
	}
	PreparedBowProjectile = nullptr;

	if (UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
		AnimInstance && DrawMontageToStop && AnimInstance->Montage_IsPlaying(DrawMontageToStop))
	{
		AnimInstance->Montage_Stop(0.12f, DrawMontageToStop);
	}

	RestoreBowAimPresentation();

	if (bImmediateFOVReset)
	{
		ResetBowChargeCameraFOV();
	}

	RefreshBowPresentation();
}

void AMyCharacter::RestoreBowAimPresentation()
{
	if (ABow* Bow = GetEquippedBow())
	{
		Bow->SetBowPresentationState(IsBowAiming()
			? EBowPresentationState::EBPS_Aiming
			: EBowPresentationState::EBPS_Relaxed);
	}
}

bool AMyCharacter::ResolveBowAimLaunchContext(const ABow* Bow, FVector& OutSpawnLocation,
	FVector& OutLaunchDirection) const
{
	UWorld* World = GetWorld();
	if (!Bow || !World || !Camera)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: Bow launch context failed: Bow, World, or Camera is unavailable."), *GetName());
		return false;
	}

	const FVector CameraLocation = Camera->GetComponentLocation();
	const FVector CameraDirection = Camera->GetForwardVector().GetSafeNormal();
	if (CameraDirection.IsNearlyZero())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: Bow launch context failed: camera forward direction is zero."), *GetName());
		return false;
	}

	const FProjectileDeliveryConfig& DeliveryConfig = Bow->GetProjectileDeliveryConfig();
	const float AimDistance = DeliveryConfig.InitialSpeed * DeliveryConfig.MaxLifetime;
	FVector AimPoint = CameraLocation + CameraDirection * AimDistance;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BowAim), false, this);
	QueryParams.AddIgnoredActor(Bow);
	FHitResult AimHit;
	if (World->LineTraceSingleByChannel(AimHit, CameraLocation, AimPoint, ECC_Visibility, QueryParams))
	{
		AimPoint = AimHit.ImpactPoint;
	}

	FTransform LaunchTransform;
	FString FailureReason;
	if (!Bow->TryGetLaunchTransform(LaunchTransform, FailureReason))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: Bow launch context failed: %s"), *GetName(), *FailureReason);
		return false;
	}

	OutSpawnLocation = LaunchTransform.GetLocation();
	OutLaunchDirection = (AimPoint - OutSpawnLocation).GetSafeNormal();
	if (OutLaunchDirection.IsNearlyZero())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: Bow launch context failed: launch direction is zero."), *GetName());
		return false;
	}

	return true;
}

void AMyCharacter::PlayBowAimRaisePresentation()
{
	ABow* Bow = GetEquippedBow();
	UAnimMontage* AimRaiseMontage = Bow ? Bow->GetAimRaiseMontage() : nullptr;
	if (!AimRaiseMontage)
	{
		return;
	}

	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInstance || AnimInstance->Montage_Play(AimRaiseMontage) <= 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: Bow AimRaiseMontage could not play; aiming continues."), *GetName());
	}
}

bool AMyCharacter::TryStartBowReleasePresentation(ABow* Bow)
{
	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!Bow || !AnimInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: Bow release blocked: equipped Bow or AnimInstance is unavailable."), *GetName());
		return false;
	}

	UAnimMontage* ReleaseMontage = Bow->GetReleaseMontage();
	UAnimMontage* LoadMontage = Bow->GetLoadMontage();
	if (!ReleaseMontage || !LoadMontage)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("%s: Bow release blocked: ReleaseMontage and LoadMontage are required for player firing."), *GetName());
		return false;
	}

	if (bBowReleasePresentationPending || ActiveBowLoadMontage
		|| AnimInstance->Montage_IsPlaying(ReleaseMontage)
		|| (LoadMontage && AnimInstance->Montage_IsPlaying(LoadMontage)))
	{
		return false;
	}

	if (AnimInstance->Montage_Play(ReleaseMontage) <= 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: Bow release blocked: ReleaseMontage could not play."), *GetName());
		return false;
	}

	Bow->SetBowPresentationState(EBowPresentationState::EBPS_Releasing);
	bBowReleasePresentationPending = true;
	ActiveBowReleaseMontage = ReleaseMontage;
	BindMontageEndDelegate(AnimInstance, ReleaseMontage, &AMyCharacter::OnBowReleaseMontageEnded);
	RefreshBowPresentation();
	return true;
}

bool AMyCharacter::TryStartBowLoadPresentation(ABow* Bow)
{
	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!Bow || !AnimInstance || !IsBowAiming() || !bBlockInputHeld || !ItemOwnershipComponent
		|| !Attributes || !Attributes->IsAlive())
	{
		return false;
	}

	const FName AmmoDefinitionId = Bow->GetAmmoDefinitionId();
	if (ItemOwnershipComponent->GetLoadedAmmoQuantity(AmmoDefinitionId) <= 0)
	{
		return false;
	}

	UAnimMontage* LoadMontage = Bow->GetLoadMontage();
	if (!LoadMontage)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("%s: Bow reload blocked: LoadMontage is required to return to aim after Release."), *GetName());
		return false;
	}

	if (LoadMontage == Bow->GetReleaseMontage())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("%s: Bow reload blocked: LoadMontage must be different from ReleaseMontage."), *GetName());
		return false;
	}

	if (AnimInstance->Montage_IsPlaying(LoadMontage)
		|| (Bow->GetReleaseMontage() && AnimInstance->Montage_IsPlaying(Bow->GetReleaseMontage())))
	{
		return false;
	}

	if (AnimInstance->Montage_Play(LoadMontage) <= 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: Bow reload blocked: LoadMontage could not play."), *GetName());
		return false;
	}

	ActiveBowLoadMontage = LoadMontage;
	bBowReleasePresentationPending = true;
	Bow->SetBowPresentationState(EBowPresentationState::EBPS_Loading);
	BindMontageEndDelegate(AnimInstance, LoadMontage, &AMyCharacter::OnBowLoadMontageEnded);
	RefreshBowPresentation();
	return true;
}

void AMyCharacter::AbortBowReleasePresentation()
{
	UAnimMontage* MontageToStop = ActiveBowReleaseMontage;
	ActiveBowReleaseMontage = nullptr;
	bBowReleasePresentationPending = false;
	ClearBowChargeInputState();

	if (UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
		AnimInstance && MontageToStop && AnimInstance->Montage_IsPlaying(MontageToStop))
	{
		AnimInstance->Montage_Stop(0.12f, MontageToStop);
	}

	RestoreBowAimPresentation();

	RefreshBowPresentation();
}

void AMyCharacter::StopBowPresentationMontages()
{
	ActiveBowDrawMontage = nullptr;
	ActiveBowReleaseMontage = nullptr;
	ActiveBowLoadMontage = nullptr;

	ABow* Bow = GetEquippedBow();
	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!Bow || !AnimInstance)
	{
		return;
	}

	if (UAnimMontage* AimRaiseMontage = Bow->GetAimRaiseMontage();
		AimRaiseMontage && AnimInstance->Montage_IsPlaying(AimRaiseMontage))
	{
		AnimInstance->Montage_Stop(0.12f, AimRaiseMontage);
	}

	if (UAnimMontage* DrawMontage = Bow->GetDrawMontage(); DrawMontage && AnimInstance->Montage_IsPlaying(DrawMontage))
	{
		AnimInstance->Montage_Stop(0.12f, DrawMontage);
	}

	if (UAnimMontage* ReleaseMontage = Bow->GetReleaseMontage(); ReleaseMontage && AnimInstance->Montage_IsPlaying(ReleaseMontage))
	{
		AnimInstance->Montage_Stop(0.12f, ReleaseMontage);
	}

	if (UAnimMontage* LoadMontage = Bow->GetLoadMontage(); LoadMontage && AnimInstance->Montage_IsPlaying(LoadMontage))
	{
		AnimInstance->Montage_Stop(0.12f, LoadMontage);
	}
}

void AMyCharacter::RefreshBowPresentation()
{
	ABow* Bow = GetEquippedBow();
	const bool bShouldShowLoadedArrow = Bow && ItemOwnershipComponent && IsBowAiming()
		&& !bBowReleasePresentationPending
		&& !IsValid(PreparedBowProjectile)
		&& ItemOwnershipComponent->GetLoadedAmmoQuantity(Bow->GetAmmoDefinitionId()) > 0;
	if (Bow)
	{
		Bow->SetLoadedArrowVisualVisible(bShouldShowLoadedArrow);
	}
	UpdateAimReticleHUD();
}

void AMyCharacter::UpdateBowChargeCameraFOV(float DeltaTime)
{
	if (!Camera)
	{
		return;
	}

	const float TargetFOV = bBowChargeFOVActive
		? FMath::Max(1.f, CachedBaseCameraFOV - BowChargeFOVDelta)
		: CachedBaseCameraFOV;
	const float NewFOV = FMath::FInterpTo(Camera->FieldOfView, TargetFOV, DeltaTime, BowChargeFOVInterpSpeed);
	if (!FMath::IsNearlyEqual(Camera->FieldOfView, NewFOV))
	{
		Camera->SetFieldOfView(NewFOV);
	}

	if (PlayerHUDWidget)
	{
		const float ChargeDenominator = FMath::Max(BowChargeFOVDelta, KINDA_SMALL_NUMBER);
		const float ReticleCharge = FMath::Clamp((CachedBaseCameraFOV - NewFOV) / ChargeDenominator, 0.f, 1.f);
		PlayerHUDWidget->SetAimReticleCharge(ReticleCharge);
	}
}

void AMyCharacter::ResetBowChargeCameraFOV()
{
	bBowChargeFOVActive = false;
	if (Camera)
	{
		Camera->SetFieldOfView(CachedBaseCameraFOV);
	}
	if (PlayerHUDWidget)
	{
		PlayerHUDWidget->SetAimReticleCharge(0.f);
	}
}

void AMyCharacter::OnBowDrawMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (!ActiveBowDrawMontage || Montage != ActiveBowDrawMontage)
	{
		return;
	}

	ActiveBowDrawMontage = nullptr;
	if (bInterrupted || !IsBowAiming() || !bBowDrawInputHeld || !IsValid(PreparedBowProjectile)
		|| !PreparedBowProjectile->IsPreparedForActivation())
	{
		CancelBowCharge(false);
		return;
	}

	bBowDrawReady = true;
}

void AMyCharacter::OnBowReleaseMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (!ActiveBowReleaseMontage || Montage != ActiveBowReleaseMontage)
	{
		UpdateAimReticleHUD();
		return;
	}

	ActiveBowReleaseMontage = nullptr;
	if (!bInterrupted && IsBowAiming() && bBlockInputHeld)
	{
		if (TryStartBowLoadPresentation(GetEquippedBow()))
		{
			return;
		}
	}

	bBowReleasePresentationPending = false;
	ClearBowChargeInputState();
	RestoreBowAimPresentation();
	RefreshBowPresentation();
}

void AMyCharacter::OnBowLoadMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (!ActiveBowLoadMontage || Montage != ActiveBowLoadMontage)
	{
		UpdateAimReticleHUD();
		return;
	}

	ActiveBowLoadMontage = nullptr;
	bBowReleasePresentationPending = false;

	ABow* Bow = GetEquippedBow();
	const bool bCanReturnToAim = !bInterrupted
		&& Bow
		&& IsBowAiming()
		&& bBlockInputHeld
		&& Attributes
		&& Attributes->IsAlive();
	if (bCanReturnToAim)
	{
		Bow->SetBowPresentationState(EBowPresentationState::EBPS_Aiming);
		TryConsumeBufferedBowCharge(Bow);
		RefreshBowPresentation();
		return;
	}

	ClearBowChargeInputState();
	if (IsBowAiming())
	{
		CancelBowAim(false);
		return;
	}

	if (Bow)
	{
		Bow->SetBowPresentationState(EBowPresentationState::EBPS_Relaxed);
	}
	RefreshBowPresentation();
}

void AMyCharacter::HandleBowProjectileImpactResolved(AActor* HitActor, const FCombatHitResult& Result)
{
	if (!IsLocallyControlled() || !Attributes || !Attributes->IsAlive() || !PlayerHUDWidget || !IsValid(HitActor) || !Result.bResolved
		|| Result.bSuppressed || Result.bSameTeam || Result.bParried)
	{
		return;
	}

	PlayerHUDWidget->ShowBowHitMarker();
}

void AMyCharacter::HandleLoadedAmmoQuantityChanged(FName DefinitionId, int32 /*NewQuantity*/)
{
	const ABow* Bow = GetEquippedBow();
	if (!Bow || Bow->GetAmmoDefinitionId() != DefinitionId)
	{
		return;
	}

	UpdateBowAmmoHUD();
	RefreshBowPresentation();
}

bool AMyCharacter::ConsumeProjectilePrepareFailureForDebug()
{
#if UE_BUILD_SHIPPING
	return false;
#else
	if (!bFailNextProjectilePrepareForDebug)
	{
		return false;
	}

	bFailNextProjectilePrepareForDebug = false;
	return true;
#endif
}

// ==================== 移动 ====================

void AMyCharacter::Sprint()
{
	if (bIsBlocking || ActionState == EActionState::EAS_GuardBroken) return;
	bIsSprinting = true;

	// 冲刺开始时立即发出噪音（不等定时器）
	if (GetVelocity().Size2D() > 10.f && !bIsWalking)
	{
		EmitMovementNoise();
	}
}

void AMyCharacter::StopSprinting()
{
	bIsSprinting = false;
}

void AMyCharacter::Walk()
{
	if (ActionState == EActionState::EAS_GuardBroken) return;
	bIsWalking = true;
}

void AMyCharacter::StopWalking()
{
	bIsWalking = false;
}

void AMyCharacter::UpdateMovementSpeed()
{
	if (ActionState == EActionState::EAS_UsingPotion)
	{
		GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
		return;
	}

	TickSprintStamina();

	FVector Velocity = GetVelocity();
	Velocity.Z = 0.f;

	const ABow* Bow = GetEquippedBow();
	const bool bBowAiming = Bow && IsBowAiming();
	const float AimMoveSpeed = bBowAiming ? WalkSpeed * Bow->GetAimMoveSpeedMultiplier() : 0.f;
	const float SpeedMultiplier = (bIsBlocking && EquippedShield) ? EquippedShield->GetBlockMoveSpeedMultiplier() : 1.0f;

	if (!Velocity.IsNearlyZero())
	{
		float DotProduct = CalcForwardDot2D(Velocity);
		const bool bLockOnFreeRun = ShouldUseLockOnFreeRun();
		// free-run 时传 1.f，让 CalcBaseSpeed 的冲刺判断无条件生效
		float BaseSpeed = bBowAiming
			? AimMoveSpeed
			: CalcBaseSpeed((IsLockingOn() && !bLockOnFreeRun) ? DotProduct : 1.f);
		float DirectionMultiplier = 1.f;

		// 普通锁定战斗步伐：在前/侧/后之间连续插值；free-run 时不吃降速。
		if (IsLockingOn() && !bLockOnFreeRun)
		{
			const float ClampedDot = FMath::Clamp(DotProduct, -1.f, 1.f);
			if (ClampedDot >= 0.f)
			{
				DirectionMultiplier = FMath::Lerp(LockOnComponent->GetStrafeSpeedMultiplier(), 1.f, ClampedDot);
			}
			else
			{
				DirectionMultiplier = FMath::Lerp(LockOnComponent->GetStrafeSpeedMultiplier(), LockOnComponent->GetBackSpeedMultiplier(), -ClampedDot);
			}
		}

		GetCharacterMovement()->MaxWalkSpeed = BaseSpeed * DirectionMultiplier * SpeedMultiplier;
	}
	else
	{
		GetCharacterMovement()->MaxWalkSpeed = (bBowAiming ? AimMoveSpeed : RunSpeed) * SpeedMultiplier;
	}

	if (IsLocallyControlled() && FDebugDrawHelper::IsPlayerEnabled())
	{
		FDebugDrawHelper::Add(FString::Printf(TEXT("Speed: %.0f"), GetCharacterMovement()->MaxWalkSpeed), FColor::Cyan);  // [调试]
	}
}

void AMyCharacter::TickSprintStamina()
{
	if (bIsSprinting && ActionState == EActionState::EAS_UnOccupied
		&& !GetCharacterMovement()->IsFalling() && !bIsBlocking && IsCombatPresenceActive())
	{
		FVector Velocity = GetVelocity();
		Velocity.Z = 0.f;
		if (!Velocity.IsNearlyZero())
		{
			// free-run 时侧跑/后跑也扣体力，绕过 Dot > 0.2f 门槛
			float Dot = CalcForwardDot2D(Velocity);
			if (ShouldUseLockOnFreeRun() || Dot > 0.2f)
			{
				float DeltaTime = GetWorld()->GetDeltaSeconds();
				Attributes->UseStamina(12.f * DeltaTime);
				Attributes->ResetStaminaRegenCooldown();
			}
		}
	}
}

float AMyCharacter::CalcBaseSpeed(float DotProduct) const
{
	if (bIsSprinting && ActionState == EActionState::EAS_UnOccupied && DotProduct > 0.2f)
	{
		return SprintSpeed;
	}
	if (bIsWalking && ActionState == EActionState::EAS_UnOccupied)
	{
		return WalkSpeed;
	}
	return RunSpeed;
}

// ==================== 蒙太奇 ====================

void AMyCharacter::PlayBlockMontage(const FName& SectionName)
{
	UAnimMontage* BlockMontage = GetBlockMontage();
	if (!BlockMontage)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: PlayBlockMontage(%s) skipped, BlockMontage is not configured."),
		       *GetName(), *SectionName.ToString());
		return;
	}

	PlayMontageSection(BlockMontage, SectionName);
}

void AMyCharacter::BindMontageEndDelegate(UAnimInstance* AnimInstance, UAnimMontage* Montage,
                                          void (AMyCharacter::*Callback)(UAnimMontage*, bool))
{
	if (!AnimInstance || !Montage)
	{
		return;
	}

	FOnMontageEnded EndDelegate;
	EndDelegate.BindUObject(this, Callback);
	AnimInstance->Montage_SetEndDelegate(EndDelegate, Montage);
}

bool AMyCharacter::BeginAttackMontagePlayback(UAnimInstance* AnimInstance, UAnimMontage* Montage)
{
	if (!AnimInstance || !Montage)
	{
		return false;
	}

	++NextAttackPlaybackId;
	if (NextAttackPlaybackId == 0)
	{
		++NextAttackPlaybackId;
	}

	ActiveAttackMontage = Montage;
	ActiveAttackPlaybackId = NextAttackPlaybackId;

	FOnMontageEnded EndDelegate;
	const uint32 PlaybackId = ActiveAttackPlaybackId;
	EndDelegate.BindWeakLambda(this, [this, PlaybackId](UAnimMontage* EndedMontage, bool bInterrupted)
	{
		HandleActiveAttackMontageEnded(EndedMontage, bInterrupted, PlaybackId);
	});
	AnimInstance->Montage_SetEndDelegate(EndDelegate, Montage);
	return true;
}

bool AMyCharacter::RebindActiveAttackMontageEndDelegate(UAnimInstance* AnimInstance, UAnimMontage* Montage)
{
	if (!AnimInstance || !Montage || ActiveAttackMontage != Montage || ActiveAttackPlaybackId == 0)
	{
		return false;
	}

	FOnMontageEnded EndDelegate;
	const uint32 PlaybackId = ActiveAttackPlaybackId;
	EndDelegate.BindWeakLambda(this, [this, PlaybackId](UAnimMontage* EndedMontage, bool bInterrupted)
	{
		HandleActiveAttackMontageEnded(EndedMontage, bInterrupted, PlaybackId);
	});
	AnimInstance->Montage_SetEndDelegate(EndDelegate, Montage);
	return true;
}

void AMyCharacter::InvalidateActiveAttackMontagePlayback()
{
	ActiveAttackMontage = nullptr;
	ActiveAttackPlaybackId = 0;
}

void AMyCharacter::HandleActiveAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted, uint32 PlaybackId)
{
	if (!ActiveAttackMontage || Montage != ActiveAttackMontage || PlaybackId != ActiveAttackPlaybackId
		|| ActionState != EActionState::EAS_Attacking)
	{
		return;
	}

	// 先使 token 失效，再进入任何会重置 Combo、Motion Warping、旋转或体力的恢复路径。
	InvalidateActiveAttackMontagePlayback();
	OnAttackMontageEnded(Montage, bInterrupted);
}

void AMyCharacter::OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	Super::OnAttackMontageEnded(Montage, bInterrupted);

	if (bInterrupted)
	{
		CleanupInterruptedAttack();
		return;
	}

	RecoverFromAttackMontageEnd();
}

void AMyCharacter::OnHitReactEnd()
{
	const EActionState RecoveredState = RecoverActionStateAfterMontage(EActionState::EAS_Stunning, false);
	if (RecoveredState == EActionState::EAS_UnOccupied
		&& GetLastMovementInputVector().SizeSquared2D() > KINDA_SMALL_NUMBER)
	{
		StartMovementNoiseTimer();
	}
}

// ==================== 锁定 ====================

bool AMyCharacter::IsLockingOn() const
{
	return LockOnComponent && LockOnComponent->IsLockingOn();
}

AEnemy* AMyCharacter::GetLockedTarget() const
{
	return LockOnComponent ? LockOnComponent->GetLockedTarget() : nullptr;
}

void AMyCharacter::ToggleLockOn()
{
	if (IsLockingOn())
	{
		ClearLockOn();
	}
	else
	{
		FindLockOnTarget();
	}
}

bool AMyCharacter::SwitchLockOnTarget(bool bSwitchToRight)
{
	if (!LockOnComponent || !IsLockingOn()
		|| ActionState == EActionState::EAS_Dead
		|| ActionState == EActionState::EAS_Stunning
		|| ActionState == EActionState::EAS_GuardBroken)
	{
		return false;
	}

	if (!IsLockOnTargetValid())
	{
		ClearLockOn();
		return false;
	}

	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	AEnemy* NextTarget = PlayerController
		? LockOnComponent->FindScreenSideTarget(PlayerController, bSwitchToRight)
		: nullptr;
	if (!NextTarget)
	{
		return false;
	}

	// 仅转交组件内的目标标记；首次锁定路径会重缓存旋转状态，不能在锁定内复用。
	LockOnComponent->SetLockedTarget(NextTarget);
	return true;
}

void AMyCharacter::ClearLockOn()
{
	if (!IsLockingOn()) return;

	ClearCurrentLockOnTarget();
	RestoreCachedRotationState();
	ResetCameraObstructionRecovery(true);
}

void AMyCharacter::FindLockOnTarget()
{
	if (!LockOnComponent)
	{
		return;
	}

	if (AEnemy* BestTarget = LockOnComponent->FindBestTarget(GetActorLocation(), Camera->GetForwardVector()))
	{
		SetLockOnTarget(BestTarget);
	}
	else
	{
		StartCameraRecenter();
	}
}

void AMyCharacter::UpdateLockOn(float DeltaTime)
{
	if (!IsLockingOn()) return;

	// 1. 有效性检查（无论是否应用旋转都必须执行，防止 targeted 标记残留）
	if (!IsLockOnTargetValid())
	{
		if (ActionState != EActionState::EAS_Dead
			&& ActionState != EActionState::EAS_Stunning
			&& ActionState != EActionState::EAS_GuardBroken
			&& TryRetargetLockOnAfterTargetDeath())
		{
			return;
		}

		ClearLockOn();
		return;
	}

	// 2. 死亡/硬直中不应用转向
	if (ActionState == EActionState::EAS_Dead
		|| ActionState == EActionState::EAS_Stunning
		|| ActionState == EActionState::EAS_GuardBroken) return;

	if (!ShouldSuspendLockOnCameraForAim())
	{
		UpdateLockOnControlRotation(DeltaTime);
		UpdateLockOnActorFacing(DeltaTime);
	}

	// [调试]
	if (FDebugDrawHelper::IsPlayerEnabled())
	{
		if (const AEnemy* LockedTarget = GetLockedTarget())
		{
			FDebugDrawHelper::Add(FString::Printf(TEXT("LockOn: %s"), *LockedTarget->GetName()), FColor::Yellow);
		}
	}
}

bool AMyCharacter::ShouldUseLockOnFreeRun() const
{
	return IsLockingOn() && bIsSprinting
		&& ActionState == EActionState::EAS_UnOccupied
		&& !GetCharacterMovement()->IsFalling()
		&& GetLastMovementInputVector().SizeSquared2D() > KINDA_SMALL_NUMBER;
}

FVector AMyCharacter::GetLockOnFreeRunDirection() const
{
	return GetLastMovementInputVector().GetSafeNormal2D();
}

FVector AMyCharacter::GetLockOnFreeRunCameraInputLocal() const
{
	const FVector InputWorld = GetLastMovementInputVector().GetSafeNormal2D();
	const FRotator YawRot(0.f, GetControlRotation().Yaw, 0.f);
	return YawRot.UnrotateVector(InputWorld);
}

FVector AMyCharacter::GetLockOnFreeRunCameraOffsetTarget() const
{
	if (!LockOnComponent)
	{
		return FVector::ZeroVector;
	}

	const FVector Local = GetLockOnFreeRunCameraInputLocal();
	const float SideAlpha = Local.Y;   // [-1, 1]：左负右正
	const float BackAlpha = FMath::Max(0.f, -Local.X);  // [0, 1]：后撤权重

	return FVector(
		0.f,
		SideAlpha * LockOnComponent->GetFreeRunCameraSideOffset(),
		BackAlpha * LockOnComponent->GetFreeRunCameraBackHeightOffset()
	);
}

void AMyCharacter::SetMovementRotationMode(bool bOrientToMovement, bool bUseControllerYaw)
{
	GetCharacterMovement()->bOrientRotationToMovement = bOrientToMovement;
	bUseControllerRotationYaw = bUseControllerYaw;
}

void AMyCharacter::ApplyCurrentLockOnRotationMode()
{
	const bool bFreeRun = ShouldUseLockOnFreeRun();
	SetMovementRotationMode(bFreeRun, false);
}

void AMyCharacter::ApplyLockOnRotationMode()
{
	// 攻击中不覆盖旋转：普通锁定攻击保持双 true，free-run 攻击保持 Attack() 入口设的双 false
	if (ActionState == EActionState::EAS_Attacking || ActionState == EActionState::EAS_Dodging) return;
	if (IsBowAiming())
	{
		ApplyAimRotationMode();
		return;
	}

	if (IsLockingOn())
	{
		ApplyCurrentLockOnRotationMode();
	}
}

void AMyCharacter::RestoreRotationMode()
{
	if (IsBowAiming())
	{
		ApplyAimRotationMode();
		return;
	}

	if (IsLockingOn())
	{
		ApplyCurrentLockOnRotationMode();
	}
	else
	{
		SetMovementRotationMode(bCachedOrientRotationToMovement, bCachedUseControllerRotationYaw);
	}
}

void AMyCharacter::UpdateLockOnCamera(float DeltaTime)
{
	FVector SocketTarget = FVector::ZeroVector;
	float ArmLengthTarget = CachedTargetArmLength;
	float InterpSpeed = 0.f;
	GetCameraTargets(SocketTarget, ArmLengthTarget, InterpSpeed);

	SpringArm->SocketOffset = FMath::VInterpTo(
		SpringArm->SocketOffset, SocketTarget, DeltaTime, InterpSpeed);
	SpringArm->TargetArmLength = FMath::FInterpTo(
		SpringArm->TargetArmLength, FMath::Max(0.f, ArmLengthTarget), DeltaTime, InterpSpeed);
}

void AMyCharacter::ResetCameraObstructionRecovery(bool bRestoreCurrentTarget)
{
	if (!SpringArm)
	{
		return;
	}

	if (UObstructionRecoverySpringArmComponent* RecoverySpringArm = Cast<UObstructionRecoverySpringArmComponent>(SpringArm))
	{
		RecoverySpringArm->ResetObstructionRecovery();
	}

	if (!bRestoreCurrentTarget)
	{
		return;
	}

	FVector SocketTarget = FVector::ZeroVector;
	float ArmLengthTarget = 0.f;
	float InterpSpeed = 0.f;
	GetCameraTargets(SocketTarget, ArmLengthTarget, InterpSpeed);
	SpringArm->TargetArmLength = FMath::Max(0.f, ArmLengthTarget);
}

void AMyCharacter::GetCameraTargets(FVector& OutSocketTarget, float& OutArmLengthTarget, float& OutInterpSpeed) const
{
	if (IsBowAiming())
	{
		OutSocketTarget = AimCameraSocketOffset;
		OutArmLengthTarget = FMath::Max(0.f, CachedTargetArmLength + AimCameraArmLengthDelta);
		OutInterpSpeed = AimCameraInterpSpeed;
		return;
	}

	if (!LockOnComponent)
	{
		OutSocketTarget = CachedSocketOffset;
		OutArmLengthTarget = CachedTargetArmLength;
		OutInterpSpeed = 0.f;
		return;
	}

	OutSocketTarget = IsLockingOn() ? LockOnComponent->GetSocketOffset() : CachedSocketOffset;
	OutArmLengthTarget = CachedTargetArmLength;

	const bool bFreeRun = IsLockingOn() && ShouldUseLockOnFreeRun();
	if (bFreeRun)
	{
		const FVector DynamicOffset = GetLockOnFreeRunCameraOffsetTarget();
		OutSocketTarget = LockOnComponent->GetSocketOffset() + DynamicOffset;

		// 后撤时拉远弹簧臂（BackAlpha > 0 -> ArmLength bonus）
		const FVector InputLocal = GetLockOnFreeRunCameraInputLocal();
		const float BackAlpha = FMath::Max(0.f, -InputLocal.X);
		OutArmLengthTarget = CachedTargetArmLength + LockOnComponent->GetFreeRunCameraBackArmLengthBonus() * BackAlpha;
	}

	OutInterpSpeed = bFreeRun ? LockOnComponent->GetFreeRunCameraInterpSpeed() : LockOnComponent->GetSocketOffsetInterpSpeed();
}

bool AMyCharacter::ShouldSuspendLockOnCameraForAim() const
{
	return IsBowAiming() && IsLockingOn();
}

void AMyCharacter::CacheAimRotationState()
{
	if (bAimRotationStateCached)
	{
		return;
	}

	bAimRotationStateCached = true;
	if (IsLockingOn())
	{
		bCachedAimOrientRotationToMovement = bCachedOrientRotationToMovement;
		bCachedAimUseControllerRotationYaw = bCachedUseControllerRotationYaw;
		return;
	}

	bCachedAimOrientRotationToMovement = GetCharacterMovement()->bOrientRotationToMovement;
	bCachedAimUseControllerRotationYaw = bUseControllerRotationYaw;
}

void AMyCharacter::ApplyAimRotationMode()
{
	SetMovementRotationMode(false, true);
}

void AMyCharacter::RestoreAimRotationMode()
{
	if (!bAimRotationStateCached)
	{
		return;
	}

	if (IsLockingOn())
	{
		ApplyCurrentLockOnRotationMode();
	}
	else
	{
		SetMovementRotationMode(bCachedAimOrientRotationToMovement, bCachedAimUseControllerRotationYaw);
	}

	bAimRotationStateCached = false;
}

// ==================== 听觉感知 ====================

void AMyCharacter::EmitNoise(float Loudness, float MaxRange)
{
	if (GetWorld())
	{
		UAISense_Hearing::ReportNoiseEvent(
			GetWorld(),
			GetActorLocation(),
			Loudness,
			this,
			MaxRange,
			FName("PlayerNoise")
		);

		// 调试可视化
		FDebugDrawHelper::AddNoiseRange(GetWorld(), GetActorLocation(), MaxRange);
	}
}

void AMyCharacter::EmitMovementNoise()
{
	// 空中不发声
	if (GetCharacterMovement()->IsFalling())
	{
		return;
	}

	// 步行静音（潜行）
	if (bIsWalking)
	{
		return;
	}

	// 静止不发声
	float Speed2D = GetVelocity().Size2D();
	if (Speed2D < 10.f)
	{
		return;
	}

	// 根据移动状态发出不同噪音
	if (bIsSprinting)
	{
		EmitNoise(SprintNoiseLoudness, SprintNoiseRange);
	}
	else
	{
		EmitNoise(RunNoiseLoudness, RunNoiseRange);
	}
}

void AMyCharacter::StartMovementNoiseTimer()
{
	if (!GetWorldTimerManager().IsTimerActive(MovementNoiseTimerHandle))
	{
		GetWorldTimerManager().SetTimer(
			MovementNoiseTimerHandle,
			this,
			&AMyCharacter::EmitMovementNoise,
			MovementNoiseInterval,
			true
		);
		EmitMovementNoise();
	}
}

void AMyCharacter::StopMovementNoiseTimer()
{
	GetWorldTimerManager().ClearTimer(MovementNoiseTimerHandle);
}

void AMyCharacter::CacheLockOnRotationState()
{
	// SocketOffset 由 BeginPlay 初始化，不在此处覆盖
	bCachedOrientRotationToMovement = bAimRotationStateCached
		? bCachedAimOrientRotationToMovement
		: GetCharacterMovement()->bOrientRotationToMovement;
	bCachedUseControllerRotationYaw = bAimRotationStateCached
		? bCachedAimUseControllerRotationYaw
		: bUseControllerRotationYaw;
	bCachedSpringArmUsePawnControlRotation = SpringArm->bUsePawnControlRotation;
}

void AMyCharacter::EnterLockOnRotationMode()
{
	SpringArm->bUsePawnControlRotation = true;
	if (IsBowAiming())
	{
		ApplyAimRotationMode();
		return;
	}

	SetMovementRotationMode(false, false);
}

void AMyCharacter::RestoreCachedRotationState()
{
	// SocketOffset 由 Tick 插值平滑恢复，ClearLockOn 只恢复旋转控制模式
	SpringArm->bUsePawnControlRotation = bCachedSpringArmUsePawnControlRotation;
	if (IsBowAiming())
	{
		ApplyAimRotationMode();
		return;
	}

	SetMovementRotationMode(bCachedOrientRotationToMovement, bCachedUseControllerRotationYaw);
}

void AMyCharacter::ClearCurrentLockOnTarget()
{
	if (LockOnComponent)
	{
		LockOnComponent->ClearLockedTarget();
	}
}

void AMyCharacter::SetLockOnTarget(AEnemy* NewTarget)
{
	if (!NewTarget || !LockOnComponent)
	{
		return;
	}

	StopCameraRecenter();
	LockOnComponent->SetLockedTarget(NewTarget);
	CacheLockOnRotationState();
	EnterLockOnRotationMode();
	ResetCameraObstructionRecovery(true);
}

bool AMyCharacter::TryRetargetLockOnAfterTargetDeath()
{
	AEnemy* CurrentTarget = GetLockedTarget();
	if (!LockOnComponent || !Camera || !IsValid(CurrentTarget) || !CurrentTarget->GetAttributes()
		|| CurrentTarget->GetAttributes()->IsAlive())
	{
		return false;
	}

	AEnemy* ReplacementTarget = LockOnComponent->FindBestTarget(GetActorLocation(), Camera->GetForwardVector());
	if (!ReplacementTarget)
	{
		return false;
	}

	// 保持已有锁定旋转状态，只转交旧/新目标的锁定标记。
	LockOnComponent->SetLockedTarget(ReplacementTarget);
	return true;
}

bool AMyCharacter::IsLockOnTargetValid() const
{
	return LockOnComponent && LockOnComponent->IsCurrentTargetValid(GetActorLocation());
}

void AMyCharacter::UpdateLockOnControlRotation(float DeltaTime) const
{
	const AEnemy* LockedTarget = GetLockedTarget();
	if (!LockedTarget)
	{
		return;
	}

	// 锁定时保持居中的后上方视角，只让 yaw 跟随目标
	const FVector PlayerLoc = GetActorLocation();
	const FVector TargetLoc = LockedTarget->GetActorLocation();
	FRotator LookAt = UKismetMathLibrary::FindLookAtRotation(PlayerLoc, TargetLoc);
	LookAt.Pitch = LockOnComponent->GetCameraPitch();

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		const FRotator Current = PC->GetControlRotation();
		PC->SetControlRotation(FMath::RInterpTo(Current, LookAt, DeltaTime, LockOnComponent->GetRotationInterpSpeed()));
	}
}

void AMyCharacter::UpdateLockOnActorFacing(float DeltaTime)
{
	if (!IsLockingOn() || ShouldSuspendLockOnCameraForAim() || ShouldUseLockOnFreeRun() ||
		ActionState == EActionState::EAS_Attacking ||
		ActionState == EActionState::EAS_Dodging ||
		ActionState == EActionState::EAS_Stunning ||
		ActionState == EActionState::EAS_Dead)
	{
		return;
	}

	const AEnemy* LockedTarget = GetLockedTarget();
	if (!LockedTarget || !LockOnComponent)
	{
		return;
	}

	FVector ToTarget = LockedTarget->GetActorLocation() - GetActorLocation();
	ToTarget.Z = 0.f;
	if (ToTarget.IsNearlyZero())
	{
		return;
	}

	const FRotator CurrentRotation(0.f, GetActorRotation().Yaw, 0.f);
	const FRotator TargetRotation(0.f, ToTarget.Rotation().Yaw, 0.f);
	SetActorRotation(FMath::RInterpConstantTo(
		CurrentRotation,
		TargetRotation,
		DeltaTime,
		LockOnComponent->GetFacingTurnRate()));
}

void AMyCharacter::FaceDirection2D(const FVector& FacingDirection)
{
	const FRotator TargetRot(0.f, FacingDirection.Rotation().Yaw, 0.f);
	SetActorRotation(TargetRot);
}

// ==================== 提取方法 ====================

void AMyCharacter::InitializePlayerHUD()
{
	if (!IsLocallyControlled() || PlayerHUDWidget || !PlayerHUDClass)
	{
		return;
	}

	PlayerHUDWidget = CreateWidget<UPlayerHUDWidget>(GetWorld(), PlayerHUDClass);
	if (PlayerHUDWidget)
	{
		PlayerHUDWidget->AddToViewport();
		PlayerHUDWidget->BindToAttributes(Attributes);
		UpdatePotionCooldownHUD();
		UpdateAimReticleHUD();
		UpdateBowAmmoHUD();
	}
}

void AMyCharacter::UpdatePotionCooldownHUD() const
{
	if (!PlayerHUDWidget)
	{
		return;
	}

	const float Remaining = bPotionOnCooldown
		                        ? GetWorldTimerManager().GetTimerRemaining(PotionCooldownTimer)
		                        : 0.f;
	PlayerHUDWidget->SetPotionCooldown(Remaining, bPotionOnCooldown ? GetPotionCooldown(false) : 0.f);
}

void AMyCharacter::UpdateAimReticleHUD() const
{
	if (PlayerHUDWidget)
	{
		const bool bVisible = IsBowAiming();
		PlayerHUDWidget->SetAimReticleVisible(bVisible);
		PlayerHUDWidget->SetAimReticleReFireBlocked(bVisible && IsBowReFireGateBlocked());
	}
}

void AMyCharacter::UpdateBowAmmoHUD() const
{
	if (!PlayerHUDWidget)
	{
		return;
	}

	const ABow* Bow = GetEquippedBow();
	if (!Bow || !ItemOwnershipComponent)
	{
		PlayerHUDWidget->SetArrowCount(0, 0, false);
		return;
	}

	const FName AmmoDefinitionId = Bow->GetAmmoDefinitionId();
	PlayerHUDWidget->SetArrowCount(
		ItemOwnershipComponent->GetLoadedAmmoQuantity(AmmoDefinitionId),
		ItemOwnershipComponent->GetLoadedAmmoCapacity(AmmoDefinitionId),
		true);
}

void AMyCharacter::DrawDebugInfo() const
{
	if (!IsLocallyControlled() || !FDebugDrawHelper::IsPlayerEnabled()) return;
	if (!Attributes) return;

	// [调试] 角色状态面板
	FDebugDrawHelper::Add(FString::Printf(TEXT("HP: %.1f / %.1f"), Attributes->GetCurrentHealth(), Attributes->GetMaxHealth()), FColor::Red);
	FDebugDrawHelper::Add(FString::Printf(TEXT("SP: %.1f / %.1f"), Attributes->GetCurrentStamina(), Attributes->GetMaxStamina()), FColor::Green);

	FString PotionInfo = FString::Printf(TEXT("Potion: %d/%d"),
		Attributes->GetPotionCount(),
		Attributes->GetMaxPotionCount());
	if (bPotionOnCooldown)
	{
		float Remaining = GetWorldTimerManager().GetTimerRemaining(PotionCooldownTimer);
		PotionInfo += FString::Printf(TEXT(" [CD: %.1fs]"), Remaining);
	}
	FDebugDrawHelper::Add(PotionInfo, FColor::Cyan);

	static const TCHAR* ActionStateNames[] = {
		TEXT("UnOccupied"), TEXT("Attacking"), TEXT("Stunning"), TEXT("Exhausted"), TEXT("Parrying"), TEXT("Dodging"), TEXT("UsingPotion"), TEXT("Dead"), TEXT("Aiming"), TEXT("GuardBroken")
	};
	const uint8 ActionStateIndex = static_cast<uint8>(ActionState);
	const TCHAR* ActionStateName = ActionStateIndex < UE_ARRAY_COUNT(ActionStateNames)
		? ActionStateNames[ActionStateIndex]
		: TEXT("Invalid");
	FDebugDrawHelper::Add(FString::Printf(TEXT("State: %s"), ActionStateName), FColor::Yellow);

	if (SpringArm)
	{
		const UObstructionRecoverySpringArmComponent* RecoverySpringArm =
			Cast<UObstructionRecoverySpringArmComponent>(SpringArm);
		const bool bCollisionFixed = RecoverySpringArm
			? RecoverySpringArm->IsPhysicallyObstructed()
			: SpringArm->IsCollisionFixApplied();
		const bool bRecoveryActive = RecoverySpringArm && RecoverySpringArm->IsObstructionRecoveryActive();
		if (bCollisionFixed || bRecoveryActive)
		{
			FVector CameraSocketTarget = FVector::ZeroVector;
			float CameraArmTarget = 0.f;
			float CameraInterpSpeed = 0.f;
			GetCameraTargets(CameraSocketTarget, CameraArmTarget, CameraInterpSpeed);

			const FVector UnfixedCameraPosition = SpringArm->GetUnfixedCameraPosition();
			const FVector FixedCameraPosition = SpringArm->GetSocketLocation(USpringArmComponent::SocketName);
			const float CollisionDisplacement = FVector::Dist(UnfixedCameraPosition, FixedCameraPosition);
			const float RecoveryAlpha = RecoverySpringArm ? RecoverySpringArm->GetObstructionRecoveryAlpha() : 1.f;
			const float ObstructionClearElapsed = RecoverySpringArm ? RecoverySpringArm->GetObstructionClearElapsed() : 0.f;
			FDebugDrawHelper::Add(FString::Printf(
				TEXT("Camera: %s | Arm %.1f -> %.1f @ %.1f | Offset Y %.1f Z %.1f | Recovery %.3f%s"),
				bCollisionFixed ? TEXT("Blocked") : TEXT("Clear"),
				SpringArm->TargetArmLength,
				CameraArmTarget,
				CameraInterpSpeed,
				CameraSocketTarget.Y,
				CameraSocketTarget.Z,
				RecoveryAlpha,
				bRecoveryActive ? TEXT(" [active]") : TEXT("")),
				bCollisionFixed ? FColor::Orange : FColor::Cyan);
			FDebugDrawHelper::Add(FString::Printf(
				TEXT("CameraPos: Fixed Z %.1f | Unfixed Z %.1f | Delta %.1f | Clear %.3fs"),
				FixedCameraPosition.Z,
				UnfixedCameraPosition.Z,
				CollisionDisplacement,
				ObstructionClearElapsed),
				bCollisionFixed ? FColor::Orange : FColor::White);
		}
	}
	if (IsCombatPresenceActive())
	{
		const float Remaining = FMath::Max(0.f, CombatPresenceExitDelay -
			(GetWorld()->GetTimeSeconds() - LastCombatPresenceTime));
		FDebugDrawHelper::Add(FString::Printf(TEXT("Combat Presence: %.1fs"), Remaining), FColor::Orange);
	}

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	UAnimMontage* ActiveMontage = AnimInstance ? AnimInstance->GetCurrentActiveMontage() : nullptr;

	// [调试] 蒙太奇 — 仅播放时显示
	if (ActiveMontage)
	{
		const FName ActiveSection = AnimInstance->Montage_GetCurrentSection(ActiveMontage);
		FDebugDrawHelper::Add(FString::Printf(TEXT("Montage: %s [%s]"),
			*ActiveMontage->GetName(),
			ActiveSection.IsNone() ? TEXT("?") : *ActiveSection.ToString()),
			FColor::Cyan);
	}

	// [调试] 弹反 — 仅激活时显示
	if (bIsParrying || bParryOnCooldown)
	{
		FString ParryDebug;
		if (bIsParrying) ParryDebug += TEXT("[parrying] ");
		if (bParryActive) ParryDebug += TEXT("[active] ");
		if (bParryOnCooldown) ParryDebug += TEXT("[CD] ");
		FDebugDrawHelper::Add(FString::Printf(TEXT("Parry: %s"), *ParryDebug),
			bParryActive ? FColor::Yellow : FColor::White);
	}

	// [调试] 防御门卫 — 仅激活时显示
	FString BlockDebug;
	if (bBlockInputHeld) BlockDebug += TEXT("[held] ");
	if (bIsBlocking) BlockDebug += TEXT("[blocking] ");
	if (CanStartBlock()) BlockDebug += TEXT("[canStart] ");
	if (!EquippedShield) BlockDebug += TEXT("[noShield] ");
	if (GetCharacterMovement()->IsFalling()) BlockDebug += TEXT("[falling] ");
	if (!BlockDebug.IsEmpty())
	{
		FDebugDrawHelper::Add(FString::Printf(TEXT("Block: %s"), *BlockDebug),
			bIsBlocking ? FColor::Green : FColor::White);
	}

	// [调试] 防御蒙太奇 — 仅播放时显示
	UAnimMontage* BlockMontage = GetBlockMontage();
	if (AnimInstance && BlockMontage && AnimInstance->Montage_IsPlaying(BlockMontage))
	{
		const FName BlockSection = AnimInstance->Montage_GetCurrentSection(BlockMontage);
		FDebugDrawHelper::Add(FString::Printf(TEXT("BlockMontage: %s [%s]"),
			*BlockMontage->GetName(),
			BlockSection.IsNone() ? TEXT("?") : *BlockSection.ToString()),
			FColor::Green);
	}

	// [调试] 连招信息 — 仅配置了连招且在连招过程中或连招窗口打开时显示
	UAttackConfigDataAsset* AttackConfig = GetAttackConfig();
	if (AttackConfig && AttackConfig->LightAttackCombo)
	{
		FString ComboInfo = FString::Printf(
			TEXT("Combo: %d/%d %s%s (x%.1f)"),
			ComboCounter + 1,
			AttackConfig->LightAttackCombo->GetComboCount(),
			bComboWindowOpen ? TEXT("[WINDOW]") : TEXT(""),
			bComboInputReceived ? TEXT("[INPUT]") : TEXT(""),
			GetAttackDamageMultiplier()
		);
		FDebugDrawHelper::Add(ComboInfo, FColor::Orange);
	}
}

void AMyCharacter::StopBlockMontage(float BlendOutTime)
{
	UAnimMontage* BlockMontage = GetBlockMontage();
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance(); AnimInstance && BlockMontage)
	{
		// 禁用 Auto Blend Out 的持盾 Montage 可能保留末帧姿势但不再报告为 IsPlaying；
		// 对指定 Montage 的 Stop 在没有活动实例时是安全空操作。
		AnimInstance->Montage_Stop(BlendOutTime, BlockMontage);
	}
}

bool AMyCharacter::ShouldInterruptBlock() const
{
	return !EquippedShield || ActionState != EActionState::EAS_UnOccupied || GetCharacterMovement()->IsFalling();
}

void AMyCharacter::StartCameraRecenter()
{
	if (!CanUpdateCameraRecenter())
	{
		StopCameraRecenter();
		return;
	}

	// 快照当前朝向作为目标
	RecenterTargetRotation = GetActorRotation();
	RecenterTargetRotation.Pitch = RecenterTargetPitch;
	RecenterTargetRotation.Roll = 0.f;

	bRecenteringCamera = true;
}

void AMyCharacter::UpdateCameraRecenter(float DeltaTime)
{
	if (!CanUpdateCameraRecenter())
	{
		StopCameraRecenter();
		return;
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		StopCameraRecenter();
		return;
	}

	FRotator Current = PC->GetControlRotation();

	// 使用快照目标旋转，不再每帧读取 GetActorRotation()
	FRotator NewRotation = FMath::RInterpTo(Current, RecenterTargetRotation, DeltaTime, RecenterInterpSpeed);
	PC->SetControlRotation(NewRotation);

	// 到达阈值
	if (FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(NewRotation.Yaw, RecenterTargetRotation.Yaw), 1.f) &&
		FMath::IsNearlyEqual(NewRotation.Pitch, RecenterTargetRotation.Pitch, 1.f))
	{
		bRecenteringCamera = false;
	}
}

bool AMyCharacter::CanUpdateCameraRecenter() const
{
	return !IsLockingOn()
		&& !IsBowAiming()
		&& !bBonfireServiceProtected
		&& ActionState != EActionState::EAS_Dead
		&& ActionState != EActionState::EAS_Stunning
		&& ActionState != EActionState::EAS_GuardBroken
		&& Attributes
		&& Attributes->IsAlive();
}

// ==================== 状态恢复 Helpers ====================

bool AMyCharacter::ShouldRecoverToExhausted_Generic() const
{
	return IsExhaustionTimerActive();
}

bool AMyCharacter::ShouldRecoverToExhausted_Attack() const
{
	return bPendingExhaustedAfterAttack || IsExhaustionTimerActive();
}

void AMyCharacter::EnsureExhaustionRecoveryTimer()
{
	if (!IsExhaustionTimerActive())
	{
		GetWorldTimerManager().SetTimer(
			ExhaustionTimerHandle, this,
			&AMyCharacter::RecoverFromExhaustion,
			ExhaustedTime, false);
	}
}

EActionState AMyCharacter::RecoverActionStateAfterMontage(EActionState ExpectedState, bool bResumeStaminaRegen)
{
	if (ActionState != ExpectedState) return ActionState;

	if (ShouldRecoverToExhausted_Generic())
	{
		ActionState = EActionState::EAS_Exhausted;
		EnsureExhaustionRecoveryTimer();
	}
	else
	{
		ActionState = EActionState::EAS_UnOccupied;
	}

	if (bResumeStaminaRegen && Attributes)
	{
		Attributes->ResumeStaminaRegen();
	}

	return ActionState;
}

void AMyCharacter::RecoverFromAttackMontageEnd()
{
	InvalidateActiveAttackMontagePlayback();

	// 旋转恢复必须在早期还原：free-run 攻击设了双 false，打断后必须还原
	RestoreRotationMode();
	CancelChargeInputState();
	ResetCombo();

	if (ActionState == EActionState::EAS_Attacking)
	{
		if (ShouldRecoverToExhausted_Attack())
		{
			ActionState = EActionState::EAS_Exhausted;
			EnsureExhaustionRecoveryTimer();
		}
		else
		{
			ActionState = EActionState::EAS_UnOccupied;
		}
	}

	bPendingExhaustedAfterAttack = false;

	if (Attributes)
	{
		Attributes->ResumeStaminaRegen();
	}
}

void AMyCharacter::CleanupInterruptedAttack()
{
	InvalidateActiveAttackMontagePlayback();
	RecoverFromAttackMontageEnd();
}
