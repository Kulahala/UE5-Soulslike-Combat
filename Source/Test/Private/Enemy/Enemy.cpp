// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/Enemy.h"

#include "World/EncounterController.h"

#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "AttributeComponent/AttributeComponent.h"
#include "Combat/CombatTeamHelper.h"
#include "Combat/CombatProjectile.h"
#include "Combat/EnemyAttackConfigDataAsset.h"
#include "Combat/HitReactionConfigDataAsset.h"
#include "components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/TargetPoint.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HUD/BaseHealthBarWidget.h"
#include "HUD/HealthBarComponent.h"
#include "Items/Weapon/Weapon.h"
#include "Items/Bow/BowBase.h"
#include "Items/Treasures/Treasure.h"
#include "Kismet/GameplayStatics.h"
#include "MotionWarpingComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "UObject/UnrealType.h"
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
	HealthBarWidgetComp->SetWidgetSpace(EWidgetSpace::Screen);
	// 血条尺寸由 WBP_EnemyHealthBar 根 SizeBox 决定，避免角色类固定渲染画布。
	HealthBarWidgetComp->SetDrawAtDesiredSize(true);
	HealthBarWidgetComp->SetPivot(FVector2D(0.5f, 0.5f));
	HealthBarWidgetComp->SetRelativeLocation(FVector(13.153872f, 8.550018f, 86.763085f));

	// 锁定标记：WidgetClass 在敌人蓝图中指定，C++ 负责共用布局和显隐。
	LockOnMarkerWidgetComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("LockOnMarker"));
	LockOnMarkerWidgetComp->SetupAttachment(RootComponent);
	LockOnMarkerWidgetComp->SetWidgetSpace(EWidgetSpace::Screen);
	LockOnMarkerWidgetComp->SetDrawSize(FVector2D(4.f, 4.f));
	LockOnMarkerWidgetComp->SetPivot(FVector2D(0.5f, 0.5f));
	LockOnMarkerWidgetComp->SetRelativeLocation(FVector(0.f, 0.f, 30.f));
	LockOnMarkerWidgetComp->SetVisibility(false);

	// AI感知
	AIPerceptionComp = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerceptionComp"));

	// Motion Warping：跳劈/跃进类攻击按 DataAsset 写入一次性 WarpTarget
	MotionWarpingComponent = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarpingComponent"));

	// 视觉配置
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	SightConfig->SightRadius = ChasingRadius;
	SightConfig->LoseSightRadius = ChasingRadius + 200.f;
	SightConfig->PeripheralVisionAngleDegrees = VisionAngleDegrees;
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	AIPerceptionComp->ConfigureSense(*SightConfig);
	AIPerceptionComp->SetDominantSense(SightConfig->GetSenseImplementation());

	// 听觉配置
	HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearingConfig"));
	HearingConfig->HearingRange = HearingRange;
	HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
	HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
	HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;
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
		// 运行时敌人武器不是固定世界拾取物；必须在 BeginPlay 前建立所有权。
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = this;
		SpawnParameters.Instigator = this;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AWeapon* Weapon = GetWorld()->SpawnActor<AWeapon>(WeaponClass, GetActorTransform(), SpawnParameters);
		if (!Weapon)
		{
			UE_LOG(LogTemp, Warning, TEXT("%s failed to spawn weapon from class %s"),
				*GetName(),
				*GetNameSafe(WeaponClass));
			return;
		}

		const FName AttachSocketName = Weapon->GetDefaultEquipSocketName();
		if (!Weapon->Equip(GetMesh(), AttachSocketName, this, this))
		{
			UE_LOG(LogTemp, Warning, TEXT("%s failed to equip weapon '%s' to its default socket '%s'."),
				*GetName(), *GetNameSafe(Weapon), *AttachSocketName.ToString());
			Weapon->Destroy();
			return;
		}

		EquippedWeapon = Weapon;
	}
}

void AEnemy::BeginPlay()
{
	Super::BeginPlay();
	Tags.Add(FName("Enemy"));
	ApplyAuthoredPerceptionConfig();

	// 韧性初始化
	CurrentPoise = MaxPoise;

	// 血条
	if (HealthBarWidgetComp)
	{
		HealthBarWidgetComp->SetVisibility(false);
	}
	if (LockOnMarkerWidgetComp)
	{
		LockOnMarkerWidgetComp->SetVisibility(false);
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

#if !UE_BUILD_SHIPPING
	if (!bIsRangedDebugProbeInstance)
#endif
	{
		SpawnPointInit();
	}

	RefreshEnemyControllerBinding();

	WeaponInit();
	ValidateEnemyAttackConfig();
	if (!HitReactionConfig)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: HitReactionConfig is not set. Hit reactions and death montages will not play."),
		       *GetName());
	}
	ValidateStanceBreakConfig();
}

#if WITH_EDITOR
void AEnemy::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	ApplyAuthoredPerceptionConfig();
	ValidateEnemyAttackConfig();
	ValidateStanceBreakConfig();
}
#endif

void AEnemy::ApplyAuthoredPerceptionConfig()
{
	if (!AIPerceptionComp)
	{
		return;
	}

	if (SightConfig)
	{
		SightConfig->SightRadius = ChasingRadius;
		SightConfig->LoseSightRadius = ChasingRadius + 200.f;
		SightConfig->PeripheralVisionAngleDegrees = VisionAngleDegrees;
		AIPerceptionComp->ConfigureSense(*SightConfig);
	}

	if (HearingConfig)
	{
		HearingConfig->HearingRange = HearingRange;
		AIPerceptionComp->ConfigureSense(*HearingConfig);
	}
}

bool AEnemy::HandleArchetypeCombatPriority(float, float, const FVector&)
{
	return false;
}

void AEnemy::TickArchetypeAttack(float)
{
}

bool AEnemy::HandleArchetypeAttackCooldownEnded()
{
	return false;
}

bool AEnemy::HandleArchetypeMoveCompleted(FAIRequestID, EPathFollowingResult::Type)
{
	return false;
}

void AEnemy::ClearArchetypeCombatState()
{
}

void AEnemy::ValidateArchetypeCombatConfig() const
{
}

FString AEnemy::GetArchetypeCombatDebugText() const
{
	return FString();
}

void AEnemy::DrawArchetypeCombatDebug() const
{
}

void AEnemy::SetArchetypeMovementDefaults(float InPatrolSpeed, float InCombatManeuverSpeed, float InChaseSpeed)
{
	PatrolSpeed = InPatrolSpeed;
	CombatManeuverSpeed = InCombatManeuverSpeed;
	ChaseSpeed = InChaseSpeed;
	GetCharacterMovement()->MaxWalkSpeed = PatrolSpeed;
}

void AEnemy::SetArchetypeCombatSpacingDefaults(float InTooCloseRadius, float InAttackMaxRadius,
	float InPreferredMinRadius, float InPreferredMaxRadius, float InPressMargin, float InRetreatMinSpeedRatio)
{
	CombatTooCloseRadius = InTooCloseRadius;
	CombatAttackMaxRadius = InAttackMaxRadius;
	CombatPreferredMinRadius = InPreferredMinRadius;
	CombatPreferredMaxRadius = InPreferredMaxRadius;
	CombatPressMargin = InPressMargin;
	CombatRetreatMinSpeedRatio = InRetreatMinSpeedRatio;
}

void AEnemy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(SpawnPoint))
	{
		SpawnPoint->Destroy();
		SpawnPoint = nullptr;
	}

	// 兜底：定时器全量清理，覆盖 Die() 未执行的路径（关卡切换、编辑器 Stop 等）
	ClearStanceBreakMontagePresentation(true);
	ClearAllTimers();
	ClearArchetypeCombatState();
	ClearActiveProjectileAttack();
#if !UE_BUILD_SHIPPING
	bRangedDebugProbeActive = false;
	bRangedDebugProbeCompleted = true;
	DebugProbeController.Reset();
#endif
	SetEnemyControllerBinding(nullptr);
	Super::EndPlay(EndPlayReason);
}

void AEnemy::RefreshEnemyControllerBinding()
{
	SetEnemyControllerBinding(Cast<AAIController>(GetController()));
}

void AEnemy::SetEnemyControllerBinding(AAIController* NewEnemyController)
{
	if (IsValid(EnemyController))
	{
		EnemyController->ReceiveMoveCompleted.RemoveDynamic(this, &AEnemy::OnRepositionMoveCompleted);
	}

	EnemyController = NewEnemyController;
	if (IsValid(EnemyController))
	{
		EnemyController->ReceiveMoveCompleted.RemoveDynamic(this, &AEnemy::OnRepositionMoveCompleted);
		EnemyController->ReceiveMoveCompleted.AddDynamic(this, &AEnemy::OnRepositionMoveCompleted);
	}
}

// ==================== 受击/死亡 ====================

void AEnemy::GetHit_Implementation(const FVector& ImpactPoint, AActor* HitInstigator)
{
	if (bEncounterDormant)
	{
		ResetPendingHitContext();
		return;
	}

	const bool bWasInStanceBreak = EnemyState == EEnemyState::EES_StanceBreak;
	if (bWasInStanceBreak)
	{
		// 失衡窗口仍承受伤害和击退，但不能让普通受击蒙太奇/状态覆盖专用失衡蒙太奇。
		PendingHitContext.bSuppressNormalHitReact = true;
		PendingHitContext.bApplyStun = false;
	}

	//DrawDebugSphere(this->GetWorld(), ImpactPoint, 5, 10, FColor::Red, false, 5.0f, 0, 0.5f);
	Super::GetHit_Implementation(ImpactPoint, HitInstigator);
	ShowHealthBar();

	if (bWasInStanceBreak && Attributes->IsAlive() && !PendingHitContext.bWasBlocked)
	{
		// BaseCharacter 因抑制普通受击而跳过了这次反馈；失衡中仍应保留一次命中特效/音效。
		PlayHitEffects(ImpactPoint);
	}

	// 武器命中才触发硬直（DOT 不经过 GetHit，不会触发）
	if (Attributes->IsAlive() && HitInstigator)
	{
		// 只锁定不同阵营的目标，防止被同类打到后锁定队友
		if (!FCombatTeamHelper::ShareTeamTag(this, HitInstigator))
		{
			ChasingTarget = HitInstigator;
		}
		if (!bWasInStanceBreak && PendingHitContext.bApplyStun)
		{
			SetEnemyState(EEnemyState::EES_Stunned);
		}
	}

	ResetPendingHitContext();
}

float AEnemy::TakeDamage(float DamageAmount, const struct FDamageEvent& DamageEvent, class AController* EventInstigator,
                         AActor* DamageCauser)
{
	if (bEncounterDormant)
	{
		return 0.f;
	}

	Attributes->ReceiveDamage(DamageAmount);

	if (!Attributes->IsAlive())
	{
		SetEnemyState(EEnemyState::EES_Dead);
	}

	return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}

void AEnemy::Die()
{
	ClearStanceBreakMontagePresentation(true);
	ClearAllTimers();
	ClearArchetypeCombatState();
#if !UE_BUILD_SHIPPING
	bRangedDebugProbeActive = false;
	bRangedDebugProbeCompleted = true;
	DebugProbeController.Reset();
#endif
	ClearPendingStanceBreak();

	StopEnemyMovementIfPossible();

	// 关闭碰撞
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCapsuleComponent()->SetCollisionResponseToAllChannels(ECR_Ignore);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 清理附加物和UI
	if (IsValid(SpawnPoint))
	{
		SpawnPoint->Destroy();
		SpawnPoint = nullptr;
	}
	HealthBarWidgetComp->SetVisibility(false);

	PlayDeathMontage();
	SpawnDeathTreasure();

	// 设定销毁时间
	SetLifeSpan(CorpseLifespan);

	if (!bDeathNotificationBroadcast)
	{
		bDeathNotificationBroadcast = true;
		EnemyDiedDelegate.Broadcast(this);
	}
}

void AEnemy::SpawnDeathTreasure()
{
	if (bDeathTreasureSpawnAttempted)
	{
		return;
	}
	bDeathTreasureSpawnAttempted = true;

	if (!DeathTreasureClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: DeathTreasureClass is not configured; no death reward will spawn."), *GetName());
		return;
	}

	if (DeathGoldValue <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: DeathGoldValue must be positive; no death reward will spawn."), *GetName());
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: cannot spawn death treasure because the world is unavailable."), *GetName());
		return;
	}

	const FTransform SpawnTransform = GetActorTransform();
	ATreasure* Treasure = World->SpawnActorDeferred<ATreasure>(DeathTreasureClass, SpawnTransform,
		nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Treasure)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: failed to deferred-spawn death treasure class '%s'."),
			*GetName(), *GetNameSafe(DeathTreasureClass.Get()));
		return;
	}

	Treasure->SetGoldValue(DeathGoldValue);
	Treasure->SetPickupTriggerPolicy(EItemPickupTriggerPolicy::Interact);

	AActor* FinishedTreasure = UGameplayStatics::FinishSpawningActor(Treasure, SpawnTransform);
	if (!IsValid(FinishedTreasure) || FinishedTreasure != Treasure)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: death treasure class '%s' was invalid after FinishSpawningActor."),
			*GetName(), *GetNameSafe(DeathTreasureClass.Get()));
	}
}

void AEnemy::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	RefreshEnemyControllerBinding();
}

void AEnemy::UnPossessed()
{
	SetEnemyControllerBinding(nullptr);
	Super::UnPossessed();
}

bool AEnemy::ClaimEncounterOwner(AEncounterController* NewOwner)
{
	if (!NewOwner)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s rejected a null encounter owner."), *GetName());
		return false;
	}

	if (EncounterOwner && EncounterOwner != NewOwner)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s is already owned by encounter controller '%s'."),
			*GetName(), *GetNameSafe(EncounterOwner));
		return false;
	}

	EncounterOwner = NewOwner;
	return true;
}

void AEnemy::ReleaseEncounterOwner(AEncounterController* CurrentOwner)
{
	if (EncounterOwner != CurrentOwner)
	{
		return;
	}

	EncounterOwner = nullptr;
	bEncounterDormant = false;
}

void AEnemy::SetEncounterDormant(AEncounterController* CurrentOwner)
{
	if (EncounterOwner != CurrentOwner || EnemyState == EEnemyState::EES_Dead)
	{
		return;
	}

	// 先建立状态屏障，再停止 Montage，避免其结束回调重新驱动 AI。
	bEncounterDormant = true;
	ClearStanceBreakMontagePresentation(true);
	ClearAllTimers();
#if !UE_BUILD_SHIPPING
	bRangedDebugProbeActive = false;
	bRangedDebugProbeCompleted = true;
	DebugProbeController.Reset();
#endif
	bAttackOnCooldown = false;
	ClearCurrentAttackConfig(false);
	ClearPendingAttack();
	ClearArchetypeCombatState();
	LastBlockedPendingAttackIndex = INDEX_NONE;
	PendingAttackRetryBlockUntil = 0.f;
	CombatSubState = EEnemyCombatSubState::None;
	ResetCombatReposition();
	bSearchingLostTarget = false;
	LastAllyAttackCheckTime = -1000.f;
	bCachedAllyAttackingNearby = false;
	CachedAllySuggestedWaitTime = 0.f;
	CachedAllyCheckTarget = nullptr;
	ClearPendingStanceBreak();
	ResetPendingHitContext();
	ResetPoise();
	ChasingTarget = nullptr;

	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		AnimInstance->Montage_Stop(0.05f);
	}

	StopEnemyMovementIfPossible();
}

bool AEnemy::ActivateForEncounter(AEncounterController* CurrentOwner, AActor* InitialTarget)
{
	if (EncounterOwner != CurrentOwner || EnemyState == EEnemyState::EES_Dead || !IsValidCombatTarget(InitialTarget))
	{
		return false;
	}

	bEncounterDormant = false;
	bAttackOnCooldown = false;
	ChasingTarget = InitialTarget;
	SetEnemyState(EEnemyState::EES_Chasing);
	return true;
}

bool AEnemy::IsEngagingActor(const AActor* Actor) const
{
	if (!IsValid(Actor) || bEncounterDormant || EnemyState == EEnemyState::EES_Dead || ChasingTarget != Actor)
	{
		return false;
	}

	return EnemyState == EEnemyState::EES_Chasing || EnemyState == EEnemyState::EES_Combating
		|| EnemyState == EEnemyState::EES_Attacking || EnemyState == EEnemyState::EES_Stunned
		|| EnemyState == EEnemyState::EES_StanceBreak;
}

// ==================== 韧性系统 ====================

void AEnemy::ApplyPoiseDamage(float Damage, AActor* DamageInstigator)
{
	if (bEncounterDormant || EnemyState == EEnemyState::EES_Dead)
	{
		return;
	}

	if (EnemyState == EEnemyState::EES_StanceBreak)
	{
		// 失衡窗口不允许二次破防重播或延长；保持韧性回满，等待当前专用 Montage 结束。
		ClearPendingStanceBreak();
		ResetPoise();
		return;
	}

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

void AEnemy::ApplyStanceBreak()
{
	if (bEncounterDormant || EnemyState == EEnemyState::EES_Dead)
	{
		ClearPendingStanceBreak();
		return;
	}

	if (EnemyState == EEnemyState::EES_StanceBreak)
	{
		// 迟到的弹反/韧性结算不能重播、延长或覆盖当前失衡窗口。
		ClearPendingStanceBreak();
		ResetPoise();
		return;
	}

	// 先确认专用失衡蒙太奇真正可播放，再提交失衡状态，缺失资产不能伪造成功。
	if (!PlayStanceBreakMontage())
	{
		ClearPendingStanceBreak();
		ResetPoise();
		return;
	}

	SetEnemyState(EEnemyState::EES_StanceBreak);
	ClearPendingStanceBreak();
	ResetPoise();

	if (UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
	{
		FOnMontageEnded EndDelegate;
		EndDelegate.BindUObject(this, &AEnemy::OnStanceBreakMontageEnded);
		AnimInstance->Montage_SetEndDelegate(EndDelegate, ActiveStanceBreakMontage);
	}
}

UAnimMontage* AEnemy::GetStanceBreakMontage() const
{
	return HitReactionConfig && HitReactionConfig->StanceBreak.Montage
		? HitReactionConfig->StanceBreak.Montage.Get()
		: nullptr;
}

bool AEnemy::PlayStanceBreakMontage()
{
	if (!HitReactionConfig)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("%s: HitReactionConfig is unavailable; refusing to enter EES_StanceBreak."), *GetName());
		return false;
	}

	UAnimMontage* StanceBreakMontage = GetStanceBreakMontage();
	if (!StanceBreakMontage)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("%s: StanceBreak.Montage is unavailable; refusing to enter EES_StanceBreak."), *GetName());
		return false;
	}

	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("%s: StanceBreak.Montage '%s' cannot play because its AnimInstance is unavailable."),
			*GetName(), *GetNameSafe(StanceBreakMontage));
		return false;
	}

	if (AnimInstance->Montage_Play(StanceBreakMontage) <= 0.f)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("%s: failed to play StanceBreak.Montage '%s'; refusing to enter EES_StanceBreak."),
			*GetName(), *GetNameSafe(StanceBreakMontage));
		return false;
	}

	ActiveStanceBreakMontage = StanceBreakMontage;
	return true;
}

void AEnemy::OnStanceBreakMontageEnded(UAnimMontage* Montage, bool /*bInterrupted*/)
{
	if (Montage != ActiveStanceBreakMontage)
	{
		return;
	}

	ClearStanceBreakMontagePresentation(false);
	RecoverFromStanceBreak();
}

void AEnemy::RecoverFromStanceBreak()
{
	if (bEncounterDormant || !Attributes || !Attributes->IsAlive()
		|| EnemyState != EEnemyState::EES_StanceBreak)
	{
		return;
	}

	// 委托 CheckCombatTarget 统一判定恢复后的 Combat / Chase / Patrol 入口。
	CheckCombatTarget();
}

void AEnemy::ClearStanceBreakMontagePresentation(bool bStopMontage)
{
	UAnimMontage* MontageToClear = ActiveStanceBreakMontage;
	ActiveStanceBreakMontage = nullptr;
	if (!MontageToClear)
	{
		return;
	}

	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		return;
	}

	FOnMontageEnded EmptyEndDelegate;
	AnimInstance->Montage_SetEndDelegate(EmptyEndDelegate, MontageToClear);
	if (bStopMontage && AnimInstance->Montage_IsActive(MontageToClear))
	{
		AnimInstance->Montage_Stop(0.05f, MontageToClear);
	}
}

void AEnemy::ClearPendingStanceBreak()
{
	bPendingStanceBreak = false;
	LastPoiseDamageInstigator = nullptr;
}

void AEnemy::ValidateStanceBreakConfig() const
{
	if (HitReactionConfig && !HitReactionConfig->StanceBreak.Montage)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("%s: HitReactionConfig '%s' has no StanceBreak.Montage. Poise breaks and parries will not enter EES_StanceBreak."),
			*GetName(), *GetNameSafe(HitReactionConfig));
	}
}

void AEnemy::ResetPoise()
{
	CurrentPoise = MaxPoise;
	GetWorldTimerManager().ClearTimer(PoiseResetTimer);
}

UHitReactionConfigDataAsset* AEnemy::GetReactionConfig() const
{
	return HitReactionConfig.Get();
}

// ==================== 弹反（已废弃，保留用于重构） ====================

// ==================== 攻击 ====================

void AEnemy::Attack()
{
	if (!CanAttack())
	{
		return;
	}

	if (!EnemyAttackConfig)
	{
		WarnNoEnemyAttackCandidate(0.f);
		return;
	}

	const float DistanceToTarget = FVector::Dist2D(GetActorLocation(), ChasingTarget->GetActorLocation());
	PerformConfiguredAttack(DistanceToTarget);
}

bool AEnemy::CanAttack() const
{
	return !bEncounterDormant && EnemyState == EEnemyState::EES_Combating && !bAttackOnCooldown &&
		IsValidCombatTarget(ChasingTarget);
}

void AEnemy::PerformConfiguredAttack(float DistanceToTarget)
{
	const int32 AttackIndex = EnemyAttackConfig->ChooseAttackIndex(DistanceToTarget);
	PerformConfiguredAttackByIndex(AttackIndex);
}

bool AEnemy::PerformConfiguredAttackByIndex(int32 AttackIndex)
{
	if (!EnemyAttackConfig)
	{
		WarnNoEnemyAttackCandidate(0.f);
		return false;
	}

	if (!EnemyAttackConfig->Attacks.IsValidIndex(AttackIndex))
	{
		const float DistanceToTarget = IsValidCombatTarget(ChasingTarget)
			                               ? FVector::Dist2D(GetActorLocation(), ChasingTarget->GetActorLocation())
			                               : 0.f;
		WarnNoEnemyAttackCandidate(DistanceToTarget);
		return false;
	}

	const FEnemyAttackEntry& Entry = EnemyAttackConfig->Attacks[AttackIndex];
	if (!EnemyAttackConfig->IsEntrySelectable(Entry))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s rejected invalid enemy attack entry '%s'."),
			*GetName(), *Entry.AttackName.ToString());
		return false;
	}

	FActiveProjectileAttack ProjectileSnapshot;
	if (Entry.DeliveryType == EEnemyAttackDeliveryType::Projectile
		&& !BuildProjectileAttackSnapshot(Entry, ProjectileSnapshot))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s rejected projectile attack entry '%s' because its launch snapshot is invalid."),
			*GetName(), *Entry.AttackName.ToString());
		return false;
	}

	if (Entry.DeliveryType == EEnemyAttackDeliveryType::Projectile)
	{
		const float DistanceToTarget = FVector::Dist2D(GetActorLocation(), ChasingTarget->GetActorLocation());
		FVector SpawnLocation;
		FVector TargetLocation;
		if (!IsProjectileAttackWithinStartRange(ProjectileSnapshot, DistanceToTarget)
			|| !HasClearProjectileLineOfSight(ProjectileSnapshot, SpawnLocation, TargetLocation))
		{
			UE_LOG(LogTemp, Warning, TEXT("%s rejected projectile attack entry '%s' because range or LOS is no longer valid."),
				*GetName(), *Entry.AttackName.ToString());
			return false;
		}
	}

	CurrentAttackIndex = AttackIndex;
	bCurrentAttackCooldownStarted = false;
	SetAttackDamageMultiplier(Entry.DamageMultiplier);
	SetBlockStaminaDamageMultiplier(Entry.BlockStaminaDamageMultiplier);
	SetCurrentAttackCannotBeParried(Entry.bCannotBeParried);
	if (Entry.DeliveryType == EEnemyAttackDeliveryType::Projectile)
	{
		ActiveProjectileAttack = MoveTemp(ProjectileSnapshot);
	}
	SetEnemyState(EEnemyState::EES_Attacking);
	if (Entry.DeliveryType == EEnemyAttackDeliveryType::Melee)
	{
		UpdateAttackMotionWarpTarget(Entry);
	}

	if (!PlayEnemyAttackMontage(Entry))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s failed to play enemy attack montage for entry '%s'."),
		       *GetName(), *Entry.AttackName.ToString());
		ClearCurrentAttackConfig(false);
		CheckCombatTarget();
		return false;
	}

	ClearPendingAttack();
	return true;
}

void AEnemy::StartAttackCooldown(float MinCooldown, float MaxCooldown)
{
	const float Cooldown = FMath::RandRange(MinCooldown, FMath::Max(MinCooldown, MaxCooldown));
	if (Cooldown <= 0.f)
	{
		bAttackOnCooldown = false;
		GetWorldTimerManager().ClearTimer(AttackCooldownTimer);
		return;
	}

	bAttackOnCooldown = true;
	GetWorldTimerManager().SetTimer(AttackCooldownTimer, this,
	                                &AEnemy::OnAttackCooldownEnd, Cooldown, false);
}

bool AEnemy::PlayEnemyAttackMontage(const FEnemyAttackEntry& Entry)
{
	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInstance || !Entry.Montage)
	{
		return false;
	}

	const float PlayResult = AnimInstance->Montage_Play(Entry.Montage);
	if (PlayResult <= 0.f)
	{
		return false;
	}

	if (Entry.StartSection != NAME_None)
	{
		AnimInstance->Montage_JumpToSection(Entry.StartSection, Entry.Montage);
	}

	FOnMontageEnded EndDelegate;
	EndDelegate.BindUObject(this, &AEnemy::OnAttackMontageEnded);
	AnimInstance->Montage_SetEndDelegate(EndDelegate, Entry.Montage);

	return true;
}

void AEnemy::ClearCurrentAttackConfig(bool bStartCooldown)
{
	if (EnemyAttackConfig && EnemyAttackConfig->Attacks.IsValidIndex(CurrentAttackIndex))
	{
		ClearAttackMotionWarpTarget(EnemyAttackConfig->Attacks[CurrentAttackIndex]);
	}

	if (bStartCooldown)
	{
		StartCurrentAttackCooldownIfNeeded();
	}

	CurrentAttackIndex = INDEX_NONE;
	ClearActiveProjectileAttack();
	bCurrentAttackCooldownStarted = false;
	SetAttackDamageMultiplier(1.f);
	SetBlockStaminaDamageMultiplier(1.f);
	SetCurrentAttackCannotBeParried(false);
}

bool AEnemy::BuildProjectileAttackSnapshot(const FEnemyAttackEntry& Entry,
	FActiveProjectileAttack& OutSnapshot) const
{
	OutSnapshot = FActiveProjectileAttack{};
	if (Entry.DeliveryType != EEnemyAttackDeliveryType::Projectile || !Entry.ProjectileClass
		|| !Entry.ProjectileDeliveryConfig.IsValid())
	{
		return false;
	}

	const float EffectiveMaxDistance = FMath::Min(Entry.MaxDistance, CombatAttackMaxRadius);
	if (EffectiveMaxDistance <= 0.f || Entry.MinDistance > EffectiveMaxDistance)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("%s rejected projectile attack entry '%s': range %.1f-%.1f conflicts with CombatAttackMaxRadius %.1f."),
			*GetName(), *Entry.AttackName.ToString(), Entry.MinDistance, Entry.MaxDistance, CombatAttackMaxRadius);
		return false;
	}

	OutSnapshot.ProjectileClass = Entry.ProjectileClass;
	OutSnapshot.DeliveryConfig = Entry.ProjectileDeliveryConfig;
	OutSnapshot.DeliveryConfig.Damage *= Entry.DamageMultiplier;
	OutSnapshot.DeliveryConfig.BlockStaminaDamageMultiplier *= Entry.BlockStaminaDamageMultiplier;
	OutSnapshot.DeliveryConfig.bCanBeParried = Entry.ProjectileDeliveryConfig.bCanBeParried
		&& !Entry.bCannotBeParried;
	OutSnapshot.SpawnSocketName = Entry.ProjectileSpawnSocketName;
	OutSnapshot.TargetHeightOffset = Entry.ProjectileTargetHeightOffset;
	OutSnapshot.MinDistance = Entry.MinDistance;
	OutSnapshot.MaxDistance = EffectiveMaxDistance;
	OutSnapshot.MinCooldown = Entry.MinCooldown;
	OutSnapshot.MaxCooldown = Entry.MaxCooldown;
	OutSnapshot.bIsActive = OutSnapshot.DeliveryConfig.IsValid();
	return OutSnapshot.IsValid();
}

#if !UE_BUILD_SHIPPING
bool AEnemy::BuildDebugProbeProjectileSnapshot(FActiveProjectileAttack& OutSnapshot) const
{
	OutSnapshot = FActiveProjectileAttack{};
	OutSnapshot.ProjectileClass = ACombatProjectile::StaticClass();
	OutSnapshot.DeliveryConfig.Damage = 10.f;
	OutSnapshot.DeliveryConfig.PoiseDamage = 1.f;
	OutSnapshot.DeliveryConfig.BlockStaminaDamageMultiplier = 1.f;
	OutSnapshot.DeliveryConfig.bCanBeParried = false;
	OutSnapshot.DeliveryConfig.InitialSpeed = 3000.f;
	OutSnapshot.DeliveryConfig.MaxSpeed = 3000.f;
	OutSnapshot.DeliveryConfig.CollisionRadius = 10.f;
	OutSnapshot.DeliveryConfig.MaxLifetime = 3.f;
	OutSnapshot.MinDistance = 500.f;
	OutSnapshot.MaxDistance = 1100.f;
	OutSnapshot.MinCooldown = 0.f;
	OutSnapshot.MaxCooldown = 0.f;
	OutSnapshot.bIsActive = true;
	OutSnapshot.bDebugProbe = true;
	return OutSnapshot.IsValid();
}
#endif

bool AEnemy::ResolveProjectileSpawnLocation(const FActiveProjectileAttack& Snapshot, FVector& OutSpawnLocation) const
{
	if (const ABowBase* Bow = Cast<ABowBase>(EquippedWeapon))
	{
		FTransform LaunchTransform;
		FString FailureReason;
		if (!Bow->TryGetLaunchTransform(LaunchTransform, FailureReason))
		{
			if (LastInvalidProjectileBow.Get() != Bow)
			{
				UE_LOG(LogTemp, Warning,
					TEXT("%s rejected Bow projectile release: shared Bow PhysicalProfile/BowArrowSocket is invalid: %s"),
					*GetName(), *FailureReason);
				LastInvalidProjectileBow = const_cast<ABowBase*>(Bow);
			}
			return false;
		}

		LastInvalidProjectileBow.Reset();
		OutSpawnLocation = LaunchTransform.GetLocation();
		return true;
	}

	LastInvalidProjectileBow.Reset();
	if (Snapshot.SpawnSocketName == NAME_None)
	{
		FRotator EyeRotation;
		GetActorEyesViewPoint(OutSpawnLocation, EyeRotation);
		return true;
	}

	const USkeletalMeshComponent* MeshComponent = GetMesh();
	if (!MeshComponent || !MeshComponent->DoesSocketExist(Snapshot.SpawnSocketName))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s rejected projectile release: socket '%s' does not exist."),
			*GetName(), *Snapshot.SpawnSocketName.ToString());
		return false;
	}

	OutSpawnLocation = MeshComponent->GetSocketLocation(Snapshot.SpawnSocketName);
	return true;
}

bool AEnemy::ResolveProjectileTargetLocation(const FActiveProjectileAttack& Snapshot, FVector& OutTargetLocation) const
{
	if (!IsValidCombatTarget(ChasingTarget))
	{
		return false;
	}

	FRotator TargetEyeRotation;
	ChasingTarget->GetActorEyesViewPoint(OutTargetLocation, TargetEyeRotation);
	OutTargetLocation.Z += Snapshot.TargetHeightOffset;
	return true;
}

bool AEnemy::HasClearProjectileLineOfSight(const FActiveProjectileAttack& Snapshot, FVector& OutSpawnLocation,
	FVector& OutTargetLocation) const
{
	if (!GetWorld() || !ResolveProjectileSpawnLocation(Snapshot, OutSpawnLocation)
		|| !ResolveProjectileTargetLocation(Snapshot, OutTargetLocation))
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EnemyProjectileLineOfSight), false, this);
	QueryParams.AddIgnoredActor(this);
	if (IsValid(EquippedWeapon))
	{
		QueryParams.AddIgnoredActor(EquippedWeapon);
	}

	FHitResult HitResult;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, OutSpawnLocation, OutTargetLocation,
		ECC_Visibility, QueryParams);
	const bool bHasLineOfSight = !bHit || HitResult.GetActor() == ChasingTarget;
	if (FDebugDrawHelper::IsRangesEnabled())
	{
		DrawDebugLine(GetWorld(), OutSpawnLocation, OutTargetLocation,
			bHasLineOfSight ? FColor::Green : FColor::Red, false, 0.1f, 0, 1.5f);
	}

	return bHasLineOfSight;
}

bool AEnemy::IsProjectileAttackWithinStartRange(const FActiveProjectileAttack& Snapshot,
	float DistanceToTarget) const
{
	return Snapshot.IsValid()
		&& DistanceToTarget >= Snapshot.MinDistance
		&& DistanceToTarget <= Snapshot.MaxDistance;
}

bool AEnemy::IsProjectileAttackWithinCommittedReleaseRange(const FActiveProjectileAttack& Snapshot,
	float DistanceToTarget) const
{
	if (!Snapshot.IsValid())
	{
		return false;
	}

	// Commit 后的箭以 InitialSpeed 匀速飞行至寿命结束；AI 起手距离不应让已播放的 Release 变为空动作。
	const float PhysicalTravelRange = Snapshot.DeliveryConfig.InitialSpeed * Snapshot.DeliveryConfig.MaxLifetime;
	return DistanceToTarget <= PhysicalTravelRange;
}

bool AEnemy::HasUnreleasedActiveProjectileAttack() const
{
	return EnemyState == EEnemyState::EES_Attacking
		&& ActiveProjectileAttack.IsValid()
		&& !ActiveProjectileAttack.bDebugProbe
		&& !ActiveProjectileAttack.bReleaseAttempted;
}

bool AEnemy::HasClearActiveProjectileLineOfSight() const
{
	if (!HasUnreleasedActiveProjectileAttack())
	{
		return false;
	}

	FVector SpawnLocation;
	FVector TargetLocation;
	return HasClearProjectileLineOfSight(ActiveProjectileAttack, SpawnLocation, TargetLocation);
}

bool AEnemy::CancelUnreleasedActiveProjectileAttack(float BlendOutTime, const TCHAR* Reason)
{
	if (!HasUnreleasedActiveProjectileAttack())
	{
		return false;
	}

	// 先封锁 Release，再停止 Montage，防止同帧或迟到 Notify 在状态恢复前补射。
	ActiveProjectileAttack.bReleaseAttempted = true;
	UE_LOG(LogTemp, Display, TEXT("%s cancelled unreleased projectile attack because %s."),
		*GetName(), Reason ? Reason : TEXT("the cancellation guard was triggered"));

	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	UAnimMontage* CurrentMontage = EnemyAttackConfig && EnemyAttackConfig->Attacks.IsValidIndex(CurrentAttackIndex)
		? EnemyAttackConfig->Attacks[CurrentAttackIndex].Montage
		: nullptr;
	if (AnimInstance && CurrentMontage && AnimInstance->Montage_IsActive(CurrentMontage))
	{
		AnimInstance->Montage_Stop(BlendOutTime, CurrentMontage);
		return true;
	}

	// 缺失动画实例或 Montage 已结束时仍必须离开攻击态，避免卡在 EES_Attacking。
	CheckCombatTarget();
	return true;
}

void AEnemy::ClearActiveProjectileAttack()
{
	GetWorldTimerManager().ClearTimer(ProjectileReleaseTimer);
	GetWorldTimerManager().ClearTimer(ProjectileAttackEndTimer);
#if !UE_BUILD_SHIPPING
	if (ActiveProjectileAttack.bDebugProbe && !bDebugProbeRetryAfterAttackEnd)
	{
		bRangedDebugProbeActive = false;
		bRangedDebugProbeCompleted = true;
		DebugProbeController.Reset();
	}
	bDebugProbeRetryAfterAttackEnd = false;
#endif
	ActiveProjectileAttack = FActiveProjectileAttack{};
}

void AEnemy::UpdateAttackMotionWarpTarget(const FEnemyAttackEntry& Entry)
{
	/*
	 * 敌人跃进攻击使用出手瞬间的固定 WarpTarget。
	 * Motion Warping NotifyState 只修正这次写入的目标，不持续追踪移动中的玩家。
	 * 这样能保留跳劈/前冲的命中校正，同时避免空中 root motion 变成强吸附。
	 */
	if (!Entry.bUseMotionWarping)
	{
		return;
	}

	if (!MotionWarpingComponent || Entry.WarpTargetName == NAME_None)
	{
		ClearAttackMotionWarpTarget(Entry);
		return;
	}

	if (Entry.MaxWarpDistance <= 0.f)
	{
		ClearAttackMotionWarpTarget(Entry);
		return;
	}

	if (!IsValidCombatTarget(ChasingTarget))
	{
		ClearAttackMotionWarpTarget(Entry);
		return;
	}

	const FVector EnemyLocation = GetActorLocation();
	const FVector TargetLocation = ChasingTarget->GetActorLocation();
	const FVector ToTarget = (TargetLocation - EnemyLocation).GetSafeNormal2D();
	if (ToTarget.IsNearlyZero())
	{
		ClearAttackMotionWarpTarget(Entry);
		return;
	}

	const FVector WarpLocation = TargetLocation - ToTarget * Entry.WarpStopDistance;
	const float WarpDistance = FVector::Dist2D(EnemyLocation, WarpLocation);
	if (WarpDistance > Entry.MaxWarpDistance)
	{
		ClearAttackMotionWarpTarget(Entry);
		return;
	}

	const FRotator WarpRotation = ToTarget.Rotation();
	MotionWarpingComponent->AddOrUpdateWarpTargetFromTransform(
		Entry.WarpTargetName,
		FTransform(WarpRotation, WarpLocation));
}

void AEnemy::ClearAttackMotionWarpTarget(const FEnemyAttackEntry& Entry)
{
	if (!Entry.bUseMotionWarping || !MotionWarpingComponent || Entry.WarpTargetName == NAME_None)
	{
		return;
	}

	MotionWarpingComponent->RemoveWarpTarget(Entry.WarpTargetName);
}

void AEnemy::ValidateEnemyAttackConfig() const
{
	if (!EnemyAttackConfig)
	{
		ValidateArchetypeCombatConfig();
		return;
	}

	if (CombatTooCloseRadius >= CombatPreferredMinRadius
		|| CombatPreferredMinRadius > CombatPreferredMaxRadius)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("%s: Combat spacing requires CombatTooCloseRadius < CombatPreferredMinRadius <= CombatPreferredMaxRadius; current values are %.1f / %.1f / %.1f."),
			*GetName(), CombatTooCloseRadius, CombatPreferredMinRadius, CombatPreferredMaxRadius);
	}

	if (CombatPreferredMaxRadius >= CombatingRadius || CombatingRadius >= ChasingRadius)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("%s: Combat spacing requires CombatPreferredMaxRadius < CombatingRadius < ChasingRadius; current values are %.1f / %.1f / %.1f."),
			*GetName(), CombatPreferredMaxRadius, CombatingRadius, ChasingRadius);
	}

	if (CombatRepositionIntervalMin > CombatRepositionIntervalMax)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("%s: Combat reposition interval requires Min <= Max; current values are %.2f / %.2f."),
			*GetName(), CombatRepositionIntervalMin, CombatRepositionIntervalMax);
	}

	const float CombatPreferredIntervalWidth = CombatPreferredMaxRadius - CombatPreferredMinRadius;
	if (CombatPressMargin <= CombatRepositionAcceptanceRadius
		|| CombatPressMargin >= CombatAttackMaxRadius
		|| CombatPressMargin >= CombatPreferredMaxRadius
		|| CombatPressMargin > CombatPreferredIntervalWidth)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("%s: CombatPressMargin %.1f must be greater than CombatRepositionAcceptanceRadius %.1f, smaller than CombatAttackMaxRadius / CombatPreferredMaxRadius (%.1f / %.1f), and no greater than Preferred interval width %.1f so the press goal stays inside the safe range."),
			*GetName(), CombatPressMargin, CombatRepositionAcceptanceRadius,
			CombatAttackMaxRadius, CombatPreferredMaxRadius, CombatPreferredIntervalWidth);
	}

	for (const FEnemyAttackEntry& Entry : EnemyAttackConfig->Attacks)
	{
		if (Entry.MaxDistance > CombatAttackMaxRadius)
		{
			UE_LOG(LogTemp, Warning,
			       TEXT("%s: Enemy attack entry '%s' MaxDistance %.1f is greater than CombatAttackMaxRadius %.1f; pending intent will clamp execution to CombatAttackMaxRadius."),
				       *GetName(), *Entry.AttackName.ToString(), Entry.MaxDistance, CombatAttackMaxRadius);
		}

	}

	ValidateArchetypeCombatConfig();
}

void AEnemy::ValidatePureProjectileTacticalConfig(float EscapeEnterRadius, float EscapeExitRadius) const
{
	if (!EnemyAttackConfig || !IsPureProjectileAttackProfile())
	{
		return;
	}

	for (const FEnemyAttackEntry& Entry : EnemyAttackConfig->Attacks)
	{
		if (!EnemyAttackConfig->IsEntrySelectable(Entry)
			|| Entry.DeliveryType != EEnemyAttackDeliveryType::Projectile)
		{
			continue;
		}

		const float EffectiveMaxDistance = FMath::Min(Entry.MaxDistance, CombatAttackMaxRadius);
		if (CombatPreferredMinRadius < Entry.MinDistance || CombatPreferredMaxRadius > EffectiveMaxDistance)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("%s: Pure Projectile safe interval %.1f-%.1f must stay inside entry '%s' launch range %.1f-%.1f."),
				*GetName(), CombatPreferredMinRadius, CombatPreferredMaxRadius,
				*Entry.AttackName.ToString(), Entry.MinDistance, EffectiveMaxDistance);
		}

		if (EscapeEnterRadius > Entry.MinDistance)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("%s: Ranged Escape enter radius %.1f is greater than Projectile entry '%s' MinDistance %.1f; the interval between them can start a shot instead of Escape."),
				*GetName(), EscapeEnterRadius, *Entry.AttackName.ToString(), Entry.MinDistance);
		}
	}

	if (EscapeEnterRadius > CombatTooCloseRadius)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("%s: Ranged Escape enter radius %.1f must not exceed CombatTooCloseRadius %.1f; Escape and normal Retreat would overlap."),
			*GetName(), EscapeEnterRadius, CombatTooCloseRadius);
	}

	if (EscapeExitRadius < CombatPreferredMinRadius || EscapeExitRadius > CombatPreferredMaxRadius)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("%s: Ranged Escape exit radius %.1f must lie inside the pure Projectile safe interval %.1f-%.1f."),
			*GetName(), EscapeExitRadius, CombatPreferredMinRadius, CombatPreferredMaxRadius);
	}
}

void AEnemy::WarnNoEnemyAttackCandidate(float DistanceToTarget)
{
	const UWorld* World = GetWorld();
	const float CurrentTime = World ? World->GetTimeSeconds() : 0.f;
	if (CurrentTime - LastAttackConfigWarningTime < 1.f)
	{
		return;
	}

	LastAttackConfigWarningTime = CurrentTime;
	if (!EnemyAttackConfig)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s has no EnemyAttackConfig; enemy attacks are DataAsset-only now."),
		       *GetName());
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("%s has no valid enemy attack candidate at distance %.1f."),
	       *GetName(), DistanceToTarget);
}

bool AEnemy::HasPendingAttack() const
{
	return PendingAttackIndex != INDEX_NONE;
}

void AEnemy::ClearPendingAttack()
{
	PendingAttackIndex = INDEX_NONE;
	PendingAttackStartTime = 0.f;
	bPendingAttackMoveIssued = false;
}

void AEnemy::RollPendingAttackIntent()
{
#if !UE_BUILD_SHIPPING
	if (bRangedDebugProbeActive)
	{
		PendingAttackIndex = 0;
		PendingAttackStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
		bPendingAttackMoveIssued = false;
		return;
	}
#endif

	if (!EnemyAttackConfig)
	{
		WarnNoEnemyAttackCandidate(0.f);
		return;
	}

	const int32 BlockedAttackIndex = GetBlockedPendingAttackIndex();
	PendingAttackIndex = EnemyAttackConfig->ChooseAttackIntentIndex(BlockedAttackIndex);
	PendingAttackStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	bPendingAttackMoveIssued = false;

	if (!EnemyAttackConfig->Attacks.IsValidIndex(PendingAttackIndex))
	{
		if (BlockedAttackIndex == INDEX_NONE)
		{
			WarnNoEnemyAttackCandidate(IsValidCombatTarget(ChasingTarget)
				                           ? FVector::Dist2D(GetActorLocation(), ChasingTarget->GetActorLocation())
				                           : 0.f);
		}
		ClearPendingAttack();
	}
}

bool AEnemy::TryExecutePendingAttack(float DistanceToTarget, float ForwardDot, const FVector& ToTarget)
{
	/*
	 * PendingAttack 保留已抽中的招式意图。
	 * 敌人先尝试移动到该招式自己的距离窗口内，转正后再出手；距离不合法或超时才屏蔽/重抽。
	 * 这样避免每帧按当前距离重新随机选招，导致 AI 在多个招式边界间抖动。
	 */
	if (!HasPendingAttack())
	{
		return false;
	}

	if (!EnemyAttackConfig || !EnemyAttackConfig->Attacks.IsValidIndex(PendingAttackIndex))
	{
#if !UE_BUILD_SHIPPING
		if (bRangedDebugProbeActive && PendingAttackIndex == 0)
		{
			return TryExecuteRangedDebugProbePendingAttack(DistanceToTarget, ForwardDot, ToTarget);
		}
#endif
		ClearPendingAttack();
		return false;
	}

	const FEnemyAttackEntry& Entry = EnemyAttackConfig->Attacks[PendingAttackIndex];
	const float EffectiveMaxDistance = FMath::Min(Entry.MaxDistance, CombatAttackMaxRadius);
	if (IsPendingAttackExpired())
	{
		BlockPendingAttackRetry(PendingAttackIndex);
		ClearPendingAttack();
		return true;
	}

	if (DistanceToTarget > EffectiveMaxDistance)
	{
		HandlePendingAttackPositioning(DistanceToTarget, ToTarget);
		return true;
	}

	if (DistanceToTarget < Entry.MinDistance)
	{
		if (Entry.DeliveryType == EEnemyAttackDeliveryType::Projectile)
		{
			FActiveProjectileAttack PendingSnapshot;
			if (BuildProjectileAttackSnapshot(Entry, PendingSnapshot))
			{
				HandlePendingProjectilePositioning(PendingSnapshot, DistanceToTarget, ToTarget, false);
				return true;
			}
		}

		BlockPendingAttackRetry(PendingAttackIndex);
		ClearPendingAttack();
		return true;
	}

	if (ForwardDot <= AttackAngleThreshold)
	{
		StopEnemyMovementIfPossible();
		CombatMoveDetailDebug = TEXT("PendingOrient");
		return true;
	}

	if (Entry.DeliveryType == EEnemyAttackDeliveryType::Projectile)
	{
		FActiveProjectileAttack PendingSnapshot;
		FVector SpawnLocation;
		FVector TargetLocation;
		if (!BuildProjectileAttackSnapshot(Entry, PendingSnapshot)
			|| !HasClearProjectileLineOfSight(PendingSnapshot, SpawnLocation, TargetLocation))
		{
			if (PendingSnapshot.IsValid())
			{
				HandlePendingProjectilePositioning(PendingSnapshot, DistanceToTarget, ToTarget, true);
			}
			else
			{
				BlockPendingAttackRetry(PendingAttackIndex);
				ClearPendingAttack();
			}
			return true;
		}
	}

	const int32 AttackIndex = PendingAttackIndex;
	return PerformConfiguredAttackByIndex(AttackIndex);
}

void AEnemy::HandlePendingAttackPositioning(float DistanceToTarget, const FVector& ToTarget)
{
	if (!EnemyAttackConfig || !EnemyAttackConfig->Attacks.IsValidIndex(PendingAttackIndex))
	{
		return;
	}

	const FEnemyAttackEntry& Entry = EnemyAttackConfig->Attacks[PendingAttackIndex];
	const float EffectiveMaxDistance = FMath::Min(Entry.MaxDistance, CombatAttackMaxRadius);
	if (DistanceToTarget <= EffectiveMaxDistance)
	{
		StopEnemyMovementIfPossible();
		return;
	}

	if (EnemyController && (!bPendingAttackMoveIssued || EnemyController->GetMoveStatus() != EPathFollowingStatus::Moving))
	{
		ClearCombatRetreatSpeedEase();
		GetCharacterMovement()->MaxWalkSpeed = ChaseSpeed;
		const float AcceptanceRadius = FMath::Max(15.f, EffectiveMaxDistance - CombatPressMargin);
		if (MoveToCombatTarget(AcceptanceRadius))
		{
			bPendingAttackMoveIssued = true;
			CombatMoveDetailDebug = FString::Printf(TEXT("PendingPress:%s"),
				*Entry.AttackName.ToString());
		}
	}
}

bool AEnemy::IsPendingAttackExpired() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	return HasPendingAttack() && World->GetTimeSeconds() - PendingAttackStartTime >= PendingAttackTimeout;
}

void AEnemy::BlockPendingAttackRetry(int32 AttackIndex)
{
	if (AttackIndex == INDEX_NONE || PendingAttackRetryBlockDuration <= 0.f)
	{
		return;
	}

	LastBlockedPendingAttackIndex = AttackIndex;
	PendingAttackRetryBlockUntil = GetWorld()
		                               ? GetWorld()->GetTimeSeconds() + PendingAttackRetryBlockDuration
		                               : PendingAttackRetryBlockDuration;
}

int32 AEnemy::GetBlockedPendingAttackIndex() const
{
	const UWorld* World = GetWorld();
	if (!World || LastBlockedPendingAttackIndex == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	return World->GetTimeSeconds() < PendingAttackRetryBlockUntil
		       ? LastBlockedPendingAttackIndex
		       : INDEX_NONE;
}

void AEnemy::StartCurrentAttackCooldownIfNeeded()
{
	if (bCurrentAttackCooldownStarted)
	{
		return;
	}

	if (ActiveProjectileAttack.IsValid())
	{
		if (ActiveProjectileAttack.bDebugProbe)
		{
			return;
		}

		bCurrentAttackCooldownStarted = true;
		StartAttackCooldown(ActiveProjectileAttack.MinCooldown, ActiveProjectileAttack.MaxCooldown);
		return;
	}

	if (!EnemyAttackConfig || !EnemyAttackConfig->Attacks.IsValidIndex(CurrentAttackIndex))
	{
		return;
	}

	const FEnemyAttackEntry& Entry = EnemyAttackConfig->Attacks[CurrentAttackIndex];
	bCurrentAttackCooldownStarted = true;
	StartAttackCooldown(Entry.MinCooldown, Entry.MaxCooldown);
}

FString AEnemy::GetPendingAttackDebugString() const
{
	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;
	const int32 BlockedIndex = GetBlockedPendingAttackIndex();
	FString RetryText;
	if (EnemyAttackConfig && EnemyAttackConfig->Attacks.IsValidIndex(BlockedIndex))
	{
		const FEnemyAttackEntry& BlockedEntry = EnemyAttackConfig->Attacks[BlockedIndex];
		RetryText = FString::Printf(TEXT(" Blocked:%s %.1fs"),
			*BlockedEntry.AttackName.ToString(),
			FMath::Max(0.f, PendingAttackRetryBlockUntil - Now));
	}

	if (!EnemyAttackConfig || !EnemyAttackConfig->Attacks.IsValidIndex(PendingAttackIndex))
	{
		return RetryText.IsEmpty() ? TEXT("Pending:None") : FString::Printf(TEXT("Pending:None%s"), *RetryText);
	}

	const FEnemyAttackEntry& Entry = EnemyAttackConfig->Attacks[PendingAttackIndex];
	const float RemainingTime = World ? FMath::Max(0.f, PendingAttackTimeout - (Now - PendingAttackStartTime)) : 0.f;
	return FString::Printf(TEXT("Pending:%s %.0f-%.0f Timeout:%.1fs%s"),
		*Entry.AttackName.ToString(),
		Entry.MinDistance,
		Entry.MaxDistance,
		RemainingTime,
		*RetryText);
}

float AEnemy::GetAttackCooldownRemaining() const
{
	const UWorld* World = GetWorld();
	return World ? FMath::Max(0.f, GetWorldTimerManager().GetTimerRemaining(AttackCooldownTimer)) : 0.f;
}

// ==================== 蒙太奇回调 ====================

void AEnemy::OnHitReactEnd()
{
	if (bEncounterDormant)
	{
		return;
	}

	// 只有在硬直状态下才恢复，防止覆盖了死亡状态
	if (EnemyState == EEnemyState::EES_Stunned)
	{
		CheckCombatTarget();
	}
}

void AEnemy::OnAttackEnd()
{
	if (bEncounterDormant)
	{
		return;
	}

	// 只有在攻击状态下才恢复，防止覆盖了受击状态或死亡状态
	if (EnemyState == EEnemyState::EES_Attacking)
	{
		CheckCombatTarget();
	}
}

void AEnemy::TryReleaseConfiguredProjectileAttack()
{
	const bool bCurrentAttackIsProjectile = ActiveProjectileAttack.bDebugProbe || (EnemyAttackConfig
		&& EnemyAttackConfig->Attacks.IsValidIndex(CurrentAttackIndex)
		&& EnemyAttackConfig->Attacks[CurrentAttackIndex].DeliveryType == EEnemyAttackDeliveryType::Projectile);
	if (IsActorBeingDestroyed() || bEncounterDormant || EnemyState != EEnemyState::EES_Attacking || !bCurrentAttackIsProjectile
		|| !ActiveProjectileAttack.IsValid()
		|| ActiveProjectileAttack.bReleaseAttempted || !IsValidCombatTarget(ChasingTarget))
	{
		return;
	}

#if !UE_BUILD_SHIPPING
	if (ActiveProjectileAttack.bDebugProbe && !DebugProbeController.IsValid())
	{
		UE_LOG(LogTemp, Display, TEXT("%s rejected debug projectile release because its temporary controller is no longer valid."),
			*GetName());
		return;
	}
	if (ActiveProjectileAttack.bDebugProbe && GetController() != DebugProbeController.Get())
	{
		UE_LOG(LogTemp, Display, TEXT("%s rejected debug projectile release because its temporary controller no longer possesses it."),
			*GetName());
		return;
	}
#endif

	// 一个攻击只有一次 Release 尝试；重复或迟到 Notify 不能在位置重新有效后补射。
	ActiveProjectileAttack.bReleaseAttempted = true;

	const float DistanceToTarget = FVector::Dist2D(GetActorLocation(), ChasingTarget->GetActorLocation());
	// 起手距离只控制攻击选择；已承诺的 Release 允许目标短暂脱离 AI 射程，但不能超过箭矢物理飞行距离。
	if (!IsProjectileAttackWithinCommittedReleaseRange(ActiveProjectileAttack, DistanceToTarget))
	{
		const float PhysicalTravelRange = ActiveProjectileAttack.DeliveryConfig.InitialSpeed
			* ActiveProjectileAttack.DeliveryConfig.MaxLifetime;
		UE_LOG(LogTemp, Warning,
			TEXT("%s rejected projectile release beyond its physical flight range %.1f cm at %.1f cm."),
			*GetName(), PhysicalTravelRange, DistanceToTarget);
		return;
	}

	FVector SpawnLocation;
	FVector TargetLocation;
	if (!HasClearProjectileLineOfSight(ActiveProjectileAttack, SpawnLocation, TargetLocation))
	{
		UE_LOG(LogTemp, Display, TEXT("%s rejected projectile release because LOS is blocked."), *GetName());
		return;
	}

	const FVector LaunchDirection = (TargetLocation - SpawnLocation).GetSafeNormal();
	if (LaunchDirection.IsNearlyZero())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s rejected projectile release because launch direction is zero."), *GetName());
		return;
	}

	FProjectileLaunchParams LaunchParams;
	LaunchParams.Attacker = this;
	LaunchParams.EventInstigator = GetController();
	LaunchParams.SpawnLocation = SpawnLocation;
	LaunchParams.LaunchDirection = LaunchDirection;
	LaunchParams.bOverrideDeliveryConfig = true;
	LaunchParams.DeliveryConfigOverride = ActiveProjectileAttack.DeliveryConfig;

	if (ACombatProjectile* Projectile = ACombatProjectile::SpawnConfiguredProjectile(
		GetWorld(), ActiveProjectileAttack.ProjectileClass, LaunchParams))
	{
		ActiveProjectileAttack.bReleaseSucceeded = true;
		UE_LOG(LogTemp, Display, TEXT("%s released projectile '%s'."), *GetName(), *Projectile->GetName());
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("%s failed to spawn its configured projectile."), *GetName());
}

void AEnemy::OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (bEncounterDormant)
	{
		return;
	}

	if (EnemyState == EEnemyState::EES_Attacking)
	{
		CheckCombatTarget();
	}
}

void AEnemy::OnHitReactMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (bEncounterDormant || bInterrupted) return;
	if (EnemyState == EEnemyState::EES_Stunned)
	{
		CheckCombatTarget();
	}
}

void AEnemy::OnAttackCooldownEnd()
{
	if (bEncounterDormant)
	{
		return;
	}

	bAttackOnCooldown = false;
	if (HandleArchetypeAttackCooldownEnded())
	{
		return;
	}

	// CoordinatedWaiting 的 Timer 到期后必须解除旧子状态，下一帧才能重新建立等待 Timer。
	if (CombatSubState == EEnemyCombatSubState::CoordinatedWaiting)
	{
		CombatSubState = EEnemyCombatSubState::None;
	}

	// 非战斗态或目标无效时只清冷却状态，不打断当前导航（避免打断 Chasing MoveTo）。
	if (EnemyState != EEnemyState::EES_Combating || !IsValidCombatTarget(ChasingTarget))
	{
		return;
	}

	// Timer 回调不自行判断距离、LOS、朝向或攻击协调。OnCombating() 以原型优先级 -> AttackIntent -> MovementIntent 的唯一顺序重评估。
	ResetCombatReposition();
}

// ==================== 战斗局部 HFSM ====================

AEnemy::EEnemyCombatSubState AEnemy::EvaluateCombatSubState(float DistanceToTarget, float ForwardDot, float& OutAllySuggestedWaitTime) const
{
	if (bAttackOnCooldown)
	{
		if (CombatSubState == EEnemyCombatSubState::CoordinatedWaiting)
		{
			return EEnemyCombatSubState::CoordinatedWaiting;
		}
		return EEnemyCombatSubState::CooldownSpacing;
	}

	if (IsAllyAttackingNearby(OutAllySuggestedWaitTime))
	{
		return EEnemyCombatSubState::CoordinatedWaiting;
	}

	if (ShouldTriggerAttack(DistanceToTarget, ForwardDot))
	{
		return EEnemyCombatSubState::None;
	}

	if (DistanceToTarget <= CombatAttackMaxRadius)
	{
		return EEnemyCombatSubState::Orienting;
	}

	return EEnemyCombatSubState::AttackReadyPressing;
}

void AEnemy::SetCombatSubState(EEnemyCombatSubState NewSubState, float AllySuggestedWaitTime)
{
	if (CombatSubState == NewSubState) return;

	const EEnemyCombatSubState OldSubState = CombatSubState;
	if (OldSubState == EEnemyCombatSubState::CooldownSpacing || OldSubState == EEnemyCombatSubState::CoordinatedWaiting)
	{
		ClearCombatRetreatSpeedEase();
	}

	CombatSubState = NewSubState;
	CombatMoveDetailDebug.Empty();

	if (CombatSubState == EEnemyCombatSubState::Orienting)
	{
		StopEnemyMovementIfPossible();
	}
	else if (CombatSubState == EEnemyCombatSubState::CoordinatedWaiting)
	{
		float NewCooldown = FMath::Clamp(AllySuggestedWaitTime, 0.1f, MaxAttackCoordinationWait);
		GetWorldTimerManager().SetTimer(AttackCooldownTimer, this, &AEnemy::OnAttackCooldownEnd, NewCooldown, false);
		bAttackOnCooldown = true;
	}
}

void AEnemy::TickCombatFacing(float DeltaTime, const FVector& ToTarget)
{
	FRotator TargetRot = ToTarget.Rotation();
	SetActorRotation(FMath::RInterpTo(GetActorRotation(), TargetRot, DeltaTime, CombatRotationSpeed));
}

void AEnemy::TickActiveProjectileAttackFacing(float DeltaTime)
{
	if (!ActiveProjectileAttack.IsValid() || !IsValidCombatTarget(ChasingTarget))
	{
		return;
	}

	const FVector ToTarget = (ChasingTarget->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	if (!ToTarget.IsNearlyZero())
	{
		TickCombatFacing(DeltaTime, ToTarget);
	}
}

void AEnemy::TickCombatSubState(float DeltaTime, EEnemyCombatSubState SubState, float DistanceToTarget, const FVector& ToTarget, float AllySuggestedWaitTime)
{
	switch (SubState)
	{
	case EEnemyCombatSubState::Orienting:
		break;
	case EEnemyCombatSubState::AttackReadyPressing:
		HandleAttackReadyPositioning(DistanceToTarget, ToTarget);
		break;
	case EEnemyCombatSubState::CoordinatedWaiting:
	case EEnemyCombatSubState::CooldownSpacing:
		HandleCooldownPositioning(DeltaTime, DistanceToTarget, ToTarget);
		break;
	default:
		break;
	}
}

FString AEnemy::GetCombatSubStateDebugText() const
{
	FString StateStr;
	switch (CombatSubState)
	{
	case EEnemyCombatSubState::None: StateStr = TEXT("Ready"); break;
	case EEnemyCombatSubState::Orienting: StateStr = TEXT("Orienting"); break;
	case EEnemyCombatSubState::AttackReadyPressing: StateStr = TEXT("Pressing"); break;
	case EEnemyCombatSubState::CoordinatedWaiting: StateStr = TEXT("WaitAlly"); break;
	case EEnemyCombatSubState::CooldownSpacing: StateStr = TEXT("Cooldown"); break;
	}

	if (!CombatMoveDetailDebug.IsEmpty())
	{
		return FString::Printf(TEXT("%s [%s]"), *StateStr, *CombatMoveDetailDebug);
	}
	return StateStr;
}

// ==================== 状态机 ====================

void AEnemy::CheckCombatTarget()
{
	if (bEncounterDormant)
	{
		return;
	}

	if (!IsValidCombatTarget(ChasingTarget))
	{
		ClearPendingAttack();
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

	const EEnemyState OldState = EnemyState;

	// --- 退出旧状态的逻辑 ---
	if (OldState == EEnemyState::EES_StanceBreak && OldState != NewState)
	{
		ClearStanceBreakMontagePresentation(true);
	}
	if (EnemyState == EEnemyState::EES_Patrolling || EnemyState == EEnemyState::EES_Searching)
	{
		ClearPatrolTimers();
		bSearchingLostTarget = false;
	}
	if ((OldState == EEnemyState::EES_Combating || OldState == EEnemyState::EES_Attacking)
		&& OldState != NewState)
	{
		ClearArchetypeCombatState();
	}
	if (OldState == EEnemyState::EES_Attacking && NewState != EEnemyState::EES_Attacking)
	{
		ClearCurrentAttackConfig(NewState != EEnemyState::EES_Dead);
	}
	if (OldState == EEnemyState::EES_Combating && NewState != EEnemyState::EES_Combating)
	{
		CombatSubState = EEnemyCombatSubState::None;
	}
	if (NewState != EEnemyState::EES_Combating && NewState != EEnemyState::EES_Attacking)
	{
		ClearPendingAttack();
		if (NewState == EEnemyState::EES_Dead)
		{
			LastBlockedPendingAttackIndex = INDEX_NONE;
			PendingAttackRetryBlockUntil = 0.f;
		}
	}

	// --- 切换状态 ---
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
		GetCharacterMovement()->MaxWalkSpeed = PatrolSpeed;
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
		SetCombatSubState(EEnemyCombatSubState::None);
		break;
	}
	case EEnemyState::EES_Attacking:
	case EEnemyState::EES_Stunned:
	case EEnemyState::EES_StanceBreak:
		StopEnemyMovementIfPossible();
		bRepositionInProgress = false;
		SetCombatSubState(EEnemyCombatSubState::None);
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
	if (bEncounterDormant)
	{
		return;
	}

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
	ChasingTarget = Actor;
}

// ==================== AI Tick ====================

void AEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bEncounterDormant)
	{
		return;
	}

	if (EnemyState == EEnemyState::EES_Attacking)
	{
		// 只有远程攻击在蒙太奇期间持续转向；近战维持既有攻击锁定朝向。
		TickActiveProjectileAttackFacing(DeltaTime);
		TickArchetypeAttack(DeltaTime);
		DrawDebugInfo();
		return;
	}

	if (EnemyState == EEnemyState::EES_Dead || EnemyState == EEnemyState::EES_Stunned
		|| EnemyState == EEnemyState::EES_StanceBreak)
	{
		DrawDebugInfo();
		return;
	}

	DrawDebugInfo();

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

void AEnemy::DrawDebugInfo() const
{
	// 受击/死亡保持简洁；攻击态额外保留原型调试，便于确认远程 LOS 取消的运行时状态。
	const bool bTickBlockedState = EnemyState == EEnemyState::EES_Dead || EnemyState == EEnemyState::EES_Stunned
		|| EnemyState == EEnemyState::EES_Attacking || EnemyState == EEnemyState::EES_StanceBreak;
	if (bTickBlockedState)
	{
		if (FDebugDrawHelper::IsEnemyEnabled())
		{
			if (EnemyState == EEnemyState::EES_StanceBreak)
			{
				FDebugDrawHelper::Add(TEXT("BREAK"), FColor::Red);
			}
			else if (EnemyState == EEnemyState::EES_Attacking)
			{
				const float MaxWalkSpeed = GetCharacterMovement() ? GetCharacterMovement()->MaxWalkSpeed : 0.f;
				FDebugDrawHelper::Add(FString::Printf(TEXT("EnemyState: %s | Speed: %.0f / Max: %.0f"),
					*UEnum::GetValueAsString(EnemyState), GroundSpeed, MaxWalkSpeed), FColor::White);

				const FString ArchetypeCombatDebugText = GetArchetypeCombatDebugText();
				if (!ArchetypeCombatDebugText.IsEmpty())
				{
					FDebugDrawHelper::Add(FString::Printf(TEXT("Combat: %s"), *ArchetypeCombatDebugText), FColor::Cyan);
				}
			}
		}
		return;
	}

	if (FDebugDrawHelper::IsEnemyEnabled())
	{
		// TODO: 多敌人时调试文字会混在一起，加 GetName() 或编号区分。
		const float MaxWalkSpeed = GetCharacterMovement() ? GetCharacterMovement()->MaxWalkSpeed : 0.f;
		FDebugDrawHelper::Add(FString::Printf(TEXT("EnemyState: %s | Speed: %.0f / Max: %.0f"),
			*UEnum::GetValueAsString(EnemyState), GroundSpeed, MaxWalkSpeed), FColor::White);

		// 在头顶渲染血量和韧性（解决多敌人文字混在一起的问题）
		if (Attributes)
		{
			FString OverheadText = FString::Printf(TEXT("HP: %.0f/%.0f | Poise: %.1f/%.1f"), 
				Attributes->GetCurrentHealth(), Attributes->GetMaxHealth(), CurrentPoise, MaxPoise);
			DrawDebugString(GetWorld(), GetActorLocation() + FVector(0.f, 0.f, 120.f), OverheadText, nullptr, FColor::White, 0.f, true);
		}

		if (ChasingTarget)
		{
			const float Dist = FVector::Dist2D(GetActorLocation(), ChasingTarget->GetActorLocation());
			FDebugDrawHelper::Add(FString::Printf(TEXT("Dist: %.0f / %.0f"), Dist, ChasingRadius),
				Dist <= CombatingRadius ? FColor::Red : Dist <= ChasingRadius ? FColor::Yellow : FColor::White);
		}

		if (EnemyState == EEnemyState::EES_Combating)
		{
			const FString ArchetypeCombatDebugText = GetArchetypeCombatDebugText();
			const FString CombatDebugText = ArchetypeCombatDebugText.IsEmpty()
				? GetCombatSubStateDebugText()
				: FString::Printf(TEXT("%s | %s"), *GetCombatSubStateDebugText(), *ArchetypeCombatDebugText);
			FDebugDrawHelper::Add(FString::Printf(TEXT("CombatMove: %s"), *CombatDebugText), FColor::Cyan);
			FDebugDrawHelper::Add(FString::Printf(TEXT("AttackPlan: %s | CD: %.1f"),
				*GetPendingAttackDebugString(), GetAttackCooldownRemaining()), FColor::Orange);
		}
	}

	if (ChasingTarget)
	{
		FDebugDrawHelper::AddSphere(GetWorld(), GetActorLocation(), ChasingRadius, FColor::Yellow);
		FDebugDrawHelper::AddSphere(GetWorld(), GetActorLocation(), CombatingRadius, FColor(255, 165, 0), 16);
		FDebugDrawHelper::AddSphere(GetWorld(), GetActorLocation(), CombatAttackMaxRadius, FColor::Red, 16);
	}


	DrawArchetypeCombatDebug();
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
	if (bEncounterDormant)
	{
		return;
	}

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

	// 已承诺的攻击由 Tick() 的 EES_Attacking 分支独占。本处的仲裁顺序固定为原型优先级 -> AttackIntent -> MovementIntent。
	if (HandleArchetypeCombatPriority(DeltaTime, DistanceToTarget, ToTarget))
	{
		return;
	}

	TickCombatFacing(DeltaTime, ToTarget);
	UpdateCombatRetreatSpeedEase();

	float AllyWaitTime = 0.f;
	EEnemyCombatSubState NewSubState = EvaluateCombatSubState(DistanceToTarget, Dot, AllyWaitTime);

	if (NewSubState == EEnemyCombatSubState::CooldownSpacing
		|| NewSubState == EEnemyCombatSubState::CoordinatedWaiting)
	{
		ClearPendingAttack();
		SetCombatSubState(NewSubState, AllyWaitTime);
		TickCombatSubState(DeltaTime, CombatSubState, DistanceToTarget, ToTarget, AllyWaitTime);
		return;
	}

	// AttackIntent 可以占用本 Tick，并按其条目距离/LOS 驱动自身的定位；只有它未占用时才落到通用 MovementIntent。
	if (HasPendingAttack())
	{
		SetCombatSubState(EEnemyCombatSubState::None, 0.f);
		if (TryExecutePendingAttack(DistanceToTarget, Dot, ToTarget))
		{
			return;
		}
	}

	RollPendingAttackIntent();
	if (HasPendingAttack())
	{
		SetCombatSubState(EEnemyCombatSubState::None, 0.f);
		if (TryExecutePendingAttack(DistanceToTarget, Dot, ToTarget))
		{
			return;
		}
	}

	if (GetBlockedPendingAttackIndex() != INDEX_NONE)
	{
		if (NewSubState == EEnemyCombatSubState::None)
		{
			StopEnemyMovementIfPossible();
			CombatMoveDetailDebug = TEXT("IntentBlocked");
			return;
		}
		SetCombatSubState(NewSubState, AllyWaitTime);
		TickCombatSubState(DeltaTime, CombatSubState, DistanceToTarget, ToTarget, AllyWaitTime);
		return;
	}

	if (NewSubState == EEnemyCombatSubState::None)
	{
		SetCombatSubState(EEnemyCombatSubState::None, 0.f);
		Attack();
		return;
	}

	SetCombatSubState(NewSubState, AllyWaitTime);
	TickCombatSubState(DeltaTime, CombatSubState, DistanceToTarget, ToTarget, AllyWaitTime);
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
	return BuildCombatMovePlanForRange(DistanceToTarget, ToTarget, CombatTooCloseRadius,
		CombatPreferredMinRadius, CombatPreferredMaxRadius, false);
}

AEnemy::FEnemyCombatMovePlan AEnemy::BuildCombatMovePlanForRange(float DistanceToTarget, const FVector& ToTarget,
	float TooCloseRadius, float PreferredMinRadius, float PreferredMaxRadius, bool bForceStrafe) const
{
	FEnemyCombatMovePlan Plan;
	if (!ChasingTarget)
	{
		return Plan;
	}

	Plan.MoveSpeed = CombatManeuverSpeed;

	const FVector Right = FVector::CrossProduct(FVector::UpVector, ToTarget).GetSafeNormal2D();
	const float SideDir = FMath::RandBool() ? 1.f : -1.f;

	if (DistanceToTarget < TooCloseRadius)
	{
		// 后撤
		const float TargetDist = FMath::FRandRange(PreferredMinRadius, PreferredMaxRadius);
		Plan.MoveType = EEnemyCombatMoveType::Retreat;
		Plan.GoalLocation = ChasingTarget->GetActorLocation() - ToTarget * TargetDist;
		Plan.bUseRetreatSpeedEase = true;
	}
	else if (DistanceToTarget < PreferredMinRadius)
	{
		// 斜后撤
		const float TargetDist = FMath::FRandRange(PreferredMinRadius, PreferredMaxRadius);
		const FVector BackOffset = -ToTarget * TargetDist;
		const FVector SideOffset = Right * SideDir * TargetDist * 0.3f;
		Plan.MoveType = EEnemyCombatMoveType::BackDiag;
		Plan.GoalLocation = ChasingTarget->GetActorLocation() + BackOffset + SideOffset;
		Plan.bUseRetreatSpeedEase = true;
	}
	else if (bForceStrafe || DistanceToTarget <= PreferredMaxRadius)
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
		const float TargetDist = PreferredMaxRadius - CombatPressMargin;
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
	ExecuteCombatMovePlan(MovePlan, Now);
}

bool AEnemy::ExecuteCombatMovePlan(const FEnemyCombatMovePlan& MovePlan, float CurrentTime,
	FAIRequestID* OutMoveRequestId)
{
	if (!MovePlan.IsValid())
	{
		return false;
	}

	ClearCombatRetreatSpeedEase();
	GetCharacterMovement()->MaxWalkSpeed = MovePlan.MoveSpeed;
	CombatMoveDetailDebug = GetCombatMoveDebugName(MovePlan.MoveType);

	if (MoveToCombatLocation(MovePlan.GoalLocation, OutMoveRequestId))
	{
		if (MovePlan.bUseRetreatSpeedEase)
		{
			StartCombatRetreatSpeedEase(MovePlan.GoalLocation);
		}
		// 成功：正常节奏间隔
		const float Interval = FMath::FRandRange(CombatRepositionIntervalMin, CombatRepositionIntervalMax);
		NextCombatRepositionTime = CurrentTime + Interval;
		return true;
	}

	// 失败：短间隔重试
	NextCombatRepositionTime = CurrentTime + MovePlan.RetryDelay;
	return false;
}

void AEnemy::HandlePendingProjectilePositioning(const FActiveProjectileAttack& Snapshot, float DistanceToTarget,
	const FVector& ToTarget, bool bForceStrafeForLineOfSight, float PreferredMinRadius, float PreferredMaxRadius)
{
	if (!Snapshot.IsValid() || bRepositionInProgress)
	{
		return;
	}

	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;
	if (Now < NextCombatRepositionTime)
	{
		return;
	}

	const float EffectivePreferredMinRadius = PreferredMinRadius >= Snapshot.MinDistance
		? PreferredMinRadius
		: FMath::Max(Snapshot.MinDistance, CombatPreferredMinRadius);
	const float EffectivePreferredMaxRadius = PreferredMaxRadius >= EffectivePreferredMinRadius
		? PreferredMaxRadius
		: FMath::Max(EffectivePreferredMinRadius, CombatPreferredMaxRadius);
	const FEnemyCombatMovePlan MovePlan = BuildCombatMovePlanForRange(DistanceToTarget, ToTarget,
		Snapshot.MinDistance, EffectivePreferredMinRadius, EffectivePreferredMaxRadius, bForceStrafeForLineOfSight);
	if (ExecuteCombatMovePlan(MovePlan, Now))
	{
		CombatMoveDetailDebug = bForceStrafeForLineOfSight ? TEXT("ProjectileLOSReposition")
			: FString::Printf(TEXT("Projectile%s"), GetCombatMoveDebugName(MovePlan.MoveType));
	}
}

#if !UE_BUILD_SHIPPING
void AEnemy::OnDebugProjectileReleaseTimerElapsed()
{
	TryReleaseConfiguredProjectileAttack();
}

void AEnemy::OnDebugProjectileAttackEndTimerElapsed()
{
	const bool bReleaseSucceeded = ActiveProjectileAttack.bReleaseSucceeded;
	bDebugProbeRetryAfterAttackEnd = !bReleaseSucceeded;
	OnAttackEnd();
	if (bReleaseSucceeded)
	{
		SetLifeSpan(0.1f);
	}
}

void AEnemy::StartDebugProjectileAttack(FActiveProjectileAttack&& Snapshot)
{
	if (!Snapshot.IsValid() || bEncounterDormant || EnemyState != EEnemyState::EES_Combating)
	{
		return;
	}

	ActiveProjectileAttack = MoveTemp(Snapshot);
	SetEnemyState(EEnemyState::EES_Attacking);
	GetWorldTimerManager().SetTimer(ProjectileReleaseTimer, this,
		&AEnemy::OnDebugProjectileReleaseTimerElapsed, DebugProbeReleaseDelay, false);
	GetWorldTimerManager().SetTimer(ProjectileAttackEndTimer, this,
		&AEnemy::OnDebugProjectileAttackEndTimerElapsed, DebugProbeReleaseDelay + 0.15f, false);
}

void AEnemy::PrepareRangedDebugProbeSpawn()
{
	bIsRangedDebugProbeInstance = true;
	AutoPossessAI = EAutoPossessAI::Disabled;
}

bool AEnemy::StartRangedDebugProbe(AActor* Target, float ReleaseDelay, AAIController* ProbeController)
{
	if (!bIsRangedDebugProbeInstance || !IsValidCombatTarget(Target) || !IsValid(ProbeController) || ReleaseDelay < 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s could not initialize EnemyRangedDebugProbe."), *GetName());
		return false;
	}

	bRangedDebugProbeActive = true;
	bRangedDebugProbeCompleted = false;
	DebugProbeReleaseDelay = ReleaseDelay;
	DebugProbePendingStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	DebugProbeController = ProbeController;
	ChasingTarget = Target;
	Tags.AddUnique(FName(TEXT("Enemy")));
	CombatAttackMaxRadius = 1100.f;
	CombatPreferredMinRadius = 700.f;
	CombatPreferredMaxRadius = 900.f;
	CombatingRadius = 1200.f;
	ChasingRadius = 1600.f;
	SetEnemyState(EEnemyState::EES_Combating);
	UE_LOG(LogTemp, Display, TEXT("EnemyRangedDebugProbe '%s' initialized with ReleaseDelay %.2f."),
		*GetName(), ReleaseDelay);
	return true;
}

bool AEnemy::TryExecuteRangedDebugProbePendingAttack(float DistanceToTarget, float ForwardDot, const FVector& ToTarget)
{
	if (!bRangedDebugProbeActive || bRangedDebugProbeCompleted || EnemyState != EEnemyState::EES_Combating
		|| !DebugProbeController.IsValid() || GetController() != DebugProbeController.Get())
	{
		if (bRangedDebugProbeActive && (!DebugProbeController.IsValid()
			|| GetController() != DebugProbeController.Get()))
		{
			bRangedDebugProbeActive = false;
			bRangedDebugProbeCompleted = true;
		}
		ClearPendingAttack();
		return true;
	}

	FActiveProjectileAttack Snapshot;
	if (!BuildDebugProbeProjectileSnapshot(Snapshot))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s EnemyRangedDebugProbe could not build a projectile snapshot."), *GetName());
		bRangedDebugProbeCompleted = true;
		SetLifeSpan(0.1f);
		ClearPendingAttack();
		return true;
	}

	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;
	if (Now - DebugProbePendingStartTime >= PendingAttackTimeout)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s EnemyRangedDebugProbe timed out before a valid release."), *GetName());
		BlockPendingAttackRetry(PendingAttackIndex);
		ClearPendingAttack();
		bRangedDebugProbeCompleted = true;
		SetLifeSpan(0.1f);
		return true;
	}

	if (DistanceToTarget > Snapshot.MaxDistance || DistanceToTarget < Snapshot.MinDistance)
	{
		HandlePendingProjectilePositioning(Snapshot, DistanceToTarget, ToTarget, false, 700.f, 900.f);
		return true;
	}

	if (ForwardDot <= AttackAngleThreshold)
	{
		StopEnemyMovementIfPossible();
		CombatMoveDetailDebug = TEXT("ProbeOrient");
		return true;
	}

	FVector SpawnLocation;
	FVector TargetLocation;
	if (!HasClearProjectileLineOfSight(Snapshot, SpawnLocation, TargetLocation))
	{
		HandlePendingProjectilePositioning(Snapshot, DistanceToTarget, ToTarget, true, 700.f, 900.f);
		return true;
	}

	StartDebugProjectileAttack(MoveTemp(Snapshot));
	ClearPendingAttack();
	return true;
}
#endif

bool AEnemy::MoveToCombatLocation(const FVector& Location, FAIRequestID* OutMoveRequestId)
{
	if (bEncounterDormant || !EnemyController) return false;

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
		if (OutMoveRequestId)
		{
			*OutMoveRequestId = Result.MoveId;
		}
		bRepositionInProgress = true;
		return true;
	}

	if (Result.Code == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		CombatMoveDetailDebug = TEXT("AlreadyAtGoal");
	}
	else
	{
		CombatMoveDetailDebug = TEXT("MoveFail");
	}
	return false;
}

bool AEnemy::MoveToCombatTarget(float AcceptanceRadiusOverride)
{
	if (bEncounterDormant || !EnemyController || !ChasingTarget) return false;

	FAIMoveRequest MoveRequest;
	MoveRequest.SetGoalActor(ChasingTarget);
	const float AcceptanceRadius = AcceptanceRadiusOverride >= 0.f
		                               ? AcceptanceRadiusOverride
		                               : FMath::Max(15.f, CombatAttackMaxRadius - CombatPressMargin);
	MoveRequest.SetAcceptanceRadius(AcceptanceRadius);
	MoveRequest.SetReachTestIncludesAgentRadius(false);
	MoveRequest.SetReachTestIncludesGoalRadius(false);
	MoveRequest.SetUsePathfinding(true);
	MoveRequest.SetAllowPartialPath(true);
	FPathFollowingRequestResult Result = EnemyController->MoveTo(MoveRequest);

	if (Result.Code == EPathFollowingRequestResult::RequestSuccessful
		|| Result.Code == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		CombatMoveDetailDebug = TEXT("AttackPress");
		return true;
	}

	CombatMoveDetailDebug = TEXT("MoveFail");
	return false;
}

void AEnemy::ResetCombatReposition()
{
	bRepositionInProgress = false;
	NextCombatRepositionTime = 0.f;
	ClearCombatRetreatSpeedEase();
	CombatMoveDetailDebug.Empty();
}

void AEnemy::FinishCombatReposition()
{
	bRepositionInProgress = false;
	ClearCombatRetreatSpeedEase();
}

void AEnemy::SetCombatRepositionDelay(float Delay)
{
	const UWorld* World = GetWorld();
	NextCombatRepositionTime = World ? World->GetTimeSeconds() + FMath::Max(0.f, Delay) : 0.f;
}

void AEnemy::SetCombatMoveDebugDetail(const FString& Detail)
{
	CombatMoveDetailDebug = Detail;
}

bool AEnemy::IsPureProjectileAttackProfile() const
{
	if (!EnemyAttackConfig)
	{
		return false;
	}

	bool bHasSelectableProjectile = false;
	for (const FEnemyAttackEntry& Entry : EnemyAttackConfig->Attacks)
	{
		if (!EnemyAttackConfig->IsEntrySelectable(Entry))
		{
			continue;
		}

		if (Entry.DeliveryType == EEnemyAttackDeliveryType::Melee)
		{
			return false;
		}

		if (Entry.DeliveryType == EEnemyAttackDeliveryType::Projectile)
		{
			bHasSelectableProjectile = true;
		}
	}

	return bHasSelectableProjectile;
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
	const float MinSpeed = CombatManeuverSpeed * CombatRetreatMinSpeedRatio;
	GetCharacterMovement()->MaxWalkSpeed = FMath::Lerp(CombatManeuverSpeed, MinSpeed, SlowAlpha);
}

void AEnemy::ClearCombatRetreatSpeedEase()
{
	bRetreatSpeedEaseActive = false;
	RetreatSpeedEaseGoalLocation = FVector::ZeroVector;
	RetreatSpeedEaseTotalDistance = 0.f;
}

void AEnemy::OnRepositionMoveCompleted(FAIRequestID RequestID, EPathFollowingResult::Type Result)
{
	if (bEncounterDormant)
	{
		return;
	}

	if (HandleArchetypeMoveCompleted(RequestID, Result))
	{
		return;
	}

	FinishCombatReposition();
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
	if (bEncounterDormant || !EnemyController || !Target)
	{
		return;
	}

	FAIMoveRequest MoveRequest;
	MoveRequest.SetGoalActor(Target);

	// 追击：中心距到达 CombatAttackMaxRadius - CombatPressMargin 时停下，与前压目标距离对齐。
	// 射手探针的最小射程大于常规近战半径，保持在其有效距离窗口外侧，避免刚生成就贴脸。
	const bool bUseRangedProbeAcceptance =
#if !UE_BUILD_SHIPPING
		bRangedDebugProbeActive && Target == ChasingTarget;
#else
		false;
#endif
	const float StopRadius = Target == ChasingTarget
		? (bUseRangedProbeAcceptance ? 900.f : FMath::Max(15.f, CombatAttackMaxRadius - CombatPressMargin))
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
	if (bEncounterDormant || !EnemyController)
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
	if (LockOnMarkerWidgetComp)
	{
		LockOnMarkerWidgetComp->SetVisibility(bTargeted);
	}

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
	if (bEncounterDormant)
	{
		return;
	}

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
	GetWorldTimerManager().ClearTimer(ProjectileReleaseTimer);
	GetWorldTimerManager().ClearTimer(ProjectileAttackEndTimer);
}

bool AEnemy::IsAllyAttackingNearby(float& OutSuggestedWaitTime) const
{
	OutSuggestedWaitTime = 0.f;

	if (!IsValidCombatTarget(ChasingTarget))
	{
		return false;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const float CurrentTime = World->GetTimeSeconds();
	if (CachedAllyCheckTarget == ChasingTarget
		&& CurrentTime - LastAllyAttackCheckTime < AllyAttackCheckCacheDuration)
	{
		OutSuggestedWaitTime = CachedAllySuggestedWaitTime;
		return bCachedAllyAttackingNearby;
	}

	LastAllyAttackCheckTime = CurrentTime;
	CachedAllyCheckTarget = ChasingTarget;
	bCachedAllyAttackingNearby = false;
	CachedAllySuggestedWaitTime = 0.f;

	TArray<AActor*> NearbyEnemies;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AEnemy::StaticClass(), NearbyEnemies);

	for (AActor* Actor : NearbyEnemies)
	{
		AEnemy* OtherEnemy = Cast<AEnemy>(Actor);
		if (OtherEnemy && OtherEnemy != this)
		{
			float Distance = FVector::Dist(GetActorLocation(), OtherEnemy->GetActorLocation());
			if (Distance <= AttackCoordinationRange
				&& OtherEnemy->GetEnemyState() == EEnemyState::EES_Attacking
				&& OtherEnemy->ChasingTarget == ChasingTarget)
			{
				OutSuggestedWaitTime = AttackCoordinationBuffer;
				CachedAllySuggestedWaitTime = OutSuggestedWaitTime;
				bCachedAllyAttackingNearby = true;
				return true;
			}
		}
	}

	return false;
}
