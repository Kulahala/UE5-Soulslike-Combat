// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CharacterTypes.h"
#include "GameFramework/Character.h"
#include "Interfaces/HitInterface.h"
#include "BaseCharacter.generated.h"

class UAttributeComponent;
class AWeapon;

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

protected:
	/* 蒙太奇 */
	virtual void PlayAttackMontage(const FName& SectionName);
	virtual void PlayHitReactMontage(const FName& SectionName);
	virtual bool CanAttack() const;
	virtual void OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	virtual void OnHitReactMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	/* 蒙太奇资源 */
	UPROPERTY(EditDefaultsOnly, Category = "Montages")
	UAnimMontage* AttackMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Montages")
	UAnimMontage* HitReactMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Montages")
	UAnimMontage* DeathMontage;

	/* 状态 */
	UPROPERTY(BlueprintReadOnly, Category = "State", meta = (AllowPrivateAccess = "true"))
	EWeaponState WeaponState = EWeaponState::EWS_Unequipped; // 装备状态

	UPROPERTY(BlueprintReadOnly, Category = "State", meta = (AllowPrivateAccess = "true"))
	EArmWeaponState ArmWeaponState = EArmWeaponState::AWS_Disarming; // 拔刀/收刀状态

	/* 动画驱动变量 */
	UPROPERTY(BlueprintReadOnly, Category = "Animation", meta = (AllowPrivateAccess = "true"))
	float GroundSpeed; // 地速（2D），驱动 BlendSpace

	UPROPERTY(BlueprintReadOnly, Category = "Animation", meta = (AllowPrivateAccess = "true"))
	float Direction; // 移动方向（-180~180），驱动 BlendSpace

	/* 组件 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UAttributeComponent* Attributes; // 属性组件（血量、金币等）

	/* 受击效果 */
	UPROPERTY(EditAnywhere, Category = "Effect")
	USoundBase* HitSound; // 受击音效

	UPROPERTY(EditAnywhere, Category = "Effect")
	UParticleSystem* HitParticle; // 受击粒子

	/* 武器 */
	UPROPERTY()
	AWeapon* EquippedWeapon; // 当前装备的武器

public:
	FORCEINLINE AWeapon* GetWeapon() const { return EquippedWeapon; }
	FORCEINLINE UAttributeComponent* GetAttributes() const { return Attributes; }
	FORCEINLINE float GetGroundSpeed() const { return GroundSpeed; }
	FORCEINLINE float GetDirection() const { return Direction; }
	FORCEINLINE EWeaponState GetCharacterState() const { return WeaponState; }
	FORCEINLINE EArmWeaponState GetArmWeaponState() const { return ArmWeaponState; }
};
