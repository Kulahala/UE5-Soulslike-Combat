// AnimNotify_AttachWeapon.cpp
#include "AnimNotify/AnimNotify_AttachWeapon.h"

#include "Character/MyCharacter.h"
#include "Items/Weapon/Weapon.h"

void UAnimNotify_AttachWeapon::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (MeshComp && MeshComp->GetOwner())
	{
		AMyCharacter* MyCharacter = Cast<AMyCharacter>(MeshComp->GetOwner());
		if (MyCharacter)
		{
			AWeapon* CurrentWeapon = MyCharacter->GetWeapon();
			if (CurrentWeapon)
			{
				CurrentWeapon->AttachMeshToSocket(MeshComp, SocketName); 
			}
		}
	}
}