// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "Items/ItemDefinitionDataAsset.h"
#include "CharacterController.generated.h"

class UInputMappingContext;
class UInputAction;
class ACheckpointActor;
class UBonfireMenuWidget;
class UPauseMenuWidget;
class UDeathOverlayWidget;
class UInteractionPromptWidget;

/**
 * 
 */
UCLASS()
class TEST_API ACharacterController : public APlayerController
{
	GENERATED_BODY()

public:
	FORCEINLINE FVector2D GetCachedMoveInput() const { return CachedMoveInput; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;

	/* ================= 输入回调函数 ================= */
	void Input_Move(const FInputActionValue& Value);
	void Input_MoveEnd();  // 松开时清零移动输入缓存
	void Input_Look(const FInputActionValue& Value);
	void Input_Jump();
	void Input_StopJumping();
	void Input_Interact();
	void Input_AttackPressed();
	void Input_AttackReleased();
	void Input_AttackCanceled();
	void Input_SprintStart();
	void Input_SprintEnd();
	void Input_WalkStart();
	void Input_WalkEnd();
	void Input_BlockStart();
	void Input_BlockEnd();
	void Input_LockOn();
	void Input_LockTargetSwitch(const FInputActionValue& Value);
	void Input_LockTargetSwitchEnd();
	void Input_Parry();
	void Input_Dodge();
	void Input_Pause();
	void Input_UsePotion();
	void HandleApplicationActivationChanged(bool bIsActive);

	/* ================= 增强输入资产声明 ================= */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (ToolTip = "默认输入映射上下文。"))
	UInputMappingContext* DefaultMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (ToolTip = "移动输入动作。"))
	UInputAction* MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (ToolTip = "视角输入动作。"))
	UInputAction* LookAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (ToolTip = "跳跃输入动作。"))
	UInputAction* JumpAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (ToolTip = "世界交互输入动作。"))
	UInputAction* InteractAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (ToolTip = "攻击输入动作。"))
	UInputAction* AttackAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (ToolTip = "冲刺输入动作。"))
	UInputAction* SprintAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (ToolTip = "步行输入动作。"))
	UInputAction* WalkAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (ToolTip = "防御输入动作。"))
	UInputAction* BlockAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (ToolTip = "锁定输入动作（中键）。"))
	UInputAction* LockOnAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (ToolTip = "锁定目标切换输入动作（鼠标滚轮）。"))
	UInputAction* LockTargetSwitchAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input|LockOn", meta = (ClampMin = "0.0", ToolTip = "滚轮轴值达到该绝对值后触发一次锁定目标切换。"))
	float LockTargetSwitchInputThreshold = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Input|LockOn", meta = (ClampMin = "0.0", ToolTip = "滚轮轴值回落至该绝对值内后，允许下一次锁定目标切换。"))
	float LockTargetSwitchRearmThreshold = 0.1f;

	UPROPERTY(EditDefaultsOnly, Category = "Input|LockOn", meta = (ClampMin = "0.0", ToolTip = "成功切换锁定目标后的最短间隔（秒）。"))
	float LockTargetSwitchCooldown = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (ToolTip = "弹反输入动作。"))
	UInputAction* ParryAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (ToolTip = "翻滚输入动作。"))
	UInputAction* DodgeAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (ToolTip = "暂停输入动作。"))
	UInputAction* PauseAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (ToolTip = "喝药输入动作。"))
	UInputAction* UsePotionAction;

private:
	// 最近一次移动输入轴；攻击等状态会拦截实际移动，但翻滚方向仍需要这份输入。
	FVector2D CachedMoveInput = FVector2D::ZeroVector;

	// 输入调试状态
	bool bDebugSprintHeld = false;
	bool bDebugWalkHeld = false;
	bool bDebugBlockHeld = false;
	bool bSuppressNextAttackReleaseAfterFocusLoss = false;
	bool bLockTargetSwitchInputArmed = true;
	FDelegateHandle ApplicationActivationChangedHandle;
	float NextLockTargetSwitchTime = 0.f;
	float DebugAttackExpireTime = 0.f;
	float DebugJumpExpireTime = 0.f;
	float DebugInteractExpireTime = 0.f;
	float DebugLockOnExpireTime = 0.f;
	float DebugParryExpireTime = 0.f;
	float DebugDodgeExpireTime = 0.f;
	float DebugPotionExpireTime = 0.f;

	/* 暂停系统 */
	bool bIsPaused = false;
	bool bCanPause = true;

	UPROPERTY(EditDefaultsOnly, Category = "UI", meta = (ToolTip = "暂停菜单Widget类（蓝图子类）"))
	TSubclassOf<UPauseMenuWidget> PauseMenuClass;

	UPROPERTY()
	UPauseMenuWidget* PauseMenuWidget = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "UI", meta = (ToolTip = "交互提示 Widget 类（蓝图子类）。"))
	TSubclassOf<UInteractionPromptWidget> InteractionPromptClass;

	UPROPERTY()
	UInteractionPromptWidget* InteractionPromptWidget = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "UI", meta = (ToolTip = "被动死亡 Overlay Widget 类（蓝图子类）。"))
	TSubclassOf<UDeathOverlayWidget> DeathOverlayClass;

	UPROPERTY()
	UDeathOverlayWidget* DeathOverlayWidget = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "UI", meta = (ToolTip = "火堆服务菜单 Widget 类（蓝图子类）。"))
	TSubclassOf<UBonfireMenuWidget> BonfireMenuClass;

	UPROPERTY()
	UBonfireMenuWidget* BonfireMenuWidget = nullptr;

	TWeakObjectPtr<ACheckpointActor> ActiveBonfireCheckpoint;
	bool bBonfireMenuOpen = false;

	void TogglePause();
	void RestoreGameplayInput();
	void DismissBonfireMenu(bool bRestoreGameplayInput);

	UFUNCTION()
	void OnResumeRequested();

	UFUNCTION()
	void OnQuitRequested();

	UFUNCTION()
	void OnBonfireRestRequested();

	UFUNCTION()
	void OnBonfireLeaveRequested();

	UFUNCTION()
	void OnBonfireEquipmentRequested();

	UFUNCTION()
	void OnBonfireLoadoutSelectionRequested(EItemEquipmentSlot EquipmentSlot, FName InstanceId);

	void RefreshBonfireLoadoutOptions();

public:
	FString GetDebugInputText() const;
	class AMyCharacter* GetMyCharacter() const;

	// 非 Shipping 的物品存档验收入口；不创建 UI 或修改正式输入映射。
	UFUNCTION(Exec)
	void ItemDebugGrant(FName DefinitionId);

	UFUNCTION(Exec)
	void ItemDebugGrantQuantity(FName DefinitionId, int32 Quantity);

	UFUNCTION(Exec)
	void ItemDebugEquip(FName InstanceId);

	UFUNCTION(Exec)
	void ItemDebugDump();

	UFUNCTION(Exec)
	void ItemDebugFailNextClaimSave();

	UFUNCTION(Exec)
	void GoldDebugFailNextClaimSave();

	UFUNCTION(Exec)
	void EncounterDebugFailNextClearSave();

	UFUNCTION(Exec)
	void OneTimeEnemyDebugFailNextDefeatSave();

	UFUNCTION(Exec)
	void BowDebugFailNextAmmoConsumeSave();

	UFUNCTION(Exec)
	void BowDebugFailNextProjectilePrepare();

	UFUNCTION(Exec)
	void BowDebugFailNextAmmoRefillSave();

	UFUNCTION(Exec)
	void ItemDebugVerifyAmmoRefillFixture(FName DefinitionId);

	// 非 Shipping 的投射物验收入口；不创建正式输入映射或地图内容。
	UFUNCTION(Exec)
	void ProjectileDebugFire();

	UFUNCTION(Exec)
	void ProjectileDebugFireSelf(FName SourceTeam);

	// 非 Shipping 的无资产射手验收入口；正式 Erika 资产和 Montage 由 TODO-04D-B 接线。
	UFUNCTION(Exec)
	void EnemyRangedDebugProbe(float ReleaseDelay = 0.35f);

	bool IsPaused() const { return bIsPaused; }
	void SetCanPause(bool bCanPauseNew) { bCanPause = bCanPauseNew; }
	void ClearPauseIfActive();
	void ShowInteractionPrompt(const FText& PromptText);
	void HideInteractionPrompt();
	void ShowDeathOverlay();
	void PrepareForMapTransition();
	bool OpenBonfireMenu(ACheckpointActor* Checkpoint);
	void CloseBonfireMenu();
};
