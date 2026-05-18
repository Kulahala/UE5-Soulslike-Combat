// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "CharacterController.generated.h"

class UInputMappingContext;
class UInputAction;

/**
 * 
 */
UCLASS()
class TEST_API ACharacterController : public APlayerController
{
	GENERATED_BODY()

public:

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	/* ================= 输入回调函数 ================= */
	void Input_Move(const FInputActionValue& Value);
	void Input_MoveEnd();  // [调试] 松开清零方向
	void Input_Look(const FInputActionValue& Value);
	void Input_Jump();
	void Input_StopJumping();
	void Input_Equip();
	void Input_Attack();
	void Input_Arm();
	void Input_SprintStart();
	void Input_SprintEnd();
	void Input_WalkStart();
	void Input_WalkEnd();
	void Input_BlockStart();
	void Input_BlockEnd();
	void Input_LockOn();
	void Input_Parry();

	/* ================= 增强输入资产声明 ================= */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (ToolTip = "默认输入映射上下文。"))
	UInputMappingContext* DefaultMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (ToolTip = "移动输入动作。"))
	UInputAction* MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (ToolTip = "视角输入动作。"))
	UInputAction* LookAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (ToolTip = "跳跃输入动作。"))
	UInputAction* JumpAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (ToolTip = "拾取/装备输入动作。"))
	UInputAction* EquipAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (ToolTip = "攻击输入动作。"))
	UInputAction* AttackAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (ToolTip = "拔刀/收刀输入动作。"))
	UInputAction* ArmAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (ToolTip = "冲刺输入动作。"))
	UInputAction* SprintAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (ToolTip = "步行输入动作。"))
	UInputAction* WalkAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (ToolTip = "防御输入动作。"))
	UInputAction* BlockAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (ToolTip = "锁定输入动作（中键）。"))
	UInputAction* LockOnAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (ToolTip = "弹反输入动作。"))
	UInputAction* ParryAction;

private:
	// 输入调试状态
	FVector2D DebugMoveInput = FVector2D::ZeroVector;
	bool bDebugSprintHeld = false;
	bool bDebugWalkHeld = false;
	bool bDebugBlockHeld = false;
	float DebugAttackExpireTime = 0.f;
	float DebugJumpExpireTime = 0.f;
	float DebugEquipExpireTime = 0.f;
	float DebugArmExpireTime = 0.f;
	float DebugLockOnExpireTime = 0.f;
	float DebugParryExpireTime = 0.f;

public:
	FString GetDebugInputText() const;
	class AMyCharacter* GetMyCharacter() const;
};
