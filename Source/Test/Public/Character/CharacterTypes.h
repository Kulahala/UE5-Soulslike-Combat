#pragma once

UENUM(BlueprintType)
enum class EWeaponState : uint8
{
	EWS_Unequipped UMETA(DisplayName = "Unequipped"), EWS_OneHandEquipped UMETA(DisplayName = "One Hand Equipped"),
	EWS_TwoHandEquipped UMETA(DisplayName = "Two Hand Equipped")
};

UENUM(BlueprintType)
enum class EActionState : uint8
{
	EAS_UnOccupied UMETA(DisplayName = "UnOccupied"),
	EAS_Attacking UMETA(DisplayName = "Attacking"),
	EAS_Arming UMETA(DisplayName = "Arming"),
	EAS_Stunning UMETA(DisplayName = "Stunning"),
	EAS_Exhausted UMETA(DisplayName = "Exhausted"),
	EAS_Dead UMETA(DisplayName = "Dead")
};

UENUM(BlueprintType)
enum class EArmWeaponState : uint8
{
	AWS_Arming UMETA(DisplayName = "Arming"), AWS_Disarming UMETA(DisplayName = "Disarming")
};

UENUM()
enum class EEnemyState : uint8 {
	EES_UnOccupied UMETA(DisplayName = "UnOccupied"), // 初始态
	EES_Patrolling UMETA(DisplayName = "Patrolling"), // 巡逻中
	EES_Searching UMETA(DisplayName = "Searching"),   // 张望搜索
	EES_Chasing UMETA(DisplayName = "Chasing"),       // 追逐中
	EES_Attacking UMETA(DisplayName = "Attacking"),   // 攻击硬直
	EES_Combating UMETA(DisplayName = "Combating"),	  // 战斗中
	EES_Stunned UMETA(DisplayName = "Stunned"),       // 受击硬直
	EES_Dead UMETA(DisplayName = "Dead")              // 已死亡
};
