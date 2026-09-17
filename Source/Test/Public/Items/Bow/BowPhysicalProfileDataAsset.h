#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BowPhysicalProfileDataAsset.generated.h"

class UAnimInstance;
class USkeletalMesh;
class UStaticMesh;

/**
 * 共享 Bow 的物理与视觉身份。角色输入、弹药和敌人攻击决策不属于此资产。
 */
UCLASS(BlueprintType)
class TEST_API UBowPhysicalProfileDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	bool ValidateProfile(FString& OutFailureReason) const;

	FORCEINLINE USkeletalMesh* GetBowSkeletalMesh() const { return BowSkeletalMesh; }
	FORCEINLINE TSubclassOf<UAnimInstance> GetBowAnimInstanceClass() const { return BowAnimInstanceClass; }
	FORCEINLINE const FTransform& GetSkeletalVisualRelativeTransform() const { return SkeletalVisualRelativeTransform; }
	FORCEINLINE UStaticMesh* GetNockedArrowStaticMesh() const { return NockedArrowStaticMesh; }
	FORCEINLINE const FTransform& GetNockedArrowVisualRelativeTransform() const { return NockedArrowVisualRelativeTransform; }

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Physical", meta = (AllowPrivateAccess = "true", ToolTip = "共享 Bow 的 Skeletal Mesh；该 Mesh 必须提供 Mesh-only BowArrowSocket。"))
	USkeletalMesh* BowSkeletalMesh = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Physical", meta = (AllowPrivateAccess = "true", ToolTip = "共享 Bow Skeleton 使用的 AnimInstance 类。未写入状态时保持 Relaxed。"))
	TSubclassOf<UAnimInstance> BowAnimInstanceClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Physical", meta = (AllowPrivateAccess = "true", ToolTip = "BowSkeletalVisual 相对静态兼容锚点的统一变换。"))
	FTransform SkeletalVisualRelativeTransform = FTransform::Identity;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Nocked Arrow", meta = (AllowPrivateAccess = "true", ToolTip = "玩家 Aim-ready 待机箭使用的共享静态 Mesh。"))
	UStaticMesh* NockedArrowStaticMesh = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Nocked Arrow", meta = (AllowPrivateAccess = "true", ToolTip = "待机箭相对 BowArrowSocket 的唯一 Pivot/轴修正。"))
	FTransform NockedArrowVisualRelativeTransform = FTransform::Identity;
};
