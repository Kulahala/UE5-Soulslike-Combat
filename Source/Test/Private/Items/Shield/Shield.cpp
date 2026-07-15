// Fill out your copyright notice in the Description page of Project Settings.

#include "Items/Shield/Shield.h"

#include "Character/MyCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"

bool AShield::EquipToOffhand(USceneComponent* Parent, const FName& SocketName, AActor* NewOwner,
	bool bPlayEquipSound)
{
	if (!Parent || SocketName == NAME_None || !Parent->DoesSocketExist(SocketName))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s failed to attach shield: parent or socket '%s' is invalid."),
			*GetName(), *SocketName.ToString());
		return false;
	}

	SetOwner(NewOwner);
	FAttachmentTransformRules Rules(EAttachmentRule::SnapToTarget, true);
	if (!AttachToComponent(Parent, Rules, SocketName))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s failed to attach shield to socket '%s'."),
			*GetName(), *SocketName.ToString());
		return false;
	}

	ItemState = EItemState::EIS_Equipped;
	DisablePickupCollision();
	if (GetEffect()) GetEffect()->Deactivate();
	if (bPlayEquipSound) PlayEquipSound();
	return true;
}

void AShield::PlayEquipSound() const
{
	if (EquipSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, EquipSound, GetActorLocation());
	}
}

void AShield::OnPickup_Implementation(AActor* Picker)
{
	if (AMyCharacter* Character = Cast<AMyCharacter>(Picker))
	{
		TryClaimPersistentWorldPickup(Character);
	}
}
