// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Character/BaseCharacter.h"
#include "Character/CharacterTypes.h"
#include "Perception/AIPerceptionTypes.h"
#include "Enemy.generated.h"

class AEnemy;
class UAIPerceptionComponent;
class AAIController;
class UHealthBarComponent;
class UWidgetComponent;


UCLASS()
class TEST_API AEnemy : public ABaseCharacter
{
	GENERATED_BODY()

public:
	/* 生命周期 */
	AEnemy();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/* 初始化 */
	void SpawnPointInit();
	void WeaponInit();

	/* 受击/死亡 */
	virtual void GetHit_Implementation(const FVector& ImpactPoint, AActor* HitInstigator) override;
	virtual float TakeDamage(float DamageAmount, const struct FDamageEvent& DamageEvent,
	                         class AController* EventInstigator, AActor* DamageCauser) override;
	virtual void DirectionalHitReact(const FVector& ImpactPoint, const AActor* HitInstigator) override;
	void Die(); // 死亡演出

	/* 攻击 */
	virtual void Attack() override;

	/* 蒙太奇回调 */
	UFUNCTION(BlueprintCallable)
	void OnHitReactEnd();
	UFUNCTION(BlueprintCallable)
	void OnAttackEnd();
	virtual void OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted) override;
	virtual void OnHitReactMontageEnded(UAnimMontage* Montage, bool bInterrupted) override;

protected:
	/* 攻击 */
	virtual bool CanAttack() const override;
	virtual void PlayHitReactMontage(const FName& SectionName) override;
	void OnAttackCooldownEnd(); // 攻击冷却到期回调

	/* 状态机 */
	void CheckCombatTarget(); // 根据目标距离决定战斗/追击/巡逻
	void SetEnemyState(EEnemyState NewState); // 状态切换并处理进入/退出状态的一次性事件

	/* AI感知 */
	UFUNCTION()
	void TargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus); // 感知到目标的回调

	/* AI Tick */
	void OnPatrolling(float DeltaTime); // 巡逻Tick逻辑
	void OnSearching(float DeltaTime); // 巡逻到达点张望Tick逻辑
	void OnLostTargetSearch(float DeltaTime); // 追丢搜寻Tick逻辑
	void OnChasing(); // 追逐Tick逻辑
	void OnCombating(float DeltaTime); // 战斗Tick逻辑

	/* 导航/工具 */
	void MoveToTarget(const AActor* Target); // 导航移动到目标
	void MoveToLocation(const FVector& Location); // 导航到坐标点
	bool BInTargetRange(AActor* Target, double Range) const; // 检查目标是否在范围内

	/* 血条 */
	void ShowHealthBar();
	void HideHealthBar();

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	float HealthBarDisplayTime = 4.0f; // 血条显示持续时间

private:
	/* 组件 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UAIPerceptionComponent* AIPerceptionComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UHealthBarComponent* HealthBarWidgetComp;

	/* 状态 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State", meta = (AllowPrivateAccess = "true"))
	EEnemyState EnemyState = EEnemyState::EES_UnOccupied;

	/* 战斗属性 */
	UPROPERTY(VisibleInstanceOnly)
	AActor* ChasingTarget; // 当前追击目标

	// 感知/追击范围：玩家进入此距离 → 追击；超出 → 丢失目标回到巡逻
	UPROPERTY(EditAnywhere, Category = "Combat")
	float ChasingRadius = 1000.f;

	// 视野锥角（半角，总FOV = 此值 × 2）
	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ClampMin = "10", ClampMax = "180"))
	float VisionAngleDegrees = 75.f;

	// 战斗范围：目标进入此距离 → 停止追击，开始攻击
	// 需大于 AI 实际能靠近的最小距离（受导航网格和胶囊体碰撞影响）
	UPROPERTY(EditAnywhere, Category = "Combat")
	float CombatingRadius = 200.f;

	// 攻击面朝阈值：DotProduct > 此值才允许攻击（0.965 ≈ ±15°）
	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ClampMin = "0.5", ClampMax = "1.0"))
	float AttackAngleThreshold = 0.965f;

	// 战斗中转向目标的插值速度
	UPROPERTY(EditAnywhere, Category = "Combat")
	float CombatRotationSpeed = 6.f;

	// 巡逻移动速度
	UPROPERTY(EditAnywhere, Category = "Combat")
	float PatrolSpeed = 150.f;

	// 追击移动速度
	UPROPERTY(EditAnywhere, Category = "Combat")
	float ChaseSpeed = 330.f;

	// 死亡后尸体销毁时间（秒）
	UPROPERTY(EditAnywhere, Category = "Combat")
	float CorpseLifespan = 5.f;

	// 攻击最小间隔（秒，从攻击开始算）
	UPROPERTY(EditAnywhere, Category = "Combat")
	float MinAttackInterval = 3.f;

	// 攻击最大间隔（秒，从攻击开始算）
	UPROPERTY(EditAnywhere, Category = "Combat")
	float MaxAttackInterval = 5.f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	TSubclassOf<AWeapon> WeaponClass; // 选择武器类，BeginPlay自动生成

	/* 巡逻 */
	UPROPERTY()
	AActor* SpawnPoint; // 动态生成的出生点

	UPROPERTY()
	AAIController* EnemyController;

	UPROPERTY(EditAnywhere, Category = "Ai Navigation", meta = (AllowPrivateAccess = "true"))
	AActor* PatrolTarget; // 当前巡逻目标点

	// 巡逻到达判定半径：进入此距离 → 视为到达巡逻点，开始等待/张望
	UPROPERTY(EditAnywhere, Category = "Ai Navigation", meta = (AllowPrivateAccess = "true"))
	float PatrolRadius = 200.f;

	UPROPERTY(EditAnywhere, Category = "Ai Navigation", meta = (AllowPrivateAccess = "true"))
	TArray<AActor*> PatrolTargets; // 巡逻点列表

	UPROPERTY(EditAnywhere, Category = "Ai Navigation", meta = (AllowPrivateAccess = "true"))
	float PatrolWaitMin = 4.f; // 到达巡逻点后最短等待时间

	UPROPERTY(EditAnywhere, Category = "Ai Navigation", meta = (AllowPrivateAccess = "true"))
	float PatrolWaitMax = 6.f; // 到达巡逻点后最长等待时间

	UPROPERTY(EditAnywhere, Category = "Ai Navigation", meta = (AllowPrivateAccess = "true"))
	float PatrolRotationSpeed = 2.f; // 巡逻张望旋转速度

	UPROPERTY(EditAnywhere, Category = "Ai Navigation", meta = (AllowPrivateAccess = "true"))
	float SingleLookTime = 1.5f; // 单次张望持续时间

	void SearchTimerFinished(); // 巡逻张望结束回调
	void LostTargetSearchFinished(); // 追丢搜寻结束回调
	void GenerateNewLookRotation(); // 生成新的张望方向
	AActor* ChooseRadomTarget(const TArray<AActor*>& TargetArray); // 随机选择巡逻点
	FRotator PatrolWaitTargetRotation; // 张望目标旋转

	/* 追丢搜寻 */
	FVector LastKnownLocation; // 玩家最后已知位置
	bool bSearchingLostTarget = false; // 区分巡逻张望 vs 追丢搜寻

	UPROPERTY(EditAnywhere, Category = "Combat", meta = (AllowPrivateAccess = "true"))
	float SearchAcceptanceRadius = 50.f; // 追丢搜寻到达判定半径

	/* 定时器 */
	FTimerHandle PatrolTimer; // 巡逻等待定时器
	FTimerHandle LookTimer; // 张望定时器
	FTimerHandle AttackCooldownTimer; // 攻击冷却定时器
	FTimerHandle HealthBarHideTimer; // 血条延迟隐藏定时器
	bool bAttackOnCooldown = false; // 攻击冷却中
	void ClearPatrolTimers(); // 清理巡逻相关定时器
	void ClearAllTimers(); // 清理所有定时器（巡逻 + 冷却 + 血条）
};
