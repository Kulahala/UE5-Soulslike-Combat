#pragma once

#include "CoreMinimal.h"
#include "Items/Weapon/Weapon.h"
#include "BowBase.generated.h"

class ACombatProjectile;
class UBowPhysicalProfileDataAsset;
class USkeletalMeshComponent;

/** 仅供 Bow 自身 AnimBP 消费的表现状态，不拥有玩家玩法状态或发射时机。 */
UENUM(BlueprintType)
enum class EBowPresentationState : uint8
{
	// 前三个值已被既有 Blueprint 资产序列化，不能重排。
	EBPS_Relaxed = 0 UMETA(DisplayName = "Relaxed"),
	EBPS_Aiming = 1 UMETA(DisplayName = "Aiming"),
	EBPS_Releasing = 2 UMETA(DisplayName = "Releasing"),
	EBPS_Charging = 3 UMETA(DisplayName = "Charging"),
	// Release 后仍按住瞄准键时，Bow 正在取箭并重新搭箭。
	EBPS_Loading = 4 UMETA(DisplayName = "Loading")
};

/**
 * Bow 的共享物理 Runtime：默认左手附着、Skeletal Bow、唯一箭槽与发射 Transform。
 * 玩家弹药/瞄准与敌人攻击决策分别留在各自系统。
 */
UCLASS(Abstract)
class TEST_API ABowBase : public AWeapon
{
	GENERATED_BODY()

public:
	ABowBase();

	/** 读取唯一 BowArrowSocket 的世界 Transform；失败时提供可显示的配置原因。 */
	bool TryGetLaunchTransform(FTransform& OutLaunchTransform, FString& OutFailureReason) const;
	/** 将尚未激活的候选箭根组件 Snap 到共享 BowArrowSocket；失败会销毁候选箭。 */
	bool NockPreparedProjectile(ACombatProjectile* PreparedProjectile) const;
	void SetBowPresentationState(EBowPresentationState NewState);

	UFUNCTION(BlueprintPure, Category = "Bow|Presentation")
	EBowPresentationState GetBowPresentationState() const;

	FORCEINLINE const UBowPhysicalProfileDataAsset* GetPhysicalProfile() const { return PhysicalProfile; }
	static FName GetBowArrowSocketName();

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	/** Profile 应用完成后供玩家 Bow 同步其待机箭表现。 */
	virtual void OnPhysicalProfileApplied();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true", ToolTip = "共享 Bow 的无碰撞 Skeletal 渲染层；静态 Mesh 仍保留为附着与 Trace 锚点。"))
	USkeletalMeshComponent* BowSkeletalVisual = nullptr;

private:
	void ApplyPhysicalProfile();
	bool ValidatePhysicalProfile(FString& OutFailureReason) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Physical", meta = (AllowPrivateAccess = "true", ToolTip = "共享 Bow Mesh、AnimInstance 与待机箭视觉配置。"))
	UBowPhysicalProfileDataAsset* PhysicalProfile = nullptr;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Bow|Presentation", meta = (AllowPrivateAccess = "true", ToolTip = "仅驱动 Bow 自身 AnimBP 的瞬态表现状态。"))
	EBowPresentationState BowPresentationState = EBowPresentationState::EBPS_Relaxed;
};
