// Fill out your copyright notice in the Description page of Project Settings.

#include "Items/Treasures/Treasure.h"

#include "Character/MyCharacter.h"
#include "Engine/Engine.h"
#include "Items/Treasures/TreasureData.h"

ATreasure::ATreasure()
{
	PrimaryActorTick.bCanEverTick = true;

	if (GetMesh())
	{
		GetMesh()->SetGenerateOverlapEvents(true);
		GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		GetMesh()->SetCollisionResponseToAllChannels(ECR_Ignore);
		GetMesh()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	}
}

void ATreasure::InitializeFromData(UTreasureData* Data)
{
	if (Data == nullptr)
	{
		return;
	}

	if (Data->TreasureMesh && GetMesh())
	{
		GetMesh()->SetStaticMesh(Data->TreasureMesh);
		GetMesh()->SetRelativeScale3D(FVector(Data->TreasureScale));
	}
	TreasureName = Data->TreasureName;
	GoldValue = Data->GoldValue;
	if (Data->PickUpSound)
	{
		PickSound = Data->PickUpSound;
	}
}

bool ATreasure::TryGrantPickup(AMyCharacter* Picker, USoundBase*& OutPickupSound)
{
	OutPickupSound = nullptr;
	if (!Picker || GoldValue <= 0)
	{
		return false;
	}

	int32 NewGold = 0;
	if (!Picker->TryAddGold(GoldValue, NewGold))
	{
		return false;
	}

	OutPickupSound = PickSound;
	if (GEngine)
	{
		const FString Message = FString::Printf(TEXT("捡到%s,价值%d,总金币:%d"),
			*TreasureName, GoldValue, NewGold);
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Green, Message);
	}
	return true;
}
