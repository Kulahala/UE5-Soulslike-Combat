#include "AnimNotify/AnimNotifyState_ComboWindow.h"
#include "Character/MyCharacter.h"

void UAnimNotifyState_ComboWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!MeshComp || !MeshComp->GetOwner()) return;

	if (AMyCharacter* Character = Cast<AMyCharacter>(MeshComp->GetOwner()))
	{
		Character->OpenComboWindow();
	}
}

void UAnimNotifyState_ComboWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (!MeshComp || !MeshComp->GetOwner()) return;

	if (AMyCharacter* Character = Cast<AMyCharacter>(MeshComp->GetOwner()))
	{
		Character->CloseComboWindow();
	}
}
