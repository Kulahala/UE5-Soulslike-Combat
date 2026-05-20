// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BaseCharacter.h"
#include "Character/CharacterTypes.h"
#include "Interfaces/BlockableInterface.h"
#include "MyCharacter.generated.h"

class Aitem;
class AEnemy;
class AShield;
class USpringArmComponent;
class UCameraComponent;
class UPlayerHUDWidget;
class UCameraShakeBase;
class UPlayerLockOnComponent;

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
	void RecoverFromExhaustion(); // ExhaustedTime 后恢复

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

	/* 弹反 */
	void Input_Parry();
	bool CanStartParry() const;
	void SetParryActive(bool bActive);
	void StartParryCooldown();
	void ResetParryCooldown();
	void InterruptParry();
	void ClearParryState();
	void OnParryMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	/* 移动状态 */
	void Sprint();
	void StopSprinting();
	void Walk();
	void StopWalking();
	void UpdateMovementSpeed(); // 每帧根据方向/状态动态调整移速

	/* 锁定 */
	void ToggleLockOn();
	void ClearLockOn();
	bool IsLockingOn() const;

protected:
	/* 蒙太奇 */
	void PlayArmMontage(const FName& SectionName); // 播放拔刀/收刀动画
	void PlayBlockMontage(const FName& SectionName); // 播放防御蒙太奇
	virtual bool CanAttack() const override;
	virtual void OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted) override;
	void OnArmMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	// 拔刀/收刀蒙太奇
	UPROPERTY(EditDefaultsOnly, Category = "Montages", meta = (ToolTip = "拔刀/收刀蒙太奇，含 Section：Arm, Disarm。"))
	UAnimMontage* ArmMontage;

	// 防御蒙太奇（Section: BlockRaise, BlockIdle）
	UPROPERTY(EditDefaultsOnly, Category = "Montages", meta = (ToolTip = "防御蒙太奇，含 Section：BlockRaise, BlockIdle。"))
	UAnimMontage* BlockMontage;

	/* 动作状态 */
	UPROPERTY(BlueprintReadOnly, Category = "State")
	EActionState ActionState = EActionState::EAS_UnOccupied;

	// 体力耗尽后恢复时间
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "State", meta = (ToolTip = "体力耗尽后自动恢复的时间（秒）。"))
	float ExhaustedTime = 5.f;

	/* 移动速度 */
	UPROPERTY(EditDefaultsOnly, Category = "Movement", meta = (ToolTip = "步行速度。"))
	float WalkSpeed = 200.f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement", meta = (ToolTip = "普通奔跑速度。"))
	float RunSpeed = 300.f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement", meta = (ToolTip = "冲刺速度。"))
	float SprintSpeed = 360.f;

private:
	/* 提取方法 */
	void InitializePlayerHUD();
	void DrawDebugInfo() const;
	void StopBlockMontage(float BlendOutTime);
	bool ShouldInterruptBlock() const;
	void TickSprintStamina();
	float CalcBaseSpeed(float DotProduct) const;
	void UpdateLockOnCamera(float DeltaTime);
	void GetLockOnCameraTargets(FVector& OutSocketTarget, float& OutArmLengthTarget, float& OutInterpSpeed) const;
	AEnemy* GetLockedTarget() const;
	void CacheLockOnRotationState();
	void EnterLockOnRotationMode();
	void RestoreCachedRotationState();
	void ClearCurrentLockOnTarget();
	void SetLockOnTarget(AEnemy* NewTarget);
	bool IsLockOnTargetValid() const;
	void UpdateLockOnControlRotation(float DeltaTime) const;
	bool ShouldUseLockOnFreeRun() const;
	FVector GetLockOnFreeRunDirection() const;
	FVector GetLockOnFreeRunCameraInputLocal() const;
	FVector GetLockOnFreeRunCameraOffsetTarget() const;
	void SetMovementRotationMode(bool bOrientToMovement, bool bUseControllerYaw);
	void ApplyCurrentLockOnRotationMode();
	void ApplyLockOnRotationMode();
	void RestorePostAttackRotationMode();
	void FaceDirection2D(const FVector& FacingDirection);

	/* 相机组件 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true", ToolTip = "玩家相机。"))
	UCameraComponent* Camera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true", ToolTip = "玩家弹簧臂。"))
	USpringArmComponent* SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true", ToolTip = "玩家锁定组件，负责锁定状态、目标筛选和参数持有。"))
	UPlayerLockOnComponent* LockOnComponent;

	/* HUD */
	// 玩家 HUD 控件类，BeginPlay 时创建并绑定到视口
	UPROPERTY(EditDefaultsOnly, Category = "HUD", meta = (ToolTip = "玩家 HUD 控件类，BeginPlay 时创建并绑定到视口。"))
	TSubclassOf<UPlayerHUDWidget> PlayerHUDClass;

	UPROPERTY()
	UPlayerHUDWidget* PlayerHUDWidget;

	/* 状态 */
	// 当前重叠的可拾取物品
	UPROPERTY(VisibleInstanceOnly, Category = "State", meta = (AllowPrivateAccess = "true", ToolTip = "当前重叠的可拾取物品。"))
	Aitem* OverLapItem;

	UPROPERTY(BlueprintReadOnly, Category = "State", meta = (AllowPrivateAccess = "true"))
	bool bIsSprinting = false;

	UPROPERTY(BlueprintReadOnly, Category = "State", meta = (AllowPrivateAccess = "true"))
	bool bIsWalking = false;

	// 是否正在播放切刀/拔刀蒙太奇（短暂状态）
	UPROPERTY(VisibleInstanceOnly, Category = "State", meta = (AllowPrivateAccess = "true", ToolTip = "是否正在播放拔刀/收刀蒙太奇。"))
	bool bIsArming = false;

	/* 防御 */
	bool bIsBlocking = false;
	bool bBlockInputHeld = false;
	UPROPERTY()
	AShield* EquippedShield;

	/* 弹反 */
	bool bIsParrying = false;
	bool bParryActive = false;
	bool bParryOnCooldown = false;
	FTimerHandle ParryCooldownTimer;
	UPROPERTY(EditDefaultsOnly, Category = "Montages", meta = (ToolTip = "弹反蒙太奇。"))
	UAnimMontage* ParryMontage;

	FTimerHandle ExhaustionTimerHandle;

	// 受击相机晃动（所有受击路径触发，含格挡）
	UPROPERTY(EditDefaultsOnly, Category = "Combat", meta = (ToolTip = "受击时播放的相机晃动类，含格挡也会触发。"))
	TSubclassOf<UCameraShakeBase> HitReceivedCameraShake;

	// 受击染红缩放系数（TryBlockHit 设置，TakeDamage 推送给 HUD 后归位）
	float LastDamageFlashScale = 1.f;

	/* 锁定 */
	// 锁定开启前缓存的状态（ClearLockOn 时恢复）
	bool bCachedOrientRotationToMovement = true;
	bool bCachedUseControllerRotationYaw = false;
	bool bCachedSpringArmUsePawnControlRotation = false;
	FVector CachedSocketOffset = FVector::ZeroVector;
	float CachedTargetArmLength = 300.f;

	void FindLockOnTarget();
	void UpdateLockOn(float DeltaTime);

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
};
