#pragma once

#include "CoreMinimal.h"
#include "Combat/CombatProjectile.h"
#include "Items/Bow/BowBase.h"
#include "Bow.generated.h"

class USceneComponent;
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
	FVector GetProjectileSpawnLocation() const;
	void PlayShotSound() const;
	void PlayEmptyAmmoSound() const;

	FORCEINLINE FName GetAmmoDefinitionId() const { return AmmoDefinitionId; }
	FORCEINLINE TSubclassOf<ACombatProjectile> GetProjectileClass() const { return ProjectileClass; }
	FORCEINLINE const FProjectileDeliveryConfig& GetProjectileDeliveryConfig() const { return ProjectileDeliveryConfig; }
	FORCEINLINE float GetAimMoveSpeedMultiplier() const { return AimMoveSpeedMultiplier; }
	FORCEINLINE float GetShotCooldown() const { return ShotCooldown; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true", ToolTip = "弓箭的作者可调发射点；正式 Bow Blueprint 应将它对齐到箭尾。"))
	USceneComponent* ProjectileSpawnPoint = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Ammo", meta = (AllowPrivateAccess = "true", ToolTip = "发射一箭需要消费的非装备物品 DefinitionId。"))
	FName AmmoDefinitionId = FName(TEXT("Item_DarkKnightArrow"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Projectile", meta = (AllowPrivateAccess = "true", ToolTip = "弓释放时创建的投射物类。"))
	TSubclassOf<ACombatProjectile> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Projectile", meta = (AllowPrivateAccess = "true", ToolTip = "本次箭矢发射时复制的不可变命中与移动配置。"))
	FProjectileDeliveryConfig ProjectileDeliveryConfig;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Aim", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0", ToolTip = "瞄准期间相对角色步行速度的移动倍率。默认 1.0 表示与按住步行键时一致。"))
	float AimMoveSpeedMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Aim", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0", ToolTip = "成功放箭后的最短再次释放间隔。"))
	float ShotCooldown = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Presentation", meta = (AllowPrivateAccess = "true", ToolTip = "正式表现阶段可配置的瞄准循环蒙太奇。"))
	UAnimMontage* AimMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Presentation", meta = (AllowPrivateAccess = "true", ToolTip = "正式表现阶段可配置的放箭蒙太奇。"))
	UAnimMontage* ReleaseMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Audio", meta = (AllowPrivateAccess = "true", ToolTip = "成功放箭时播放的可选音效。"))
	USoundBase* ShotSound = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Audio", meta = (AllowPrivateAccess = "true", ToolTip = "箭数不足时播放的可选音效。"))
	USoundBase* EmptyAmmoSound = nullptr;
};
