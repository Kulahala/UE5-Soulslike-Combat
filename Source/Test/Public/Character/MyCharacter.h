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
class UComboDataAsset;
class UAttackConfigDataAsset;
class USoundBase;

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
	bool ShouldUseSprintAttack() const;
	void PerformSprintAttack();
	void OnAttackInputPressed();
	void OnAttackInputReleased();
	void EnterChargeMode();
	void PerformChargedRelease();
	void CancelChargeInputState();
	bool CanStartChargedAttack() const;
	virtual void Jump() override;
	virtual void GetHit_Implementation(const FVector& ImpactPoint, AActor* HitInstigator) override;
	virtual float TakeDamage(float DamageAmount, const struct FDamageEvent& DamageEvent,
	                         class AController* EventInstigator, AActor* DamageCauser) override;
	void Die(); // 死亡演出
	UFUNCTION()
	void HandleExhausted(); // 体力耗尽回调
	void RecoverFromExhaustion(); // ExhaustedTime 后恢复
	bool IsExhaustionTimerActive() const;

	/* 连招系统 */
	void OpenComboWindow();
	void CloseComboWindow();
	void ResetCombo();
	FORCEINLINE bool IsComboWindowOpen() const { return bComboWindowOpen; }
	FORCEINLINE void SetComboInputReceived(bool bReceived) { bComboInputReceived = bReceived; }
	UFUNCTION(BlueprintCallable, Category = "Combat")
	FORCEINLINE int32 GetComboCounter() const { return ComboCounter; }

	/* 翻滚 */
	void Dodge();
	void OnDodgeMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	/* 装备 */
	virtual void Equip() override;

	/* 药瓶系统 */
	void UsePotion();
	bool CanUsePotion() const;
	void HealFromPotion(float Percent);
	void InterruptPotion();

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
	void SetDodgeInvulnerable(bool bInvulnerable);
	void SetAttackHyperArmor(bool bHyperArmor);
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

	/* 相机归中 */
	FORCEINLINE bool IsRecenteringCamera() const { return bRecenteringCamera; }
	void StopCameraRecenter() { bRecenteringCamera = false; }

	/* 锁定目标访问 */
	AEnemy* GetLockedTarget() const;

	/* 听觉感知 */
	void StartMovementNoiseTimer();
	void StopMovementNoiseTimer();

protected:
	/* 蒙太奇 */
	void PlayBlockMontage(const FName& SectionName); // 播放防御蒙太奇
	virtual void PlayAttackMontage(const FName& SectionName) override;
	void PlayPotionMontage();
	virtual bool CanAttack() const override;
	virtual void OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted) override;
	void OnPotionMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	// 防御蒙太奇（Section: BlockRaise, BlockIdle）
	UPROPERTY(EditDefaultsOnly, Category = "Montages", meta = (ToolTip = "防御蒙太奇，含 Section：BlockRaise, BlockIdle。"))
	UAnimMontage* BlockMontage;

	// 翻滚蒙太奇（需含根运动）
	UPROPERTY(EditDefaultsOnly, Category = "Montages", meta = (ToolTip = "翻滚蒙太奇，需含根运动。"))
	UAnimMontage* DodgeMontage;

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
	/* 动作状态恢复 Helpers */
	bool ShouldRecoverToExhausted_Generic() const;
	bool ShouldRecoverToExhausted_Attack() const;
	void EnsureExhaustionRecoveryTimer();
	EActionState RecoverActionStateAfterMontage(EActionState ExpectedState, bool bResumeStaminaRegen);
	void CleanupInterruptedAttack();

	/* 提取方法 */
	void InitializePlayerHUD();
	void UpdatePotionCooldownHUD() const;
	void DrawDebugInfo() const;
	void StopBlockMontage(float BlendOutTime);
	bool ShouldInterruptBlock() const;
	void TickSprintStamina();
	float CalcBaseSpeed(float DotProduct) const;
	void UpdateLockOnCamera(float DeltaTime);
	void GetLockOnCameraTargets(FVector& OutSocketTarget, float& OutArmLengthTarget, float& OutInterpSpeed) const;
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
	void RestoreRotationMode();
	void FaceDirection2D(const FVector& FacingDirection);
	bool CanDodge() const;
	FVector ComputeDodgeDirection() const;
	FName SelectDodgeSection(const FVector& WorldDirection) const;

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
	/* Charged Attack */
	bool bAttackInputHeld = false;
	bool bIsChargingAttack = false;
	float AttackInputPressTime = 0.f;
	FTimerHandle ChargeDecisionTimer;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Charge")
	float ChargeInputThreshold = 0.2f;

	bool bPendingExhaustedAfterAttack = false;

	// 当前重叠的可拾取物品
	UPROPERTY(VisibleInstanceOnly, Category = "State", meta = (AllowPrivateAccess = "true", ToolTip = "当前重叠的可拾取物品。"))
	Aitem* OverLapItem;

	UPROPERTY(BlueprintReadOnly, Category = "State", meta = (AllowPrivateAccess = "true"))
	bool bIsSprinting = false;

	UPROPERTY(BlueprintReadOnly, Category = "State", meta = (AllowPrivateAccess = "true"))
	bool bIsWalking = false;

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

	/* 翻滚 */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Dodge", meta = (ToolTip = "翻滚体力消耗。"))
	float DodgeStaminaCost = 15.f;
	bool bDodgeInvulnerable = false;

	/* 药瓶 */
	bool bPotionOnCooldown = false;
	FTimerHandle PotionCooldownTimer;
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Potion")
	float PotionCooldown = 2.f;

	UPROPERTY(EditDefaultsOnly, Category = "Montages")
	UAnimMontage* PotionMontage;

	void StartPotionCooldown();
	void ResetPotionCooldown();

	/* 攻击霸体 */
	bool bAttackHyperArmor = false;

	FTimerHandle ExhaustionTimerHandle;

	// 受击相机晃动（所有受击路径触发，含格挡）
	UPROPERTY(EditDefaultsOnly, Category = "Combat", meta = (ToolTip = "受击时播放的相机晃动类，含格挡也会触发。"))
	TSubclassOf<UCameraShakeBase> HitReceivedCameraShake;

	// 受击染红缩放系数（TryBlockHit 设置，TakeDamage 推送给 HUD 后归位）
	float LastDamageFlashScale = 1.f;

	/* 攻击配置 */
	UPROPERTY(EditDefaultsOnly, Category = "Combat", meta = (AllowPrivateAccess = "true", ToolTip = "攻击配置资产，管理所有攻击类型"))
	TObjectPtr<UAttackConfigDataAsset> AttackConfig;

	/* 听觉感知 */
	FTimerHandle MovementNoiseTimerHandle;
	void EmitMovementNoise();
	void EmitNoise(float Loudness, float MaxRange);

	UPROPERTY(EditAnywhere, Category = "Combat|Hearing", meta = (ToolTip = "移动噪音发射间隔（秒）"))
	float MovementNoiseInterval = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Combat|Hearing", meta = (ToolTip = "跑步噪音音量（0.0-1.0）"))
	float RunNoiseLoudness = 0.4f;

	UPROPERTY(EditAnywhere, Category = "Combat|Hearing", meta = (ToolTip = "跑步噪音范围（cm）"))
	float RunNoiseRange = 500.f;

	UPROPERTY(EditAnywhere, Category = "Combat|Hearing", meta = (ToolTip = "冲刺噪音音量（0.0-1.0）"))
	float SprintNoiseLoudness = 0.6f;

	UPROPERTY(EditAnywhere, Category = "Combat|Hearing", meta = (ToolTip = "冲刺噪音范围（cm）"))
	float SprintNoiseRange = 600.f;

	UPROPERTY(EditAnywhere, Category = "Combat|Hearing", meta = (ToolTip = "攻击噪音音量（0.0-1.0）"))
	float AttackNoiseLoudness = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Combat|Hearing", meta = (ToolTip = "攻击噪音范围（cm）"))
	float AttackNoiseRange = 800.f;

	UPROPERTY(EditAnywhere, Category = "Combat|Hearing", meta = (ToolTip = "翻滚噪音音量（0.0-1.0）"))
	float DodgeNoiseLoudness = 0.4f;

	UPROPERTY(EditAnywhere, Category = "Combat|Hearing", meta = (ToolTip = "翻滚噪音范围（cm）"))
	float DodgeNoiseRange = 400.f;

	UPROPERTY(EditAnywhere, Category = "Combat|Hearing", meta = (ToolTip = "喝药噪音音量（0.0-1.0）"))
	float PotionNoiseLoudness = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Combat|Hearing", meta = (ToolTip = "喝药噪音范围（cm）"))
	float PotionNoiseRange = 500.f;

	/* 连招系统 */
	int32 ComboCounter = 0;
	bool bComboWindowOpen = false;
	bool bComboInputReceived = false;

	/* 锁定 */
	// 锁定开启前缓存的状态（ClearLockOn 时恢复）
	bool bCachedOrientRotationToMovement = true;
	bool bCachedUseControllerRotationYaw = false;
	bool bCachedSpringArmUsePawnControlRotation = false;
	FVector CachedSocketOffset = FVector::ZeroVector;
	float CachedTargetArmLength = 300.f;

	/* 相机归中 */
	bool bRecenteringCamera = false;

	UPROPERTY(EditDefaultsOnly, Category = "Camera", meta = (ToolTip = "归中插值速度"))
	float RecenterInterpSpeed = 8.f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera", meta = (ToolTip = "归中目标俯仰角（负值=俯视）"))
	float RecenterTargetPitch = -10.f;

	FRotator RecenterTargetRotation = FRotator::ZeroRotator;

	void StartCameraRecenter();
	void UpdateCameraRecenter(float DeltaTime);

	void FindLockOnTarget();
	void UpdateLockOn(float DeltaTime);

public:
	FORCEINLINE void SetEquippedItem(Aitem* Item) { OverLapItem = Item; }
	FORCEINLINE Aitem* GetEquippedItem() const { return OverLapItem; }
	FORCEINLINE void SetActionState(const EActionState NewState) { ActionState = NewState; }
	FORCEINLINE UCameraComponent* GetCamera() const { return Camera; }
	FORCEINLINE USpringArmComponent* GetSpringArm() const { return SpringArm; }
	FORCEINLINE EActionState GetActionState() const { return ActionState; }
	FORCEINLINE bool IsBlocking() const { return bIsBlocking; }
	FORCEINLINE AShield* GetEquippedShield() const { return EquippedShield; }
};
