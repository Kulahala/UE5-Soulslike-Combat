#include "Items/Bow/BowAnimInstance.h"

void UBowAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	RefreshBowReference();
}

void UBowAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	RefreshBowReference();
	BowPresentationState = Bow
		? Bow->GetBowPresentationState()
		: EBowPresentationState::EBPS_Relaxed;
}

void UBowAnimInstance::RefreshBowReference()
{
	Bow = Cast<ABowBase>(GetOwningActor());
}
