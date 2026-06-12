// Fill out your copyright notice in the Description page of Project Settings.

#include "Items/Shield/Shield.h"

#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"

void AShield::EquipToOffhand(USceneComponent* Parent, const FName& SocketName, AActor* NewOwner)
{
	SetOwner(NewOwner);
	FAttachmentTransformRules Rules(EAttachmentRule::SnapToTarget, true);
	AttachToComponent(Parent, Rules, SocketName);
	ItemState = EItemState::EIS_Equipped;
	DisablePickupCollision();
	if (GetEffect()) GetEffect()->Deactivate();
	if (EquipSound) UGameplayStatics::PlaySoundAtLocation(this, EquipSound, GetActorLocation());
}
