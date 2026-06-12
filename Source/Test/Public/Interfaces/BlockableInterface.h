#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "BlockableInterface.generated.h"

USTRUCT(BlueprintType)
struct FBlockResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Block", meta = (ToolTip = "是否成功格挡本次命中。"))
	bool bBlocked = false;

	UPROPERTY(BlueprintReadOnly, Category = "Block", meta = (ToolTip = "格挡后实际承受的伤害。"))
	float DamageAfterBlock = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Block", meta = (ToolTip = "是否播放普通受击硬直。格挡成功通常为 false。"))
	bool bPlayNormalHitReact = true;

	UPROPERTY(BlueprintReadOnly, Category = "Block", meta = (ToolTip = "是否弹反成功。弹反成功会尝试破防攻击方。"))
	bool bParried = false;
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
