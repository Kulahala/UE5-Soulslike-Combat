// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/MyCharacter.h"
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
#include "Kismet/GameplayStatics.h"
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

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm);
}

void AMyCharacter::BeginPlay()
{
	Super::BeginPlay();
	Tags.Add(FName("Player"));

	if (Attributes)
	{
		Attributes->OnExhausted.AddDynamic(this, &AMyCharacter::HandleExhausted);
		Attributes->EnableHealthRegen();
	}

	InitializePlayerHUD();
}

void AMyCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// [调试] 输入状态，不受 Stunning/Dead 限制
	if (ACharacterController* CC = Cast<ACharacterController>(GetController()))
	{
		const FString InputText = CC->GetDebugInputText();
		if (!InputText.IsEmpty())
		{
			FDebugDrawHelper::Add(FString::Printf(TEXT("Input: %s"), *InputText), FColor::White);
		}
	}

	if (ActionState == EActionState::EAS_Stunning || ActionState == EActionState::EAC_Dead) return;

	if (bIsBlocking && ShouldInterruptBlock())
	{
		InterruptBlock(!EquippedShield || ActionState == EActionState::EAS_Exhausted || ActionState == EActionState::EAC_Dead);
	}
	TryResumeBlock();
	UpdateMovementSpeed();
	DrawDebugInfo();  // [调试] 角色状态面板，放在更新之后读取本帧最终状态
}

// ==================== 战斗 ====================

void AMyCharacter::Attack()
{
	if (bIsBlocking) return;
	Super::Attack();
	if (CanAttack())
	{
		Attributes->UseStamina(15.f);
		Attributes->PauseStaminaRegen();
		ActionState = EActionState::EAS_Attacking;
		PlayAttackMontage(FName("Attack2"));
	}
}

void AMyCharacter::Jump()
{
	if (bIsBlocking) return;
	if (CanJump() && ActionState != EActionState::EAS_Exhausted)
	{
		Attributes->UseStamina(10.f);
		Super::Jump();
	}
}

void AMyCharacter::GetHit_Implementation(const FVector& ImpactPoint, AActor* HitInstigator)
{
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
		InterruptBlock(false);
		ActionState = EActionState::EAS_Stunning;
		Attributes->ResumeStaminaRegen();  // 硬直接管，恢复体力暂停
	}

	ResetPendingHitContext();  // 最末层清理
}

void AMyCharacter::Die()
{
	InterruptBlock(true);
	ActionState = EActionState::EAC_Dead;

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
	InterruptBlock(true);
	ActionState = EActionState::EAS_Exhausted;
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

float AMyCharacter::TakeDamage(float DamageAmount, const struct FDamageEvent& DamageEvent,
                               class AController* EventInstigator, AActor* DamageCauser)
{
	Attributes->ReceiveDamage(DamageAmount);
	if (DamageAmount <= 0.f)
	{
		LastDamageFlashScale = 1.f;  // 100% 减伤：无掉血，手动归位防串味
	}
	if (!Attributes->IsAlive())
	{
		Die();
	}
	return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}

bool AMyCharacter::CanAttack() const
{
	return ActionState == EActionState::EAS_UnOccupied && WeaponState != EWeaponState::EWS_Unequipped &&
		ArmWeaponState == EArmWeaponState::AWS_Arming;
}

// ==================== 防御 ====================

bool AMyCharacter::CanStartBlock() const
{
	return EquippedShield
		&& ActionState == EActionState::EAS_UnOccupied
		&& !bIsArming
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
			ArmWeaponState = EArmWeaponState::AWS_Arming;
		}
	}
}

void AMyCharacter::ArmWeapon()
{
	if (bIsBlocking) return;
	if (ActionState != EActionState::EAS_UnOccupied || WeaponState == EWeaponState::EWS_Unequipped)
	{
		return;
	}

	ActionState = EActionState::EAS_Arming;
	bIsArming = true;

	if (ArmWeaponState == EArmWeaponState::AWS_Disarming)
	{
		PlayArmMontage(FName("ArmWeapon"));
	}
	else
	{
		PlayArmMontage(FName("DisarmWeapon"));
	}
}

// ==================== 移动 ====================

void AMyCharacter::Sprint()
{
	if (bIsBlocking) return;
	bIsSprinting = true;
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
	TickSprintStamina();

	FVector Velocity = GetVelocity();
	Velocity.Z = 0.f;

	float SpeedMultiplier = (bIsBlocking && EquippedShield) ? EquippedShield->BlockMoveSpeedMultiplier
		: (ArmWeaponState == EArmWeaponState::AWS_Arming) ? 0.875f : 1.0f;

	if (!Velocity.IsNearlyZero())
	{
		float DotProduct = CalcForwardDot2D(Velocity);
		float BaseSpeed = CalcBaseSpeed(DotProduct);

		// 分段移速缩放：全速(前) -> 80%(侧) -> 65%(后)
		if (DotProduct > 0.2f)
		{
			GetCharacterMovement()->MaxWalkSpeed = BaseSpeed * SpeedMultiplier;
		}
		else if (DotProduct >= -0.2f)
		{
			GetCharacterMovement()->MaxWalkSpeed = BaseSpeed * 0.8f * SpeedMultiplier;
		}
		else
		{
			GetCharacterMovement()->MaxWalkSpeed = BaseSpeed * 0.65f * SpeedMultiplier;
		}
	}
	else
	{
		GetCharacterMovement()->MaxWalkSpeed = 300.f * SpeedMultiplier;
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
			float Dot = CalcForwardDot2D(Velocity);
			if (Dot > 0.2f)
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
		return 450.f;
	}
	if (bIsWalking && ActionState == EActionState::EAS_UnOccupied)
	{
		return 150.f;
	}
	return 300.f;
}

// ==================== 蒙太奇 ====================

void AMyCharacter::PlayArmMontage(const FName& SectionName)
{
	PlayMontageSection(ArmMontage, SectionName);

	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance(); AnimInstance && ArmMontage)
	{
		FOnMontageEnded EndDelegate;
		EndDelegate.BindUObject(this, &AMyCharacter::OnArmMontageEnded);
		AnimInstance->Montage_SetEndDelegate(EndDelegate, ArmMontage);
	}
}

void AMyCharacter::PlayBlockMontage(const FName& SectionName)
{
	PlayMontageSection(BlockMontage, SectionName);
}

void AMyCharacter::OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	Super::OnAttackMontageEnded(Montage, bInterrupted);
	if (bInterrupted) return;  // 更高优先级逻辑（受击/死亡）已接管状态

	ActionState = EActionState::EAS_UnOccupied;
	Attributes->ResumeStaminaRegen();
}

void AMyCharacter::OnArmMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	bIsArming = false;
	if (bInterrupted) return;  // 更高优先级逻辑已接管状态

	ActionState = EActionState::EAS_UnOccupied;

	if (ArmWeaponState == EArmWeaponState::AWS_Arming)
	{
		ArmWeaponState = EArmWeaponState::AWS_Disarming;
	}
	else
	{
		ArmWeaponState = EArmWeaponState::AWS_Arming;
	}
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
			PlayerHUDWidget->BindToAttributes(Attributes, this);
		}
	}
}

void AMyCharacter::DrawDebugInfo() const
{
	if (!Attributes) return;

	// [调试] 角色状态面板
	FDebugDrawHelper::Add(FString::Printf(TEXT("HP: %.1f / %.1f"), Attributes->GetCurrentHealth(), Attributes->GetMaxHealth()), FColor::Red);
	FDebugDrawHelper::Add(FString::Printf(TEXT("SP: %.1f / %.1f"), Attributes->GetCurrentStamina(), Attributes->GetMaxStamina()), FColor::Green);

	static const TCHAR* ActionStateNames[] = {
		TEXT("UnOccupied"), TEXT("Attacking"), TEXT("Arming"), TEXT("Stunning"), TEXT("Exhausted"), TEXT("Dead")
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

	// [调试] 防御门卫 — 仅激活时显示
	FString BlockDebug;
	if (bBlockInputHeld) BlockDebug += TEXT("[held] ");
	if (bIsBlocking) BlockDebug += TEXT("[blocking] ");
	if (CanStartBlock()) BlockDebug += TEXT("[canStart] ");
	if (!EquippedShield) BlockDebug += TEXT("[noShield] ");
	if (bIsArming) BlockDebug += TEXT("[isArming] ");
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
