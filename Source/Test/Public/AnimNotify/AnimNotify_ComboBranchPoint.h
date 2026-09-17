#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_ComboBranchPoint.generated.h"

/**
 * 连招分支点：消费 ComboWindow 中缓存的输入，决定是否跳到下一段攻击。
 */
UCLASS()
class TEST_API UAnimNotify_ComboBranchPoint : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	                    const FAnimNotifyEventReference& EventReference) override;
};
