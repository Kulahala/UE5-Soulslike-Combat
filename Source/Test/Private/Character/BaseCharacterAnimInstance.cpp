#include "Character/BaseCharacterAnimInstance.h"

#include "Character/BaseCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

void UBaseCharacterAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	RefreshCharacterReferences();
}

void UBaseCharacterAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	RefreshCharacterReferences();
	if (!BaseCharacter || !CharacterMovement)
	{
		ResetMovementAnimationData();
		return;
	}

	GroundSpeed = BaseCharacter->GetGroundSpeed();
	Direction = BaseCharacter->GetDirection();
	ZSpeed = CharacterMovement->Velocity.Z;
	IsFalling = CharacterMovement->IsFalling();
}

void UBaseCharacterAnimInstance::RefreshCharacterReferences()
{
	ABaseCharacter* CurrentBaseCharacter = Cast<ABaseCharacter>(TryGetPawnOwner());
	if (BaseCharacter != CurrentBaseCharacter)
	{
		BaseCharacter = CurrentBaseCharacter;
		CharacterMovement = BaseCharacter ? BaseCharacter->GetCharacterMovement() : nullptr;
		return;
	}

	if (BaseCharacter && !CharacterMovement)
	{
		CharacterMovement = BaseCharacter->GetCharacterMovement();
	}
}

void UBaseCharacterAnimInstance::ResetMovementAnimationData()
{
	GroundSpeed = 0.f;
	Direction = 0.f;
	ZSpeed = 0.f;
	IsFalling = false;
}
