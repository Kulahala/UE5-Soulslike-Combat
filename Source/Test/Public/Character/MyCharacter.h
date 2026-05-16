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

	/* 移动状态 */
	void Sprint();
	void StopSprinting();
	void Walk();
	void StopWalking();
	void UpdateMovementSpeed(); // 每帧根据方向/状态动态调整移速

	/* 锁定 */
	void ToggleLockOn();
	void ClearLockOn();
	bool IsLockingOn() const { return bIsLockingOn; }

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

private:
	/* 提取方法 */
	void InitializePlayerHUD();
	void DrawDebugInfo() const;
	void StopBlockMontage(float BlendOutTime);
	bool ShouldInterruptBlock() const;
	void TickSprintStamina();
	float CalcBaseSpeed(float DotProduct) const;
	bool ShouldUseLockOnFreeRun() const;
	FVector GetLockOnFreeRunDirection() const;
	FVector GetLockOnFreeRunCameraInputLocal() const;
	FVector GetLockOnFreeRunCameraOffsetTarget() const;
	void ApplyLockOnRotationMode();
	void RestorePostAttackRotationMode();
	void FaceDirection2D(const FVector& FacingDirection);

	/* 相机组件 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true", ToolTip = "玩家相机。"))
	UCameraComponent* Camera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true", ToolTip = "玩家弹簧臂。"))
	USpringArmComponent* SpringArm;

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

	FTimerHandle ExhaustionTimerHandle;

	// 受击相机晃动（所有受击路径触发，含格挡）
	UPROPERTY(EditDefaultsOnly, Category = "Combat", meta = (ToolTip = "受击时播放的相机晃动类，含格挡也会触发。"))
	TSubclassOf<UCameraShakeBase> HitReceivedCameraShake;

	// 受击染红缩放系数（TryBlockHit 设置，SetHealthPercent 消费后归位）
	float LastDamageFlashScale = 1.f;

	/* 锁定 */
	UPROPERTY()
	AEnemy* LockedTarget = nullptr;

	bool bIsLockingOn = false;

	// 锁定开启前缓存的状态（ClearLockOn 时恢复）
	bool bCachedOrientRotationToMovement = true;
	bool bCachedUseControllerRotationYaw = false;
	bool bCachedSpringArmUsePawnControlRotation = false;
	FVector CachedSocketOffset = FVector::ZeroVector;
	float CachedTargetArmLength = 300.f;

	// 锁定参数
	UPROPERTY(EditDefaultsOnly, Category = "LockOn", meta = (ToolTip = "锁定目标搜索半径（cm）。"))
	float LockOnRadius = 1500.f;

	UPROPERTY(EditDefaultsOnly, Category = "LockOn", meta = (ToolTip = "前方视角过滤半角（度）。只有在此角度内的敌人才会被选中。"))
	float LockOnViewAngleDegrees = 45.f;

	UPROPERTY(EditDefaultsOnly, Category = "LockOn", meta = (ToolTip = "锁定朝向插值速度。值越大转向越快。"))
	float LockOnRotationInterpSpeed = 8.f;

	UPROPERTY(EditDefaultsOnly, Category = "LockOn", meta = (ToolTip = "自动解锁距离（cm）。目标超出此距离自动解锁，带滞后防抖动。"))
	float LockOnBreakRadius = 2000.f;

	UPROPERTY(EditDefaultsOnly, Category = "LockOn|Movement", meta = (ClampMin = "0.0", ToolTip = "锁定时侧移速度倍率。1.0 表示不低于基础移动速度。"))
	float LockOnStrafeSpeedMultiplier = 0.95f;

	UPROPERTY(EditDefaultsOnly, Category = "LockOn|Movement", meta = (ClampMin = "0.0", ToolTip = "锁定时后撤速度倍率。1.0 表示不低于基础移动速度。"))
	float LockOnBackSpeedMultiplier = 0.9f;

	// 锁定越肩相机
	UPROPERTY(EditDefaultsOnly, Category = "LockOnCamera", meta = (ToolTip = "锁定时 SpringArm 右肩偏移（Y=右, Z=上）。"))
	FVector LockOnSocketOffset = FVector(0.f, 80.f, 80.f);

	UPROPERTY(EditDefaultsOnly, Category = "LockOnCamera", meta = (ToolTip = "SocketOffset 插值速度。"))
	float LockOnSocketOffsetInterpSpeed = 6.f;

	// 锁定 free-run 动态相机偏移
	UPROPERTY(EditDefaultsOnly, Category = "LockOnCamera|FreeRun", meta = (ClampMin = "0.0", ToolTip = "锁定冲刺侧移时相机横向最大偏移幅度（cm），正值按输入方向偏移。"))
	float LockOnFreeRunCameraSideOffset = 60.f;

	UPROPERTY(EditDefaultsOnly, Category = "LockOnCamera|FreeRun", meta = (ClampMin = "0.0", ToolTip = "锁定冲刺后撤时相机抬高幅度（cm）。"))
	float LockOnFreeRunCameraBackHeightOffset = 40.f;

	UPROPERTY(EditDefaultsOnly, Category = "LockOnCamera|FreeRun", meta = (ClampMin = "0.0", ToolTip = "锁定冲刺后撤时弹簧臂额外拉远（cm），默认 0 不改变距离手感。"))
	float LockOnFreeRunCameraBackArmLengthBonus = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "LockOnCamera|FreeRun", meta = (ToolTip = "锁定冲刺动态偏移插值速度。"))
	float LockOnFreeRunCameraInterpSpeed = 10.f;

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
	FORCEINLINE float GetLastDamageFlashScale() const { return LastDamageFlashScale; }
	FORCEINLINE void SetLastDamageFlashScale(float Scale) { LastDamageFlashScale = Scale; }
};
