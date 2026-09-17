#include "Character/SlashAnimInstance.h"

#include "Character/MyCharacter.h"

void USlashAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	MyCharacter = Cast<AMyCharacter>(GetBaseCharacter());
}

void USlashAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	MyCharacter = Cast<AMyCharacter>(GetBaseCharacter());
	if (!MyCharacter)
	{
		WeaponState = EWeaponState::EWS_Unequipped;
		bIsBlocking = false;
		bIsStunning = false;
		bIsBowAiming = false;
		BowAimYaw = 0.f;
		BowAimPitch = 0.f;
		return;
	}

	// 同步角色状态
	WeaponState = MyCharacter->GetCharacterState();
	bIsBlocking = MyCharacter->IsBlocking();
	bIsStunning = MyCharacter->GetActionState() == EActionState::EAS_Stunning
		|| MyCharacter->GetActionState() == EActionState::EAS_GuardBroken;
	bIsBowAiming = MyCharacter->IsBowAiming();
	if (bIsBowAiming)
	{
		const FRotator AimDelta = (MyCharacter->GetBaseAimRotation() - MyCharacter->GetActorRotation()).GetNormalized();
		BowAimYaw = AimDelta.Yaw;
		BowAimPitch = AimDelta.Pitch;
	}
	else
	{
		BowAimYaw = 0.f;
		BowAimPitch = 0.f;
	}
}
