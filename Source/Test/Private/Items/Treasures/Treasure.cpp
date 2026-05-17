// Fill out your copyright notice in the Description page of Project Settings.

#include "Items/Treasures/Treasure.h"

#include "AttributeComponent/AttributeComponent.h"
#include "Character/MyCharacter.h"
#include "Items/Treasures/TreasureData.h"
#include "Kismet/GameplayStatics.h"

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
	if (Data->PickUpSound && PickSound)
	{
		PickSound = Data->PickUpSound;
	}
}

void ATreasure::SphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                                 UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	Super::SphereEndOverlap(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex);
}

void ATreasure::SphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                              UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
                              const FHitResult& SweepResult)
{
	if (AMyCharacter* SlashCharacter = Cast<AMyCharacter>(OtherActor))
	{
		if (UAttributeComponent* CharacterAttributes = SlashCharacter->GetAttributes())
		{
			CharacterAttributes->AddGold(GoldValue);
			if (GEngine)
			{
				FString Message = FString::Printf(
					TEXT("捡到%s,价值%d,总金币:%d"), *TreasureName, GoldValue, CharacterAttributes->GetGold());
				GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Green, Message);
			}
			if (PickSound)
			{
				UGameplayStatics::PlaySoundAtLocation(this, PickSound, OtherActor->GetActorLocation());
			}
			Destroy();
		}
	}
}
