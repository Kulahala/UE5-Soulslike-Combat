// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CharacterTypes.h"
#include "GameFramework/Character.h"
#include "Interfaces/HitInterface.h"
#include "BaseCharacter.generated.h"

class UAttributeComponent;
class AWeapon;

USTRUCT()
struct FPendingHitContext
{
	GENERATED_BODY()

	AActor* HitInstigator = nullptr;
	float KnockbackScale = 1.f;
	bool bWasBlocked = false;
	bool bApplyStun = true;
};

UCLASS()
class TEST_API ABaseCharacter : public ACharacter, public IHitInterface
{
	GENERATED_BODY()

public:
	/* 生命周期 */
	ABaseCharacter();
	virtual void Tick(float DeltaTime) override;
	virtual void BeginPlay() override;

	/* 战斗 */
	virtual void GetHit_Implementation(const FVector& ImpactPoint, AActor* HitInstigator) override;
	virtual void PlayHitEffects(const FVector& ImpactPoint) override;
	virtual float TakeDamage(float DamageAmount, const struct FDamageEvent& DamageEvent,
	                         class AController* EventInstigator, AActor* DamageCauser) override; // 受击扣血逻辑
	virtual void DirectionalHitReact(const FVector& ImpactPoint, const AActor* HitInstigator); // 方向性受击反应
	virtual void Attack();
	virtual void Equip();

	FORCEINLINE float GetAttackDamageMultiplier() const { return CurrentAttackDamageMultiplier; }
	FORCEINLINE void SetAttackDamageMultiplier(float Multiplier) { CurrentAttackDamageMultiplier = Multiplier; }

	/* 命中上下文 + 后退 */
	void CachePendingHitContext(AActor* HitInstigator, float KnockbackScale, bool bWasBlocked, bool bApplyStun);
	void ResetPendingHitContext();

protected:
	/* 蒙太奇 */
	void PlayMontageSection(UAnimMontage* Montage, const FName& Section);

	// 前向向量与给定方向的 2D 点积（纯数学，调用方自行转换方向）
	float CalcForwardDot2D(const FVector& WorldDirection) const;
	virtual void PlayAttackMontage(const FName& SectionName);
	virtual void PlayHitReactMontage(const FName& SectionName);
	virtual bool CanAttack() const;
	virtual void OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	virtual void OnHitReactMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	/* 蒙太奇资源 */
	// 攻击蒙太奇（含 Section：Attack1, Attack2 等）
	UPROPERTY(EditDefaultsOnly, Category = "Montages", meta = (ToolTip = "攻击蒙太奇，含 Section：Attack1, Attack2 等。"))
	UAnimMontage* AttackMontage;

	// 受击反应蒙太奇（含 Section：FromFront, FromBack, FromLeft, FromRight）
	UPROPERTY(EditDefaultsOnly, Category = "Montages", meta = (ToolTip = "受击反应蒙太奇，含 Section：FromFront, FromBack, FromLeft, FromRight。"))
	UAnimMontage* HitReactMontage;

	// 死亡蒙太奇
	UPROPERTY(EditDefaultsOnly, Category = "Montages", meta = (ToolTip = "死亡蒙太奇。"))
	UAnimMontage* DeathMontage;

	/* 状态 */
	UPROPERTY(BlueprintReadOnly, Category = "State", meta = (AllowPrivateAccess = "true"))
	EWeaponState WeaponState = EWeaponState::EWS_Unequipped; // 装备状态

	/* 动画驱动变量 */
	UPROPERTY(BlueprintReadOnly, Category = "Animation", meta = (AllowPrivateAccess = "true"))
	float GroundSpeed; // 地速（2D），驱动 BlendSpace

	UPROPERTY(BlueprintReadOnly, Category = "Animation", meta = (AllowPrivateAccess = "true"))
	float Direction; // 移动方向（-180~180），驱动 BlendSpace

	/* 组件 */
	// 属性组件（血量、体力、金币等）
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true", ToolTip = "属性组件，管理血量、体力、金币。"))
	UAttributeComponent* Attributes;

	/* 受击效果 */
	// 普通受击音效（格挡时不播放，由 Shield::BlockSound 替代）
	UPROPERTY(EditAnywhere, Category = "Effect", meta = (ToolTip = "普通受击音效，格挡时不播放。"))
	USoundBase* HitSound;

	// 普通受击粒子（格挡时不播放，由 Shield::BlockParticle 替代）
	UPROPERTY(EditAnywhere, Category = "Effect", meta = (ToolTip = "普通受击粒子，格挡时不播放。"))
	UParticleSystem* HitParticle;

	/* 武器 */
	UPROPERTY()
	AWeapon* EquippedWeapon; // 当前装备的武器

	/* 受击后退 */
	// 受击后退基础距离（cm），实际距离 = 此值 × KnockbackScale。子类构造函数覆写：Player=10, Enemy=5
	UPROPERTY(EditAnywhere, Category = "Combat|Knockback", meta = (ToolTip = "受击后退基础距离（cm），实际距离 = 此值 × KnockbackScale。"))
	float BaseHitKnockbackDistance = 0.f;

	// 后退完成时间（秒），quadratic ease-out 曲线
	UPROPERTY(EditAnywhere, Category = "Combat|Knockback", meta = (ToolTip = "后退完成时间（秒），quadratic ease-out 曲线。"))
	float HitKnockbackDuration = 0.12f;

	FPendingHitContext PendingHitContext;
	bool bKnockbackActive = false;
	FVector KnockbackDirection = FVector::ZeroVector;
	float KnockbackElapsed = 0.f;
	float KnockbackAppliedDistance = 0.f;
	float KnockbackTargetDistance = 0.f;

	void ConsumePendingHitKnockback();
	void StartHitKnockback(AActor* HitInstigator, float Scale);
	void TickHitKnockback(float DeltaTime);

	// 当前攻击伤害倍率（连招系统使用）
	float CurrentAttackDamageMultiplier = 1.0f;

public:
	FORCEINLINE AWeapon* GetWeapon() const { return EquippedWeapon; }
	FORCEINLINE UAttributeComponent* GetAttributes() const { return Attributes; }
	FORCEINLINE float GetGroundSpeed() const { return GroundSpeed; }
	FORCEINLINE float GetDirection() const { return Direction; }
	FORCEINLINE EWeaponState GetCharacterState() const { return WeaponState; }
};
