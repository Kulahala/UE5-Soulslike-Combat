#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ItemDefinitionDataAsset.generated.h"

class Aitem;
struct FPropertyChangedEvent;

/** 物品定义声明的装备槽；存档中仍保存稳定 FName 槽位 ID。 */
UENUM(BlueprintType)
enum class EItemEquipmentSlot : uint8
{
	None UMETA(DisplayName = "None"),
	MainHand UMETA(DisplayName = "Main Hand"),
	OffHand UMETA(DisplayName = "Off Hand")
};

/**
 * 作者填写的静态物品定义。可变数据属于 FTestItemInstanceRecord，不能回写到此资产。
 */
UCLASS(BlueprintType)
class TEST_API UItemDefinitionDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	FORCEINLINE FName GetDefinitionId() const { return DefinitionId; }
	FORCEINLINE const FText& GetDisplayName() const { return DisplayName; }
	FORCEINLINE EItemEquipmentSlot GetEquipmentSlot() const { return EquipmentSlot; }
	FORCEINLINE TSubclassOf<Aitem> GetRuntimeItemActorClass() const { return RuntimeItemActorClass; }

	bool IsDefinitionValid(FString& OutFailureReason) const;

	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (AllowPrivateAccess = "true", ToolTip = "作者填写的全局稳定物品定义 ID；绝不从资产名、Actor 名称或运行时对象推导。"))
	FName DefinitionId = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (AllowPrivateAccess = "true", ToolTip = "物品在未来装备与背包 UI 中使用的显示名称。"))
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true", ToolTip = "该定义可以占用的装备槽。None 表示当前不可装备。"))
	EItemEquipmentSlot EquipmentSlot = EItemEquipmentSlot::None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true", ToolTip = "TODO-03B 实体化并附着装备表现时使用的物品 Actor 类。"))
	TSubclassOf<Aitem> RuntimeItemActorClass;

	void LogConfigWarnings() const;
};
