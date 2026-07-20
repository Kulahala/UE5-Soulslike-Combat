#include "Items/Bow/Bow.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"

ABow::ABow()
{
	LoadedArrowAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("LoadedArrowAnchor"));
	LoadedArrowAnchor->SetupAttachment(GetMesh());

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
