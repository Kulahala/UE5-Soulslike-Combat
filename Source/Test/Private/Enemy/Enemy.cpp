// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/Enemy.h"

#include "AIController.h"
#include "AttributeComponent/AttributeComponent.h"
#include "Combat/CombatTeamHelper.h"
#include "components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/TargetPoint.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HUD/BaseHealthBarWidget.h"
#include "HUD/HealthBarComponent.h"
#include "Items/Weapon/Weapon.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Utils/DebugDrawHelper.h"

// ==================== 生命周期 ====================

AEnemy::AEnemy()
{
	// 碰撞设置
	GetMesh()->SetCollisionObjectType(ECC_Pawn);
	GetMesh()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Overlap);
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetGenerateOverlapEvents(true);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	// 移动设置
	GetCharacterMovement()->MaxWalkSpeed = PatrolSpeed;

	// 受击后退
	BaseHitKnockbackDistance = 5.f;

	// 血条组件
	HealthBarWidgetComp = CreateDefaultSubobject<UHealthBarComponent>(TEXT("HealthBar"));
	HealthBarWidgetComp->SetupAttachment(RootComponent);

	// AI感知
	AIPerceptionComp = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerceptionComp"));

	// 视觉配置
	UAISenseConfig_Sight* SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	SightConfig->SightRadius = ChasingRadius;
	SightConfig->LoseSightRadius = ChasingRadius + 200.f;
	SightConfig->PeripheralVisionAngleDegrees = VisionAngleDegrees;
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	AIPerceptionComp->ConfigureSense(*SightConfig);
	AIPerceptionComp->SetDominantSense(SightConfig->GetSenseImplementation());

	// 听觉配置
	UAISenseConfig_Hearing* HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearingConfig"));
	HearingConfig->HearingRange = HearingRange;
	HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
	HearingConfig->DetectionByAffiliation.bDetectNeutrals = false;
	HearingConfig->DetectionByAffiliation.bDetectFriendlies = false;
	AIPerceptionComp->ConfigureSense(*HearingConfig);
}

void AEnemy::SpawnPointInit()
{
	// 出生点（将Z轴坐标下移到胶囊体底部，防止悬空导致后续寻路失败）
	FVector SpawnLocation = GetActorLocation();
	if (GetCapsuleComponent())
	{
		SpawnLocation.Z -= GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	}
	// 如果未设定巡逻点数组，则在出生点生成一个临时的巡逻点
	SpawnPoint = GetWorld()->SpawnActor<ATargetPoint>(ATargetPoint::StaticClass(), SpawnLocation, GetActorRotation());
	if (SpawnPoint)
	{
		PatrolTargets.Add(SpawnPoint);
		if (!PatrolTarget)
		{
			PatrolTarget = SpawnPoint;
		}
	}
}

void AEnemy::WeaponInit()
{
	// 武器
	if (WeaponClass)
	{
		AWeapon* Weapon = GetWorld()->SpawnActor<AWeapon>(WeaponClass);
		Weapon->Equip(GetMesh(), FName("RightHandSocket"), this, this);
		EquippedWeapon = Weapon;
	}
}

void AEnemy::BeginPlay()
{
	Super::BeginPlay();
	Tags.Add(FName("Enemy"));

	// 韧性初始化
	CurrentPoise = MaxPoise;

	// 血条
	if (HealthBarWidgetComp)
	{
		HealthBarWidgetComp->SetVisibility(false);
	}
	if (Attributes)
	{
		// 绑定广播：当血量改变时，自动调用血条更新
		Attributes->OnHealthChanged.AddDynamic(HealthBarWidgetComp, &UHealthBarComponent::SetHealthPercent);
		HealthBarWidgetComp->SetHealthPercent(Attributes->GetHealthPercent());
	}

	// AI感知
	if (AIPerceptionComp)
	{
		AIPerceptionComp->OnTargetPerceptionUpdated.AddDynamic(this, &AEnemy::TargetPerceptionUpdated);
	}

	SpawnPointInit();

	EnemyController = Cast<AAIController>(GetController());
	if (EnemyController)
	{
		EnemyController->ReceiveMoveCompleted.AddDynamic(this, &AEnemy::OnRepositionMoveCompleted);
	}

	WeaponInit();
}

void AEnemy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 兜底：定时器全量清理，覆盖 Die() 未执行的路径（关卡切换、编辑器 Stop 等）
	ClearAllTimers();
	if (EnemyController)
	{
		EnemyController->ReceiveMoveCompleted.RemoveDynamic(this, &AEnemy::OnRepositionMoveCompleted);
	}
	Super::EndPlay(EndPlayReason);
}

// ==================== 受击/死亡 ====================

void AEnemy::GetHit_Implementation(const FVector& ImpactPoint, AActor* HitInstigator)
{
	//DrawDebugSphere(this->GetWorld(), ImpactPoint, 5, 10, FColor::Red, false, 5.0f, 0, 0.5f);
	Super::GetHit_Implementation(ImpactPoint, HitInstigator);
	ShowHealthBar();

	// 武器命中才触发硬直（DOT 不经过 GetHit，不会触发）
	if (Attributes->IsAlive() && HitInstigator)
	{
		// 只锁定不同阵营的目标，防止被同类打到后锁定队友
		if (!FCombatTeamHelper::ShareTeamTag(this, HitInstigator))
		{
			ChasingTarget = HitInstigator;
		}
		if (PendingHitContext.bApplyStun)
		{
			SetEnemyState(EEnemyState::EES_Stunned);
		}
	}

	ResetPendingHitContext();
}

float AEnemy::TakeDamage(float DamageAmount, const struct FDamageEvent& DamageEvent, class AController* EventInstigator,
                         AActor* DamageCauser)
{
	Attributes->ReceiveDamage(DamageAmount);

	if (!Attributes->IsAlive())
	{
		SetEnemyState(EEnemyState::EES_Dead);
	}

	return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}

void AEnemy::Die()
{
	ClearAllTimers();
	bPendingStanceBreak = false;
	LastPoiseDamageInstigator = nullptr;

	StopEnemyMovementIfPossible();

	// 关闭碰撞
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCapsuleComponent()->SetCollisionResponseToAllChannels(ECR_Ignore);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 清理附加物和UI
	if (SpawnPoint)
	{
		SpawnPoint->Destroy();
	}
	HealthBarWidgetComp->SetVisibility(false);

	// 播放死亡动画
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && DeathMontage)
	{
		//强制打断其他动画
		AnimInstance->Montage_Stop(0.1f);

		int32 RandomIndex = FMath::RandRange(1, 3);
		FName SectionName = FName(FString::Printf(TEXT("Section%d"), RandomIndex));

		AnimInstance->Montage_Play(DeathMontage);
		AnimInstance->Montage_JumpToSection(SectionName, DeathMontage);
	}

	// 设定销毁时间
	SetLifeSpan(CorpseLifespan);
}

// ==================== 韧性系统 ====================

void AEnemy::ApplyPoiseDamage(float Damage, AActor* DamageInstigator)
{
	if (EnemyState == EEnemyState::EES_Dead || EnemyState == EEnemyState::EES_StanceBreak) return;

	CurrentPoise = FMath::Max(0.f, CurrentPoise - Damage);
	LastPoiseDamageInstigator = DamageInstigator;

	GetWorldTimerManager().ClearTimer(PoiseResetTimer);

	if (CurrentPoise <= 0.f)
	{
		bPendingStanceBreak = true;
	}
	else
	{
		GetWorldTimerManager().SetTimer(PoiseResetTimer, this, &AEnemy::ResetPoise, PoiseResetDelay, false);
	}
}

void AEnemy::ApplyStanceBreak(float Duration, float PlayRate)
{
	// 连续破防覆盖：先清旧 timer 和恢复速率
	GetWorldTimerManager().ClearTimer(StanceBreakRecoveryTimer);
	if (UAnimInstance* Anim = GetMesh()->GetAnimInstance())
	{
		if (UAnimMontage* ActiveMontage = Anim->GetCurrentActiveMontage())
		{
			Anim->Montage_SetPlayRate(ActiveMontage, 1.f);
		}
	}

	// 停止当前蒙太奇（NotifyEnd 自动清 IgnoreActors 黑名单）
	if (UAnimInstance* AnimForStop = GetMesh()->GetAnimInstance())
	{
		AnimForStop->Montage_Stop(0.05f);
	}

	SetEnemyState(EEnemyState::EES_StanceBreak);

	// 从 LastPoiseDamageInstigator 获取方向
	if (LastPoiseDamageInstigator)
	{
		DirectionalHitReact(GetActorLocation(), LastPoiseDamageInstigator);
	}

	// 设置慢放
	if (UAnimInstance* Anim = GetMesh()->GetAnimInstance())
	{
		if (UAnimMontage* ActiveMontage = Anim->GetCurrentActiveMontage())
		{
			Anim->Montage_SetPlayRate(ActiveMontage, PlayRate);
		}
	}

	// 清除破防flag
	bPendingStanceBreak = false;

	// 立即重置韧性
	ResetPoise();

	// 启动恢复计时器
	GetWorldTimerManager().SetTimer(
		StanceBreakRecoveryTimer, this, &AEnemy::RecoverFromStanceBreak, Duration, false);
}

void AEnemy::RecoverFromStanceBreak()
{
	if (EnemyState != EEnemyState::EES_StanceBreak) return;

	// 恢复蒙太奇速率
	if (UAnimInstance* Anim = GetMesh()->GetAnimInstance())
	{
		if (UAnimMontage* ActiveMontage = Anim->GetCurrentActiveMontage())
		{
			Anim->Montage_SetPlayRate(ActiveMontage, 1.f);
		}
	}

	// 委托 CheckCombatTarget 统一判定
	CheckCombatTarget();
}

void AEnemy::ResetPoise()
{
	CurrentPoise = MaxPoise;
	GetWorldTimerManager().ClearTimer(PoiseResetTimer);
}

// ==================== 弹反（已废弃，保留用于重构） ====================

// ==================== 攻击 ====================

void AEnemy::Attack()
{
	if (!CanAttack())
	{
		return;
	}

	bAttackOnCooldown = true;
	const float Cooldown = FMath::RandRange(MinAttackInterval, MaxAttackInterval);
	GetWorldTimerManager().SetTimer(AttackCooldownTimer, this,
	                                &AEnemy::OnAttackCooldownEnd, Cooldown, false);

	SetEnemyState(EEnemyState::EES_Attacking);
	PlayAttackMontage(FName("Attack1"));
}

bool AEnemy::CanAttack() const
{
	return EnemyState == EEnemyState::EES_Combating && !bAttackOnCooldown && IsValidCombatTarget(ChasingTarget);
}

// ==================== 蒙太奇回调 ====================

void AEnemy::OnHitReactEnd()
{
	// 只有在硬直状态下才恢复，防止覆盖了死亡状态
	if (EnemyState == EEnemyState::EES_Stunned)
	{
		CheckCombatTarget();
	}
}

void AEnemy::OnAttackEnd()
{
	// 只有在攻击状态下才恢复，防止覆盖了受击状态或死亡状态
	if (EnemyState == EEnemyState::EES_Attacking)
	{
		CheckCombatTarget();
	}
}

void AEnemy::OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (bInterrupted) return;
	if (EnemyState == EEnemyState::EES_Attacking)
	{
		CheckCombatTarget();
	}
}

void AEnemy::OnHitReactMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (bInterrupted) return;
	if (EnemyState == EEnemyState::EES_Stunned)
	{
		CheckCombatTarget();
	}
}

void AEnemy::OnAttackCooldownEnd()
{
	bAttackOnCooldown = false;
	ResetCombatReposition();

	// 非战斗态或目标无效时只清标志，不打断当前导航（避免打断 Chasing MoveTo）
	if (EnemyState != EEnemyState::EES_Combating || !IsValidCombatTarget(ChasingTarget))
	{
		return;
	}

	// 攻击协调检查
	float AllyRemainingTime = 0.f;
	if (IsAllyAttackingNearby(AllyRemainingTime))
	{
		// 延长CD，等待队友结束
		float NewCooldown = FMath::Clamp(
			AllyRemainingTime + AttackCoordinationBuffer,
			0.1f,
			MaxAttackCoordinationWait
		);

		GetWorldTimerManager().SetTimer(AttackCooldownTimer, this,
			&AEnemy::OnAttackCooldownEnd, NewCooldown, false);
		bAttackOnCooldown = true;

		if (FDebugDrawHelper::IsEnemyEnabled())
		{
			FDebugDrawHelper::Add(TEXT("WaitAlly"), FColor::Yellow);
		}
		return;
	}

	const FVector ToTarget = (ChasingTarget->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	const float DistanceToTarget = FVector::Dist2D(GetActorLocation(), ChasingTarget->GetActorLocation());
	const float ForwardDot = CalcForwardDot2D(ToTarget);
	if (ShouldTriggerAttack(DistanceToTarget, ForwardDot))
	{
		// 满足出手条件：停住等转身出手
		StopEnemyMovementIfPossible();
	}
	else
	{
		// 未满足出手条件：复用 attack-ready 定位钩子，让子类策略自动生效
		HandleAttackReadyPositioning(DistanceToTarget, ToTarget);
	}
}

// ==================== 状态机 ====================

void AEnemy::CheckCombatTarget()
{
	if (!IsValidCombatTarget(ChasingTarget))
	{
		ChasingTarget = nullptr;
		if (EnemyState != EEnemyState::EES_Patrolling && EnemyState != EEnemyState::EES_Searching)
		{
			SetEnemyState(EEnemyState::EES_Patrolling);
		}
		return;
	}

	// 战斗族状态（Combating/Attacking/Stunned/Parried）使用退出滞后半径，防止边界抖动
	const bool bInCombatFamily = EnemyState == EEnemyState::EES_Combating
		|| EnemyState == EEnemyState::EES_Attacking
		|| EnemyState == EEnemyState::EES_Stunned
		|| EnemyState == EEnemyState::EES_StanceBreak;
	const float CombatCheckRadius = bInCombatFamily
		? (CombatingRadius + CombatExitBuffer)
		: CombatingRadius;

	if (BInTargetRange(ChasingTarget, CombatCheckRadius))
	{
		SetEnemyState(EEnemyState::EES_Combating);
	}
	else if (BInTargetRange(ChasingTarget, ChasingRadius))
	{
		SetEnemyState(EEnemyState::EES_Chasing);
	}
	else
	{
		LastKnownLocation = ChasingTarget->GetActorLocation();
		ChasingTarget = nullptr;
		bSearchingLostTarget = true;
		SetEnemyState(EEnemyState::EES_Searching);
	}
}

void AEnemy::SetEnemyState(EEnemyState NewState)
{
	if (EnemyState == NewState)
	{
		return;
	}

	// --- 退出旧状态的逻辑 ---
	if (EnemyState == EEnemyState::EES_Patrolling || EnemyState == EEnemyState::EES_Searching)
	{
		ClearPatrolTimers();
		bSearchingLostTarget = false;
	}

	// --- 切换状态 ---
	EEnemyState OldState = EnemyState;
	EnemyState = NewState;
	ClearCombatRetreatSpeedEase();

	// --- 进入新状态的逻辑（一次性动作） ---
	switch (EnemyState)
	{
	case EEnemyState::EES_Patrolling:
		GetCharacterMovement()->MaxWalkSpeed = PatrolSpeed;
		GetCharacterMovement()->bOrientRotationToMovement = true;
		MoveToTarget(PatrolTarget);
		break;
	case EEnemyState::EES_Searching:
		// OnSearching / OnLostTargetSearch 首次 Tick 时处理启动逻辑
		break;
	case EEnemyState::EES_Chasing:
		GetCharacterMovement()->MaxWalkSpeed = ChaseSpeed;
		GetCharacterMovement()->bOrientRotationToMovement = true;
		MoveToTarget(ChasingTarget);
		break;
	case EEnemyState::EES_Combating:
	{
		GetCharacterMovement()->bOrientRotationToMovement = false;
		// 仅在攻击距离内或非追逐入口时停导航；远距离追逐入口让 OnCombating 前压逻辑接管
		const float DistToTarget = ChasingTarget
			? FVector::Dist2D(GetActorLocation(), ChasingTarget->GetActorLocation())
			: 0.f;
		if (OldState != EEnemyState::EES_Chasing || DistToTarget <= CombatAttackMaxRadius)
		{
			StopEnemyMovementIfPossible();
		}
		ResetCombatReposition();
		break;
	}
	case EEnemyState::EES_Attacking:
	case EEnemyState::EES_Stunned:
	case EEnemyState::EES_StanceBreak:
		StopEnemyMovementIfPossible();
		bRepositionInProgress = false;
		break;
	case EEnemyState::EES_Dead:
		Die();
		break;
	default:
		break;
	}
}

// ==================== AI感知 ====================

void AEnemy::TargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	// 只处理"看到"的瞬间
	if (!Stimulus.WasSuccessfullySensed())
	{
		return;
	}
	// 身份校验
	if (!Actor || !Actor->ActorHasTag(FName("Player")))
	{
		return;
	}
	// 冗余校验
	if (ChasingTarget == Actor)
	{
		return;
	}
	// 专注度校验（Searching 允许被感知打断，由 CheckCombatTarget 自然切到 Chasing）
	if (EnemyState == EEnemyState::EES_Chasing || EnemyState == EEnemyState::EES_Attacking)
	{
		return;
	}
	// 存活校验：死亡目标不能重新写入 ChasingTarget
	if (!IsValidCombatTarget(Actor))
	{
		return;
	}
	// 锁定新目标
	APawn* SeenPawn = dynamic_cast<APawn*>(Actor);
	ChasingTarget = SeenPawn;
}

// ==================== AI Tick ====================

void AEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (EnemyState == EEnemyState::EES_Dead || EnemyState == EEnemyState::EES_Stunned || EnemyState ==
		EEnemyState::EES_Attacking || EnemyState == EEnemyState::EES_StanceBreak)
	{
		// [调试] 破防硬直标识（硬停早退前，确保可见）
		if (FDebugDrawHelper::IsEnemyEnabled() && EnemyState == EEnemyState::EES_StanceBreak)
		{
			FDebugDrawHelper::Add(TEXT("BREAK"), FColor::Red);
		}
		return;
	}

	// [调试] 敌人状态 + 距离 + 韧性 + 范围球体
	// TODO: 多敌人时调试文字会混在一起，加 GetName() 或编号区分
	if (FDebugDrawHelper::IsEnemyEnabled())
	{
		FDebugDrawHelper::Add(FString::Printf(TEXT("EnemyState: %s | Speed: %.0f"),
			*UEnum::GetValueAsString(EnemyState), GroundSpeed),
			EnemyState == EEnemyState::EES_Dead ? FColor::Red : FColor::White);

		// 韧性显示
		FDebugDrawHelper::Add(FString::Printf(TEXT("Poise: %.1f/%.1f"), CurrentPoise, MaxPoise), FColor::Cyan);

		if (ChasingTarget)
		{
			float Dist = FVector::Dist2D(GetActorLocation(), ChasingTarget->GetActorLocation());
			FDebugDrawHelper::Add(FString::Printf(TEXT("Dist: %.0f / %.0f"), Dist, ChasingRadius),
				Dist <= CombatingRadius ? FColor::Red : Dist <= ChasingRadius ? FColor::Yellow : FColor::White);
		}

		if (EnemyState == EEnemyState::EES_Combating)
		{
			const FString MoveState = LastCombatMoveDebug.IsEmpty()
				? TEXT("Ready")
				: LastCombatMoveDebug;
			FDebugDrawHelper::Add(FString::Printf(TEXT("CombatMove: %s"), *MoveState), FColor::Cyan);
		}
	}

	if (ChasingTarget)
	{
		FDebugDrawHelper::AddSphere(GetWorld(), GetActorLocation(), ChasingRadius, FColor::Yellow);
		FDebugDrawHelper::AddSphere(GetWorld(), GetActorLocation(), CombatingRadius, FColor(255, 165, 0), 16);
		FDebugDrawHelper::AddSphere(GetWorld(), GetActorLocation(), CombatAttackMaxRadius, FColor::Red, 16);
	}

	// 1. 初步判断
	CheckCombatTarget();

	// 2. 持续反应
	switch (EnemyState)
	{
	case EEnemyState::EES_Patrolling:
		OnPatrolling(DeltaTime);
		break;
	case EEnemyState::EES_Searching:
		if (bSearchingLostTarget)
			OnLostTargetSearch(DeltaTime);
		else
			OnSearching(DeltaTime);
		break;
	case EEnemyState::EES_Chasing:
		OnChasing();
		break;
	case EEnemyState::EES_Combating:
		OnCombating(DeltaTime);
		break;
	default:
		break;
	}
}

void AEnemy::OnPatrolling(float DeltaTime)
{
	if (!IsValid(PatrolTarget))
	{
		return;
	}
	if (BInTargetRange(PatrolTarget, PatrolRadius))
	{
		SetEnemyState(EEnemyState::EES_Searching);
	}
}

void AEnemy::OnSearching(float DeltaTime)
{
	if (!GetWorldTimerManager().IsTimerActive(PatrolTimer))
	{
		// 刚进入搜索：停止移动，关闭自动朝向，启动定时器
		if (EnemyController)
		{
			EnemyController->StopMovement();
		}
		GetCharacterMovement()->bOrientRotationToMovement = false;

		const float WaitTime = FMath::RandRange(PatrolWaitMin, PatrolWaitMax);
		GetWorldTimerManager().SetTimer(PatrolTimer, this, &AEnemy::SearchTimerFinished, WaitTime);
		GetWorldTimerManager().SetTimer(LookTimer, this, &AEnemy::GenerateNewLookRotation, SingleLookTime, true);
		GenerateNewLookRotation();
	}
	else
	{
		// 等待期间：平滑旋转到张望目标方向
		FRotator CurrentRotation = GetActorRotation();
		FRotator NewRotation = FMath::RInterpTo(CurrentRotation, PatrolWaitTargetRotation, DeltaTime,
		                                        PatrolRotationSpeed);
		SetActorRotation(NewRotation);
	}
}

void AEnemy::OnLostTargetSearch(float DeltaTime)
{
	if (!GetWorldTimerManager().IsTimerActive(PatrolTimer))
	{
		// 首次 Tick：导航到最后已知位置
		MoveToLocation(LastKnownLocation);

		const float WaitTime = FMath::RandRange(PatrolWaitMin, PatrolWaitMax);
		GetWorldTimerManager().SetTimer(PatrolTimer, this, &AEnemy::LostTargetSearchFinished, WaitTime);
		GetWorldTimerManager().SetTimer(LookTimer, this, &AEnemy::GenerateNewLookRotation, SingleLookTime, true);
		GenerateNewLookRotation();
	}
	else
	{
		// 等待期间：到达后平滑旋转张望
		FRotator CurrentRotation = GetActorRotation();
		FRotator NewRotation = FMath::RInterpTo(CurrentRotation, PatrolWaitTargetRotation, DeltaTime,
		                                        PatrolRotationSpeed);
		SetActorRotation(NewRotation);
	}
}

void AEnemy::LostTargetSearchFinished()
{
	GetWorldTimerManager().ClearTimer(LookTimer);
	bSearchingLostTarget = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;

	if (AActor* Target = ChooseRadomTarget(PatrolTargets))
	{
		PatrolTarget = Target;
		SetEnemyState(EEnemyState::EES_Patrolling);
	}
}

void AEnemy::OnChasing()
{
	// 玩家跑出网格体边缘时，AI 会走到边缘并停下（变为 Idle 状态）
	// 如果此时玩家又回来了，我们需要重新激活寻路指令
	if (EnemyController && EnemyController->GetMoveStatus() == EPathFollowingStatus::Idle)
	{
		MoveToTarget(ChasingTarget);
	}
}

void AEnemy::OnCombating(float DeltaTime)
{
	if (!ChasingTarget) return;

	FVector ToTarget = (ChasingTarget->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	float Dot = CalcForwardDot2D(ToTarget);
	float DistanceToTarget = FVector::Dist2D(GetActorLocation(), ChasingTarget->GetActorLocation());

	// 平滑面向目标
	FRotator TargetRot = ToTarget.Rotation();
	SetActorRotation(FMath::RInterpTo(GetActorRotation(), TargetRot, DeltaTime, CombatRotationSpeed));
	UpdateCombatRetreatSpeedEase();

	if (ShouldTriggerAttack(DistanceToTarget, Dot))
	{
		Attack();
	}
	else if (!bAttackOnCooldown)
	{
		// 攻击协调检查
		float AllyRemainingTime = 0.f;
		if (IsAllyAttackingNearby(AllyRemainingTime))
		{
			// 延长自己的CD到队友结束 + 缓冲时间
			float NewCooldown = FMath::Clamp(
				AllyRemainingTime + AttackCoordinationBuffer,
				0.1f,
				MaxAttackCoordinationWait
			);

			GetWorldTimerManager().SetTimer(AttackCooldownTimer, this,
				&AEnemy::OnAttackCooldownEnd, NewCooldown, false);
			bAttackOnCooldown = true;

			// 调试可视化
			if (FDebugDrawHelper::IsEnemyEnabled())
			{
				FDebugDrawHelper::Add(TEXT("WaitAlly"), FColor::Yellow);
			}
		}
		else
		{
			HandleAttackReadyPositioning(DistanceToTarget, ToTarget);
		}
	}
	else
	{
		HandleCooldownPositioning(DeltaTime, DistanceToTarget, ToTarget);
	}
}

bool AEnemy::ShouldTriggerAttack(float DistanceToTarget, float ForwardDot) const
{
	return !bAttackOnCooldown && ForwardDot > AttackAngleThreshold && DistanceToTarget <= CombatAttackMaxRadius;
}

void AEnemy::HandleAttackReadyPositioning(float DistanceToTarget, const FVector& ToTarget)
{
	if (DistanceToTarget > CombatAttackMaxRadius)
	{
		// 攻击 ready 但还在攻击距离外 -> 动态追踪目标 Actor，不受 NextCombatRepositionTime 节流
		// 仅在导航空闲时重新下发，避免每帧重规划路径
		if (EnemyController && EnemyController->GetMoveStatus() != EPathFollowingStatus::Moving)
		{
			ClearCombatRetreatSpeedEase();
			GetCharacterMovement()->MaxWalkSpeed = ChaseSpeed;
			MoveToCombatTarget();
		}
	}
	else
	{
		// 已在攻击距离内但未转正 -> 停住等转身出手
		StopEnemyMovementIfPossible();
	}
}

void AEnemy::HandleCooldownPositioning(float DeltaTime, float DistanceToTarget, const FVector& ToTarget)
{
	UpdateCombatMovement(DeltaTime, DistanceToTarget, ToTarget);
}

// ==================== 战斗拉扯 ====================

const TCHAR* AEnemy::GetCombatMoveDebugName(EEnemyCombatMoveType MoveType)
{
	switch (MoveType)
	{
	case EEnemyCombatMoveType::Retreat:
		return TEXT("Retreat");
	case EEnemyCombatMoveType::BackDiag:
		return TEXT("BackDiag");
	case EEnemyCombatMoveType::Strafe:
		return TEXT("Strafe");
	case EEnemyCombatMoveType::Press:
		return TEXT("Press");
	default:
		return TEXT("Ready");
	}
}

AEnemy::FEnemyCombatMovePlan AEnemy::BuildCombatMovePlan(float DistanceToTarget, const FVector& ToTarget) const
{
	FEnemyCombatMovePlan Plan;
	if (!ChasingTarget)
	{
		return Plan;
	}

	Plan.MoveSpeed = PatrolSpeed;

	const FVector Right = FVector::CrossProduct(FVector::UpVector, ToTarget).GetSafeNormal2D();
	const float SideDir = FMath::RandBool() ? 1.f : -1.f;

	if (DistanceToTarget < CombatTooCloseRadius)
	{
		// 后撤
		const float TargetDist = FMath::FRandRange(CombatPreferredMinRadius, CombatPreferredMaxRadius);
		Plan.MoveType = EEnemyCombatMoveType::Retreat;
		Plan.GoalLocation = ChasingTarget->GetActorLocation() - ToTarget * TargetDist;
		Plan.bUseRetreatSpeedEase = true;
	}
	else if (DistanceToTarget < CombatPreferredMinRadius)
	{
		// 斜后撤
		const float TargetDist = FMath::FRandRange(CombatPreferredMinRadius, CombatPreferredMaxRadius);
		const FVector BackOffset = -ToTarget * TargetDist;
		const FVector SideOffset = Right * SideDir * TargetDist * 0.3f;
		Plan.MoveType = EEnemyCombatMoveType::BackDiag;
		Plan.GoalLocation = ChasingTarget->GetActorLocation() + BackOffset + SideOffset;
		Plan.bUseRetreatSpeedEase = true;
	}
	else if (DistanceToTarget <= CombatPreferredMaxRadius)
	{
		// 侧移：围绕目标旋转固定角度，保持当前半径
		const FVector OffsetFromTarget = -ToTarget * DistanceToTarget;
		const FVector RotatedOffset = OffsetFromTarget.RotateAngleAxis(
			SideDir * CombatStrafeAngleDegrees, FVector::UpVector);
		Plan.MoveType = EEnemyCombatMoveType::Strafe;
		Plan.GoalLocation = ChasingTarget->GetActorLocation() + RotatedOffset;
	}
	else
	{
		// 冷却期目标太远时只压回到距离环外侧，不直接压进攻击距离
		const float TargetDist = CombatPreferredMaxRadius - CombatPressMargin;
		Plan.MoveType = EEnemyCombatMoveType::Press;
		Plan.GoalLocation = ChasingTarget->GetActorLocation() - ToTarget * TargetDist;
		Plan.MoveSpeed = ChaseSpeed;
	}

	return Plan;
}

void AEnemy::UpdateCombatMovement(float DeltaTime, float DistanceToTarget, const FVector& ToTarget)
{
	if (bRepositionInProgress) return;

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now < NextCombatRepositionTime) return;

	const FEnemyCombatMovePlan MovePlan = BuildCombatMovePlan(DistanceToTarget, ToTarget);
	if (!MovePlan.IsValid()) return;

	ClearCombatRetreatSpeedEase();
	GetCharacterMovement()->MaxWalkSpeed = MovePlan.MoveSpeed;
	LastCombatMoveDebug = GetCombatMoveDebugName(MovePlan.MoveType);

	if (MoveToCombatLocation(MovePlan.GoalLocation))
	{
		if (MovePlan.bUseRetreatSpeedEase)
		{
			StartCombatRetreatSpeedEase(MovePlan.GoalLocation);
		}
		// 成功：正常节奏间隔
		const float Interval = FMath::FRandRange(CombatRepositionIntervalMin, CombatRepositionIntervalMax);
		NextCombatRepositionTime = Now + Interval;
	}
	else
	{
		// 失败：短间隔重试
		NextCombatRepositionTime = Now + MovePlan.RetryDelay;
	}
}

bool AEnemy::MoveToCombatLocation(const FVector& Location)
{
	if (!EnemyController) return false;

	FAIMoveRequest MoveRequest;
	MoveRequest.SetGoalLocation(Location);
	MoveRequest.SetAcceptanceRadius(CombatRepositionAcceptanceRadius);
	MoveRequest.SetReachTestIncludesAgentRadius(false);
	MoveRequest.SetReachTestIncludesGoalRadius(false);
	MoveRequest.SetUsePathfinding(true);
	MoveRequest.SetAllowPartialPath(true);
	FPathFollowingRequestResult Result = EnemyController->MoveTo(MoveRequest);

	if (Result.Code == EPathFollowingRequestResult::RequestSuccessful)
	{
		bRepositionInProgress = true;
		return true;
	}

	if (Result.Code == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		LastCombatMoveDebug = TEXT("AlreadyAtGoal");
	}
	else
	{
		LastCombatMoveDebug = TEXT("MoveFail");
	}
	return false;
}

bool AEnemy::MoveToCombatTarget()
{
	if (!EnemyController || !ChasingTarget) return false;

	FAIMoveRequest MoveRequest;
	MoveRequest.SetGoalActor(ChasingTarget);
	MoveRequest.SetAcceptanceRadius(FMath::Max(15.f, CombatAttackMaxRadius - CombatPressMargin));
	MoveRequest.SetReachTestIncludesAgentRadius(false);
	MoveRequest.SetReachTestIncludesGoalRadius(false);
	MoveRequest.SetUsePathfinding(true);
	MoveRequest.SetAllowPartialPath(true);
	FPathFollowingRequestResult Result = EnemyController->MoveTo(MoveRequest);

	if (Result.Code == EPathFollowingRequestResult::RequestSuccessful
		|| Result.Code == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		LastCombatMoveDebug = TEXT("AttackPress");
		return true;
	}

	LastCombatMoveDebug = TEXT("MoveFail");
	return false;
}

void AEnemy::ResetCombatReposition()
{
	bRepositionInProgress = false;
	NextCombatRepositionTime = 0.f;
	ClearCombatRetreatSpeedEase();
	LastCombatMoveDebug.Empty();
}

void AEnemy::StartCombatRetreatSpeedEase(const FVector& GoalLocation)
{
	RetreatSpeedEaseGoalLocation = GoalLocation;
	RetreatSpeedEaseTotalDistance = FVector::Dist2D(GetActorLocation(), GoalLocation);
	bRetreatSpeedEaseActive = RetreatSpeedEaseTotalDistance > KINDA_SMALL_NUMBER;
}

void AEnemy::UpdateCombatRetreatSpeedEase()
{
	if (!bRetreatSpeedEaseActive)
	{
		return;
	}

	const float RemainingDistance = FVector::Dist2D(GetActorLocation(), RetreatSpeedEaseGoalLocation);
	const float Progress = 1.f - FMath::Clamp(RemainingDistance / RetreatSpeedEaseTotalDistance, 0.f, 1.f);
	const float SlowAlpha = FMath::Square(Progress);
	const float MinSpeed = PatrolSpeed * CombatRetreatMinSpeedRatio;
	GetCharacterMovement()->MaxWalkSpeed = FMath::Lerp(PatrolSpeed, MinSpeed, SlowAlpha);
}

void AEnemy::ClearCombatRetreatSpeedEase()
{
	bRetreatSpeedEaseActive = false;
	RetreatSpeedEaseGoalLocation = FVector::ZeroVector;
	RetreatSpeedEaseTotalDistance = 0.f;
}

void AEnemy::OnRepositionMoveCompleted(FAIRequestID RequestID, EPathFollowingResult::Type Result)
{
	bRepositionInProgress = false;
	ClearCombatRetreatSpeedEase();
}

// ==================== 导航/工具 ====================

void AEnemy::StopEnemyMovementIfPossible()
{
	if (EnemyController)
	{
		EnemyController->StopMovement();
	}
}

void AEnemy::MoveToTarget(const AActor* Target)
{
	if (!EnemyController || !Target)
	{
		return;
	}

	FAIMoveRequest MoveRequest;
	MoveRequest.SetGoalActor(Target);

	// 追击：中心距到达 CombatAttackMaxRadius - CombatPressMargin 时停下，与前压目标距离对齐
	// 巡逻：保持原逻辑
	float StopRadius = (Target == ChasingTarget)
		                   ? FMath::Max(15.f, CombatAttackMaxRadius - CombatPressMargin)
		                   : FMath::Max(15.f, PatrolRadius - 50.f);

	MoveRequest.SetAcceptanceRadius(StopRadius);
	if (Target == ChasingTarget)
	{
		MoveRequest.SetReachTestIncludesAgentRadius(false);
		MoveRequest.SetReachTestIncludesGoalRadius(false);
	}
	MoveRequest.SetUsePathfinding(true);
	MoveRequest.SetAllowPartialPath(true);
	EnemyController->MoveTo(MoveRequest);
}

void AEnemy::MoveToLocation(const FVector& Location)
{
	if (!EnemyController)
	{
		return;
	}

	FAIMoveRequest MoveRequest;
	MoveRequest.SetGoalLocation(Location);
	MoveRequest.SetAcceptanceRadius(SearchAcceptanceRadius);
	MoveRequest.SetUsePathfinding(true);
	MoveRequest.SetAllowPartialPath(true);
	EnemyController->MoveTo(MoveRequest);
}

bool AEnemy::BInTargetRange(AActor* Target, double Range) const
{
	if (!IsValid(Target))
	{
		return false;
	}
	double Distance = (Target->GetActorLocation() - GetActorLocation()).SizeSquared2D();
	return Distance <= FMath::Square(Range);
}

bool AEnemy::IsValidCombatTarget(const AActor* Target) const
{
	if (!IsValid(Target))
	{
		return false;
	}
	const ABaseCharacter* Character = Cast<ABaseCharacter>(Target);
	if (Character)
	{
		const UAttributeComponent* Attrs = Character->GetAttributes();
		return Attrs && Attrs->IsAlive();
	}
	return true;
}

// ==================== 血条 ====================

void AEnemy::RevealHealthBar()
{
	if (!HealthBarWidgetComp)
	{
		return;
	}

	HealthBarWidgetComp->SetVisibility(true);

	if (UUserWidget* Widget = HealthBarWidgetComp->GetUserWidgetObject())
	{
		// FadeOut 可能把 widget 自身设为 Hidden/Collapsed，重显时显式拉回可见态。
		Widget->SetVisibility(ESlateVisibility::Visible);
		Widget->SetRenderOpacity(1.0f);

		if (UBaseHealthBarWidget* HealthWidget = Cast<UBaseHealthBarWidget>(Widget))
		{
			HealthWidget->CancelFadeOutAnim();
		}
	}
}

void AEnemy::ShowHealthBar()
{
	RevealHealthBar();

	// 每次调用都会重置倒计时
	GetWorldTimerManager().SetTimer(
		HealthBarHideTimer,
		this,
		&AEnemy::HideHealthBar,
		HealthBarDisplayTime,
		false
	);
}

void AEnemy::HideHealthBar()
{
	// 被玩家锁定中，保持血条显示
	if (bIsTargetedByPlayer) return;

	// 玩家还在追击范围内，不隐藏，重新计时
	if (BInTargetRange(ChasingTarget, ChasingRadius))
	{
		GetWorldTimerManager().SetTimer(
			HealthBarHideTimer,
			this,
			&AEnemy::HideHealthBar,
			HealthBarDisplayTime,
			false
		);
		return;
	}

	if (HealthBarWidgetComp)
	{
		// 获取组件内部的 Widget 实例，并尝试转换为基类
		UBaseHealthBarWidget* HealthWidget = Cast<UBaseHealthBarWidget>(HealthBarWidgetComp->GetUserWidgetObject());

		if (HealthWidget)
		{
			// 触发蓝图事件：播放动画 -> 延迟 0.5s -> 隐藏 -> 恢复透明度
			HealthWidget->PlayFadeOutAnim();
		}
		else
		{
			// 如果强转失败或还没绑定蓝图，降级处理为直接隐藏
			HealthBarWidgetComp->SetVisibility(false);
		}
	}
}

void AEnemy::SetTargetedByPlayer(bool bTargeted)
{
	bIsTargetedByPlayer = bTargeted;
	if (bTargeted)
	{
		// 锁定时确保血条可见（覆盖未挨打过的敌人和正在淡出的敌人）
		RevealHealthBar();
		GetWorldTimerManager().ClearTimer(HealthBarHideTimer);
	}
	else
	{
		// 解除锁定后重排隐藏计时器，恢复自然隐藏流程
		GetWorldTimerManager().SetTimer(
			HealthBarHideTimer, this, &AEnemy::HideHealthBar,
			HealthBarDisplayTime, false);
	}
}

// ==================== 搜索/巡逻 ====================

void AEnemy::SearchTimerFinished()
{
	GetWorldTimerManager().ClearTimer(LookTimer);
	GetCharacterMovement()->bOrientRotationToMovement = true;

	if (AActor* Target = ChooseRadomTarget(PatrolTargets))
	{
		PatrolTarget = Target;
		SetEnemyState(EEnemyState::EES_Patrolling);
	}
	else
	{
		// 无可用巡逻点：留在 Searching 重新等待+张望
		const float WaitTime = FMath::RandRange(PatrolWaitMin, PatrolWaitMax);
		GetWorldTimerManager().SetTimer(PatrolTimer, this, &AEnemy::SearchTimerFinished, WaitTime);
		GetWorldTimerManager().SetTimer(LookTimer, this, &AEnemy::GenerateNewLookRotation, SingleLookTime, true);
		GenerateNewLookRotation();
	}
}

void AEnemy::GenerateNewLookRotation()
{
	float RandomYaw = FMath::RandRange(45.f, 120.f) * (FMath::RandBool() ? 1.f : -1.f);
	PatrolWaitTargetRotation = GetActorRotation() + FRotator(0.f, RandomYaw, 0.f);
}

AActor* AEnemy::ChooseRadomTarget(const TArray<AActor*>& TargetArray)
{
	TArray<AActor*> Candidates;
	for (AActor* Target : TargetArray)
	{
		if (Target == PatrolTarget)
		{
			continue;
		}
		if (!BInTargetRange(Target, PatrolRadius))
		{
			Candidates.AddUnique(Target);
		}
	}

	if (Candidates.Num() > 0)
	{
		return Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
	}

	return nullptr;
}

// ==================== 定时器清理 ====================

void AEnemy::ClearPatrolTimers()
{
	GetWorldTimerManager().ClearTimer(PatrolTimer);
	GetWorldTimerManager().ClearTimer(LookTimer);
}

void AEnemy::ClearAllTimers()
{
	ClearPatrolTimers();
	GetWorldTimerManager().ClearTimer(HealthBarHideTimer);
	GetWorldTimerManager().ClearTimer(AttackCooldownTimer);
	GetWorldTimerManager().ClearTimer(PoiseResetTimer);
	GetWorldTimerManager().ClearTimer(StanceBreakRecoveryTimer);
}

bool AEnemy::IsAllyAttackingNearby(float& OutMaxRemainingTime)
{
	TArray<AActor*> NearbyEnemies;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AEnemy::StaticClass(), NearbyEnemies);

	bool bFound = false;
	float MaxRemaining = 0.f;

	for (AActor* Actor : NearbyEnemies)
	{
		AEnemy* OtherEnemy = Cast<AEnemy>(Actor);
		if (OtherEnemy && OtherEnemy != this)
		{
			float Distance = FVector::Dist(GetActorLocation(), OtherEnemy->GetActorLocation());
			if (Distance <= AttackCoordinationRange && OtherEnemy->GetEnemyState() == EEnemyState::EES_Attacking)
			{
				// TODO: 检查 OtherEnemy->ChasingTarget == this->ChasingTarget 以避免不同战斗组的误协调
				float Remaining = GetWorld()->GetTimerManager().GetTimerRemaining(OtherEnemy->AttackCooldownTimer);
				if (Remaining > MaxRemaining)
				{
					MaxRemaining = Remaining;
				}
				bFound = true;
			}
		}
	}

	if (bFound)
	{
		OutMaxRemainingTime = MaxRemaining;
		return true;
	}
	return false;
}
