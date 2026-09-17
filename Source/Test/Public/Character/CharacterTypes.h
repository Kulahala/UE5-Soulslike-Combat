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
	EAS_Stunning UMETA(DisplayName = "Stunning"),
	EAS_Exhausted UMETA(DisplayName = "Exhausted"),
	EAS_Parrying UMETA(DisplayName = "Parrying"),
	EAS_Dodging UMETA(DisplayName = "Dodging"),
	EAS_UsingPotion UMETA(DisplayName = "UsingPotion"),
	EAS_Dead UMETA(DisplayName = "Dead"),
	EAS_Aiming UMETA(DisplayName = "Aiming"),
	EAS_GuardBroken UMETA(DisplayName = "GuardBroken")
};

UENUM()
enum class EComboPlaybackMode : uint8
{
	NewPlayback,
	Continuation
};

UENUM(BlueprintType)
enum class EPlayerActionType : uint8
{
	None UMETA(DisplayName = "None"),  //默认
	Attack UMETA(DisplayName = "Attack"),  //攻击
	Dodge UMETA(DisplayName = "Dodge"),  //翻滚
	Block UMETA(DisplayName = "Block"),  //防御
	Parry UMETA(DisplayName = "Parry"),  //弹反
	Potion UMETA(DisplayName = "Potion"),  //喝药
	HitReact UMETA(DisplayName = "HitReact"),  //受击 
	Death UMETA(DisplayName = "Death"),  //死亡
	RangedAim UMETA(DisplayName = "RangedAim"),  //瞄准
	RangedRelease UMETA(DisplayName = "RangedRelease")  //发射
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
	EES_StanceBreak UMETA(DisplayName = "StanceBreak"), // 韧性破防硬直
	EES_Dead UMETA(DisplayName = "Dead")              // 已死亡
};
