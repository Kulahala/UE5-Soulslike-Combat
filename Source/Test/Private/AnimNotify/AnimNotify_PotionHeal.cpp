// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimNotify/AnimNotify_PotionHeal.h"
#include "Character/MyCharacter.h"
#include "Components/SkeletalMeshComponent.h"

void UAnimNotify_PotionHeal::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (MeshComp && MeshComp->GetOwner())
	{
		if (AMyCharacter* Character = Cast<AMyCharacter>(MeshComp->GetOwner()))
		{
			Character->HealFromPotion(HealPercent);
		}
	}
}
