// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/MyCharacter.h"
#include "Combat/ComboDataAsset.h"
#include "Character/Components/PlayerLockOnComponent.h"
#include "Character/Controller/CharacterController.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Items/Weapon/Weapon.h"
#include "Items/Shield/Shield.h"
#include "NiagaraFunctionLibrary.h"
#include "Components/CapsuleComponent.h"
#include "HUD/PlayerHUDWidget.h"
#include "AttributeComponent/AttributeComponent.h"
#include "Enemy/Enemy.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Perception/AISense_Hearing.h"
#include "Utils/DebugDrawHelper.h"

// ==================== 生命周期 ====================

AMyCharacter::AMyCharacter()
{
	// 移动设置
	GetCharacterMovement()->RotationRate = FRotator(0.f, 600.f, 0.f);

	// 受击后退
	BaseHitKnockbackDistance = 10.f;

	// 相机组件
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(GetRootComponent());
	SpringArm->TargetArmLength = 300.f;
	SpringArm->bUsePawnControlRotation = true;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm);

	LockOnComponent = CreateDefaultSubobject<UPlayerLockOnComponent>(TEXT("LockOnComponent"));
}

void AMyCharacter::BeginPlay()
{
	Super::BeginPlay();
	Tags.Add(FName("Player"));

	// 初始化缓存为当前实际值（Blueprint 可能已覆盖）
	CachedSocketOffset = SpringArm->SocketOffset;
	CachedTargetArmLength = SpringArm->TargetArmLength;

	if (Attributes)
	{
		Attributes->OnExhausted.AddDynamic(this, &AMyCharacter::HandleExhausted);
		Attributes->EnableHealthRegen();
		Attributes->SetPotionCount(3);  // 初始3个药瓶
	}

	InitializePlayerHUD();
}

void AMyCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

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
		if (!InputText.IsEmpty())
		{
			FDebugDrawHelper::Add(FString::Printf(TEXT("Input: %s"), *InputText), FColor::White);
		}
	}

	UpdateLockOnCamera(DeltaTime);

	// UpdateLockOn 放在早退之前：内部已有死亡/硬直 guard，但有效性清理必须在所有状态下执行
	UpdateLockOn(DeltaTime);

	// 新增：相机归中（必须在早退之前，受击硬直时继续归中）
	if (bRecenteringCamera)
	{
		UpdateCameraRecenter(DeltaTime);
	}

	if (ActionState == EActionState::EAS_Stunning || ActionState == EActionState::EAS_Dead) return;

	if (bIsBlocking && ShouldInterruptBlock())
	{
		InterruptBlock(!EquippedShield || ActionState == EActionState::EAS_Exhausted || ActionState == EActionState::EAS_Dead);
	}
	TryResumeBlock();
	UpdateMovementSpeed();
	ApplyLockOnRotationMode();
	DrawDebugInfo();  // [调试] 角色状态面板，放在更新之后读取本帧最终状态
}

// ==================== 战斗 ====================

bool AMyCharacter::ShouldUseSprintAttack() const
{
	return bIsSprinting &&
	       GetLastMovementInputVector().SizeSquared2D() > KINDA_SMALL_NUMBER &&
	       WeaponState != EWeaponState::EWS_Unequipped &&
	       ActionState == EActionState::EAS_UnOccupied &&
	       !GetCharacterMovement()->IsFalling();
}

void AMyCharacter::PerformSprintAttack()
{
	// 0. 蒙太奇检查
	if (!SprintAttackMontage)
	{
		UE_LOG(LogTemp, Warning, TEXT("SprintAttackMontage not configured"));
		return;
	}

	// 1. 清理旧连招状态
	ResetCombo();

	// 2. 扣除体力并暂停恢复（支持透支）
	if (Attributes)
	{
		Attributes->UseStamina(SprintAttackStaminaCost);
		Attributes->PauseStaminaRegen();
	}

	// 3. 设置伤害倍率
	SetAttackDamageMultiplier(SprintAttackDamageMultiplier);

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

	// 7. 播放攻击蒙太奇
	ActionState = EActionState::EAS_Attacking;
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance)
	{
		AnimInstance->Montage_Play(SprintAttackMontage);

		// 冲刺攻击发出噪音
		EmitNoise(AttackNoiseLoudness, AttackNoiseRange);

		// 绑定结束回调（复用 OnAttackMontageEnded）
		FOnMontageEnded MontageEndedDelegate;
		MontageEndedDelegate.BindUObject(this, &AMyCharacter::OnAttackMontageEnded);
		AnimInstance->Montage_SetEndDelegate(MontageEndedDelegate, SprintAttackMontage);
	}
}

void AMyCharacter::Attack()
{
	if (bIsBlocking) return;  // 保留现有守卫

	// 冲刺攻击优先级最高，放在 CanAttack() 之前
	if (ShouldUseSprintAttack())
	{
		PerformSprintAttack();
		return;
	}

	Super::Attack();  // 保留

	if (!CanAttack()) return;

	// 连招系统检查
	if (!LightAttackCombo || !LightAttackCombo->ComboMontage)
	{
		UE_LOG(LogTemp, Warning, TEXT("LightAttackCombo not configured"));
		return;
	}

	// 获取当前段配置
	const FComboSegment* Segment = LightAttackCombo->GetSegment(ComboCounter);
	if (!Segment)
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid combo segment: %d"), ComboCounter);
		ResetCombo();
		return;
	}

	// 体力检查（支持透支）
	Attributes->UseStamina(Segment->StaminaCost);

	// 设置伤害倍率
	SetAttackDamageMultiplier(Segment->DamageMultiplier);

	// 暂停体力恢复 + 设置状态（保持原有顺序）
	Attributes->PauseStaminaRegen();
	ActionState = EActionState::EAS_Attacking;

	// 播放对应段的蒙太奇
	PlayAttackMontage(Segment->SectionName);

	// 普通攻击发出噪音
	EmitNoise(AttackNoiseLoudness, AttackNoiseRange);

	// 清除旧的输入标记
	bComboInputReceived = false;

	UE_LOG(LogTemp, Log, TEXT("Attack Segment %d: %s (Damage x%.1f)"), 
	       ComboCounter, *Segment->SectionName.ToString(), Segment->DamageMultiplier);
}

void AMyCharacter::PlayAttackMontage(const FName& SectionName)
{
	UAnimMontage* MontageToPlay = (LightAttackCombo && LightAttackCombo->ComboMontage) ? LightAttackCombo->ComboMontage.Get() : AttackMontage;
	PlayMontageSection(MontageToPlay, SectionName);

	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance(); AnimInstance && MontageToPlay)
	{
		FOnMontageEnded EndDelegate;
		EndDelegate.BindUObject(this, &AMyCharacter::OnAttackMontageEnded);
		AnimInstance->Montage_SetEndDelegate(EndDelegate, MontageToPlay);
	}
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
	SetAttackDamageMultiplier(1.0f);

	UE_LOG(LogTemp, Log, TEXT("Combo reset"));
}

void AMyCharacter::Jump()
{
	if (bIsBlocking) return;
	if (ActionState == EActionState::EAS_UsingPotion) return;
	if (CanJump() && ActionState != EActionState::EAS_Exhausted)
	{
		Attributes->UseStamina(10.f);
		Super::Jump();
	}
}

void AMyCharacter::GetHit_Implementation(const FVector& ImpactPoint, AActor* HitInstigator)
{
	if (bDodgeInvulnerable) return;  // 翻滚无敌帧

	if (ActionState == EActionState::EAS_UsingPotion)
	{
		InterruptPotion();
	}

	// 攻击霸体：扣血+击退+相机晃动+音效粒子，但不播放受击蒙太奇
	if (bAttackHyperArmor)
	{
		// 手动复制 Super 中需要的逻辑，跳过 DirectionalHitReact()
		IHitInterface::GetHit_Implementation(ImpactPoint, HitInstigator);

		if (Attributes->IsAlive())
		{
			ConsumePendingHitKnockback();  // 击退
		}

		if (!PendingHitContext.bWasBlocked)
		{
			PlayHitEffects(ImpactPoint);  // 音效+粒子
		}

		// 受击相机晃动
		if (HitReceivedCameraShake)
		{
			if (APlayerController* PC = Cast<APlayerController>(GetController()))
			{
				PC->ClientStartCameraShake(HitReceivedCameraShake);
			}
		}

		ResetPendingHitContext();
		return;
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

	if (Attributes->IsAlive() && PendingHitContext.bApplyStun)
	{
		ResetCombo();
		InterruptBlock(false);
		if (bIsParrying) InterruptParry();
		ActionState = EActionState::EAS_Stunning;
		Attributes->ResumeStaminaRegen();  // 硬直接管，恢复体力暂停

		// 硬直时停止移动噪音
		StopMovementNoiseTimer();
	}

	ResetPendingHitContext();  // 最末层清理
}

void AMyCharacter::Die()
{
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
	ClearLockOn();
	bRecenteringCamera = false; // 新增：死亡时中断归中
	ActionState = EActionState::EAS_Dead;

	// 停止移动噪音定时器
	StopMovementNoiseTimer();

	// 停止移动
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();

	// 关闭碰撞
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 播放死亡蒙太奇
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && DeathMontage)
	{
		AnimInstance->Montage_Stop(0.1f);
		AnimInstance->Montage_Play(DeathMontage);
	}
}

void AMyCharacter::HandleExhausted()
{
	if (ActionState == EActionState::EAS_UsingPotion) return;

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

bool AMyCharacter::IsExhaustionTimerActive() const
{
	return GetWorldTimerManager().IsTimerActive(ExhaustionTimerHandle);
}

float AMyCharacter::TakeDamage(float DamageAmount, const struct FDamageEvent& DamageEvent,
                                class AController* EventInstigator, AActor* DamageCauser)
{
	if (bDodgeInvulnerable) return 0.f;  // 翻滚无敌帧

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
	return ActionState == EActionState::EAS_UnOccupied && WeaponState != EWeaponState::EWS_Unequipped;
}

// ==================== 防御 ====================

bool AMyCharacter::CanStartBlock() const
{
	return EquippedShield
		&& ActionState == EActionState::EAS_UnOccupied
		&& !GetCharacterMovement()->IsFalling();
}

void AMyCharacter::StartBlockInput()
{
	bBlockInputHeld = true;
	bIsSprinting = false;
	TryResumeBlock();
}

void AMyCharacter::ReleaseBlockInput()
{
	bBlockInputHeld = false;
	bIsBlocking = false;
	StopBlockMontage(0.2f);
}

void AMyCharacter::InterruptBlock(bool bClearHeld)
{
	bIsBlocking = false;
	if (bClearHeld) bBlockInputHeld = false;
	StopBlockMontage(0.1f);
}

void AMyCharacter::TryResumeBlock()
{
	if (bBlockInputHeld && !bIsBlocking && CanStartBlock())
	{
		bIsBlocking = true;
		PlayBlockMontage(FName("BlockRaise"));
	}
}

FBlockResult AMyCharacter::TryBlockHit(const FVector& ImpactPoint, float IncomingDamage,
                                        AActor* Attacker, AActor* DamageCauser)
{
	FBlockResult Result;
	Result.DamageAfterBlock = IncomingDamage;

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
				float CosHalf = FMath::Cos(FMath::DegreesToRadians(EquippedShield->BlockHalfAngleDegrees));
				if (Dot < CosHalf) return Result; // 角度不匹配，弹反失败
			}

			// 弹反成功！完全免伤 + 攻击方硬直
			Result.bBlocked = true;
			Result.bParried = true;
			Result.DamageAfterBlock = 0.f;
			Result.bPlayNormalHitReact = false;
			Result.ParryStaggerDuration = EquippedShield->ParryStaggerDuration;
			Result.ParryStaggerPlayRate = EquippedShield->ParryStaggerPlayRate;
			if (EquippedShield->ParrySound)
			{
				UGameplayStatics::PlaySoundAtLocation(this, EquippedShield->ParrySound, ImpactPoint);
			}
			if (EquippedShield->ParryParticle)
			{
				UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, EquippedShield->ParryParticle, ImpactPoint);
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
	float CosHalf = FMath::Cos(FMath::DegreesToRadians(EquippedShield->BlockHalfAngleDegrees));
	if (Dot < CosHalf) return Result;

	float StaminaCost = IncomingDamage * EquippedShield->BlockStaminaCostPerDamage;
	if (Attributes->GetCurrentStamina() < StaminaCost) return Result;

	Attributes->UseStamina(StaminaCost);
	Result.bBlocked = true;
	Result.DamageAfterBlock = IncomingDamage * EquippedShield->BlockedDamageMultiplier;
	Result.bPlayNormalHitReact = false;
	LastDamageFlashScale = EquippedShield->BlockedDamageMultiplier;  // 染红按减伤率缩放

	// 格挡反馈
	if (EquippedShield->BlockSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, EquippedShield->BlockSound, ImpactPoint);
	}
	if (EquippedShield->BlockParticle)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, EquippedShield->BlockParticle, ImpactPoint);
	}
	return Result;
}

// ==================== 弹反 ====================

bool AMyCharacter::CanStartParry() const
{
	return EquippedShield
		&& ActionState == EActionState::EAS_UnOccupied
		&& !bIsBlocking
		&& !bParryOnCooldown
		&& Attributes
		&& !GetCharacterMovement()->IsFalling();
}

void AMyCharacter::Input_Parry()
{
	if (!CanStartParry()) return;

	// 先确认蒙太奇可播放，再扣体力和进入状态（防止卡在 EAS_Parrying）
	if (!ParryMontage || !GetMesh() || !GetMesh()->GetAnimInstance()) return;

	Attributes->UseStamina(EquippedShield->ParryStaminaCost);
	Attributes->ResetStaminaRegenCooldown();

	bIsParrying = true;
	bParryActive = false;
	ActionState = EActionState::EAS_Parrying;
	PlayMontageSection(ParryMontage, FName("Parry"));

	UAnimInstance* Anim = GetMesh()->GetAnimInstance();
	FOnMontageEnded EndDelegate;
	EndDelegate.BindUObject(this, &AMyCharacter::OnParryMontageEnded);
	Anim->Montage_SetEndDelegate(EndDelegate, ParryMontage);
}

void AMyCharacter::SetParryActive(bool bActive)
{
	bParryActive = bActive;
}

void AMyCharacter::SetDodgeInvulnerable(bool bInvulnerable)
{
	bDodgeInvulnerable = bInvulnerable;
}

void AMyCharacter::SetAttackHyperArmor(bool bHyperArmor)
{
	bAttackHyperArmor = bHyperArmor;
}

void AMyCharacter::StartParryCooldown()
{
	const float Cooldown = EquippedShield ? EquippedShield->ParryCooldown : 0.4f;
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

	if (IsExhaustionTimerActive())
	{
		ActionState = EActionState::EAS_Exhausted;
		bIsParrying = false;
		bParryActive = false;
		StartParryCooldown();
		return;
	}

	ActionState = EActionState::EAS_UnOccupied;
	bIsParrying = false;
	bParryActive = false;
	StartParryCooldown();
}

// ==================== 翻滚 ====================

bool AMyCharacter::CanDodge() const
{
	return ActionState == EActionState::EAS_UnOccupied
		&& !GetCharacterMovement()->IsFalling()
		&& Attributes;
}

void AMyCharacter::Dodge()
{
	if (!CanDodge()) return;
	if (!DodgeMontage || !GetMesh() || !GetMesh()->GetAnimInstance()) return;

	ResetCombo();

	if (bIsBlocking) InterruptBlock(true);
	if (bIsParrying) InterruptParry();

	FVector DodgeDir = ComputeDodgeDirection();
	FName Section = SelectDodgeSection(DodgeDir);

	SetMovementRotationMode(false, false);

	Attributes->UseStamina(DodgeStaminaCost);
	Attributes->PauseStaminaRegen();

	ActionState = EActionState::EAS_Dodging;

	PlayMontageSection(DodgeMontage, Section);

	// 翻滚发出单次噪音
	EmitNoise(DodgeNoiseLoudness, DodgeNoiseRange);

	// 停止移动噪音定时器（翻滚期间不持续发声）
	StopMovementNoiseTimer();

	UAnimInstance* Anim = GetMesh()->GetAnimInstance();
	FOnMontageEnded EndDelegate;
	EndDelegate.BindUObject(this, &AMyCharacter::OnDodgeMontageEnded);
	Anim->Montage_SetEndDelegate(EndDelegate, DodgeMontage);
}

FVector AMyCharacter::ComputeDodgeDirection() const
{
	FVector InputDir = GetLastMovementInputVector().GetSafeNormal2D();
	if (!InputDir.IsNearlyZero()) return InputDir;

	if (IsLockingOn())
	{
		return -GetControlRotation().Vector().GetSafeNormal2D();
	}
	return -GetActorForwardVector().GetSafeNormal2D();
}

FName AMyCharacter::SelectDodgeSection(const FVector& WorldDirection) const
{
	// 无移动输入：直接后跳（不转身）
	if (GetLastMovementInputVector().IsNearlyZero())
	{
		return FName("Dodge_B");
	}

	// 非锁定 + 有输入：前滚（会转身面向输入方向）
	if (!IsLockingOn())
	{
		return FName("Dodge_F");
	}

	// 锁定 + 有输入：根据方向判断
	FVector LocalDir = GetActorRotation().UnrotateVector(WorldDirection);
	float X = LocalDir.X;
	float Y = LocalDir.Y;

	const float Threshold = 0.3f;

	if (FMath::Abs(Y) > FMath::Abs(X) && FMath::Abs(Y) > Threshold)
	{
		return Y > 0.f ? FName("Dodge_R") : FName("Dodge_L");
	}

	if (X > Threshold)
	{
		return FName("Dodge_F");
	}

	return FName("Dodge_B");
}

void AMyCharacter::OnDodgeMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	bDodgeInvulnerable = false;
	RestoreRotationMode();

	if (bInterrupted) return;

	if (IsExhaustionTimerActive())
	{
		ActionState = EActionState::EAS_Exhausted;
		Attributes->ResumeStaminaRegen();
		return;
	}

	ActionState = EActionState::EAS_UnOccupied;
	Attributes->ResumeStaminaRegen();

	// 翻滚结束后，如果仍在移动则重启定时器
	if (GetLastMovementInputVector().SizeSquared2D() > KINDA_SMALL_NUMBER)
	{
		StartMovementNoiseTimer();
	}
}

// ==================== 装备 ====================

void AMyCharacter::Equip()
{
	if (bIsBlocking) return;

	if (OverLapItem && OverLapItem->Implements<UPickupInterface>() && !OverLapItem->GetOwner())
	{
		// 盾牌：挂到左手
		if (AShield* Shield = Cast<AShield>(OverLapItem))
		{
			if (!EquippedShield)
			{
				Shield->EquipToOffhand(GetMesh(), Shield->OffhandSocketName, this);
				EquippedShield = Shield;
			}
			return;
		}

		// 武器：继续走原逻辑
		if (WeaponState == EWeaponState::EWS_Unequipped)
		{
			IPickupInterface::Execute_OnPickup(OverLapItem, this);
			if (AWeapon* Weapon = Cast<AWeapon>(OverLapItem))
			{
				EquippedWeapon = Weapon;
			}
			WeaponState = EWeaponState::EWS_OneHandEquipped;
		}
	}
}

// ==================== 药瓶 ====================

bool AMyCharacter::CanUsePotion() const
{
	return (ActionState == EActionState::EAS_UnOccupied || ActionState == EActionState::EAS_Exhausted)
		&& !GetCharacterMovement()->IsFalling()
		&& Attributes && Attributes->HasPotion()
		&& !bPotionOnCooldown
		&& Attributes->GetHealthPercent() < 1.0f;
}

void AMyCharacter::UsePotion()
{
	if (!CanUsePotion()) return;

	if (Attributes->UsePotion())
	{
		if (bIsSprinting) StopSprinting();

		if (PotionMontage)
		{
			ActionState = EActionState::EAS_UsingPotion;
			PlayPotionMontage();
			EmitNoise(PotionNoiseLoudness, PotionNoiseRange);
		}
		else
		{
			HealFromPotion(0.5f);
			StartPotionCooldown();
		}
	}
}

void AMyCharacter::PlayPotionMontage()
{
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && PotionMontage)
	{
		AnimInstance->Montage_Play(PotionMontage);
		FOnMontageEnded EndDelegate;
		EndDelegate.BindUObject(this, &AMyCharacter::OnPotionMontageEnded);
		AnimInstance->Montage_SetEndDelegate(EndDelegate, PotionMontage);
	}
}

void AMyCharacter::OnPotionMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (bInterrupted) return;  // InterruptPotion() 已处理
	if (ActionState != EActionState::EAS_UsingPotion) return;

	if (IsExhaustionTimerActive())
	{
		ActionState = EActionState::EAS_Exhausted;
		StartPotionCooldown();
		return;
	}
	ActionState = EActionState::EAS_UnOccupied;
	StartPotionCooldown();
}

void AMyCharacter::HealFromPotion(float Percent)
{
	// 状态守卫：防止蒙太奇被打断后，残留的AnimNotify仍然触发恢复
	// 例外：没有蒙太奇时（fallback路径），允许在任何状态下恢复
	if (PotionMontage != nullptr && ActionState != EActionState::EAS_UsingPotion)
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
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
		AnimInstance && PotionMontage && AnimInstance->Montage_IsPlaying(PotionMontage))
	{
		AnimInstance->Montage_Stop(0.1f, PotionMontage);
	}
	ActionState = EActionState::EAS_UnOccupied;
	StartPotionCooldown();
}

void AMyCharacter::StartPotionCooldown()
{
	if (PotionCooldown <= 0.f)
	{
		bPotionOnCooldown = false;
		return;
	}
	bPotionOnCooldown = true;
	GetWorldTimerManager().SetTimer(
		PotionCooldownTimer, this, &AMyCharacter::ResetPotionCooldown, PotionCooldown, false);
}

void AMyCharacter::ResetPotionCooldown()
{
	bPotionOnCooldown = false;
}

// ==================== 移动 ====================

void AMyCharacter::Sprint()
{
	if (bIsBlocking) return;
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

	float SpeedMultiplier = (bIsBlocking && EquippedShield) ? EquippedShield->BlockMoveSpeedMultiplier : 1.0f;

	if (!Velocity.IsNearlyZero())
	{
		float DotProduct = CalcForwardDot2D(Velocity);
		const bool bLockOnFreeRun = ShouldUseLockOnFreeRun();
		// free-run 时传 1.f，让 CalcBaseSpeed 的冲刺判断无条件生效
		float BaseSpeed = CalcBaseSpeed((IsLockingOn() && !bLockOnFreeRun) ? DotProduct : 1.f);
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
		GetCharacterMovement()->MaxWalkSpeed = RunSpeed * SpeedMultiplier;
	}

	FDebugDrawHelper::Add(FString::Printf(TEXT("Speed: %.0f"), GetCharacterMovement()->MaxWalkSpeed), FColor::Cyan);  // [调试]
}

void AMyCharacter::TickSprintStamina()
{
	if (bIsSprinting && ActionState == EActionState::EAS_UnOccupied
		&& !GetCharacterMovement()->IsFalling() && !bIsBlocking)
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
	PlayMontageSection(BlockMontage, SectionName);
}

void AMyCharacter::OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	Super::OnAttackMontageEnded(Montage, bInterrupted);

	// 旋转恢复必须在 interrupted guard 之前：free-run 攻击设了双 false，打断后必须还原
	RestoreRotationMode();

	if (bInterrupted)
	{
		ResetCombo();
		// 打断时必须恢复状态，否则会永远卡在 EAS_Attacking
		if (ActionState == EActionState::EAS_Attacking)
		{
			ActionState = EActionState::EAS_UnOccupied;
		}
		return;
	}

	// 连招续接判断（在状态恢复之前）
	const bool bShouldContinueCombo = bComboInputReceived &&
	                                   LightAttackCombo &&
	                                   (ComboCounter + 1) < LightAttackCombo->GetComboCount();

	const bool bWillExhaust = IsExhaustionTimerActive();

	if (bShouldContinueCombo && !bWillExhaust)
	{
		// 连招续接：临时恢复 UnOccupied 让 CanAttack() 通过
		ComboCounter++;
		bComboInputReceived = false;
		ActionState = EActionState::EAS_UnOccupied;  // 临时设置，Attack() 会立刻改回 EAS_Attacking
		Attack();
		return;  // 不执行后续状态恢复
	}

	// 连招结束，恢复状态
	ResetCombo();

	if (ActionState == EActionState::EAS_Attacking)
	{
		ActionState = EActionState::EAS_UnOccupied;
	}

	// 体力恢复处理（保持原有逻辑）
	if (bWillExhaust)
	{
		ActionState = EActionState::EAS_Exhausted;
		Attributes->ResumeStaminaRegen();
	}
	else
	{
		Attributes->ResumeStaminaRegen();
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

void AMyCharacter::ClearLockOn()
{
	if (!IsLockingOn()) return;

	ClearCurrentLockOnTarget();
	RestoreCachedRotationState();
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
		ClearLockOn();
		return;
	}

	// 2. 死亡/硬直中不应用转向
	if (ActionState == EActionState::EAS_Dead || ActionState == EActionState::EAS_Stunning) return;

	UpdateLockOnControlRotation(DeltaTime);

	// [调试]
	if (const AEnemy* LockedTarget = GetLockedTarget())
	{
		FDebugDrawHelper::Add(FString::Printf(TEXT("LockOn: %s"), *LockedTarget->GetName()), FColor::Yellow);
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
	SetMovementRotationMode(bFreeRun, !bFreeRun);
}

void AMyCharacter::ApplyLockOnRotationMode()
{
	// 攻击中不覆盖旋转：普通锁定攻击保持双 true，free-run 攻击保持 Attack() 入口设的双 false
	if (ActionState == EActionState::EAS_Attacking || ActionState == EActionState::EAS_Dodging) return;

	if (IsLockingOn())
	{
		ApplyCurrentLockOnRotationMode();
	}
}

void AMyCharacter::RestoreRotationMode()
{
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
	if (!LockOnComponent)
	{
		return;
	}

	FVector SocketTarget = FVector::ZeroVector;
	float ArmLengthTarget = CachedTargetArmLength;
	float InterpSpeed = LockOnComponent->GetSocketOffsetInterpSpeed();
	GetLockOnCameraTargets(SocketTarget, ArmLengthTarget, InterpSpeed);

	SpringArm->SocketOffset = FMath::VInterpTo(
		SpringArm->SocketOffset, SocketTarget, DeltaTime, InterpSpeed);
	SpringArm->TargetArmLength = FMath::FInterpTo(
		SpringArm->TargetArmLength, ArmLengthTarget, DeltaTime, InterpSpeed);
}

void AMyCharacter::GetLockOnCameraTargets(FVector& OutSocketTarget, float& OutArmLengthTarget, float& OutInterpSpeed) const
{
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
	bCachedOrientRotationToMovement = GetCharacterMovement()->bOrientRotationToMovement;
	bCachedUseControllerRotationYaw = bUseControllerRotationYaw;
	bCachedSpringArmUsePawnControlRotation = SpringArm->bUsePawnControlRotation;
}

void AMyCharacter::EnterLockOnRotationMode()
{
	SetMovementRotationMode(false, true);
	SpringArm->bUsePawnControlRotation = true;
}

void AMyCharacter::RestoreCachedRotationState()
{
	// SocketOffset 由 Tick 插值平滑恢复，ClearLockOn 只恢复旋转控制模式
	SetMovementRotationMode(bCachedOrientRotationToMovement, bCachedUseControllerRotationYaw);
	SpringArm->bUsePawnControlRotation = bCachedSpringArmUsePawnControlRotation;
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

	bRecenteringCamera = false; // 新增：锁定时中断归中
	LockOnComponent->SetLockedTarget(NewTarget);
	CacheLockOnRotationState();
	EnterLockOnRotationMode();
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

	// 只锁 yaw，插值转向目标
	const FVector PlayerLoc = GetActorLocation();
	const FVector TargetLoc = LockedTarget->GetActorLocation();
	FRotator LookAt = UKismetMathLibrary::FindLookAtRotation(PlayerLoc, TargetLoc);
	LookAt.Pitch = 0.f;

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		const FRotator Current = PC->GetControlRotation();
		PC->SetControlRotation(FMath::RInterpTo(Current, LookAt, DeltaTime, LockOnComponent->GetRotationInterpSpeed()));
	}
}

void AMyCharacter::FaceDirection2D(const FVector& FacingDirection)
{
	const FRotator TargetRot(0.f, FacingDirection.Rotation().Yaw, 0.f);
	SetActorRotation(TargetRot);
}

// ==================== 提取方法 ====================

void AMyCharacter::InitializePlayerHUD()
{
	if (PlayerHUDClass)
	{
		PlayerHUDWidget = CreateWidget<UPlayerHUDWidget>(GetWorld(), PlayerHUDClass);
		if (PlayerHUDWidget)
		{
			PlayerHUDWidget->AddToViewport();
			PlayerHUDWidget->BindToAttributes(Attributes);
		}
	}
}

void AMyCharacter::DrawDebugInfo() const
{
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
		TEXT("UnOccupied"), TEXT("Attacking"), TEXT("Stunning"), TEXT("Exhausted"), TEXT("Parrying"), TEXT("Dodging"), TEXT("UsingPotion"), TEXT("Dead")
	};
	FDebugDrawHelper::Add(FString::Printf(TEXT("State: %s"), ActionStateNames[static_cast<uint8>(ActionState)]), FColor::Yellow);

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
	if (AnimInstance && BlockMontage && AnimInstance->Montage_IsPlaying(BlockMontage))
	{
		const FName BlockSection = AnimInstance->Montage_GetCurrentSection(BlockMontage);
		FDebugDrawHelper::Add(FString::Printf(TEXT("BlockMontage: %s [%s]"),
			*BlockMontage->GetName(),
			BlockSection.IsNone() ? TEXT("?") : *BlockSection.ToString()),
			FColor::Green);
	}

	// [调试] 连招信息 — 仅配置了连招且在连招过程中或连招窗口打开时显示
	if (LightAttackCombo)
	{
		FString ComboInfo = FString::Printf(
			TEXT("Combo: %d/%d %s%s (x%.1f)"),
			ComboCounter + 1,
			LightAttackCombo->GetComboCount(),
			bComboWindowOpen ? TEXT("[WINDOW]") : TEXT(""),
			bComboInputReceived ? TEXT("[INPUT]") : TEXT(""),
			GetAttackDamageMultiplier()
		);
		FDebugDrawHelper::Add(ComboInfo, FColor::Orange);
	}
}

void AMyCharacter::StopBlockMontage(float BlendOutTime)
{
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
		AnimInstance && BlockMontage && AnimInstance->Montage_IsPlaying(BlockMontage))
	{
		AnimInstance->Montage_Stop(BlendOutTime, BlockMontage);
	}
}

bool AMyCharacter::ShouldInterruptBlock() const
{
	return !EquippedShield || ActionState != EActionState::EAS_UnOccupied || GetCharacterMovement()->IsFalling();
}

void AMyCharacter::StartCameraRecenter()
{
	if (IsLockingOn()) return; // 锁定中不归中
	
	// 快照当前朝向作为目标
	RecenterTargetRotation = GetActorRotation();
	RecenterTargetRotation.Pitch = RecenterTargetPitch;
	RecenterTargetRotation.Roll = 0.f;

	bRecenteringCamera = true;
}

void AMyCharacter::UpdateCameraRecenter(float DeltaTime)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;

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
