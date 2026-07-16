#include "Items/Bow/Bow.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"

ABow::ABow()
{
	ProjectileSpawnPoint = CreateDefaultSubobject<USceneComponent>(TEXT("ProjectileSpawnPoint"));
	ProjectileSpawnPoint->SetupAttachment(GetMesh());
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
