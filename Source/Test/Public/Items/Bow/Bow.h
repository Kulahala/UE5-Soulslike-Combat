#pragma once

#include "CoreMinimal.h"
#include "Combat/CombatProjectile.h"
#include "Items/Bow/BowBase.h"
#include "Bow.generated.h"

class UStaticMeshComponent;
class USoundBase;
class UAnimMontage;

/** 主手弓的静态投递配置；物品所有权、箭数和玩家动作仍由各自系统持有。 */
UCLASS()
class TEST_API ABow : public ABowBase
{
	GENERATED_BODY()

public:
	ABow();

	bool HasValidProjectileConfig(FString& OutFailureReason) const;
	void SetLoadedArrowVisualVisible(bool bVisible);
	void PlayShotSound() const;
	void PlayEmptyAmmoSound() const;

	FORCEINLINE FName GetAmmoDefinitionId() const { return AmmoDefinitionId; }
	FORCEINLINE TSubclassOf<ACombatProjectile> GetProjectileClass() const { return ProjectileClass; }
	FORCEINLINE const FProjectileDeliveryConfig& GetProjectileDeliveryConfig() const { return ProjectileDeliveryConfig; }
	FORCEINLINE float GetAimMoveSpeedMultiplier() const { return AimMoveSpeedMultiplier; }
	FORCEINLINE UAnimMontage* GetAimRaiseMontage() const { return AimRaiseMontage; }
	FORCEINLINE UAnimMontage* GetDrawMontage() const { return DrawMontage; }
	FORCEINLINE UAnimMontage* GetReleaseMontage() const { return ReleaseMontage; }
	FORCEINLINE UAnimMontage* GetLoadMontage() const { return LoadMontage; }

protected:
	virtual void OnPhysicalProfileApplied() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true", ToolTip = "仅用于已装填箭表现的无碰撞 Mesh；其轴修正由共享 PhysicalProfile 提供。"))
	UStaticMeshComponent* LoadedArrowVisual = nullptr;

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

};
