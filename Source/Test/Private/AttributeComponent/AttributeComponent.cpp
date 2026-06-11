// Fill out your copyright notice in the Description page of Project Settings.


#include "AttributeComponent/AttributeComponent.h"

UAttributeComponent::UAttributeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	Gold = 0;
}

void UAttributeComponent::ReceiveDamage(float Damage)
{
	CurrentHealth = FMath::Clamp(CurrentHealth - Damage, 0.f, MaxHealth);
	
	OnHealthChanged.Broadcast(GetHealthPercent());
}

void UAttributeComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UAttributeComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                        FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 耐力冷却倒计时
	if (StaminaRegenCooldown > 0.f)
	{
		StaminaRegenCooldown -= DeltaTime;
	}

	// 耐力自然恢复（未暂停 且 冷却结束后）
	if (!bStaminaRegenPaused && StaminaRegenCooldown <= 0.f && CurrentStamina < MaxStamina)
	{
		CurrentStamina = FMath::Clamp(CurrentStamina + StaminaRegenRate * StaminaRegenMultiplier * DeltaTime, 0.f, MaxStamina);
		OnStaminaChanged.Broadcast(GetStaminaPercent());
	}

	// 生命恢复
	if (bHealthRegenActive && IsAlive() && CurrentHealth < MaxHealth)
	{
		AddHealth(HealthRegenRate * DeltaTime);
	}
}

void UAttributeComponent::UseStamina(float Amount)
{
	CurrentStamina = FMath::Clamp(CurrentStamina - Amount, -Amount, MaxStamina);

	if (CurrentStamina <= 0.f && !bStaminaJustDepleted)
	{
		bStaminaJustDepleted = true;
		OnExhausted.Broadcast();
	}
	
	if (CurrentStamina > 0.f)
	{
		bStaminaJustDepleted = false;
	}
	CurrentStamina = FMath::Max(CurrentStamina, 0.f);

	OnStaminaChanged.Broadcast(GetStaminaPercent());
}

void UAttributeComponent::AddStamina(float Amount)
{
	CurrentStamina = FMath::Clamp(CurrentStamina + Amount, 0.f, MaxStamina);
	if (CurrentStamina > 0.f) bStaminaJustDepleted = false;
	OnStaminaChanged.Broadcast(GetStaminaPercent());
}

void UAttributeComponent::ResetExhaustionFlag()
{
	bStaminaJustDepleted = false;
}

void UAttributeComponent::ResetStaminaRegenCooldown()
{
	StaminaRegenCooldown = StaminaRegenDelay;
}

void UAttributeComponent::PauseStaminaRegen()
{
	bStaminaRegenPaused = true;
}

void UAttributeComponent::ResumeStaminaRegen()
{
	bStaminaRegenPaused = false;
	StaminaRegenCooldown = StaminaRegenDelay;
}

void UAttributeComponent::SetStaminaRegenMultiplier(float Multiplier)
{
	StaminaRegenMultiplier = FMath::Max(0.f, Multiplier);
}

void UAttributeComponent::AddHealth(float Amount)
{
	CurrentHealth = FMath::Clamp(CurrentHealth + Amount, 0.f, MaxHealth);
	OnHealthChanged.Broadcast(GetHealthPercent());
}

void UAttributeComponent::EnableHealthRegen()
{
	bHealthRegenActive = true;
}

void UAttributeComponent::DisableHealthRegen()
{
	bHealthRegenActive = false;
}

bool UAttributeComponent::UsePotion()
{
	if (CurrentPotionCount > 0)
	{
		CurrentPotionCount--;
		OnPotionCountChanged.Broadcast(CurrentPotionCount, MaxPotionCount);
		return true;
	}
	return false;
}

void UAttributeComponent::AddPotion(int32 Amount)
{
	CurrentPotionCount = FMath::Clamp(CurrentPotionCount + Amount, 0, MaxPotionCount);
	OnPotionCountChanged.Broadcast(CurrentPotionCount, MaxPotionCount);
}

void UAttributeComponent::SetPotionCount(int32 Count)
{
	CurrentPotionCount = FMath::Clamp(Count, 0, MaxPotionCount);
	OnPotionCountChanged.Broadcast(CurrentPotionCount, MaxPotionCount);
}
