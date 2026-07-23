#pragma once

#include "CoreMinimal.h"
#include "Combat/CombatProjectile.h"
#include "Items/Bow/BowBase.h"
#include "Bow.generated.h"

class USceneComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class USoundBase;
class UAnimMontage;

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

/** 主手弓的静态投递配置；物品所有权、箭数和玩家动作仍由各自系统持有。 */
UCLASS()
class TEST_API ABow : public ABowBase
{
	GENERATED_BODY()

public:
	ABow();

	bool HasValidProjectileConfig(FString& OutFailureReason) const;
	/** 将尚未激活的候选箭根组件 Snap 到 Bow 自身的箭槽 Socket；失败会销毁候选箭。 */
	bool NockPreparedProjectile(ACombatProjectile* PreparedProjectile) const;
	FVector GetProjectileSpawnLocation() const;
	void SetLoadedArrowVisualVisible(bool bVisible);
	void SetBowPresentationState(EBowPresentationState NewState);
	void PlayShotSound() const;
	void PlayEmptyAmmoSound() const;

	UFUNCTION(BlueprintPure, Category = "Bow|Presentation")
	EBowPresentationState GetBowPresentationState() const;

	FORCEINLINE FName GetAmmoDefinitionId() const { return AmmoDefinitionId; }
	FORCEINLINE TSubclassOf<ACombatProjectile> GetProjectileClass() const { return ProjectileClass; }
	FORCEINLINE const FProjectileDeliveryConfig& GetProjectileDeliveryConfig() const { return ProjectileDeliveryConfig; }
	FORCEINLINE float GetAimMoveSpeedMultiplier() const { return AimMoveSpeedMultiplier; }
	FORCEINLINE UAnimMontage* GetAimRaiseMontage() const { return AimRaiseMontage; }
	FORCEINLINE UAnimMontage* GetDrawMontage() const { return DrawMontage; }
	FORCEINLINE UAnimMontage* GetReleaseMontage() const { return ReleaseMontage; }
	FORCEINLINE UAnimMontage* GetLoadMontage() const { return LoadMontage; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true", ToolTip = "玩家 Bow 的无碰撞 Skeletal 渲染层；静态 Mesh 仍保留为附着与 Trace 锚点。"))
	USkeletalMeshComponent* BowSkeletalVisual = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true", ToolTip = "已装填箭的作者可调锚点；正式 Bow Blueprint 应将它对齐到弦上的箭轴。"))
	USceneComponent* LoadedArrowAnchor = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true", ToolTip = "仅用于已装填箭表现的无碰撞 Mesh。有效瞄准且有 Loaded Ammo 时由玩家显示。"))
	UStaticMeshComponent* LoadedArrowVisual = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true", ToolTip = "弓箭的作者可调发射点；正式 Bow Blueprint 应将它对齐到飞行箭的 Actor Pivot。"))
	USceneComponent* ProjectileSpawnPoint = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Ammo", meta = (AllowPrivateAccess = "true", ToolTip = "发射一箭需要消费的非装备物品 DefinitionId。"))
	FName AmmoDefinitionId = FName(TEXT("Item_DarkKnightArrow"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Projectile", meta = (AllowPrivateAccess = "true", ToolTip = "弓释放时创建的投射物类。"))
	TSubclassOf<ACombatProjectile> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Projectile", meta = (AllowPrivateAccess = "true", ToolTip = "本次箭矢发射时复制的不可变命中与移动配置。"))
	FProjectileDeliveryConfig ProjectileDeliveryConfig;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Aim", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0", ToolTip = "瞄准期间相对角色步行速度的移动倍率。默认 1.0 表示与按住步行键时一致。"))
	float AimMoveSpeedMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Presentation", meta = (AllowPrivateAccess = "true", ToolTip = "成功进入 Bow Aim 后可选播放的短举弓蒙太奇；缺失时瞄准仍可正常进入。"))
	UAnimMontage* AimRaiseMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Presentation", meta = (AllowPrivateAccess = "true", ToolTip = "LMB 蓄力必须成功播放并自然结束的满弓蒙太奇；其自然结束才允许释放箭。"))
	UAnimMontage* DrawMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Presentation", meta = (AllowPrivateAccess = "true", ToolTip = "正式表现阶段可配置的放箭蒙太奇。"))
	UAnimMontage* ReleaseMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Presentation", meta = (AllowPrivateAccess = "true", ToolTip = "Release 结束且仍按住瞄准键时播放的取箭与搭箭蒙太奇；不负责生成投射物或消费弹药。"))
	UAnimMontage* LoadMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Audio", meta = (AllowPrivateAccess = "true", ToolTip = "成功放箭时播放的可选音效。"))
	USoundBase* ShotSound = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Audio", meta = (AllowPrivateAccess = "true", ToolTip = "箭数不足时播放的可选音效。"))
	USoundBase* EmptyAmmoSound = nullptr;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Bow|Presentation", meta = (AllowPrivateAccess = "true", ToolTip = "仅驱动 Bow 自身 AnimBP 的瞬态表现状态。"))
	EBowPresentationState BowPresentationState = EBowPresentationState::EBPS_Relaxed;
};
