#include "Combat/ArrowProjectile.h"

#include "Components/StaticMeshComponent.h"

AArrowProjectile::AArrowProjectile()
{
	check(GetRootComponent());

	ArrowVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArrowVisual"));
	ArrowVisual->SetupAttachment(GetRootComponent());
	ArrowVisual->SetMobility(EComponentMobility::Movable);
	ArrowVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ArrowVisual->SetCollisionResponseToAllChannels(ECR_Ignore);
	ArrowVisual->SetGenerateOverlapEvents(false);
	ArrowVisual->SetSimulatePhysics(false);
	ArrowVisual->SetEnableGravity(false);
	ArrowVisual->SetCanEverAffectNavigation(false);
}
