#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Character/CharacterTypes.h" // 确保包含你的枚举头文件
#include "AnimNotify_SetState.generated.h"

// 专门用于改变 动作状态 (Action State) 的通知
UCLASS()
class TEST_API UAnimNotify_SetActionState : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

public:
	// 通知触发时设置的目标动作状态
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State", meta = (ToolTip = "通知触发时设置的目标动作状态。"))
	EActionState NewActionState = EActionState::EAS_UnOccupied; // 默认给个结束占用的状态
};

// 专门用于结束 敌人受击硬直 的通知
UCLASS()
class TEST_API UAnimNotify_EnemyHitReactEnd : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};

// 专门用于结束 敌人攻击状态 的通知
UCLASS()
class TEST_API UAnimNotify_EnemyAttackEnd : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};

// 专门用于结束 主角受击硬直 的通知
UCLASS()
class TEST_API UAnimNotify_CharacterHitReactEnd : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};