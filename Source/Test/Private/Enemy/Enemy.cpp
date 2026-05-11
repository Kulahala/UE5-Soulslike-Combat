// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/Enemy.h"

#include "AIController.h"
#include "Items/Weapon/Weapon.h"
#include "NiagaraFunctionLibrary.h"
#include "AttributeComponent/AttributeComponent.h"
#include "components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/TargetPoint.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HUD/BaseHealthBarWidget.h"
#include "HUD/HealthBarComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
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

	// 血条组件
	HealthBarWidgetComp = CreateDefaultSubobject<UHealthBarComponent>(TEXT("HealthBar"));
	HealthBarWidgetComp->SetupAttachment(RootComponent);

	// AI感知
	AIPerceptionComp = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerceptionComp"));
	UAISenseConfig_Sight* SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	SightConfig->SightRadius = ChasingRadius;
	SightConfig->LoseSightRadius = ChasingRadius + 200.f;
	SightConfig->PeripheralVisionAngleDegrees = VisionAngleDegrees;
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	AIPerceptionComp->ConfigureSense(*SightConfig);
	AIPerceptionComp->SetDominantSense(SightConfig->GetSenseImplementation());
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

	WeaponInit();
}

void AEnemy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 兜底：定时器全量清理，覆盖 Die() 未执行的路径（关卡切换、编辑器 Stop 等）
	ClearAllTimers();
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
		bool bSameTeam = false;
		for (const FName& Tag : Tags)
		{
			if (HitInstigator->ActorHasTag(Tag))
			{
				bSameTeam = true;
				break;
			}
		}
		if (!bSameTeam)
		{
			ChasingTarget = HitInstigator;
		}
		SetEnemyState(EEnemyState::EES_Stunned);
	}
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

void AEnemy::DirectionalHitReact(const FVector& ImpactPoint, const AActor* HitInstigator)
{
	Super::DirectionalHitReact(ImpactPoint, HitInstigator);
}

void AEnemy::Die()
{
	ClearAllTimers();

	// 停止移动
	if (EnemyController)
	{
		EnemyController->StopMovement();
	}

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
	return EnemyState == EEnemyState::EES_Combating && !bAttackOnCooldown;
}

void AEnemy::PlayHitReactMontage(const FName& SectionName)
{
	Super::PlayHitReactMontage(SectionName);
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
}

// ==================== 状态机 ====================

void AEnemy::CheckCombatTarget()
{
	if (!IsValid(ChasingTarget))
	{
		ChasingTarget = nullptr;
		if (EnemyState != EEnemyState::EES_Patrolling && EnemyState != EEnemyState::EES_Searching)
		{
			SetEnemyState(EEnemyState::EES_Patrolling);
		}
		return;
	}

	if (BInTargetRange(ChasingTarget, CombatingRadius))
	{
		SetEnemyState(EEnemyState::EES_Combating);
	}
	else if (BInTargetRange(ChasingTarget, ChasingRadius))
	{
		SetEnemyState(EEnemyState::EES_Chasing);
	}
	else
	{
		ChasingTarget = nullptr;
		SetEnemyState(EEnemyState::EES_Patrolling);
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
		GetCharacterMovement()->bOrientRotationToMovement = true;
	}

	// --- 切换状态 ---
	EnemyState = NewState;

	// --- 进入新状态的逻辑（一次性动作） ---
	switch (EnemyState)
	{
	case EEnemyState::EES_Patrolling:
		GetCharacterMovement()->MaxWalkSpeed = PatrolSpeed;
		MoveToTarget(PatrolTarget);
		break;
	case EEnemyState::EES_Searching:
		// OnSearching 首次 Tick 时处理启动逻辑
		break;
	case EEnemyState::EES_Chasing:
		GetCharacterMovement()->MaxWalkSpeed = ChaseSpeed;
		MoveToTarget(ChasingTarget);
		break;
	case EEnemyState::EES_Combating:
		// 不停止移动，让 AI 导航自然停在 CombatingRadius（AcceptanceRadius）距离
		break;
	case EEnemyState::EES_Attacking:
		// 攻击硬直状态，确保不移动
		if (EnemyController)
		{
			EnemyController->StopMovement();
		}
		break;
	case EEnemyState::EES_Stunned:
		// 被打出硬直瞬间，立刻刹车防止滑步
		if (EnemyController)
		{
			EnemyController->StopMovement();
		}
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
	// 锁定新目标
	APawn* SeenPawn = dynamic_cast<APawn*>(Actor);
	ChasingTarget = SeenPawn;
}

// ==================== AI Tick ====================

void AEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (EnemyState == EEnemyState::EES_Dead || EnemyState == EEnemyState::EES_Stunned || EnemyState ==
		EEnemyState::EES_Attacking)
	{
		return;
	}

	// [调试] 敌人状态 + 距离 + 范围球体
	// TODO: 多敌人时调试文字会混在一起，加 GetName() 或编号区分
	if (FDebugDrawHelper::IsEnemyEnabled())
	{
		FDebugDrawHelper::Add(FString::Printf(TEXT("EnemyState: %s | Speed: %.0f"),
			*UEnum::GetValueAsString(EnemyState), GroundSpeed),
			EnemyState == EEnemyState::EES_Dead ? FColor::Red : FColor::White);

		if (ChasingTarget)
		{
			float Dist = FVector::Dist2D(GetActorLocation(), ChasingTarget->GetActorLocation());
			FDebugDrawHelper::Add(FString::Printf(TEXT("Dist: %.0f / %.0f"), Dist, ChasingRadius),
				Dist <= CombatingRadius ? FColor::Red : Dist <= ChasingRadius ? FColor::Yellow : FColor::White);
		}
	}

	if (ChasingTarget)
	{
		FDebugDrawHelper::AddSphere(GetWorld(), GetActorLocation(), ChasingRadius, FColor::Yellow);
		FDebugDrawHelper::AddSphere(GetWorld(), GetActorLocation(), CombatingRadius, FColor::Red, 16);
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
	if (ChasingTarget)
	{
		// 校验是否面朝目标，不够面向就先转
		FVector ToTarget = (ChasingTarget->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		float Dot = FVector::DotProduct(GetActorForwardVector().GetSafeNormal2D(), ToTarget);
		if (Dot > AttackAngleThreshold)
		{
			Attack();
		}
		else
		{
			FRotator TargetRot = ToTarget.Rotation();
			SetActorRotation(FMath::RInterpTo(GetActorRotation(), TargetRot, DeltaTime, CombatRotationSpeed));
		}
	}
}

// ==================== 导航/工具 ====================

void AEnemy::MoveToTarget(const AActor* Target)
{
	if (!EnemyController || !Target)
	{
		return;
	}

	FAIMoveRequest MoveRequest;
	MoveRequest.SetGoalActor(Target);

	// 追击：减去目标胶囊体半径 + 精度余量，让中心距 ≈ CombatingRadius
	// 巡逻：保持原逻辑
	float StopRadius = (Target == ChasingTarget)
		                   ? FMath::Max(15.f, CombatingRadius - Target->GetSimpleCollisionRadius() - 10.f)
		                   : FMath::Max(15.f, PatrolRadius - 50.f);

	MoveRequest.SetAcceptanceRadius(StopRadius);
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

// ==================== 血条 ====================

void AEnemy::ShowHealthBar()
{
	if (HealthBarWidgetComp)
	{
		HealthBarWidgetComp->SetVisibility(true);

		// 重置透明度，防止上次 FadeOut 动画把 opacity 设为 0 后无法恢复
		if (UUserWidget* Widget = HealthBarWidgetComp->GetUserWidgetObject())
		{
			Widget->SetRenderOpacity(1.0f);
		}
	}

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
	// TODO: 魂系锁定机制。等待锁定功能实现后，在这里判断 if (bIsTargetedByPlayer) return; 以保持血条显示

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
}
