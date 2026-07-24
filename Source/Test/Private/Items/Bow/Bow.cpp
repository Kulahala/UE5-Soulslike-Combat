#include "Items/Bow/Bow.h"

#include "Components/StaticMeshComponent.h"
#include "Items/Bow/BowPhysicalProfileDataAsset.h"
#include "Kismet/GameplayStatics.h"

ABow::ABow()
{
	LoadedArrowVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LoadedArrowVisual"));
	LoadedArrowVisual->SetupAttachment(BowSkeletalVisual, GetBowArrowSocketName());
	LoadedArrowVisual->SetMobility(EComponentMobility::Movable);
	LoadedArrowVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LoadedArrowVisual->SetCollisionResponseToAllChannels(ECR_Ignore);
	LoadedArrowVisual->SetGenerateOverlapEvents(false);
	LoadedArrowVisual->SetSimulatePhysics(false);
	LoadedArrowVisual->SetEnableGravity(false);
	LoadedArrowVisual->SetCanEverAffectNavigation(false);
	// 待机箭 Mesh 与局部轴修正由共享 PhysicalProfile 写入，不能在子 Blueprint 覆写。
	LoadedArrowVisual->bEditableWhenInherited = false;
	LoadedArrowVisual->SetVisibility(false, true);
	LoadedArrowVisual->SetHiddenInGame(true, true);

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

	FTransform LaunchTransform;
	if (!TryGetLaunchTransform(LaunchTransform, OutFailureReason))
	{
		return false;
	}

	return true;
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

void ABow::OnPhysicalProfileApplied()
{
	Super::OnPhysicalProfileApplied();

	const UBowPhysicalProfileDataAsset* Profile = GetPhysicalProfile();
	if (!Profile || !LoadedArrowVisual)
	{
		return;
	}

	LoadedArrowVisual->SetStaticMesh(Profile->GetNockedArrowStaticMesh());
	LoadedArrowVisual->SetRelativeTransform(Profile->GetNockedArrowVisualRelativeTransform());
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
