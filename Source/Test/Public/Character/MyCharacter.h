// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BaseCharacter.h"
#include "Character/CharacterTypes.h"
#include "Combat/PlayerActionConfigDataAsset.h"
#include "Combat/PlayerCharacterProfileDataAsset.h"
#include "Interfaces/BlockableInterface.h"
#include "MyCharacter.generated.h"

class Aitem;
class AController;
class AEnemy;
class AShield;
class AWeapon;
class ABow;
class USpringArmComponent;
class UCameraComponent;
class UPlayerHUDWidget;
class UCameraShakeBase;
class UPlayerLockOnComponent;
class UItemOwnershipComponent;
class UComboDataAsset;
class UAttackConfigDataAsset;
class UHitReactionConfigDataAsset;
class USoundBase;
class UAnimInstance;
class UAnimMontage;
class UMotionWarpingComponent;
class UPawnNoiseEmitterComponent;
class UTestSaveGame;
class UItemDefinitionDataAsset;
enum class EItemEquipmentSlot : uint8;
struct FPlayerAttackMotionWarpingConfig;
struct FCombatHitRequest;

UCLASS()
class TEST_API AMyCharacter : public ABaseCharacter, public IBlockableInterface
{
	GENERATED_BODY()

public:
	/* 生命周期 */
	AMyCharacter();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void Tick(float DeltaTime) override;

	/* 战斗 */
	virtual void Attack() override;
	bool ShouldUseSprintAttack() const;
	bool PerformSprintAttack();
	void OnAttackInputPressed();
	void OnAttackInputReleased();
	void OnAttackInputCanceled();
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
	bool TryConsumeComboInputAtBranchPoint();
	FORCEINLINE bool IsComboWindowOpen() const { return bComboWindowOpen; }
	FORCEINLINE void SetComboInputReceived(bool bReceived) { bComboInputReceived = bReceived; }
	UFUNCTION(BlueprintCallable, Category = "Combat", meta = (ToolTip = "返回当前轻攻击连招段索引，主要供调试或 UI 显示使用。"))
	FORCEINLINE int32 GetComboCounter() const { return ComboCounter; }
	void OpenActionCancelWindow();
	void CloseActionCancelWindow();
	FORCEINLINE bool IsActionCancelWindowOpen() const { return bActionCancelWindowOpen; }

	/* 翻滚 */
	void Dodge();
	void OnDodgeMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	/* 装备 */
	virtual void Equip() override;
	void TryInteract();
	bool CanInteractWithWorld() const;
	void RegisterInteractable(AActor* InteractableActor);
	void UnregisterInteractable(AActor* InteractableActor);
	void SetBonfireServiceProtection(bool bEnabled);
	void RefreshInteractionPrompt();
	FORCEINLINE bool IsBonfireServiceProtected() const { return bBonfireServiceProtected; }

	/* 物品所有权与火堆装备表现。 */
	bool RestoreItemOwnershipFromSave(const UTestSaveGame* SaveGame);
	bool TryGrantOwnedItem(FName DefinitionId, FName& OutInstanceId);
	bool TryGrantOwnedItemQuantity(FName DefinitionId, int32 Quantity, FName& OutInstanceId);
	bool TryRestockAmmoAtCheckpoint(FName GameplayMapName, FName CheckpointId);
	bool VerifyAmmoRefillFixture(FName DefinitionId);
	bool TryClaimWorldItemPickup(FName PersistentId, FName ItemDefinitionId, FName& OutInstanceId,
	                             USoundBase*& OutPickupSound);
	bool TryEquipOwnedItem(FName InstanceId);
	bool TryApplyBonfireLoadoutSelection(EItemEquipmentSlot EquipmentSlot, FName InstanceId);
	void MaterializeEquippedLoadout();
	void DestroyMaterializedLoadout();
	FString GetItemOwnershipDebugSummary() const;
	int32 GetOwnedItemQuantity(FName DefinitionId) const;
	bool ArmNextProjectilePrepareFailureForDebug();

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
	virtual FBlockResult TryBlockHit(const FCombatHitRequest& Request) override;

	/* 弹反 */
	void Input_Parry();
	bool CanStartParry() const;
	void SetParryActive(bool bActive);
	void SetDodgeInvulnerable(bool bInvulnerable);
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
	/** 共享命中结算确认了一次玩家与敌人的敌对碰撞后刷新运行时战斗存在。 */
	void MarkCombatPresenceFromConfirmedHostileHit();

	/* 锁定 */
	void ToggleLockOn();
	void ClearLockOn();
	bool IsLockingOn() const;
	bool SwitchLockOnTarget(bool bSwitchToRight);

	/* 相机归中 */
	FORCEINLINE bool IsRecenteringCamera() const { return bRecenteringCamera; }
	void StopCameraRecenter() { bRecenteringCamera = false; }

	/* 锁定目标访问 */
	AEnemy* GetLockedTarget() const;

	/* 听觉感知 */
	void StartMovementNoiseTimer();
	void StopMovementNoiseTimer();
	// 由 UAnimNotify_CharacterHitReactEnd 转发调用，统一玩家受击硬直恢复入口。
	void OnHitReactEnd();

protected:
	/* 蒙太奇 */
	void PlayBlockMontage(const FName& SectionName); // 播放防御蒙太奇
	virtual void PlayAttackMontage(const FName& SectionName) override;
	void PlayPotionMontage();
	virtual bool CanAttack() const override;
	virtual void OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted) override;
	void OnPotionMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	/* 动作状态 */
	UPROPERTY(BlueprintReadOnly, Category = "State", meta = (ToolTip = "当前玩家动作状态，控制攻击、硬直、翻滚、喝药等互斥动作。"))
	EActionState ActionState = EActionState::EAS_UnOccupied;

	// 体力耗尽后恢复时间
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "State", meta = (ToolTip = "体力耗尽后自动恢复的时间（秒）。"))
	float ExhaustedTime = 3.f;

	/* 移动速度 */
	UPROPERTY(EditDefaultsOnly, Category = "Movement", meta = (ToolTip = "步行速度。"))
	float WalkSpeed = 200.f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement", meta = (ToolTip = "普通奔跑速度。"))
	float RunSpeed = 300.f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement", meta = (ToolTip = "冲刺速度。"))
	float SprintSpeed = 360.f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Stamina", meta = (ClampMin = "0.0", ToolTip = "敌人主动交战或有效敌我命中结束后，冲刺继续消耗体力的保持时间（秒）。"))
	float CombatPresenceExitDelay = 4.f;

private:
	/* 动作状态恢复 Helpers */
	bool ShouldRecoverToExhausted_Generic() const;
	bool ShouldRecoverToExhausted_Attack() const;
	void EnsureExhaustionRecoveryTimer();
	void StartGuardBreak();
	void OnGuardBreakMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	void RecoverFromGuardBreak();
	EActionState RecoverActionStateAfterMontage(EActionState ExpectedState, bool bResumeStaminaRegen);
	void RecoverFromAttackMontageEnd();
	void CleanupInterruptedAttack();
	// 玩家侧 7 处 Montage EndDelegate 绑定的统一入口；敌人侧保留局部绑定，避免收尾期扩大重构范围。
	void BindMontageEndDelegate(UAnimInstance* AnimInstance, UAnimMontage* Montage,
	                            void (AMyCharacter::*Callback)(UAnimMontage*, bool));
	bool StartComboSegment(int32 SegmentIndex, EComboPlaybackMode PlaybackMode);
	/**
	 * 为当前攻击写入玩家侧 Motion Warping 目标。
	 * Config 来自轻击段、冲刺攻击或蓄力 Release；目标名固定为 AttackTarget。
	 */
	void UpdateAttackMotionWarpTarget(const FPlayerAttackMotionWarpingConfig& Config);
	void ClearAttackMotionWarpTarget();
	/**
	 * 尝试启动指定动作。
	 * 返回 true 表示“当前处于或刚启动该动作”，不区分“新启动”和“已在执行”。
	 * 返回 false 表示“不能进入该动作”（条件不满足、资源缺失、占位类型）。
	 *
	 * 注意：Block 的幂等 return true 不代表播放了新的 BlockRaise，仅代表举盾态成立。
	 * 阶段 3 若需要区分“触发新动作 vs 已在执行”，再扩展返回类型。
	 */
	bool TryStartAction(EPlayerActionType Action);
	bool StartAttackAction();
	bool StartDodgeAction();
	bool StartBlockAction();
	bool StartParryAction();
	bool StartPotionAction();
	bool StartBowAimAction();
	bool ReleaseBowArrow();
	EPlayerActionType GetCurrentPlayerActionType() const;
	bool CanCancelCurrentActionWith(EPlayerActionType NewAction) const;
	void CleanupInterruptedAction(EPlayerActionType InterruptedAction);
	int32 GetActionPriority(EPlayerActionType Action) const;
	bool IsStrictlyHigherPriority(EPlayerActionType NewAction, EPlayerActionType CurrentAction) const;
	bool IsAtLeastSamePriority(EPlayerActionType NewAction, EPlayerActionType CurrentAction) const;
	float GetDodgeStaminaCost(bool bLogFallback) const;
	float GetPotionCooldown(bool bLogFallback) const;
	float GetPotionFallbackHealPercent() const;
	float GetBlockStaminaRegenMultiplier() const;
	FName GetBlockRaiseSection() const;

	/* 提取方法 */
	void InitializePlayerHUD();
	void UpdatePotionCooldownHUD() const;
	void DrawDebugInfo() const;
	void StopBlockMontage(float BlendOutTime);
	bool ShouldInterruptBlock() const;
	void TickSprintStamina();
	void RefreshCombatPresenceFromEnemyEngagement();
	void MarkCombatPresence();
	void ClearCombatPresence();
	bool IsCombatPresenceActive() const;
	float CalcBaseSpeed(float DotProduct) const;
	void UpdateLockOnCamera(float DeltaTime);
	void GetLockOnCameraTargets(FVector& OutSocketTarget, float& OutArmLengthTarget, float& OutInterpSpeed) const;
	void CacheLockOnRotationState();
	void EnterLockOnRotationMode();
	void RestoreCachedRotationState();
	void ClearCurrentLockOnTarget();
	void SetLockOnTarget(AEnemy* NewTarget);
	bool TryRetargetLockOnAfterTargetDeath();
	bool IsLockOnTargetValid() const;
	void UpdateLockOnControlRotation(float DeltaTime) const;
	void UpdateLockOnActorFacing(float DeltaTime);
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
	void RefreshCurrentInteractable();
	void UpdateInteractionPrompt();
	void BeginDeathRespawnFlow();
	ABow* GetEquippedBow() const;
	bool IsBowEquipped() const;
	bool IsBowAiming() const;
	bool CanStartBowAim() const;
	void CancelBowAim(bool bClearBlockHeld, bool bResetReleaseCooldown = true);
	void ResetBowReleaseCooldown();
	bool ConsumeProjectilePrepareFailureForDebug();

	/* 相机组件 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true", ToolTip = "玩家相机。"))
	UCameraComponent* Camera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true", ToolTip = "玩家弹簧臂。"))
	USpringArmComponent* SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true", ToolTip = "玩家锁定组件，负责锁定状态、目标筛选和参数持有。"))
	UPlayerLockOnComponent* LockOnComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true", ToolTip = "玩家已拥有物品与数据装备槽缓存；持久化写入由 GameInstance 负责。"))
	UItemOwnershipComponent* ItemOwnershipComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true", ToolTip = "Motion Warping 组件，用于锁定攻击在蒙太奇窗口内做短距离目标修正。"))
	UMotionWarpingComponent* MotionWarpingComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true", ToolTip = "Noise Emitter 组件，用于使感知系统能听到玩家噪音。"))
	UPawnNoiseEmitterComponent* NoiseEmitterComponent;

	/* HUD */
	// 玩家 HUD 控件类，本地玩家被控制器 Possess 后创建并绑定到视口
	UPROPERTY(EditDefaultsOnly, Category = "HUD", meta = (ToolTip = "玩家 HUD 控件类，本地玩家被控制器 Possess 后创建并绑定到视口。"))
	TSubclassOf<UPlayerHUDWidget> PlayerHUDClass;

	UPROPERTY()
	UPlayerHUDWidget* PlayerHUDWidget;

	/* 状态 */
	/* Charged Attack */
	bool bAttackInputHeld = false;
	bool bIsChargingAttack = false;
	float AttackInputPressTime = 0.f;
	FTimerHandle ChargeDecisionTimer;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Charge", meta = (ToolTip = "按住攻击超过该时间后进入蓄力判定。"))
	float ChargeInputThreshold = 0.2f;

	bool bPendingExhaustedAfterAttack = false;
	bool bGuardBreakRequested = false;

	// 弓的瞄准输入仅在主手实际是 ABow 时生效；不与剑的蓄力状态复用。
	bool bBowDrawInputHeld = false;
	bool bBowReleaseOnCooldown = false;
	bool bFailNextProjectilePrepareForDebug = false;
	FTimerHandle BowReleaseCooldownTimer;

	// 当前重叠的可拾取物品
	UPROPERTY(VisibleInstanceOnly, Category = "State", meta = (AllowPrivateAccess = "true", ToolTip = "当前重叠的可拾取物品。"))
	Aitem* OverLapItem;

	TArray<TWeakObjectPtr<AActor>> InteractableCandidates;
	TWeakObjectPtr<AActor> CurrentInteractable;

	UPROPERTY(BlueprintReadOnly, Category = "State", meta = (AllowPrivateAccess = "true", ToolTip = "当前是否处于冲刺状态。"))
	bool bIsSprinting = false;

	// 仅当前 Pawn 的运行时战斗存在时间戳；不持久化，也不向 Blueprint / UMG 暴露。
	float LastCombatPresenceTime = -1.f;

	UPROPERTY(BlueprintReadOnly, Category = "State", meta = (AllowPrivateAccess = "true", ToolTip = "当前是否处于步行状态。"))
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

	/* 翻滚 */
	float DodgeStaminaCost = 15.f;
	bool bDodgeInvulnerable = false;

	// 火堆服务菜单期间的短暂运行态；不属于可持久化或战斗 FSM 状态。
	bool bBonfireServiceProtected = false;

	bool ResolveLoadoutDefinition(EItemEquipmentSlot EquipmentSlot, FName InstanceId,
	                              const UItemDefinitionDataAsset*& OutDefinition) const;
	bool PrepareMaterializedLoadoutActor(EItemEquipmentSlot EquipmentSlot, FName InstanceId, Aitem*& OutItem);
	bool PrepareMaterializedLoadoutActorFromDefinition(EItemEquipmentSlot EquipmentSlot,
	                                                   const UItemDefinitionDataAsset* Definition, Aitem*& OutItem);
	void CommitMaterializedLoadoutActor(EItemEquipmentSlot EquipmentSlot, Aitem* Item, bool bPlayEquipSound);
	void DestroyMaterializedLoadoutSlot(EItemEquipmentSlot EquipmentSlot);

	/* 药瓶 */
	bool bPotionOnCooldown = false;
	FTimerHandle PotionCooldownTimer;
	UPROPERTY(VisibleAnywhere, Category = "Combat|Potion|Legacy", meta = (ToolTip = "DataAsset 缺失时的喝药冷却 fallback。正常路径请在 DA_PlayerActionConfig.Potion.Cooldown 调整。"))
	float PotionCooldown = 2.f;

	void StartPotionCooldown();
	void ResetPotionCooldown();

	FTimerHandle ExhaustionTimerHandle;

	// 受击相机晃动（所有受击路径触发，含格挡）
	UPROPERTY(EditDefaultsOnly, Category = "Combat", meta = (ToolTip = "受击时播放的相机晃动类，含格挡也会触发。"))
	TSubclassOf<UCameraShakeBase> HitReceivedCameraShake;

	// 受击染红缩放系数（TryBlockHit 设置，TakeDamage 推送给 HUD 后归位）
	float LastDamageFlashScale = 1.f;

	/* 玩家配置入口 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Profile", meta = (AllowPrivateAccess = "true", ToolTip = "玩家角色配置入口，引用攻击配置和玩家动作配置。"))
	TObjectPtr<UPlayerCharacterProfileDataAsset> PlayerProfile;

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

	UPROPERTY(EditDefaultsOnly, Category = "Performance", meta = (ClampMin = "0.0", ToolTip = "PIE/运行时帧率上限兜底。0 表示不在 BeginPlay 覆盖 t.MaxFPS。"))
	float PIETargetMaxFPS = 120.f;

	/* 连招系统 */
	int32 ComboCounter = 0;
	bool bComboWindowOpen = false;
	bool bComboInputReceived = false;
	bool bActionCancelWindowOpen = false;

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
	float RecenterTargetPitch = -18.f;

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
	FORCEINLINE UItemOwnershipComponent* GetItemOwnershipComponent() const { return ItemOwnershipComponent; }

private:
	virtual UHitReactionConfigDataAsset* GetReactionConfig() const override
	{
		return PlayerProfile ? PlayerProfile->GetReactionConfig() : nullptr;
	}

	FORCEINLINE UAttackConfigDataAsset* GetAttackConfig() const
	{
		return PlayerProfile ? PlayerProfile->GetAttackConfig() : nullptr;
	}

	FORCEINLINE UPlayerActionConfigDataAsset* GetActionConfig() const
	{
		return PlayerProfile ? PlayerProfile->GetActionConfig() : nullptr;
	}

	FORCEINLINE UAnimMontage* GetDodgeMontage() const
	{
		const UPlayerActionConfigDataAsset* ActionConfig = GetActionConfig();
		return ActionConfig ? ActionConfig->Dodge.Montage.Get() : nullptr;
	}

	FORCEINLINE UAnimMontage* GetBlockMontage() const
	{
		const UPlayerActionConfigDataAsset* ActionConfig = GetActionConfig();
		return ActionConfig ? ActionConfig->Block.Montage.Get() : nullptr;
	}

	FORCEINLINE UAnimMontage* GetGuardBreakMontage() const
	{
		const UPlayerActionConfigDataAsset* ActionConfig = GetActionConfig();
		return ActionConfig ? ActionConfig->GuardBreak.Montage.Get() : nullptr;
	}

	FORCEINLINE UAnimMontage* GetParryMontage() const
	{
		const UPlayerActionConfigDataAsset* ActionConfig = GetActionConfig();
		return ActionConfig ? ActionConfig->Parry.Montage.Get() : nullptr;
	}

	FORCEINLINE UAnimMontage* GetPotionMontage() const
	{
		const UPlayerActionConfigDataAsset* ActionConfig = GetActionConfig();
		return ActionConfig ? ActionConfig->Potion.Montage.Get() : nullptr;
	}
};
