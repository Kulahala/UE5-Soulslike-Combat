// Fill out your copyright notice in the Description page of Project Settings.

#include "AnimNotify/AnimNotifyState_ParryActive.h"
#include "Character/MyCharacter.h"

void UAnimNotifyState_ParryActive::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!MeshComp || !MeshComp->GetOwner()) return;

	if (AMyCharacter* Character = Cast<AMyCharacter>(MeshComp->GetOwner()))
	{
		Character->SetParryActive(true);
	}
}

void UAnimNotifyState_ParryActive::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (!MeshComp || !MeshComp->GetOwner()) return;

	if (AMyCharacter* Character = Cast<AMyCharacter>(MeshComp->GetOwner()))
	{
		Character->SetParryActive(false);
	}
}
