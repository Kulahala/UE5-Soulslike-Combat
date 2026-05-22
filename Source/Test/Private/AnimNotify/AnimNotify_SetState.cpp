#include "AnimNotify/AnimNotify_SetState.h"

#include "Character/MyCharacter.h"
#include "Enemy/Enemy.h"

void UAnimNotify_SetActionState::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	if (MeshComp && MeshComp->GetOwner())
	{
		if (AMyCharacter* MyCharacter = Cast<AMyCharacter>(MeshComp->GetOwner()))
		{
			MyCharacter->SetActionState(NewActionState);
		}
	}
}

void UAnimNotify_EnemyHitReactEnd::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	if (MeshComp && MeshComp->GetOwner())
	{
		if (AEnemy* Enemy = Cast<AEnemy>(MeshComp->GetOwner()))
		{
			Enemy->OnHitReactEnd();
		}
	}
}

void UAnimNotify_EnemyAttackEnd::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	if (MeshComp && MeshComp->GetOwner())
	{
		if (AEnemy* Enemy = Cast<AEnemy>(MeshComp->GetOwner()))
		{
			Enemy->OnAttackEnd();
		}
	}
}

void UAnimNotify_CharacterHitReactEnd::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	if (MeshComp && MeshComp->GetOwner())
	{
		if (AMyCharacter* MyCharacter = Cast<AMyCharacter>(MeshComp->GetOwner()))
		{
			if (MyCharacter->GetActionState() == EActionState::EAS_Stunning)
			{
				if (MyCharacter->IsExhaustionTimerActive())
				{
					MyCharacter->SetActionState(EActionState::EAS_Exhausted);
				}
				else
				{
					MyCharacter->SetActionState(EActionState::EAS_UnOccupied);
				}
			}
		}
	}
}
