// AnimNotify_AttachWeapon.h
#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_AttachWeapon.generated.h"

UCLASS()
class TEST_API UAnimNotify_AttachWeapon : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

public:
	// 武器挂载的骨骼插槽名
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (ToolTip = "武器挂载的骨骼插槽名，在动画编辑器中设置。"))
	FName SocketName;

};