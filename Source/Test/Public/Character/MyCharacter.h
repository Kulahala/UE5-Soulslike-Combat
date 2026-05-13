// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BaseCharacter.h"
#include "Character/CharacterTypes.h"
#include "Interfaces/BlockableInterface.h"
#include "MyCharacter.generated.h"

class Aitem;
class AShield;
class USpringArmComponent;
class UCameraComponent;
class UPlayerHUDWidget;
class UCameraShakeBase;

UCLASS()
class TEST_API AMyCharacter : public ABaseCharacter, public IBlockableInterface
{
	GENERATED_BODY()

public:
	/* 生命周期 */
	AMyCharacter();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/* 战斗 */
	virtual void Attack() override;
	virtual void Jump() override;
	virtual void GetHit_Implementation(const FVector& ImpactPoint, AActor* HitInstigator) override;
	virtual float TakeDamage(float DamageAmount, const struct FDamageEvent& DamageEvent,
	                         class AController* EventInstigator, AActor* DamageCauser) override;
	void Die(); // 死亡演出
	UFUNCTION()
	void HandleExhausted(); // 体力耗尽回调
	void RecoverFromExhaustion(); // 2秒后恢复

	/* 装备 */
	virtual void Equip() override;
	void ArmWeapon(); // 切换拔刀/收刀

	/* 防御 */
	void StartBlockInput();
	void ReleaseBlockInput();
	void InterruptBlock(bool bClearHeld);
	void TryResumeBlock();
	bool CanStartBlock() const;
	virtual FBlockResult TryBlockHit(const FVector& ImpactPoint, float IncomingDamage,
	                                 AActor* Attacker, AActor* DamageCauser) override;

	/* 移动状态 */
	void Sprint();
	void StopSprinting();
	void Walk();
	void StopWalking();
	void UpdateMovementSpeed(); // 每帧根据方向/状态动态调整移速

protected:
	/* 蒙太奇 */
	virtual void PlayAttackMontage(const FName& SectionName) override;
	void PlayArmMontage(const FName& SectionName); // 播放拔刀/收刀动画
	void PlayBlockMontage(const FName& SectionName); // 播放防御蒙太奇
	virtual bool CanAttack() const override;
	virtual void OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted) override;
	void OnArmMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	UPROPERTY(EditDefaultsOnly, Category = "Montages")
	UAnimMontage* ArmMontage; // 拔刀/收刀蒙太奇

	UPROPERTY(EditDefaultsOnly, Category = "Montages")
	UAnimMontage* BlockMontage; // 防御蒙太奇（Section: BlockRaise, BlockIdle）

	/* 动作状态 */
	UPROPERTY(BlueprintReadOnly, Category = "State")
	EActionState ActionState = EActionState::EAS_UnOccupied;

	UPROPERTY(VisibleAnywhere,BlueprintReadWrite ,Category = "State")
	float ExhaustedTime = 5.f; // 体力耗尽后恢复时间

private:
	/* 相机组件 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* Camera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* SpringArm;

	/* HUD */
	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	TSubclassOf<UPlayerHUDWidget> PlayerHUDClass;

	UPROPERTY()
	UPlayerHUDWidget* PlayerHUDWidget;

	/* 状态 */
	UPROPERTY(VisibleInstanceOnly, Category = "State", meta = (AllowPrivateAccess = "true"))
	Aitem* OverLapItem; // 当前重叠的可拾取物品

	UPROPERTY(BlueprintReadOnly, Category = "State", meta = (AllowPrivateAccess = "true"))
	bool bIsSprinting = false;

	UPROPERTY(BlueprintReadOnly, Category = "State", meta = (AllowPrivateAccess = "true"))
	bool bIsWalking = false;

	UPROPERTY(VisibleInstanceOnly, Category = "State", meta = (AllowPrivateAccess = "true"))
	bool bIsArming = false; // 是否正在播放切刀/拔刀蒙太奇（短暂状态）

	/* 防御 */
	bool bIsBlocking = false;
	bool bBlockInputHeld = false;
	UPROPERTY()
	AShield* EquippedShield;

	FTimerHandle ExhaustionTimerHandle;

	// 受击相机晃动
	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	TSubclassOf<UCameraShakeBase> HitReceivedCameraShake;

	// 受击染红缩放系数（TryBlockHit 设置，SetHealthPercent 消费后归位）
	float LastDamageFlashScale = 1.f;

public:
	FORCEINLINE void SetEquippedItem(Aitem* Item) { OverLapItem = Item; }
	FORCEINLINE Aitem* GetEquippedItem() const { return OverLapItem; }
	FORCEINLINE void SetActionState(const EActionState NewState) { ActionState = NewState; }
	FORCEINLINE void SetArmWeaponState(const EArmWeaponState NewState) { ArmWeaponState = NewState; }
	FORCEINLINE UCameraComponent* GetCamera() const { return Camera; }
	FORCEINLINE USpringArmComponent* GetSpringArm() const { return SpringArm; }
	FORCEINLINE EActionState GetActionState() const { return ActionState; }
	FORCEINLINE bool IsArming() const { return bIsArming; }
	FORCEINLINE bool IsBlocking() const { return bIsBlocking; }
	FORCEINLINE AShield* GetEquippedShield() const { return EquippedShield; }
	FORCEINLINE float GetLastDamageFlashScale() const { return LastDamageFlashScale; }
	FORCEINLINE void SetLastDamageFlashScale(float Scale) { LastDamageFlashScale = Scale; }
};
