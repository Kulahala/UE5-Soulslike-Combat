#include "AnimNotify/AnimNotifyState_PlayerActionCancelWindow.h"
#include "Character/MyCharacter.h"

void UAnimNotifyState_PlayerActionCancelWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!MeshComp || !MeshComp->GetOwner()) return;

	if (AMyCharacter* Character = Cast<AMyCharacter>(MeshComp->GetOwner()))
	{
		Character->OpenActionCancelWindow();
	}
}

void UAnimNotifyState_PlayerActionCancelWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (!MeshComp || !MeshComp->GetOwner()) return;

	if (AMyCharacter* Character = Cast<AMyCharacter>(MeshComp->GetOwner()))
	{
		Character->CloseActionCancelWindow();
	}
}
