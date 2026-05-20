#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "BlockableInterface.generated.h"

USTRUCT(BlueprintType)
struct FBlockResult
{
	GENERATED_BODY()

	bool bBlocked = false;
	float DamageAfterBlock = 0.f;
	bool bPlayNormalHitReact = true;
	bool bParried = false;
	float ParryStaggerDuration = 0.f;
	float ParryStaggerPlayRate = 0.f;
};

UINTERFACE(MinimalAPI)
class UBlockableInterface : public UInterface
{
	GENERATED_BODY()
};

class TEST_API IBlockableInterface
{
	GENERATED_BODY()

public:
	virtual FBlockResult TryBlockHit(const FVector& ImpactPoint, float IncomingDamage,
	                                 AActor* Attacker, AActor* DamageCauser) = 0;
};
