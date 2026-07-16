#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CombatProjectile.generated.h"

class AController;
class USphereComponent;
class UProjectileMovementComponent;
class UWorld;

USTRUCT(BlueprintType)
struct FProjectileDeliveryConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile|Combat", meta = (ClampMin = "0.0", ToolTip = "本次命中的基础伤害。发射时会被复制为不可变快照。"))
	float Damage = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile|Combat", meta = (ClampMin = "0.0", ToolTip = "本次命中的敌人韧性伤害。发射时会被复制为不可变快照。"))
	float PoiseDamage = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile|Combat", meta = (ClampMin = "0.0", ToolTip = "目标成功格挡时的体力消耗倍率。"))
	float BlockStaminaDamageMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile|Combat", meta = (ToolTip = "投射物是否能被当前弹反窗口弹反。默认关闭。"))
	bool bCanBeParried = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile|Movement", meta = (ClampMin = "1.0", ToolTip = "直线投射物初速度（cm/s）。"))
	float InitialSpeed = 3000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile|Movement", meta = (ClampMin = "1.0", ToolTip = "直线投射物最大速度（cm/s）。"))
	float MaxSpeed = 3000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile|Collision", meta = (ClampMin = "0.1", ToolTip = "投射物球体碰撞半径（cm）。"))
	float CollisionRadius = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile|Lifetime", meta = (ClampMin = "0.01", ToolTip = "未命中时自动销毁的最长时间（秒）。"))
	float MaxLifetime = 3.f;

	bool IsValid() const
	{
		return Damage >= 0.f
			&& PoiseDamage >= 0.f
			&& BlockStaminaDamageMultiplier >= 0.f
			&& InitialSpeed > 0.f
			&& MaxSpeed >= InitialSpeed
			&& CollisionRadius > 0.f
			&& MaxLifetime > 0.f;
	}
};

/** 发射端在生成前提供的瞬时上下文。 */
struct FProjectileLaunchParams
{
	AActor* Attacker = nullptr;
	AController* EventInstigator = nullptr;
	FVector SpawnLocation = FVector::ZeroVector;
	FVector LaunchDirection = FVector::ForwardVector;
	bool bOverrideDeliveryConfig = false;
	FProjectileDeliveryConfig DeliveryConfigOverride;
};

/**
 * 直线、零重力、一次性命中的战斗投射物。
 * 它不拥有武器输入或 AI 决策；调用方只在发射时提供来源、方向和可选数值快照。
 */
UCLASS()
class TEST_API ACombatProjectile : public AActor
{
	GENERATED_BODY()

public:
	ACombatProjectile();

	static ACombatProjectile* SpawnConfiguredProjectile(UWorld* World,
		TSubclassOf<ACombatProjectile> ProjectileClass, const FProjectileLaunchParams& LaunchParams);

	/** 创建已验证的静止投射物；调用者必须在提交自己的持久化事务后显式 Commit。 */
	static ACombatProjectile* SpawnPreparedProjectile(UWorld* World,
		TSubclassOf<ACombatProjectile> ProjectileClass, const FProjectileLaunchParams& LaunchParams);

	/**
	 * 提交已完整准备的投射物。调用前必须先以 IsPreparedForActivation() 完成可失败验证；
	 * Commit 只执行原生组件状态切换，不提供失败或恢复出口。
	 */
	void CommitPreparedLaunch();

	bool IsPreparedForActivation() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void LifeSpanExpired() override;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true", ToolTip = "投射物唯一碰撞体。"))
	USphereComponent* CollisionSphere = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true", ToolTip = "扫掠移动、停止和朝向组件。"))
	UProjectileMovementComponent* ProjectileMovement = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile", meta = (AllowPrivateAccess = "true", ToolTip = "默认投递参数。未来 Blueprint 子类可配置，飞行开始后不再读取。"))
	FProjectileDeliveryConfig DefaultDeliveryConfig;

	UPROPERTY()
	AActor* LaunchAttacker = nullptr;

	UPROPERTY()
	AController* LaunchEventInstigator = nullptr;

	FProjectileDeliveryConfig ActiveDeliveryConfig;
	FVector LaunchDirection = FVector::ForwardVector;
	FVector LaunchLocation = FVector::ZeroVector;
	bool bLaunchConfigured = false;
	bool bNativePreparationComplete = false;
	bool bLaunchActivated = false;
	bool bStartLaunchOnBeginPlay = true;
	bool bImpactResolved = false;

	static ACombatProjectile* SpawnProjectile(UWorld* World, TSubclassOf<ACombatProjectile> ProjectileClass,
		const FProjectileLaunchParams& LaunchParams, bool bStartImmediately);
	bool ConfigureLaunch(const FProjectileLaunchParams& LaunchParams);
	bool HasValidNativeLaunchState() const;
	void DrawDebugPath(const FVector& EndPoint, const FColor& Color) const;

	UFUNCTION()
	void OnProjectileStopped(const FHitResult& ImpactResult);
};
