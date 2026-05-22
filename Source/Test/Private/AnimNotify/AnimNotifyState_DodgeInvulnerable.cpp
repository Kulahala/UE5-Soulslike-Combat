#include "AnimNotify/AnimNotifyState_DodgeInvulnerable.h"
#include "Character/MyCharacter.h"

void UAnimNotifyState_DodgeInvulnerable::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!MeshComp || !MeshComp->GetOwner()) return;

	if (AMyCharacter* Character = Cast<AMyCharacter>(MeshComp->GetOwner()))
	{
		Character->SetDodgeInvulnerable(true);
	}
}

void UAnimNotifyState_DodgeInvulnerable::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (!MeshComp || !MeshComp->GetOwner()) return;

	if (AMyCharacter* Character = Cast<AMyCharacter>(MeshComp->GetOwner()))
	{
		Character->SetDodgeInvulnerable(false);
	}
}