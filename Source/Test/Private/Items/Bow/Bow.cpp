#include "Items/Bow/Bow.h"

#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	const FName BowArrowSocketName(TEXT("BowArrowSocket"));
}

ABow::ABow()
{
	BowSkeletalVisual = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("BowSkeletalVisual"));
	BowSkeletalVisual->SetupAttachment(GetMesh());
	BowSkeletalVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BowSkeletalVisual->SetCollisionResponseToAllChannels(ECR_Ignore);
	BowSkeletalVisual->SetGenerateOverlapEvents(false);
	BowSkeletalVisual->SetSimulatePhysics(false);
	BowSkeletalVisual->SetEnableGravity(false);
	BowSkeletalVisual->SetCanEverAffectNavigation(false);

	LoadedArrowAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("LoadedArrowAnchor"));
	LoadedArrowAnchor->SetupAttachment(BowSkeletalVisual, BowArrowSocketName);

	LoadedArrowVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LoadedArrowVisual"));
	LoadedArrowVisual->SetupAttachment(LoadedArrowAnchor);
	LoadedArrowVisual->SetMobility(EComponentMobility::Movable);
	LoadedArrowVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LoadedArrowVisual->SetCollisionResponseToAllChannels(ECR_Ignore);
	LoadedArrowVisual->SetGenerateOverlapEvents(false);
	LoadedArrowVisual->SetSimulatePhysics(false);
	LoadedArrowVisual->SetEnableGravity(false);
	LoadedArrowVisual->SetCanEverAffectNavigation(false);
	LoadedArrowVisual->SetVisibility(false, true);
	LoadedArrowVisual->SetHiddenInGame(true, true);

	ProjectileSpawnPoint = CreateDefaultSubobject<USceneComponent>(TEXT("ProjectileSpawnPoint"));
	ProjectileSpawnPoint->SetupAttachment(LoadedArrowAnchor);
	ProjectileClass = ACombatProjectile::StaticClass();
}

void ABow::SetBowPresentationState(EBowPresentationState NewState)
{
	BowPresentationState = NewState;
}

EBowPresentationState ABow::GetBowPresentationState() const
{
	return BowPresentationState;
}

bool ABow::HasValidProjectileConfig(FString& OutFailureReason) const
{
	if (AmmoDefinitionId == NAME_None)
	{
		OutFailureReason = TEXT("AmmoDefinitionId is empty.");
		return false;
	}

	if (!ProjectileClass)
	{
		OutFailureReason = TEXT("ProjectileClass is empty.");
		return false;
	}

	if (!ProjectileDeliveryConfig.IsValid())
	{
		OutFailureReason = TEXT("ProjectileDeliveryConfig is invalid.");
		return false;
	}

	if (!ProjectileSpawnPoint)
	{
		OutFailureReason = TEXT("ProjectileSpawnPoint is unavailable.");
		return false;
	}

	return true;
}

bool ABow::NockPreparedProjectile(ACombatProjectile* PreparedProjectile) const
{
	auto RejectNock = [this, PreparedProjectile](const TCHAR* FailureReason)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: Bow nock failed: %s"), *GetName(), FailureReason);
		if (IsValid(PreparedProjectile))
		{
			PreparedProjectile->Destroy();
		}
		return false;
	};

	if (!IsValid(PreparedProjectile) || !PreparedProjectile->IsPreparedForActivation())
	{
		return RejectNock(TEXT("prepared projectile is unavailable."));
	}

	if (!BowSkeletalVisual || !BowSkeletalVisual->DoesSocketExist(BowArrowSocketName))
	{
		return RejectNock(TEXT("BowSkeletalVisual or BowArrowSocket is unavailable."));
	}

	if (!PreparedProjectile->GetRootComponent())
	{
		return RejectNock(TEXT("prepared projectile has no root component."));
	}

	if (!PreparedProjectile->AttachToComponent(BowSkeletalVisual,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale, BowArrowSocketName))
	{
		return RejectNock(TEXT("prepared projectile root could not attach to BowArrowSocket."));
	}

	return true;
}

FVector ABow::GetProjectileSpawnLocation() const
{
	return ProjectileSpawnPoint ? ProjectileSpawnPoint->GetComponentLocation() : GetActorLocation();
}

void ABow::SetLoadedArrowVisualVisible(bool bVisible)
{
	if (!LoadedArrowVisual)
	{
		return;
	}

	LoadedArrowVisual->SetVisibility(bVisible, true);
	LoadedArrowVisual->SetHiddenInGame(!bVisible, true);
}

void ABow::PlayShotSound() const
{
	if (ShotSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ShotSound, GetActorLocation());
	}
}

void ABow::PlayEmptyAmmoSound() const
{
	if (EmptyAmmoSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, EmptyAmmoSound, GetActorLocation());
	}
}
