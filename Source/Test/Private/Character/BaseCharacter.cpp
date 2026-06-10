#include "GameFramework/CharacterMovementComponent.h"
#include "Character/BaseCharacter.h"
#include "AttributeComponent/AttributeComponent.h"
#include "Combat/HitReactionConfigDataAsset.h"
#include "Kismet/GameplayStatics.h"
#include "KismetAnimationLibrary.h"

// ==================== 生命周期 ====================

const TArray<FName> ABaseCharacter::EmptyDeathSections;

ABaseCharacter::ABaseCharacter()
{
	// 旋转设置：关闭控制器旋转，启用面向移动方向
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;

	PrimaryActorTick.bCanEverTick = true;

	// 属性组件
	Attributes = CreateDefaultSubobject<UAttributeComponent>(TEXT("Attributes"));

	CurrentAttackDamageMultiplier = 1.f;
	CurrentBlockStaminaDamageMultiplier = 1.f;
	bCurrentAttackCannotBeParried = false;
}

void ABaseCharacter::BeginPlay()
{
	Super::BeginPlay();
}

void ABaseCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	GroundSpeed = GetVelocity().Size2D();
	Direction = UKismetAnimationLibrary::CalculateDirection(GetVelocity(), GetActorRotation());

	TickHitKnockback(DeltaTime);
}

// ==================== 受击/战斗 ====================

void ABaseCharacter::GetHit_Implementation(const FVector& ImpactPoint, AActor* HitInstigator)
{
	IHitInterface::GetHit_Implementation(ImpactPoint, HitInstigator);

	if (Attributes->IsAlive())
	{
		ConsumePendingHitKnockback();  // 仅存活时后退（Die() 会立刻停移动+关碰撞）

		if (!PendingHitContext.bWasBlocked)
		{
			DirectionalHitReact(ImpactPoint, HitInstigator);  // 仅普通受击
		}
	}

	if (!PendingHitContext.bWasBlocked)
	{
		PlayHitEffects(ImpactPoint);  // 仅普通受击（格挡由 TryBlockHit 播放 BlockSound/BlockParticle）
	}
	// 注意：不清理 PendingHitContext，子类还需要读 bApplyStun
}

void ABaseCharacter::PlayHitEffects(const FVector& ImpactPoint)
{
	const UHitReactionConfigDataAsset* ReactionConfig = GetReactionConfig();
	if (!ReactionConfig)
	{
		return;
	}

	if (ReactionConfig->HitReact.HitSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ReactionConfig->HitReact.HitSound.Get(), ImpactPoint);
	}

	if (ReactionConfig->HitReact.HitParticle)
	{
		UGameplayStatics::SpawnEmitterAtLocation(this, ReactionConfig->HitReact.HitParticle.Get(), ImpactPoint);
	}
}

float ABaseCharacter::TakeDamage(float DamageAmount, const struct FDamageEvent& DamageEvent,
                                 class AController* EventInstigator, AActor* DamageCauser)
{
	return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}

void ABaseCharacter::DirectionalHitReact(const FVector& ImpactPoint, const AActor* HitInstigator)
{
	//敌人的方向向量
	const FVector Forward = GetActorForwardVector().GetSafeNormal2D();

	//受击来源方向
	FVector ToHit;
	if (HitInstigator)
	{
		//把攻击者所在的方向，作为受力判定的来源点
		ToHit = (HitInstigator->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	}
	else
	{
		ToHit = (ImpactPoint - GetActorLocation()).GetSafeNormal2D();
	}

	//方向向量与受击方向向量夹角
	double Theta = GetHitDirection(Forward, ToHit);
	FName SectionName;
	if (Theta >= -45.f && Theta <= 45.f)
	{
		SectionName = FName("FromFront");
	}
	else if (Theta > 45.f && Theta <= 135.f)
	{
		SectionName = FName("FromRight");
	}
	else if (Theta < -45.f && Theta >= -135.f)
	{
		SectionName = FName("FromLeft");
	}
	else
	{
		SectionName = FName("FromBack");
	}
	PlayHitReactMontage(GetHitReactSection(SectionName));
}

void ABaseCharacter::Attack()
{
}

void ABaseCharacter::Equip()
{
}

// ==================== 命中上下文 + 后退 ====================

void ABaseCharacter::CachePendingHitContext(AActor* InInstigator, float InScale, bool bInBlocked, bool bInStun)
{
	ResetPendingHitContext();  // 先清再写，防 stale context 残留
	PendingHitContext.HitInstigator = InInstigator;
	PendingHitContext.KnockbackScale = InScale;
	PendingHitContext.bWasBlocked = bInBlocked;
	PendingHitContext.bApplyStun = bInStun;
}

void ABaseCharacter::ResetPendingHitContext()
{
	PendingHitContext = FPendingHitContext{};
}

void ABaseCharacter::ConsumePendingHitKnockback()
{
	StartHitKnockback(PendingHitContext.HitInstigator, PendingHitContext.KnockbackScale);
}

void ABaseCharacter::StartHitKnockback(AActor* HitInstigator, float Scale)
{
	// 先清旧状态，再决定是否开新后退（零缩放 = 终止旧 knockback）
	bKnockbackActive = false;
	KnockbackElapsed = 0.f;
	KnockbackAppliedDistance = 0.f;
	KnockbackTargetDistance = 0.f;

	if (!HitInstigator || BaseHitKnockbackDistance <= 0.f || Scale <= 0.f) return;

	bKnockbackActive = true;
	KnockbackDirection = (GetActorLocation() - HitInstigator->GetActorLocation()).GetSafeNormal2D();
	KnockbackTargetDistance = BaseHitKnockbackDistance * Scale;
}

void ABaseCharacter::TickHitKnockback(float DeltaTime)
{
	if (!bKnockbackActive) return;

	KnockbackElapsed += DeltaTime;
	const float Alpha = FMath::Clamp(KnockbackElapsed / HitKnockbackDuration, 0.f, 1.f);
	const float EasedAlpha = 1.f - (1.f - Alpha) * (1.f - Alpha);  // quadratic ease-out
	const float NewTarget = KnockbackTargetDistance * EasedAlpha;
	const float Delta = NewTarget - KnockbackAppliedDistance;

	if (FMath::Abs(Delta) > 0.01f)
	{
		const FVector OldLocation = GetActorLocation();
		FHitResult Hit;
		AddActorWorldOffset(KnockbackDirection * Delta, true, &Hit);
		KnockbackAppliedDistance += FVector::Dist2D(OldLocation, GetActorLocation());
	}

	if (Alpha >= 1.f)
	{
		bKnockbackActive = false;
	}
}

// ==================== 工具 ====================

float ABaseCharacter::CalcForwardDot2D(const FVector& WorldDirection) const
{
	return FVector::DotProduct(GetActorForwardVector().GetSafeNormal2D(), WorldDirection.GetSafeNormal2D());
}

// ==================== 蒙太奇 ====================

void ABaseCharacter::PlayMontageSection(UAnimMontage* Montage, const FName& Section)
{
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && Montage)
	{
		AnimInstance->Montage_Play(Montage);
		AnimInstance->Montage_JumpToSection(Section, Montage);
	}
}

void ABaseCharacter::PlayAttackMontage(const FName& SectionName)
{
	UE_LOG(LogTemp, Warning, TEXT("%s: ABaseCharacter::PlayAttackMontage is deprecated. Use AttackConfig or EnemyAttackConfig DataAssets. Section: %s"),
		*GetName(),
		*SectionName.ToString());
}

void ABaseCharacter::PlayHitReactMontage(const FName& SectionName)
{
	UAnimMontage* MontageToPlay = GetHitReactMontage();
	PlayMontageSection(MontageToPlay, SectionName);

	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance(); AnimInstance && MontageToPlay)
	{
		FOnMontageEnded EndDelegate;
		EndDelegate.BindUObject(this, &ABaseCharacter::OnHitReactMontageEnded);
		AnimInstance->Montage_SetEndDelegate(EndDelegate, MontageToPlay);
	}
}

void ABaseCharacter::PlayDeathMontage()
{
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	UAnimMontage* DeathMontageToPlay = GetDeathMontage();
	if (!AnimInstance || !DeathMontageToPlay)
	{
		return;
	}

	AnimInstance->Montage_Stop(0.1f);

	const TArray<FName>& ConfigSections = GetDeathSections();
	FName SectionName = NAME_None;
	if (!ConfigSections.IsEmpty())
	{
		SectionName = ConfigSections[FMath::RandRange(0, ConfigSections.Num() - 1)];
	}

	AnimInstance->Montage_Play(DeathMontageToPlay);
	if (SectionName != NAME_None)
	{
		AnimInstance->Montage_JumpToSection(SectionName, DeathMontageToPlay);
	}
}

UAnimMontage* ABaseCharacter::GetHitReactMontage() const
{
	const UHitReactionConfigDataAsset* ReactionConfig = GetReactionConfig();
	return ReactionConfig && ReactionConfig->HitReact.Montage
		? ReactionConfig->HitReact.Montage.Get()
		: nullptr;
}

UAnimMontage* ABaseCharacter::GetDeathMontage() const
{
	const UHitReactionConfigDataAsset* ReactionConfig = GetReactionConfig();
	return ReactionConfig && ReactionConfig->Death.Montage
		? ReactionConfig->Death.Montage.Get()
		: nullptr;
}

FName ABaseCharacter::GetHitReactSection(const FName& DefaultDirectionSectionName) const
{
	const UHitReactionConfigDataAsset* ReactionConfig = GetReactionConfig();
	if (!ReactionConfig || !ReactionConfig->HitReact.Montage)
	{
		return DefaultDirectionSectionName;
	}

	if (DefaultDirectionSectionName == FName("FromFront"))
	{
		return ReactionConfig->HitReact.FrontSection;
	}
	if (DefaultDirectionSectionName == FName("FromBack"))
	{
		return ReactionConfig->HitReact.BackSection;
	}
	if (DefaultDirectionSectionName == FName("FromLeft"))
	{
		return ReactionConfig->HitReact.LeftSection;
	}
	if (DefaultDirectionSectionName == FName("FromRight"))
	{
		return ReactionConfig->HitReact.RightSection;
	}

	return DefaultDirectionSectionName;
}

const TArray<FName>& ABaseCharacter::GetDeathSections() const
{
	const UHitReactionConfigDataAsset* ReactionConfig = GetReactionConfig();
	return ReactionConfig && ReactionConfig->Death.Montage ? ReactionConfig->Death.Sections : EmptyDeathSections;
}

bool ABaseCharacter::HasConfiguredDeathMontage() const
{
	const UHitReactionConfigDataAsset* ReactionConfig = GetReactionConfig();
	return ReactionConfig && ReactionConfig->Death.Montage;
}

UHitReactionConfigDataAsset* ABaseCharacter::GetReactionConfig() const
{
	return nullptr;
}

bool ABaseCharacter::CanAttack() const
{
	return false;
}

void ABaseCharacter::OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
}

void ABaseCharacter::OnHitReactMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
}
