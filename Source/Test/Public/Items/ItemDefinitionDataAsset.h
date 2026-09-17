#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ItemDefinitionDataAsset.generated.h"

class Aitem;
class USoundBase;
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
	FORCEINLINE USoundBase* GetPickupSound() const { return PickupSound; }
	FORCEINLINE bool UsesAmmoContainer() const { return bUsesAmmoContainer; }
	FORCEINLINE int32 GetLoadedAmmoCapacity() const { return LoadedAmmoCapacity; }
	FORCEINLINE int32 GetReserveAmmoStackLimit() const { return ReserveAmmoStackLimit; }

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ammo", meta = (AllowPrivateAccess = "true", ToolTip = "启用后，此 None 槽位 Definition 使用已装填箭与储备栈模型。"))
	bool bUsesAmmoContainer = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ammo", meta = (AllowPrivateAccess = "true", EditCondition = "bUsesAmmoContainer", EditConditionHides, ClampMin = "1", UIMin = "1", ToolTip = "该弹药可被武器直接消费的已装填数量上限。"))
	int32 LoadedAmmoCapacity = 20;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ammo", meta = (AllowPrivateAccess = "true", EditCondition = "bUsesAmmoContainer", EditConditionHides, ClampMin = "1", UIMin = "1", ToolTip = "储备背包中单个 ItemInstance 栈的数量上限；满栈后自动创建新的稳定实例。"))
	int32 ReserveAmmoStackLimit = 99;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio", meta = (AllowPrivateAccess = "true", ToolTip = "固定世界拾取成功后播放的音效；为空时静默领取。火堆主动换装仍使用运行时 Actor 自己的 EquipSound。"))
	USoundBase* PickupSound = nullptr;

	void LogConfigWarnings() const;
};
