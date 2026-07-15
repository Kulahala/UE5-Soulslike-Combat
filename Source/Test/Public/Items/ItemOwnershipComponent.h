#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Items/ItemDefinitionDataAsset.h"
#include "Save/TestSaveGame.h"
#include "ItemOwnershipComponent.generated.h"

class USoulslikeGameInstance;

/** 供火堆装备 UI 使用的只读实例选项；持久化身份始终是 InstanceId。 */
struct TEST_API FItemLoadoutOption
{
	FName InstanceId = NAME_None;
	FText DisplayName;
};

/** 玩家 Pawn 的运行时物品数据缓存；持久化写入始终委托给 USoulslikeGameInstance。 */
UCLASS(ClassGroup = (Gameplay))
class TEST_API UItemOwnershipComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UItemOwnershipComponent();

	virtual void BeginPlay() override;

	bool RestoreFromSave(const UTestSaveGame* SaveGame);
	bool TryGrantDefinition(FName DefinitionId, USoulslikeGameInstance* GameInstance, FName& OutInstanceId);
	bool TryClaimWorldItem(FName PersistentId, FName DefinitionId, USoulslikeGameInstance* GameInstance,
	                       FName& OutInstanceId);
	bool TryEquipInstance(FName InstanceId, USoulslikeGameInstance* GameInstance);
	bool TryClearEquipmentSlot(EItemEquipmentSlot EquipmentSlot, USoulslikeGameInstance* GameInstance);
	void GetLoadoutOptions(EItemEquipmentSlot EquipmentSlot, TArray<FItemLoadoutOption>& OutOptions);

	FString BuildDebugSummary() const;

	FORCEINLINE const TArray<FTestItemInstanceRecord>& GetOwnedItemInstances() const { return OwnedItemInstances; }
	FORCEINLINE const TArray<FTestEquipmentSlotRecord>& GetEquippedSlots() const { return EquippedSlots; }
	const FTestItemInstanceRecord* GetOwnedItemInstance(FName InstanceId) const;
	const UItemDefinitionDataAsset* GetDefinition(FName DefinitionId) const;
	FName GetEquippedInstanceId(EItemEquipmentSlot EquipmentSlot) const;

private:
	bool BuildDefinitionCatalog();
	bool ValidateItemRecord(const FTestItemInstanceRecord& ItemRecord, FString& OutFailureReason) const;
	static bool IsSupportedEquipmentSlot(EItemEquipmentSlot EquipmentSlot);
	static FName GetSlotId(EItemEquipmentSlot EquipmentSlot);
	void UpdateLocalEquipmentSlot(FName SlotId, FName ItemInstanceId);
	FName GenerateUniqueInstanceId() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item Ownership", meta = (AllowPrivateAccess = "true", ToolTip = "该玩家 Pawn 可以解析和拥有的物品定义目录。每个 DefinitionId 必须唯一。"))
	TArray<TObjectPtr<UItemDefinitionDataAsset>> DefinitionCatalog;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Item Ownership", meta = (AllowPrivateAccess = "true", ToolTip = "当前 Pawn 从 SaveGame 恢复或本局获得的物品实例缓存。"))
	TArray<FTestItemInstanceRecord> OwnedItemInstances;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Item Ownership", meta = (AllowPrivateAccess = "true", ToolTip = "当前 Pawn 的数据装备槽缓存；本阶段不生成装备 Actor。"))
	TArray<FTestEquipmentSlotRecord> EquippedSlots;

	TMap<FName, UItemDefinitionDataAsset*> DefinitionsById;
	bool bDefinitionCatalogInitialized = false;
};
