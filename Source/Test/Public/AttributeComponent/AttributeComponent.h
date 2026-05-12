// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AttributeComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHealthChangedSignature, float, HealthPercent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStaminaChangedSignature, float, StaminaPercent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnExhaustedSignature);

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

	void ReceiveDamage(float Damage);


protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Actor Attributes", meta = (AllowPrivateAccess = "true"))
	int32 Gold;

	UPROPERTY(EditAnywhere, Category = "Actor Attributes", meta = (AllowPrivateAccess = "true"))
	float CurrentHealth = 100.f;

	UPROPERTY(EditAnywhere, Category = "Actor Attributes", meta = (AllowPrivateAccess = "true"))
	float MaxHealth = 100.f;

	UPROPERTY(EditAnywhere, Category = "Actor Attributes", meta = (AllowPrivateAccess = "true"))
	float CurrentStamina = 100.f;

	UPROPERTY(EditAnywhere, Category = "Actor Attributes", meta = (AllowPrivateAccess = "true"))
	float MaxStamina = 100.f;

	UPROPERTY(EditAnywhere, Category = "Actor Attributes", meta = (AllowPrivateAccess = "true"))
	float StaminaRegenRate = 10.f;

	UPROPERTY(EditAnywhere, Category = "Actor Attributes", meta = (AllowPrivateAccess = "true"))
	float StaminaRegenDelay = 1.f; // 消耗后多久开始恢复

	float StaminaRegenCooldown = 0.f; // 当前剩余冷却
	bool bStaminaRegenPaused = false; // 攻击期间暂停恢复
	bool bStaminaJustDepleted = false; // 防止重复广播耗尽

	UPROPERTY(EditAnywhere, Category = "Actor Attributes", meta = (AllowPrivateAccess = "true"))
	float HealthRegenRate = 1.f; // 每秒恢复量

	bool bHealthRegenActive = false;

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
	void ResetExhaustionFlag();
	FORCEINLINE bool CheckStamina(float RequiredAmount) const { return CurrentStamina >= RequiredAmount; }

	void AddHealth(float Amount);
	void EnableHealthRegen();
	void DisableHealthRegen();
};
