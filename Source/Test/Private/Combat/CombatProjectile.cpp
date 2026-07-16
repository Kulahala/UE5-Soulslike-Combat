#include "Combat/CombatProjectile.h"

#include "Combat/CombatHitResolver.h"
#include "Combat/CombatHitTypes.h"
#include "Components/SphereComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Interfaces/HitInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Utils/DebugDrawHelper.h"

namespace
{
	// Config/DefaultEngine.ini 的 Projectile Object Channel；投射物必须忽略同类以支持连续发射。
	constexpr ECollisionChannel ProjectileCollisionChannel = ECC_GameTraceChannel1;
}

ACombatProjectile::ACombatProjectile()
{
	PrimaryActorTick.bCanEverTick = false;

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	SetRootComponent(CollisionSphere);
	CollisionSphere->InitSphereRadius(10.f);
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CollisionSphere->SetCollisionObjectType(ProjectileCollisionChannel);
	CollisionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionSphere->SetCollisionResponseToChannel(ProjectileCollisionChannel, ECR_Ignore);
	CollisionSphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	CollisionSphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	CollisionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	CollisionSphere->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
	CollisionSphere->SetCollisionResponseToChannel(ECC_Destructible, ECR_Block);
	CollisionSphere->SetGenerateOverlapEvents(false);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->SetUpdatedComponent(CollisionSphere);
	ProjectileMovement->InitialSpeed = 3000.f;
	ProjectileMovement->MaxSpeed = 3000.f;
	ProjectileMovement->ProjectileGravityScale = 0.f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->bSweepCollision = true;
	ProjectileMovement->SetAutoActivate(false);
}

ACombatProjectile* ACombatProjectile::SpawnConfiguredProjectile(UWorld* World,
	TSubclassOf<ACombatProjectile> ProjectileClass, const FProjectileLaunchParams& LaunchParams)
{
	return SpawnProjectile(World, ProjectileClass, LaunchParams, true);
}

ACombatProjectile* ACombatProjectile::SpawnPreparedProjectile(UWorld* World,
	TSubclassOf<ACombatProjectile> ProjectileClass, const FProjectileLaunchParams& LaunchParams)
{
	return SpawnProjectile(World, ProjectileClass, LaunchParams, false);
}

ACombatProjectile* ACombatProjectile::SpawnProjectile(UWorld* World, TSubclassOf<ACombatProjectile> ProjectileClass,
	const FProjectileLaunchParams& LaunchParams, bool bStartImmediately)
{
	if (!World || !ProjectileClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("Combat projectile spawn failed: World or ProjectileClass is invalid."));
		return nullptr;
	}

	if (!IsValid(LaunchParams.Attacker))
	{
		UE_LOG(LogTemp, Warning, TEXT("Combat projectile spawn failed: Attacker is invalid."));
		return nullptr;
	}

	const FVector NormalizedDirection = LaunchParams.LaunchDirection.GetSafeNormal();
	if (NormalizedDirection.IsNearlyZero())
	{
		UE_LOG(LogTemp, Warning, TEXT("Combat projectile spawn failed: LaunchDirection is zero for attacker '%s'."),
			*GetNameSafe(LaunchParams.Attacker));
		return nullptr;
	}

	const FTransform SpawnTransform(NormalizedDirection.Rotation(), LaunchParams.SpawnLocation);
	APawn* InstigatorPawn = Cast<APawn>(LaunchParams.Attacker);
	ACombatProjectile* Projectile = World->SpawnActorDeferred<ACombatProjectile>(ProjectileClass, SpawnTransform,
		LaunchParams.Attacker, InstigatorPawn, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Projectile)
	{
		UE_LOG(LogTemp, Warning, TEXT("Combat projectile spawn failed for class '%s'."),
			*GetNameSafe(ProjectileClass.Get()));
		return nullptr;
	}

	if (!Projectile->ConfigureLaunch(LaunchParams))
	{
		UGameplayStatics::FinishSpawningActor(Projectile, SpawnTransform);
		return nullptr;
	}

	Projectile->bStartLaunchOnBeginPlay = bStartImmediately;
	UGameplayStatics::FinishSpawningActor(Projectile, SpawnTransform);
	if (!IsValid(Projectile))
	{
		UE_LOG(LogTemp, Warning, TEXT("Combat projectile spawn failed: '%s' was invalid after FinishSpawning."),
			*GetNameSafe(ProjectileClass.Get()));
		return nullptr;
	}

	if (bStartImmediately)
	{
		if (!Projectile->bLaunchActivated || !Projectile->HasValidNativeLaunchState())
		{
			UE_LOG(LogTemp, Warning,
				TEXT("Combat projectile spawn failed: immediate projectile '%s' did not complete its native launch commit."),
				*GetNameSafe(Projectile));
			Projectile->Destroy();
			return nullptr;
		}
	}
	else if (!Projectile->IsPreparedForActivation())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("Combat projectile spawn failed: prepared projectile '%s' did not complete native preparation."),
			*GetNameSafe(Projectile));
		Projectile->Destroy();
		return nullptr;
	}

	return Projectile;
}

void ACombatProjectile::BeginPlay()
{
	Super::BeginPlay();

	if (!bLaunchConfigured || !CollisionSphere || !ProjectileMovement)
	{
		UE_LOG(LogTemp, Warning, TEXT("Combat projectile '%s' began play without a valid native launch configuration."), *GetName());
		Destroy();
		return;
	}

	// Commit 语义由原生碰撞根与移动组件独占；Blueprint 表现不能替换投递 UpdatedComponent。
	ProjectileMovement->SetUpdatedComponent(CollisionSphere);
	CollisionSphere->SetSphereRadius(ActiveDeliveryConfig.CollisionRadius, true);
	CollisionSphere->IgnoreActorWhenMoving(LaunchAttacker, true);
	if (APawn* InstigatorPawn = GetInstigator())
	{
		CollisionSphere->IgnoreActorWhenMoving(InstigatorPawn, true);
	}
	ProjectileMovement->OnProjectileStop.AddDynamic(this, &ACombatProjectile::OnProjectileStopped);
	bNativePreparationComplete = true;

	if (bStartLaunchOnBeginPlay)
	{
		if (!IsPreparedForActivation())
		{
			UE_LOG(LogTemp, Warning, TEXT("Combat projectile '%s' could not complete native preparation before launch."), *GetName());
			Destroy();
			return;
		}

		CommitPreparedLaunch();
	}
}

void ACombatProjectile::CommitPreparedLaunch()
{
	checkf(IsPreparedForActivation(), TEXT("Combat projectile '%s' committed without a complete prepared launch state."), *GetName());

	bLaunchActivated = true;
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ProjectileMovement->InitialSpeed = ActiveDeliveryConfig.InitialSpeed;
	ProjectileMovement->MaxSpeed = ActiveDeliveryConfig.MaxSpeed;
	ProjectileMovement->ProjectileGravityScale = 0.f;
	ProjectileMovement->Velocity = LaunchDirection * ActiveDeliveryConfig.InitialSpeed;
	ProjectileMovement->Activate(true);
	SetLifeSpan(ActiveDeliveryConfig.MaxLifetime);
}

bool ACombatProjectile::IsPreparedForActivation() const
{
	return HasValidNativeLaunchState()
		&& !bLaunchActivated
		&& !bImpactResolved
		&& CollisionSphere->GetCollisionEnabled() == ECollisionEnabled::NoCollision
		&& !ProjectileMovement->IsActive();
}

void ACombatProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ProjectileMovement)
	{
		ProjectileMovement->OnProjectileStop.RemoveDynamic(this, &ACombatProjectile::OnProjectileStopped);
	}

	Super::EndPlay(EndPlayReason);
}

void ACombatProjectile::LifeSpanExpired()
{
	if (!bImpactResolved)
	{
		DrawDebugPath(GetActorLocation(), FColor::Yellow);
		UE_LOG(LogTemp, Display, TEXT("Combat projectile '%s' expired without an impact."), *GetName());
	}

	Super::LifeSpanExpired();
}

bool ACombatProjectile::ConfigureLaunch(const FProjectileLaunchParams& LaunchParams)
{
	const FProjectileDeliveryConfig DeliveryConfig = LaunchParams.bOverrideDeliveryConfig
		? LaunchParams.DeliveryConfigOverride
		: DefaultDeliveryConfig;
	const FVector NormalizedDirection = LaunchParams.LaunchDirection.GetSafeNormal();

	if (!IsValid(LaunchParams.Attacker) || NormalizedDirection.IsNearlyZero() || !DeliveryConfig.IsValid())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("Combat projectile '%s' received invalid launch data: Attacker='%s', Direction=%s, ConfigValid=%s."),
			*GetName(), *GetNameSafe(LaunchParams.Attacker), *LaunchParams.LaunchDirection.ToString(),
			DeliveryConfig.IsValid() ? TEXT("true") : TEXT("false"));
		return false;
	}

	LaunchAttacker = LaunchParams.Attacker;
	LaunchEventInstigator = LaunchParams.EventInstigator
		? LaunchParams.EventInstigator
		: LaunchParams.Attacker->GetInstigatorController();
	ActiveDeliveryConfig = DeliveryConfig;
	LaunchDirection = NormalizedDirection;
	LaunchLocation = LaunchParams.SpawnLocation;
	bLaunchConfigured = true;
	return true;
}

bool ACombatProjectile::HasValidNativeLaunchState() const
{
	return bLaunchConfigured
		&& bNativePreparationComplete
		&& !IsActorBeingDestroyed()
		&& GetWorld()
		&& IsValid(LaunchAttacker)
		&& ActiveDeliveryConfig.IsValid()
		&& !LaunchDirection.IsNearlyZero()
		&& IsValid(CollisionSphere)
		&& IsValid(ProjectileMovement)
		&& GetRootComponent() == CollisionSphere;
}

void ACombatProjectile::DrawDebugPath(const FVector& EndPoint, const FColor& Color) const
{
	if (!FDebugDrawHelper::IsRangesEnabled() || !GetWorld())
	{
		return;
	}

	DrawDebugLine(GetWorld(), LaunchLocation, EndPoint, Color, false, 2.f, 0, 2.f);
	DrawDebugSphere(GetWorld(), EndPoint, ActiveDeliveryConfig.CollisionRadius, 8, Color, false, 2.f, 0, 1.5f);
}

void ACombatProjectile::OnProjectileStopped(const FHitResult& ImpactResult)
{
	if (bImpactResolved)
	{
		return;
	}

	bImpactResolved = true;
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ProjectileMovement->StopMovementImmediately();

	AActor* HitActor = ImpactResult.GetActor();
	FVector ImpactPoint = GetActorLocation();
	if (!ImpactResult.ImpactPoint.IsNearlyZero())
	{
		ImpactPoint = static_cast<FVector>(ImpactResult.ImpactPoint);
	}
	DrawDebugPath(ImpactPoint, FColor::Red);

	if (!IsValid(HitActor))
	{
		UE_LOG(LogTemp, Display, TEXT("Combat projectile '%s' stopped on world geometry."), *GetName());
		Destroy();
		return;
	}

	if (!HitActor->Implements<UHitInterface>())
	{
		UE_LOG(LogTemp, Display, TEXT("Combat projectile '%s' stopped on non-combat actor '%s'."),
			*GetName(), *GetNameSafe(HitActor));
		Destroy();
		return;
	}

	FCombatHitRequest Request;
	Request.Attacker = LaunchAttacker;
	Request.DamageCauser = this;
	Request.EventInstigator = LaunchEventInstigator;
	Request.HitActor = HitActor;
	Request.HitResult = ImpactResult;
	Request.IncomingDamage = ActiveDeliveryConfig.Damage;
	Request.PoiseDamage = ActiveDeliveryConfig.PoiseDamage;
	Request.bApplyPoiseDamage = ActiveDeliveryConfig.PoiseDamage > 0.f;
	Request.BlockStaminaDamageMultiplier = ActiveDeliveryConfig.BlockStaminaDamageMultiplier;
	Request.bCanBeParried = ActiveDeliveryConfig.bCanBeParried;

	const FCombatHitResult Result = FCombatHitResolver::ResolveAndApply(Request);
	if (Result.bSuppressed)
	{
		UE_LOG(LogTemp, Display, TEXT("Combat projectile '%s' impact on dormant target '%s' was suppressed."),
			*GetName(), *GetNameSafe(HitActor));
	}
	else if (Result.bSameTeam)
	{
		UE_LOG(LogTemp, Display, TEXT("Combat projectile '%s' made a same-team impact on '%s' without damage."),
			*GetName(), *GetNameSafe(HitActor));
	}
	else if (Result.bBlocked)
	{
		UE_LOG(LogTemp, Display, TEXT("Combat projectile '%s' was blocked by '%s'; final damage %.2f."),
			*GetName(), *GetNameSafe(HitActor), Result.FinalDamage);
	}
	else
	{
		UE_LOG(LogTemp, Display, TEXT("Combat projectile '%s' hit '%s'; final damage %.2f."),
			*GetName(), *GetNameSafe(HitActor), Result.FinalDamage);
	}

	Destroy();
}
