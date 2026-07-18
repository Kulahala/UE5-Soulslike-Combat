// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Navigation/PathFollowingComponent.h"
#include "Character/BaseCharacter.h"
#include "Character/CharacterTypes.h"
#include "Combat/CombatProjectile.h"
#include "Enemy.generated.h"

struct FAIStimulus;
struct FEnemyAttackEntry;
struct FPropertyChangedEvent;

class AEnemy;
class AEncounterController;
class AController;
class UAIPerceptionComponent;
class UAISenseConfig_Hearing;
class UAISenseConfig_Sight;
class AAIController;
class UHealthBarComponent;
class UWidgetComponent;
class UEnemyAttackConfigDataAsset;
class UMotionWarpingComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnEnemyDied, AEnemy*);

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
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/* 初始化 */
	void SpawnPointInit();
	void WeaponInit();

	/* 受击/死亡 */
	virtual void GetHit_Implementation(const FVector& ImpactPoint, AActor* HitInstigator) override;
	virtual float TakeDamage(float DamageAmount, const struct FDamageEvent& DamageEvent,
	                         class AController* EventInstigator, AActor* DamageCauser) override;
	void Die(); // 死亡演出

	/* 遭遇接线 */
	bool ClaimEncounterOwner(AEncounterController* NewOwner);
	void ReleaseEncounterOwner(AEncounterController* CurrentOwner);
	void SetEncounterDormant(AEncounterController* CurrentOwner);
	bool ActivateForEncounter(AEncounterController* CurrentOwner, AActor* InitialTarget);
	FORCEINLINE bool IsEncounterDormant() const { return bEncounterDormant; }
	FOnEnemyDied& GetOnEnemyDied() { return EnemyDiedDelegate; }

	/* 攻击 */
	virtual void Attack() override;

	/* 蒙太奇回调 */
	UFUNCTION(BlueprintCallable, meta = (ToolTip = "动画受击结束时调用，恢复敌人 AI 状态。"))
	void OnHitReactEnd();
	UFUNCTION(BlueprintCallable, meta = (ToolTip = "动画攻击结束时调用，恢复敌人 AI 状态并启动攻击冷却。"))
	void OnAttackEnd();
	void TryReleaseConfiguredProjectileAttack();
	virtual void OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted) override;
	virtual void OnHitReactMontageEnded(UAnimMontage* Montage, bool bInterrupted) override;

	/* 弹反 */
	void ApplyStanceBreak(float Duration, float PlayRate);

	/* 韧性系统 */
	void ApplyPoiseDamage(float Damage, AActor* DamageInstigator);
	void ResetPoise();
	bool ShouldTriggerStanceBreak() const { return !bEncounterDormant && bPendingStanceBreak; }

	FORCEINLINE float GetMaxPoise() const { return MaxPoise; }
	FORCEINLINE float GetCurrentPoise() const { return CurrentPoise; }
	FORCEINLINE float GetStanceBreakDuration() const { return StanceBreakDuration; }
	FORCEINLINE float GetStanceBreakPlayRate() const { return StanceBreakPlayRate; }

	/* 锁定联动 */
	void SetTargetedByPlayer(bool bTargeted);

	/* Getters */
	FORCEINLINE EEnemyState GetEnemyState() const { return EnemyState; }
	bool IsEngagingActor(const AActor* Actor) const;

#if !UE_BUILD_SHIPPING
	/** 在 deferred spawn 的 FinishSpawning 前调用，避免临时 Probe 创建巡逻 TargetPoint。 */
	void PrepareRangedDebugProbeSpawn();

	/** 仅 D-A PIE 验收使用的无资产射手探针入口。 */
	bool StartRangedDebugProbe(AActor* Target, float ReleaseDelay, AAIController* ProbeController);
#endif

	/* 受击/死亡配置 */
	virtual UHitReactionConfigDataAsset* GetReactionConfig() const override;

protected:
	/* 攻击 */
	virtual bool CanAttack() const override;
	// 冷却或攻击协调等待到期后，只唤醒 OnCombating() 的统一仲裁，不在 Timer 回调内单独选攻击或移动。
	void OnAttackCooldownEnd();

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

	/* 敌人原型战术扩展。默认实现为空，避免派生类复制整个 Combat HFSM。 */
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

	virtual bool HandleArchetypeCombatPriority(float DeltaTime, float DistanceToTarget, const FVector& ToTarget);
	virtual void TickArchetypeAttack(float DeltaTime);
	virtual bool HandleArchetypeAttackCooldownEnded();
	virtual bool HandleArchetypeMoveCompleted(FAIRequestID RequestID, EPathFollowingResult::Type Result);
	virtual void ClearArchetypeCombatState();
	virtual void ValidateArchetypeCombatConfig() const;
	virtual FString GetArchetypeCombatDebugText() const;
	virtual void DrawArchetypeCombatDebug() const;

	/* 仅供原型构造函数写入 CDO 基准值，不暴露为 Blueprint 运行时接口。 */
	void SetArchetypeMovementDefaults(float InPatrolSpeed, float InCombatManeuverSpeed, float InChaseSpeed);
	void SetArchetypeCombatSpacingDefaults(float InTooCloseRadius, float InAttackMaxRadius,
		float InPreferredMinRadius, float InPreferredMaxRadius, float InPressMargin, float InRetreatMinSpeedRatio);

	FORCEINLINE AActor* GetCombatTarget() const { return ChasingTarget; }
	FORCEINLINE float GetCombatPreferredMinRadius() const { return CombatPreferredMinRadius; }
	FORCEINLINE float GetCombatPreferredMaxRadius() const { return CombatPreferredMaxRadius; }
	FORCEINLINE float GetCombatRepositionAcceptanceRadius() const { return CombatRepositionAcceptanceRadius; }
	FORCEINLINE float GetCombatRepositionIntervalMin() const { return CombatRepositionIntervalMin; }
	FORCEINLINE float GetCombatManeuverSpeed() const { return CombatManeuverSpeed; }
	FORCEINLINE float GetChaseSpeed() const { return ChaseSpeed; }
	FORCEINLINE bool IsCombatRepositionReady(float CurrentTime) const
	{
		return !bRepositionInProgress && CurrentTime >= NextCombatRepositionTime;
	}

	bool IsPureProjectileAttackProfile() const;
	bool HasUnreleasedActiveProjectileAttack() const;
	bool HasClearActiveProjectileLineOfSight() const;
	bool CancelUnreleasedActiveProjectileAttack(float BlendOutTime, const TCHAR* Reason);
	void ValidatePureProjectileTacticalConfig(float EscapeEnterRadius, float EscapeExitRadius) const;
	void ClearPendingAttack();

	FEnemyCombatMovePlan BuildCombatMovePlanForRange(float DistanceToTarget, const FVector& ToTarget,
		float TooCloseRadius, float PreferredMinRadius, float PreferredMaxRadius, bool bForceStrafe) const;
	static const TCHAR* GetCombatMoveDebugName(EEnemyCombatMoveType MoveType);
	bool ExecuteCombatMovePlan(const FEnemyCombatMovePlan& MovePlan, float CurrentTime,
		FAIRequestID* OutMoveRequestId = nullptr);
	bool MoveToCombatLocation(const FVector& Location, FAIRequestID* OutMoveRequestId = nullptr);
	void ResetCombatReposition();
	void FinishCombatReposition();
	void SetCombatRepositionDelay(float Delay);
	void StartCombatRetreatSpeedEase(const FVector& GoalLocation);
	void UpdateCombatRetreatSpeedEase();
	void ClearCombatRetreatSpeedEase();
	void TickCombatFacing(float DeltaTime, const FVector& ToTarget);
	void SetCombatMoveDebugDetail(const FString& Detail);

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

	UPROPERTY()
	UAISenseConfig_Sight* SightConfig;

	UPROPERTY()
	UAISenseConfig_Hearing* HearingConfig;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true", ToolTip = "头顶血条组件。"))
	UHealthBarComponent* HealthBarWidgetComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true", ToolTip = "锁定目标标记组件，锁定该敌人时显示。"))
	UWidgetComponent* LockOnMarkerWidgetComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true", ToolTip = "Motion Warping 组件，用于跃进类攻击在蒙太奇窗口内修正 root motion 目标。"))
	UMotionWarpingComponent* MotionWarpingComponent;

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
	// 近战常规环：CombatTooCloseRadius < CombatAttackMaxRadius <= CombatPreferredMinRadius <= CombatPreferredMaxRadius < CombatingRadius < ChasingRadius。
	// 纯 Projectile 安全环由 ARangedEnemy 在其作者化距离中额外保证，不影响近战敌人的常规环。
	// CombatAttackMaxRadius 控制可出手距离；CombatPreferredMin/Max 控制冷却期想保持的距离环或纯远程安全横移区。
	// CombatPressMargin 必须大于 CombatRepositionAcceptanceRadius，且不超过 Preferred 距离环宽度，避免前压目标落在安全区外。
	UPROPERTY(EditAnywhere, Category = "Combat|Spacing", meta = (ClampMin = "0.0", ToolTip = "普通后撤阈值。纯远程派生原型会额外验证它与 Escape 进入半径不重叠。"))
	float CombatTooCloseRadius = 90.f;
	UPROPERTY(EditAnywhere, Category = "Combat|Spacing", meta = (ClampMin = "0.0", ToolTip = "可出手的最大距离。贴近实际武器射程即可，不要为了增大后撤距离而调高此值。"))
	float CombatAttackMaxRadius = 220.f;
	UPROPERTY(EditAnywhere, Category = "Combat|Spacing", meta = (ClampMin = "0.0", ToolTip = "冷却期拉扯距离环内圈。近战通常 >= CombatAttackMaxRadius；纯 Projectile 配置可作为安全横移区内圈，必须不小于 CombatTooCloseRadius。"))
	float CombatPreferredMinRadius = 240.f;
	UPROPERTY(EditAnywhere, Category = "Combat|Spacing", meta = (ClampMin = "0.0", ToolTip = "冷却期拉扯距离环外圈。必须 >= CombatPreferredMinRadius 且 < CombatingRadius；纯 Projectile 安全横移区不应超过实际最大出手距离。"))
	float CombatPreferredMaxRadius = 270.f;
	UPROPERTY(EditAnywhere, Category = "Combat|Spacing", meta = (ClampMin = "0.0", ClampMax = "90.0", ToolTip = "冷却期在拉扯距离环内的横移角度（绕目标旋转）。"))
	float CombatStrafeAngleDegrees = 25.f;
	UPROPERTY(EditAnywhere, Category = "Combat|Spacing", meta = (ClampMin = "0.0", ToolTip = "战斗位移的导航到达判定半径。必须小于 CombatPressMargin，避免前压时停在攻击范围外。"))
	float CombatRepositionAcceptanceRadius = 12.f;
	UPROPERTY(EditAnywhere, Category = "Combat|Spacing", meta = (ClampMin = "0.0", ToolTip = "战斗位移请求的最短间隔（秒）。必须 <= CombatRepositionIntervalMax。"))
	float CombatRepositionIntervalMin = 0.8f;
	UPROPERTY(EditAnywhere, Category = "Combat|Spacing", meta = (ClampMin = "0.0", ToolTip = "战斗位移请求的最长间隔（秒）。必须 >= CombatRepositionIntervalMin。"))
	float CombatRepositionIntervalMax = 1.4f;
	UPROPERTY(EditAnywhere, Category = "Combat|Spacing", meta = (ClampMin = "0.0", ToolTip = "前压时从 PreferredMaxRadius 扣减的余量。必须大于 CombatRepositionAcceptanceRadius，小于 CombatAttackMaxRadius / CombatPreferredMaxRadius，且不超过 Preferred 距离环宽度，确保前压目标仍在安全区内。"))
	float CombatPressMargin = 25.f;
	UPROPERTY(EditAnywhere, Category = "Combat|Spacing", meta = (ClampMin = "0.1", ClampMax = "1.0", ToolTip = "后撤/斜后撤接近目标点时的最低速度倍率。1.0 表示始终保持 CombatManeuverSpeed，不做末段降速。"))
	float CombatRetreatMinSpeedRatio = 0.55f;

	// 巡逻移动速度
	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ToolTip = "非战斗 Patrol / Search 移动速度（cm/s）。"))
	float PatrolSpeed = 200.f;

	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ToolTip = "普通战斗机动速度（cm/s），用于 Retreat、BackDiag、Strafe 与 LOS 重定位。"))
	float CombatManeuverSpeed = 290.f;

	// 追击移动速度
	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ToolTip = "高速 Chase / Press 移动速度（cm/s）；ARangedEnemy 的 Escape 也使用此值。"))
	float ChaseSpeed = 330.f;

	// 死亡后尸体销毁时间（秒）
	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ToolTip = "死亡后尸体销毁延迟（秒）。"))
	float CorpseLifespan = 5.f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Attack", meta = (ToolTip = "敌人攻击配置。为空时不会攻击，并会输出配置警告。"))
	UEnemyAttackConfigDataAsset* EnemyAttackConfig = nullptr;

	/* 攻击协调 */
	UPROPERTY(EditAnywhere, Category = "Combat|Attack Coordination", meta = (ClampMin = "100.0", ToolTip = "攻击协调检测范围（cm）"))
	float AttackCoordinationRange = 800.f;

	UPROPERTY(EditAnywhere, Category = "Combat|Attack Coordination", meta = (ClampMin = "0.1", ToolTip = "攻击衔接缓冲时间（秒），队友攻击结束后等待多久再攻击"))
	float AttackCoordinationBuffer = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Combat|Attack Coordination", meta = (ClampMin = "1.0", ToolTip = "因队友攻击而被迫等待的最大秒数"))
	float MaxAttackCoordinationWait = 3.f;

	UPROPERTY(EditAnywhere, Category = "Combat|Attack Coordination", meta = (ClampMin = "0.0", ToolTip = "队友攻击扫描缓存时间。避免战斗态每帧全场景扫描。"))
	float AllyAttackCheckCacheDuration = 0.25f;

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

	/* 通用战斗拉扯 */
	void UpdateCombatMovement(float DeltaTime, float DistanceToTarget, const FVector& ToTarget);
	FEnemyCombatMovePlan BuildCombatMovePlan(float DistanceToTarget, const FVector& ToTarget) const;
	bool MoveToCombatTarget(float AcceptanceRadiusOverride = -1.f); // 攻击 ready 时动态追踪目标 Actor

	// 攻击协调：检查同目标附近队友是否正在攻击，返回建议等待时间
	bool IsAllyAttackingNearby(float& OutSuggestedWaitTime) const;
	mutable float LastAllyAttackCheckTime = -1000.f;
	mutable bool bCachedAllyAttackingNearby = false;
	mutable float CachedAllySuggestedWaitTime = 0.f;
	// 只用来做目标地址比较，不解引用，避免缓存目标生命周期耦合。
	mutable const AActor* CachedAllyCheckTarget = nullptr;
	UFUNCTION()
	void OnRepositionMoveCompleted(FAIRequestID RequestID, EPathFollowingResult::Type Result);
	float NextCombatRepositionTime = 0.f;
	bool bRepositionInProgress = false;
	bool bRetreatSpeedEaseActive = false;
	FVector RetreatSpeedEaseGoalLocation = FVector::ZeroVector;
	float RetreatSpeedEaseTotalDistance = 0.f;

	enum class EEnemyCombatSubState : uint8
	{
		None,
		Orienting,
		AttackReadyPressing,
		CoordinatedWaiting,
		CooldownSpacing
	};

	EEnemyCombatSubState CombatSubState = EEnemyCombatSubState::None;
	void PerformConfiguredAttack(float DistanceToTarget);
	bool PerformConfiguredAttackByIndex(int32 AttackIndex);
	void StartAttackCooldown(float MinCooldown, float MaxCooldown);
	bool PlayEnemyAttackMontage(const FEnemyAttackEntry& Entry);
	void ClearCurrentAttackConfig(bool bStartCooldown = false);

	struct FActiveProjectileAttack
	{
		TSubclassOf<ACombatProjectile> ProjectileClass;
		FProjectileDeliveryConfig DeliveryConfig;
		FName SpawnSocketName = NAME_None;
		float TargetHeightOffset = 0.f;
		float MinDistance = 0.f;
		float MaxDistance = 0.f;
		float MinCooldown = 0.f;
		float MaxCooldown = 0.f;
		bool bIsActive = false;
		bool bReleaseAttempted = false;
		bool bReleaseSucceeded = false;
		bool bDebugProbe = false;

		bool IsValid() const
		{
			return bIsActive && ProjectileClass && DeliveryConfig.IsValid()
				&& MaxDistance >= MinDistance && MaxDistance > 0.f;
		}
	};

	bool BuildProjectileAttackSnapshot(const FEnemyAttackEntry& Entry, FActiveProjectileAttack& OutSnapshot) const;

#if !UE_BUILD_SHIPPING
	bool BuildDebugProbeProjectileSnapshot(FActiveProjectileAttack& OutSnapshot) const;
#endif
	bool ResolveProjectileSpawnLocation(const FActiveProjectileAttack& Snapshot, FVector& OutSpawnLocation) const;
	bool ResolveProjectileTargetLocation(const FActiveProjectileAttack& Snapshot, FVector& OutTargetLocation) const;
	bool HasClearProjectileLineOfSight(const FActiveProjectileAttack& Snapshot, FVector& OutSpawnLocation,
		FVector& OutTargetLocation) const;
	// 起手使用战术距离窗口；已承诺的 Release 只受投射物物理飞行距离约束。
	bool IsProjectileAttackWithinStartRange(const FActiveProjectileAttack& Snapshot, float DistanceToTarget) const;
	bool IsProjectileAttackWithinCommittedReleaseRange(const FActiveProjectileAttack& Snapshot, float DistanceToTarget) const;
	void HandlePendingProjectilePositioning(const FActiveProjectileAttack& Snapshot, float DistanceToTarget,
		const FVector& ToTarget, bool bForceStrafeForLineOfSight, float PreferredMinRadius = -1.f,
		float PreferredMaxRadius = -1.f);
	void ClearActiveProjectileAttack();

#if !UE_BUILD_SHIPPING
	void OnDebugProjectileReleaseTimerElapsed();
	void OnDebugProjectileAttackEndTimerElapsed();
	void StartDebugProjectileAttack(FActiveProjectileAttack&& Snapshot);
	bool TryExecuteRangedDebugProbePendingAttack(float DistanceToTarget, float ForwardDot, const FVector& ToTarget);
#endif
	/**
	 * 为当前敌人招式写入/清理 Motion Warping 目标。
	 * Entry 提供 WarpTargetName、StopDistance 和 MaxWarpDistance；目标点只在出手瞬间计算一次。
	 */
	void UpdateAttackMotionWarpTarget(const FEnemyAttackEntry& Entry);
	void ClearAttackMotionWarpTarget(const FEnemyAttackEntry& Entry);
	void ValidateEnemyAttackConfig() const;
	void WarnNoEnemyAttackCandidate(float DistanceToTarget);
	bool HasPendingAttack() const;
	void RollPendingAttackIntent();
	/**
	 * 尝试执行已抽中的 PendingAttack。
	 * DistanceToTarget 是当前水平距离，ForwardDot 是朝向目标的点积，ToTarget 是指向目标的 2D 单位方向。
	 * 返回 true 表示本帧已处理攻击意图（移动、等待、清理或出手），调用方不应再重新抽招。
	 */
	bool TryExecutePendingAttack(float DistanceToTarget, float ForwardDot, const FVector& ToTarget);
	void HandlePendingAttackPositioning(float DistanceToTarget, const FVector& ToTarget);
	bool IsPendingAttackExpired() const;
	void BlockPendingAttackRetry(int32 AttackIndex);
	int32 GetBlockedPendingAttackIndex() const;
	void StartCurrentAttackCooldownIfNeeded();
	FString GetPendingAttackDebugString() const;
	float GetAttackCooldownRemaining() const;
	void DrawDebugInfo() const;
	int32 CurrentAttackIndex = INDEX_NONE;
	FActiveProjectileAttack ActiveProjectileAttack;
	float LastAttackConfigWarningTime = -1000.f;
	int32 PendingAttackIndex = INDEX_NONE;
	float PendingAttackStartTime = 0.f;
	int32 LastBlockedPendingAttackIndex = INDEX_NONE;
	float PendingAttackRetryBlockUntil = 0.f;
	bool bPendingAttackMoveIssued = false;
	UPROPERTY(EditAnywhere, Category = "Combat|Attack", meta = (ClampMin = "0.1", ToolTip = "攻击意图等待最长时间，超时后重新抽取招式。"))
	float PendingAttackTimeout = 2.f;
	UPROPERTY(EditAnywhere, Category = "Combat|Attack", meta = (ClampMin = "0.0", ToolTip = "Pending 攻击失败后，同一招式被重新抽中的短暂屏蔽时间，防止每帧反复抽中追不上的招式。"))
	float PendingAttackRetryBlockDuration = 0.8f;
	bool bCurrentAttackCooldownStarted = false;

#if !UE_BUILD_SHIPPING
	bool bIsRangedDebugProbeInstance = false;
	bool bRangedDebugProbeActive = false;
	bool bRangedDebugProbeCompleted = false;
	bool bDebugProbeRetryAfterAttackEnd = false;
	float DebugProbeReleaseDelay = 0.35f;
	float DebugProbePendingStartTime = 0.f;
	TWeakObjectPtr<AAIController> DebugProbeController;
#endif

	void SetCombatSubState(EEnemyCombatSubState NewSubState, float AllySuggestedWaitTime = 0.f);
	EEnemyCombatSubState EvaluateCombatSubState(float DistanceToTarget, float ForwardDot, float& OutAllySuggestedWaitTime) const;
	void TickActiveProjectileAttackFacing(float DeltaTime);
	void TickCombatSubState(float DeltaTime, EEnemyCombatSubState SubState, float DistanceToTarget, const FVector& ToTarget, float AllySuggestedWaitTime);
	FString GetCombatSubStateDebugText() const;
	void ApplyAuthoredPerceptionConfig();

	FString CombatMoveDetailDebug;

	/* 定时器 */
	FTimerHandle PatrolTimer; // 巡逻等待定时器
	FTimerHandle LookTimer; // 张望定时器
	FTimerHandle AttackCooldownTimer; // 攻击冷却定时器
	FTimerHandle HealthBarHideTimer; // 血条延迟隐藏定时器
	FTimerHandle PoiseResetTimer; // 韧性重置定时器
	FTimerHandle StanceBreakRecoveryTimer; // 破防硬直恢复定时器
	FTimerHandle ProjectileReleaseTimer;
	FTimerHandle ProjectileAttackEndTimer;
	bool bAttackOnCooldown = false; // 攻击冷却中
	bool bIsTargetedByPlayer = false; // 被玩家锁定中
	bool bPendingStanceBreak = false; // 韧性归零flag，等待GetHit后触发
	AActor* LastPoiseDamageInstigator = nullptr; // 最后一次韧性伤害的攻击者

	// 遭遇控制器只管理被显式登记的敌人；普通敌人保持原有 AI 路径。
	AEncounterController* EncounterOwner = nullptr;
	bool bEncounterDormant = false;
	bool bDeathNotificationBroadcast = false;
	FOnEnemyDied EnemyDiedDelegate;

	/* 韧性系统 */
	UPROPERTY(EditAnywhere, Category = "Combat|Poise", meta = (AllowPrivateAccess = "true", ToolTip = "最大韧性值。"))
	float MaxPoise = 10.f;

	UPROPERTY(VisibleInstanceOnly, Category = "Combat|Poise", meta = (AllowPrivateAccess = "true", ToolTip = "当前韧性值。"))
	float CurrentPoise = 10.f;

	UPROPERTY(EditAnywhere, Category = "Combat|Poise", meta = (AllowPrivateAccess = "true", ToolTip = "未受击多久后重置韧性（秒）。"))
	float PoiseResetDelay = 5.f;

	UPROPERTY(EditAnywhere, Category = "Combat|Poise", meta = (AllowPrivateAccess = "true", ToolTip = "破防硬直时长（秒）。"))
	float StanceBreakDuration = 2.f;

	UPROPERTY(EditAnywhere, Category = "Combat|Poise", meta = (AllowPrivateAccess = "true", ToolTip = "破防慢放速率（0.3 = 30%速度）。"))
	float StanceBreakPlayRate = 0.3f;

	/* 受击/死亡配置 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Reaction", meta = (AllowPrivateAccess = "true", ToolTip = "敌人受击反应和死亡蒙太奇配置。"))
	TObjectPtr<UHitReactionConfigDataAsset> HitReactionConfig = nullptr;

	void RecoverFromStanceBreak(); // 破防硬直恢复回调
	void ClearPatrolTimers(); // 清理巡逻相关定时器
	void ClearAllTimers(); // 清理所有定时器（巡逻 + 冷却 + 血条 + 弹反）
	void RefreshEnemyControllerBinding();
	void SetEnemyControllerBinding(AAIController* NewEnemyController);
};
