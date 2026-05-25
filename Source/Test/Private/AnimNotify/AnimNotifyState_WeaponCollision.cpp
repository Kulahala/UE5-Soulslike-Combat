// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimNotify/AnimNotifyState_WeaponCollision.h"
#include "Character/BaseCharacter.h"
#include "Character/MyCharacter.h"
#include "Items/Weapon/Weapon.h"

void UAnimNotifyState_WeaponCollision::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!MeshComp || !MeshComp->GetOwner()) return;

	if (ABaseCharacter* Character = Cast<ABaseCharacter>(MeshComp->GetOwner()))
	{
		if (AWeapon* Weapon = Character->GetWeapon())
		{
			// 挥砍开始，清空上一刀留下的黑名单，准备重新砍人
			Weapon->IgnoreActors.Empty();

			// 记录刀的起始位置
			Weapon->StartWeaponTrace();
		}

		// 玩家攻击出手阶段开启霸体
		if (AMyCharacter* Player = Cast<AMyCharacter>(Character))
		{
			Player->SetAttackHyperArmor(true);
		}
	}
}

void UAnimNotifyState_WeaponCollision::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	if (!MeshComp || !MeshComp->GetOwner()) return;

	if (ABaseCharacter* Character = Cast<ABaseCharacter>(MeshComp->GetOwner()))
	{
		if (AWeapon* Weapon = Character->GetWeapon())
		{
			// 每一帧触发检测，确保不管刀挥多快都不会穿模
			Weapon->ExecuteWeaponTrace();
		}
	}
}

void UAnimNotifyState_WeaponCollision::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (!MeshComp || !MeshComp->GetOwner()) return;

	if (ABaseCharacter* Character = Cast<ABaseCharacter>(MeshComp->GetOwner()))
	{
		if (AWeapon* Weapon = Character->GetWeapon())
		{
			// 挥砍结束，再次清空一下防万一
			Weapon->IgnoreActors.Empty();
		}

		// 玩家攻击出手阶段结束，关闭霸体
		if (AMyCharacter* Player = Cast<AMyCharacter>(Character))
		{
			Player->SetAttackHyperArmor(false);
		}
	}
}
