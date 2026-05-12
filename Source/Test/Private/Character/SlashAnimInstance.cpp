#include "Character/SlashAnimInstance.h"

#include "Character/MyCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

void USlashAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	MyCharacter = Cast<AMyCharacter>(TryGetPawnOwner());
	if (MyCharacter)
	{
		CharacterMovement = MyCharacter->GetCharacterMovement();
	}
}

void USlashAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!CharacterMovement || !MyCharacter)
	{
		return;
	}

	const FVector Velocity = CharacterMovement->Velocity;
	ZSpeed = Velocity.Z;

	GroundSpeed = MyCharacter->GetGroundSpeed();
	Direction = MyCharacter->GetDirection();

	IsFalling = CharacterMovement->IsFalling();


	// 同步角色状态
	WeaponState = MyCharacter->GetCharacterState();
	ArmWeaponState = MyCharacter->GetArmWeaponState();
	bIsArming = MyCharacter->IsArming();
	bIsBlocking = MyCharacter->IsBlocking();
	bIsStunning = MyCharacter->GetActionState() == EActionState::EAS_Stunning;
}
