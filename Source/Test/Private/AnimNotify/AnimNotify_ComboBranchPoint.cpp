#include "AnimNotify/AnimNotify_ComboBranchPoint.h"

#include "Character/MyCharacter.h"

void UAnimNotify_ComboBranchPoint::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
                                          const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp || !MeshComp->GetOwner()) return;

	if (AMyCharacter* Character = Cast<AMyCharacter>(MeshComp->GetOwner()))
	{
		Character->TryConsumeComboInputAtBranchPoint();
	}
}
