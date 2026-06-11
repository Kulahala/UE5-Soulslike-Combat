// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AttributeComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHealthChangedSignature, float, HealthPercent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStaminaChangedSignature, float, StaminaPercent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnExhaustedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPotionCountChanged, int32, CurrentCount, int32, MaxCount);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class TEST_API UAttributeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAttributeComponent();

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnHealthChangedSignature OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnStaminaChangedSignature OnStaminaChanged;

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnExhaustedSignature OnExhausted;

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnPotionCountChanged OnPotionCountChanged;

	void ReceiveDamage(float Damage);


protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Actor Attributes", meta = (AllowPrivateAccess = "true", ToolTip = "当前金币数。"))
	int32 Gold;

	// 当前生命值
	UPROPERTY(EditAnywhere, Category = "Actor Attributes", meta = (AllowPrivateAccess = "true", ToolTip = "角色当前生命值，编辑器中可调整初始值。"))
	float CurrentHealth = 100.f;

	// 最大生命值
	UPROPERTY(EditAnywhere, Category = "Actor Attributes", meta = (AllowPrivateAccess = "true", ToolTip = "生命值上限。"))
	float MaxHealth = 100.f;

	// 当前体力值
	UPROPERTY(EditAnywhere, Category = "Actor Attributes", meta = (AllowPrivateAccess = "true", ToolTip = "角色当前体力值，编辑器中可调整初始值。"))
	float CurrentStamina = 100.f;

	// 最大体力值
	UPROPERTY(EditAnywhere, Category = "Actor Attributes", meta = (AllowPrivateAccess = "true", ToolTip = "体力值上限。"))
	float MaxStamina = 100.f;

	// 体力每秒恢复量
	UPROPERTY(EditAnywhere, Category = "Actor Attributes", meta = (AllowPrivateAccess = "true", ToolTip = "体力每秒恢复量，冷却结束后生效。"))
	float StaminaRegenRate = 20.f;

	// 体力消耗后冷却时间（秒），冷却结束才开始恢复
	UPROPERTY(EditAnywhere, Category = "Actor Attributes", meta = (AllowPrivateAccess = "true", ToolTip = "体力消耗后的冷却时间（秒），冷却期间不恢复体力。"))
	float StaminaRegenDelay = 2.f;

	float StaminaRegenCooldown = 0.f; // 当前剩余冷却
	bool bStaminaRegenPaused = false; // 攻击期间暂停恢复
	bool bStaminaJustDepleted = false; // 防止重复广播耗尽
	float StaminaRegenMultiplier = 1.f; // 当前体力自然恢复倍率

	// 生命值每秒恢复量（需调用 EnableHealthRegen() 启用）
	UPROPERTY(EditAnywhere, Category = "Actor Attributes", meta = (AllowPrivateAccess = "true", ToolTip = "生命值每秒恢复量，需调用 EnableHealthRegen() 启用。"))
	float HealthRegenRate = 1.f;

	bool bHealthRegenActive = false;

	// 当前药瓶数量
	UPROPERTY(EditAnywhere, Category = "Actor Attributes|Potion", meta = (AllowPrivateAccess = "true", ToolTip = "当前药瓶数量"))
	int32 CurrentPotionCount = 3;

	// 最大药瓶数量
	UPROPERTY(EditAnywhere, Category = "Actor Attributes|Potion", meta = (AllowPrivateAccess = "true", ToolTip = "最大药瓶数量"))
	int32 MaxPotionCount = 3;

	// 药瓶恢复百分比
	UPROPERTY(EditAnywhere, Category = "Actor Attributes|Potion", meta = (AllowPrivateAccess = "true", ToolTip = "恢复百分比（默认0.5即50%）"))
	float PotionHealPercent = 0.5f;

public:
	UFUNCTION(BlueprintCallable, Category = "Actor Attributes")
	FORCEINLINE void AddGold(int32 Amount) { Gold += Amount; }

	UFUNCTION(BlueprintCallable, Category = "Actor Attributes")
	FORCEINLINE void SetGold(int32 Value) { Gold = Value; }

	UFUNCTION(BlueprintPure, Category = "Actor Attributes")
	FORCEINLINE int32 GetGold() const { return Gold; }

	FORCEINLINE float GetHealthPercent() const { return CurrentHealth / MaxHealth; }
	FORCEINLINE float GetCurrentHealth() const { return CurrentHealth; }
	FORCEINLINE float GetMaxHealth() const { return MaxHealth; }

	FORCEINLINE bool IsAlive() const { return CurrentHealth > 0.f; }

	FORCEINLINE float GetStaminaPercent() const { return CurrentStamina / MaxStamina; }
	FORCEINLINE float GetCurrentStamina() const { return CurrentStamina; }
	FORCEINLINE float GetMaxStamina() const { return MaxStamina; }

	void UseStamina(float Amount);
	void AddStamina(float Amount);
	void ResetStaminaRegenCooldown();
	void PauseStaminaRegen();
	void ResumeStaminaRegen();
	void SetStaminaRegenMultiplier(float Multiplier);
	void ResetExhaustionFlag();
	FORCEINLINE bool CheckStamina(float RequiredAmount) const { return CurrentStamina >= RequiredAmount; }

	void AddHealth(float Amount);
	void EnableHealthRegen();
	void DisableHealthRegen();

	FORCEINLINE bool HasPotion() const { return CurrentPotionCount > 0; }
	FORCEINLINE int32 GetPotionCount() const { return CurrentPotionCount; }
	FORCEINLINE int32 GetMaxPotionCount() const { return MaxPotionCount; }
	bool UsePotion();
	void AddPotion(int32 Amount);
	void SetPotionCount(int32 Count);
};
