// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/MyCharacter.h"

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
	}

	// 玩家 HUD
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

void AMyCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (ActionState == EActionState::EAS_Stunning || ActionState == EActionState::EAC_Dead) return;

	// 防御打断检查
	if (bIsBlocking && (!EquippedShield || ActionState != EActionState::EAS_UnOccupied || GetCharacterMovement()->IsFalling()))
	{
		InterruptBlock(!EquippedShield || ActionState == EActionState::EAS_Exhausted || ActionState == EActionState::EAC_Dead);
	}
	TryResumeBlock();

	UpdateMovementSpeed();

	if (Attributes)
	{
		FDebugDrawHelper::Add(FString::Printf(TEXT("HP: %.1f / %.1f"), Attributes->GetCurrentHealth(), Attributes->GetMaxHealth()), FColor::Red);
		FDebugDrawHelper::Add(FString::Printf(TEXT("SP: %.1f / %.1f"), Attributes->GetCurrentStamina(), Attributes->GetMaxStamina()), FColor::Green);

		static const TCHAR* ActionStateNames[] = {
			TEXT("UnOccupied"), TEXT("Attacking"), TEXT("Arming"), TEXT("Stunning"), TEXT("Exhausted"), TEXT("Dead")
		};
		FDebugDrawHelper::Add(FString::Printf(TEXT("State: %s"), ActionStateNames[static_cast<uint8>(ActionState)]), FColor::Yellow);

		UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
		UAnimMontage* ActiveMontage = AnimInstance ? AnimInstance->GetCurrentActiveMontage() : nullptr;
		FString MontageName = ActiveMontage ? ActiveMontage->GetName() : TEXT("None");
		FDebugDrawHelper::Add(FString::Printf(TEXT("Montage: %s"), *MontageName), FColor::Cyan);
	}
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
	if (Attributes->IsAlive())
	{
		InterruptBlock(false);
		ActionState = EActionState::EAS_Stunning;
		Attributes->ResumeStaminaRegen();  // 硬直接管，恢复体力暂停
	}
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
		&& ArmWeaponState == EArmWeaponState::AWS_Arming
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
	// TODO: 收盾动画
}

void AMyCharacter::InterruptBlock(bool bClearHeld)
{
	bIsBlocking = false;
	if (bClearHeld) bBlockInputHeld = false;
	// TODO: 收盾动画
}

void AMyCharacter::TryResumeBlock()
{
	if (bBlockInputHeld && !bIsBlocking && CanStartBlock())
	{
		bIsBlocking = true;
		// TODO: 举盾动画
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
	float Dot = FVector::DotProduct(GetActorForwardVector().GetSafeNormal2D(), ToAttacker);
	float CosHalf = FMath::Cos(FMath::DegreesToRadians(EquippedShield->BlockHalfAngleDegrees));
	if (Dot < CosHalf) return Result;

	float StaminaCost = IncomingDamage * EquippedShield->BlockStaminaCostPerDamage;
	if (Attributes->GetCurrentStamina() < StaminaCost) return Result;

	Attributes->UseStamina(StaminaCost);
	Result.bBlocked = true;
	Result.DamageAfterBlock = IncomingDamage * EquippedShield->BlockedDamageMultiplier;
	Result.bPlayNormalHitReact = false;

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
	FVector Velocity = GetVelocity();
	Velocity.Z = 0.f;

	// 奔跑每帧扣耐力（仅地面，防御中不扣）
	if (bIsSprinting && ActionState == EActionState::EAS_UnOccupied && !GetCharacterMovement()->IsFalling() && !Velocity.IsNearlyZero() && !bIsBlocking)
	{
		FVector Forward = GetActorForwardVector();
		Forward.Z = 0.f;
		float Dot = FVector::DotProduct(Velocity.GetSafeNormal(), Forward.GetSafeNormal());
		if (Dot > 0.2f)
		{
			float DeltaTime = GetWorld()->GetDeltaSeconds();
			Attributes->UseStamina(12.f * DeltaTime);
			Attributes->ResetStaminaRegenCooldown();
		}
	}

	float SpeedMultiplier = (bIsBlocking && EquippedShield) ? EquippedShield->BlockMoveSpeedMultiplier
		: (ArmWeaponState == EArmWeaponState::AWS_Arming) ? 0.875f : 1.0f;

	if (!Velocity.IsNearlyZero())
	{
		FVector Forward = GetActorForwardVector();
		Forward.Z = 0.f;
		float DotProduct = FVector::DotProduct(Velocity.GetSafeNormal(), Forward.GetSafeNormal());

		float BaseSpeed = 300.f;

		if (bIsSprinting && ActionState == EActionState::EAS_UnOccupied && DotProduct > 0.2f)
		{
			BaseSpeed = 450.f;
		}
		else if (bIsWalking && ActionState == EActionState::EAS_UnOccupied)
		{
			BaseSpeed = 150.f;
		}

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

	FDebugDrawHelper::Add(FString::Printf(TEXT("Speed: %.0f"), GetCharacterMovement()->MaxWalkSpeed), FColor::Cyan);
}

// ==================== 蒙太奇 ====================

void AMyCharacter::PlayAttackMontage(const FName& SectionName)
{
	Super::PlayAttackMontage(SectionName);
}

void AMyCharacter::PlayArmMontage(const FName& SectionName)
{
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && ArmMontage)
	{
		AnimInstance->Montage_Play(ArmMontage);
		AnimInstance->Montage_JumpToSection(SectionName, ArmMontage);

		FOnMontageEnded EndDelegate;
		EndDelegate.BindUObject(this, &AMyCharacter::OnArmMontageEnded);
		AnimInstance->Montage_SetEndDelegate(EndDelegate, ArmMontage);
	}
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
