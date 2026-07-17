#pragma once

#include "CoreMinimal.h"
#include "Combat/CombatProjectile.h"
#include "ArrowProjectile.generated.h"

class UStaticMeshComponent;

/**
 * 箭矢投射物的原生表现层。
 * 碰撞、移动、命中、生命周期和发射提交仍由 ACombatProjectile 独占。
 */
UCLASS()
class TEST_API AArrowProjectile : public ACombatProjectile
{
	GENERATED_BODY()

public:
	AArrowProjectile();

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true", ToolTip = "仅用于箭矢飞行表现的无碰撞 Mesh。由 Blueprint 子类指定具体模型与相对变换。"))
	UStaticMeshComponent* ArrowVisual = nullptr;
};
