// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Character/BaseCharacter.h"
#include "Character/CharacterTypes.h"
#include "Enemy.generated.h"

struct FAIStimulus;
struct FAIRequestID;

namespace EPathFollowingResult
{
	enum Type : int;
}

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

	/* 弹反 */
	void ApplyStanceBreak(float Duration, float PlayRate);

	/* 韧性系统 */
	UPROPERTY(EditAnywhere, Category = "Combat|Poise", meta = (ToolTip = "最大韧性值。"))
	float MaxPoise = 10.f;

	UPROPERTY(VisibleInstanceOnly, Category = "Combat|Poise", meta = (ToolTip = "当前韧性值。"))
	float CurrentPoise = 10.f;

	UPROPERTY(EditAnywhere, Category = "Combat|Poise", meta = (ToolTip = "未受击多久后重置韧性（秒）。"))
	float PoiseResetDelay = 5.f;

	UPROPERTY(EditAnywhere, Category = "Combat|Poise", meta = (ToolTip = "破防硬直时长（秒）。"))
	float StanceBreakDuration = 2.f;

	UPROPERTY(EditAnywhere, Category = "Combat|Poise", meta = (ToolTip = "破防慢放速率（0.3 = 30%速度）。"))
	float StanceBreakPlayRate = 0.3f;

	void ApplyPoiseDamage(float Damage, AActor* DamageInstigator);
	void ResetPoise();
	bool ShouldTriggerStanceBreak() const { return bPendingStanceBreak; }
	float GetCurrentPoise() const { return CurrentPoise; }

	/* 锁定联动 */
	void SetTargetedByPlayer(bool bTargeted);

	/* Getters */
	FORCEINLINE EEnemyState GetEnemyState() const { return EnemyState; }

protected:
	/* 攻击 */
	virtual bool CanAttack() const override;
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
	virtual void OnChasing(); // 追逐Tick逻辑 — 派生类可覆写（如法师后撤、自爆兵冲脸）
	void OnCombating(float DeltaTime); // 战斗Tick逻辑

	/* 战斗决策钩子 — 派生类按需覆写 */
	virtual bool ShouldTriggerAttack(float DistanceToTarget, float ForwardDot) const;
	virtual void HandleAttackReadyPositioning(float DistanceToTarget, const FVector& ToTarget);
	virtual void HandleCooldownPositioning(float DeltaTime, float DistanceToTarget, const FVector& ToTarget);

	/* 导航/工具 */
	void StopEnemyMovementIfPossible(); // 收敛：AI 停止移动
	void MoveToTarget(const AActor* Target); // 导航移动到目标
	void MoveToLocation(const FVector& Location); // 导航到坐标点
	bool BInTargetRange(AActor* Target, double Range) const; // 检查目标是否在范围内
	bool IsValidCombatTarget(const AActor* Target) const; // 目标有效且存活

	/* 血条 */
	void RevealHealthBar();
	void ShowHealthBar();
	void HideHealthBar();

	// 血条显示持续时间
	UPROPERTY(EditDefaultsOnly, Category = "UI", meta = (ToolTip = "受击后血条显示的持续时间（秒）。"))
	float HealthBarDisplayTime = 4.0f;

private:
	/* 组件 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true", ToolTip = "AI 感知组件，负责视觉/听觉检测。"))
	UAIPerceptionComponent* AIPerceptionComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true", ToolTip = "头顶血条组件。"))
	UHealthBarComponent* HealthBarWidgetComp;

	/* 状态 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State", meta = (AllowPrivateAccess = "true", ToolTip = "当前敌人状态。"))
	EEnemyState EnemyState = EEnemyState::EES_UnOccupied;

	/* 战斗属性 */
	// 当前追击目标
	UPROPERTY(VisibleInstanceOnly, meta = (ToolTip = "当前追击/战斗目标，由 AI 感知系统设置。"))
	AActor* ChasingTarget;

	// 感知/追击范围：玩家进入此距离 -> 追击；超出 -> 丢失目标回到巡逻
	// 调参关系：必须大于 CombatingRadius。
	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ClampMin = "0.0", ToolTip = "必须大于 CombatingRadius。目标进入此距离后开始追击。"))
	float ChasingRadius = 1000.f;

	// 视野锥角（半角，总FOV = 此值 × 2）
	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ClampMin = "10", ClampMax = "180", ToolTip = "视野锥半角。总FOV = 此值 × 2。"))
	float VisionAngleDegrees = 75.f;

	// 听觉感知范围
	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ClampMin = "0.0", ToolTip = "听觉感知范围（cm）。"))
	float HearingRange = 800.f;

	// 战斗范围：目标进入此距离 -> 停止追击，进入战斗拉扯
	// 调参关系：必须大于 CombatPreferredMaxRadius，建议至少多 30cm 防止后撤/侧移后立刻切回 Chasing。
	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ClampMin = "0.0", ToolTip = "必须大于 CombatPreferredMaxRadius，建议至少多 30cm 防止拉扯后立刻切回追击，且小于 ChasingRadius。"))
	float CombatingRadius = 300.f;

	// 战斗退出滞后缓冲。已在战斗族状态时，目标超出 CombatingRadius + 此值才退回 Chasing。
	// 防止 CombatingRadius 边界上状态每帧抖动。
	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ClampMin = "0.0", ToolTip = "战斗退出滞后缓冲（cm）。已在战斗族状态时，退出半径 = CombatingRadius + 此值。"))
	float CombatExitBuffer = 50.f;

	// 攻击面朝阈值：DotProduct > 此值才允许攻击（0.965 ≈ ±15°）
	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ClampMin = "0.5", ClampMax = "1.0", ToolTip = "DotProduct > 此值才允许攻击。0.965 ≈ ±15°。"))
	float AttackAngleThreshold = 0.965f;

	// 战斗中转向目标的插值速度
	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ToolTip = "战斗中转向目标的插值速度。值越大转向越快。"))
	float CombatRotationSpeed = 6.f;

	// --- 战斗拉扯参数 ---
	// 距离关系提醒：
	// CombatTooCloseRadius < CombatAttackMaxRadius <= CombatPreferredMinRadius <= CombatPreferredMaxRadius < CombatingRadius < ChasingRadius
	// CombatAttackMaxRadius 控制可出手距离；CombatPreferredMin/Max 控制冷却期想保持的距离环。
	// CombatPressMargin 必须大于 CombatRepositionAcceptanceRadius，避免前压到攻击边缘时被导航提前判定到达。
	UPROPERTY(EditAnywhere, Category = "Combat|Spacing", meta = (ClampMin = "0.0", ToolTip = "过近阈值。必须小于 CombatAttackMaxRadius。攻击CD期间，距离低于此值触发后撤。"))
	float CombatTooCloseRadius = 90.f;
	UPROPERTY(EditAnywhere, Category = "Combat|Spacing", meta = (ClampMin = "0.0", ToolTip = "可出手的最大距离。贴近实际武器射程即可，不要为了增大后撤距离而调高此值。"))
	float CombatAttackMaxRadius = 170.f;
	UPROPERTY(EditAnywhere, Category = "Combat|Spacing", meta = (ClampMin = "0.0", ToolTip = "冷却期拉扯距离环内圈。必须 >= CombatAttackMaxRadius 且 <= CombatPreferredMaxRadius。"))
	float CombatPreferredMinRadius = 210.f;
	UPROPERTY(EditAnywhere, Category = "Combat|Spacing", meta = (ClampMin = "0.0", ToolTip = "冷却期拉扯距离环外圈。必须 > CombatPreferredMinRadius 且 < CombatingRadius。默认270留出更大后撤空间。"))
	float CombatPreferredMaxRadius = 270.f;
	UPROPERTY(EditAnywhere, Category = "Combat|Spacing", meta = (ClampMin = "0.0", ClampMax = "90.0", ToolTip = "冷却期在拉扯距离环内的横移角度（绕目标旋转）。"))
	float CombatStrafeAngleDegrees = 25.f;
	UPROPERTY(EditAnywhere, Category = "Combat|Spacing", meta = (ClampMin = "0.0", ToolTip = "战斗位移的导航到达判定半径。必须小于 CombatPressMargin，避免前压时停在攻击范围外。"))
	float CombatRepositionAcceptanceRadius = 12.f;
	UPROPERTY(EditAnywhere, Category = "Combat|Spacing", meta = (ClampMin = "0.0", ToolTip = "战斗位移请求的最短间隔（秒）。必须 <= CombatRepositionIntervalMax。"))
	float CombatRepositionIntervalMin = 0.8f;
	UPROPERTY(EditAnywhere, Category = "Combat|Spacing", meta = (ClampMin = "0.0", ToolTip = "战斗位移请求的最长间隔（秒）。必须 >= CombatRepositionIntervalMin。"))
	float CombatRepositionIntervalMax = 1.4f;
	UPROPERTY(EditAnywhere, Category = "Combat|Spacing", meta = (ClampMin = "0.0", ToolTip = "前压时从目标距离环扣减的余量。必须大于 CombatRepositionAcceptanceRadius，并小于 CombatAttackMaxRadius 和 CombatPreferredMaxRadius。"))
	float CombatPressMargin = 25.f;
	UPROPERTY(EditAnywhere, Category = "Combat|Spacing", meta = (ClampMin = "0.1", ClampMax = "1.0", ToolTip = "后撤/斜后撤接近目标点时的最低速度倍率。上限使用 PatrolSpeed，默认末段降到 55%。"))
	float CombatRetreatMinSpeedRatio = 0.55f;

	// 巡逻移动速度
	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ToolTip = "巡逻状态的移动速度（cm/s）。"))
	float PatrolSpeed = 150.f;

	// 追击移动速度
	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ToolTip = "追击状态的移动速度（cm/s）。"))
	float ChaseSpeed = 330.f;

	// 死亡后尸体销毁时间（秒）
	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ToolTip = "死亡后尸体销毁延迟（秒）。"))
	float CorpseLifespan = 5.f;

	// 攻击最小间隔（秒，从攻击开始算）
	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ToolTip = "攻击冷却最短间隔（秒），从攻击开始计算。"))
	float MinAttackInterval = 3.f;

	// 攻击最大间隔（秒，从攻击开始算）
	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ToolTip = "攻击冷却最长间隔（秒），从攻击开始计算。实际间隔在 Min~Max 之间随机。"))
	float MaxAttackInterval = 5.f;

	/* 攻击协调 */
	UPROPERTY(EditAnywhere, Category = "Combat|Attack Coordination", meta = (ClampMin = "100.0", ToolTip = "攻击协调检测范围（cm）"))
	float AttackCoordinationRange = 800.f;

	UPROPERTY(EditAnywhere, Category = "Combat|Attack Coordination", meta = (ClampMin = "0.1", ToolTip = "攻击衔接缓冲时间（秒），队友攻击结束后等待多久再攻击"))
	float AttackCoordinationBuffer = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Combat|Attack Coordination", meta = (ClampMin = "1.0", ToolTip = "因队友攻击而被迫等待的最大秒数"))
	float MaxAttackCoordinationWait = 3.f;

	// 选择武器类，BeginPlay自动生成
	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ToolTip = "敌人使用的武器类，BeginPlay 时自动生成并装备。"))
	TSubclassOf<AWeapon> WeaponClass;

	/* 巡逻 */
	UPROPERTY()
	AActor* SpawnPoint; // 动态生成的出生点

	UPROPERTY()
	AAIController* EnemyController;

	// 当前巡逻目标点
	UPROPERTY(EditAnywhere, Category = "Ai Navigation", meta = (AllowPrivateAccess = "true", ToolTip = "当前巡逻目标点，运行时自动切换。"))
	AActor* PatrolTarget;

	// 巡逻到达判定半径：进入此距离 → 视为到达巡逻点，开始等待/张望
	UPROPERTY(EditAnywhere, Category = "Ai Navigation", meta = (AllowPrivateAccess = "true", ToolTip = "巡逻到达判定半径。进入此距离后切换到 Searching 状态。"))
	float PatrolRadius = 200.f;

	// 巡逻点列表
	UPROPERTY(EditAnywhere, Category = "Ai Navigation", meta = (AllowPrivateAccess = "true", ToolTip = "巡逻点列表，敌人在这些点之间循环移动。"))
	TArray<AActor*> PatrolTargets;

	// 到达巡逻点后最短等待时间
	UPROPERTY(EditAnywhere, Category = "Ai Navigation", meta = (AllowPrivateAccess = "true", ToolTip = "到达巡逻点后最短等待时间（秒）。"))
	float PatrolWaitMin = 4.f;

	// 到达巡逻点后最长等待时间
	UPROPERTY(EditAnywhere, Category = "Ai Navigation", meta = (AllowPrivateAccess = "true", ToolTip = "到达巡逻点后最长等待时间（秒）。实际等待在 Min~Max 之间随机。"))
	float PatrolWaitMax = 6.f;

	// 巡逻张望旋转速度
	UPROPERTY(EditAnywhere, Category = "Ai Navigation", meta = (AllowPrivateAccess = "true", ToolTip = "巡逻张望时的旋转速度。值越大转向越快。"))
	float PatrolRotationSpeed = 2.f;

	// 单次张望持续时间
	UPROPERTY(EditAnywhere, Category = "Ai Navigation", meta = (AllowPrivateAccess = "true", ToolTip = "单次张望持续时间（秒）。"))
	float SingleLookTime = 1.5f;

	void SearchTimerFinished(); // 巡逻张望结束回调
	void LostTargetSearchFinished(); // 追丢搜寻结束回调
	void GenerateNewLookRotation(); // 生成新的张望方向
	AActor* ChooseRadomTarget(const TArray<AActor*>& TargetArray); // 随机选择巡逻点
	FRotator PatrolWaitTargetRotation; // 张望目标旋转

	/* 追丢搜寻 */
	FVector LastKnownLocation; // 玩家最后已知位置
	bool bSearchingLostTarget = false; // 区分巡逻张望 vs 追丢搜寻

	// 追丢搜寻到达判定半径
	UPROPERTY(EditAnywhere, Category = "Combat", meta = (AllowPrivateAccess = "true", ToolTip = "追丢搜寻时的导航到达判定半径（cm）。"))
	float SearchAcceptanceRadius = 50.f;

	/* 战斗拉扯 */
	enum class EEnemyCombatMoveType : uint8
	{
		None,
		Retreat,
		BackDiag,
		Strafe,
		Press
	};

	struct FEnemyCombatMovePlan
	{
		EEnemyCombatMoveType MoveType = EEnemyCombatMoveType::None;
		FVector GoalLocation = FVector::ZeroVector;
		float MoveSpeed = 0.f;
		bool bUseRetreatSpeedEase = false;
		float RetryDelay = 0.15f;

		bool IsValid() const
		{
			return MoveType != EEnemyCombatMoveType::None;
		}
	};

	void UpdateCombatMovement(float DeltaTime, float DistanceToTarget, const FVector& ToTarget);
	FEnemyCombatMovePlan BuildCombatMovePlan(float DistanceToTarget, const FVector& ToTarget) const;
	static const TCHAR* GetCombatMoveDebugName(EEnemyCombatMoveType MoveType);
	bool MoveToCombatLocation(const FVector& Location);
	bool MoveToCombatTarget(); // 攻击 ready 时动态追踪目标 Actor
	void ResetCombatReposition();

	// 攻击协调：检查附近是否有队友正在攻击，返回最大剩余时间
	bool IsAllyAttackingNearby(float& OutMaxRemainingTime);

	void StartCombatRetreatSpeedEase(const FVector& GoalLocation);
	void UpdateCombatRetreatSpeedEase();
	void ClearCombatRetreatSpeedEase();
	UFUNCTION()
	void OnRepositionMoveCompleted(FAIRequestID RequestID, EPathFollowingResult::Type Result);
	float NextCombatRepositionTime = 0.f;
	bool bRepositionInProgress = false;
	bool bRetreatSpeedEaseActive = false;
	FVector RetreatSpeedEaseGoalLocation = FVector::ZeroVector;
	float RetreatSpeedEaseTotalDistance = 0.f;
	FString LastCombatMoveDebug;

	/* 定时器 */
	FTimerHandle PatrolTimer; // 巡逻等待定时器
	FTimerHandle LookTimer; // 张望定时器
	FTimerHandle AttackCooldownTimer; // 攻击冷却定时器
	FTimerHandle HealthBarHideTimer; // 血条延迟隐藏定时器
	FTimerHandle PoiseResetTimer; // 韧性重置定时器
	FTimerHandle StanceBreakRecoveryTimer; // 破防硬直恢复定时器
	bool bAttackOnCooldown = false; // 攻击冷却中
	bool bIsTargetedByPlayer = false; // 被玩家锁定中
	bool bPendingStanceBreak = false; // 韧性归零flag，等待GetHit后触发
	AActor* LastPoiseDamageInstigator = nullptr; // 最后一次韧性伤害的攻击者
	void RecoverFromStanceBreak(); // 破防硬直恢复回调
	void ClearPatrolTimers(); // 清理巡逻相关定时器
	void ClearAllTimers(); // 清理所有定时器（巡逻 + 冷却 + 血条 + 弹反）
};
