// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Items/item.h"
#include "Shield.generated.h"

class USoundBase;
class UNiagaraSystem;

UCLASS()
class TEST_API AShield : public Aitem
{
	GENERATED_BODY()

public:
	void EquipToOffhand(USceneComponent* Parent, const FName& SocketName, AActor* NewOwner);

	UPROPERTY(EditAnywhere, Category = "Block")
	float BlockHalfAngleDegrees = 60.f;

	UPROPERTY(EditAnywhere, Category = "Block")
	float BlockedDamageMultiplier = 0.2f;

	UPROPERTY(EditAnywhere, Category = "Block")
	float BlockStaminaCostPerDamage = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Block")
	float BlockMoveSpeedMultiplier = 0.55f;

	UPROPERTY(EditAnywhere, Category = "Equip")
	FName OffhandSocketName = FName("LeftHandSocket");

	UPROPERTY(EditAnywhere, Category = "Equip")
	USoundBase* EquipSound;

	UPROPERTY(EditAnywhere, Category = "Block")
	USoundBase* BlockSound;

	UPROPERTY(EditAnywhere, Category = "Block")
	UNiagaraSystem* BlockParticle;
};
