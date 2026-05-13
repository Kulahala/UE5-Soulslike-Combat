// Fill out your copyright notice in the Description page of Project Settings.

#include "Items/Weapon/Weapon.h"
#include "Character/BaseCharacter.h"
#include "Character/MyCharacter.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Interfaces/HitInterface.h"
#include "Interfaces/BlockableInterface.h"
#include "NiagaraComponent.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

// ==================== 生命周期 ====================

AWeapon::AWeapon()
{
	BoxTrace = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxTrace"));
	BoxTrace->SetupAttachment(GetMesh());
	BoxTrace->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BoxTrace->SetHiddenInGame(true);
}

// ==================== 装备/拾取 ====================

void AWeapon::AttachMeshToSocket(USceneComponent* Parent, const FName& SocketName)
{
	FAttachmentTransformRules AttachmentRules(EAttachmentRule::SnapToTarget, true);
	AttachToComponent(Parent, AttachmentRules, SocketName);

	// 叠加装备旋转偏移，修正不同武器模型的本地朝向差异
	if (!EquipRotationOffset.IsNearlyZero())
	{
		SetActorRelativeRotation(EquipRotationOffset);
	}
}

void AWeapon::Equip(USceneComponent* Parent, const FName& SocketName, AActor* NewOwner, APawn* NewInstigator)
{
	SetOwner(NewOwner);
	SetInstigator(NewInstigator);

	AttachMeshToSocket(Parent, SocketName);
	ItemState = EItemState::EIS_Equipped;

	if (GetEffect())
	{
		GetEffect()->Deactivate();
	}

	// 非常关键：绑定武器的主人，避免砍中自己
	if (Parent && Parent->GetOwner())
	{
		SetOwner(Parent->GetOwner());
	}

	if (EquipSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, EquipSound, GetActorLocation());
	}
}

void AWeapon::OnPickup_Implementation(AActor* Picker)
{
	if (AMyCharacter* Character = Cast<AMyCharacter>(Picker))
	{
		Equip(Character->GetMesh(), FName("RightHandSocket"), Character, Character);
	}
}

// ==================== 拾取碰撞 ====================

void AWeapon::SphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                            UPrimitiveComponent* OtherComp,
                            int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	Super::SphereOverlap(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);
}

// ==================== 武器碰撞检测 ====================

void AWeapon::StartWeaponTrace()
{
	check(BoxTrace);
	TraceCenterOld = BoxTrace->GetComponentLocation();
	TraceRotationOld = BoxTrace->GetComponentRotation();
}

void AWeapon::ExecuteWeaponTrace()
{
	check(BoxTrace);

	FVector CurrentCenter = BoxTrace->GetComponentLocation();
	FRotator TraceRotation = BoxTrace->GetComponentRotation();
	FVector BoxHalfExtent = BoxTrace->GetScaledBoxExtent();

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(this);

	if (GetOwner())
	{
		ActorsToIgnore.AddUnique(GetOwner());
	}

	for (AActor* ToIgnore : IgnoreActors)
	{
		ActorsToIgnore.AddUnique(ToIgnore);
	}

	FHitResult HitPoint;

	// 盒体扫掠：连接相邻两帧路径，防止高速挥砍漏判
	bool bHit = UKismetSystemLibrary::BoxTraceSingle(
		this, TraceCenterOld, CurrentCenter, BoxHalfExtent, TraceRotation,
		UEngineTypes::ConvertToTraceType(ECC_WorldDynamic), false, ActorsToIgnore, EDrawDebugTrace::ForOneFrame,
		HitPoint, true, FLinearColor::Red, FLinearColor::Green, 3.f);

	// 记录本帧位置，留给下帧做参考
	TraceCenterOld = CurrentCenter;
	TraceRotationOld = TraceRotation;

	if (bHit && HitPoint.GetActor())
	{
		AActor* HitActor = HitPoint.GetActor();

		// 同类豁免：武器持有者和命中目标共享标签 → 跳过
		bool bSameTeam = false;
		if (GetOwner())
		{
			for (const FName& Tag : GetOwner()->Tags)
			{
				if (HitActor->ActorHasTag(Tag))
				{
					bSameTeam = true;
					break;
				}
			}
		}

		// 格挡判定（仅跨阵营）
		bool bPlayNormalHitReact = true;
		float FinalDamage = Damage;
		float KnockbackScale = 1.f;
		bool bApplyStun = true;

		if (!bSameTeam)
		{
			if (IBlockableInterface* Blockable = Cast<IBlockableInterface>(HitActor))
			{
				FBlockResult BlockResult = Blockable->TryBlockHit(
					HitPoint.ImpactPoint, Damage, GetOwner(), this);
				if (BlockResult.bBlocked)
				{
					FinalDamage = BlockResult.DamageAfterBlock;
					bPlayNormalHitReact = BlockResult.bPlayNormalHitReact;
					KnockbackScale = Damage > 0.f ? BlockResult.DamageAfterBlock / Damage : 0.f;
					bApplyStun = BlockResult.bPlayNormalHitReact;
				}
			}
			UGameplayStatics::ApplyDamage(HitActor, FinalDamage, GetInstigatorController(), this,
			                              UDamageType::StaticClass());
		}

		// 写入命中上下文（所有命中，含格挡）
		if (ABaseCharacter* HitChar = Cast<ABaseCharacter>(HitActor))
		{
			HitChar->CachePendingHitContext(GetOwner(), KnockbackScale, !bPlayNormalHitReact, bApplyStun);
		}

		// 受击反应+特效：所有命中都走 GetHit（内部按上下文分流）
		if (HitActor->Implements<UHitInterface>())
		{
			IHitInterface::Execute_GetHit(HitActor, HitPoint.ImpactPoint, GetOwner());
		}

		// 触发摄像机震动反馈
		CameraShake();

		// 触发卡肉感 (Hit Stop)
		SetEnableHitStop(true);
		HitStop(HitActor);
		// 击中一次后加入黑名单，防止同一刀造成多次伤害（同类也加入，避免每帧重复判断）
		IgnoreActors.AddUnique(HitActor);
	}
}

// ==================== 卡肉感 ====================

void AWeapon::RestoreTimeDilation(AActor* Attacker, AActor* Victim)
{
	if (Attacker)
	{
		Attacker->CustomTimeDilation = 1.0f;
	}
	if (Victim)
	{
		Victim->CustomTimeDilation = 1.0f;
	}
}

void AWeapon::HitStop(AActor* HitActor)
{
	if (bEnableHitStop)
	{
		AActor* Attacker = GetOwner();
		if (Attacker && HitActor)
		{
			Attacker->CustomTimeDilation = HitStopTimeDilation;
			HitActor->CustomTimeDilation = HitStopTimeDilation;

			FTimerHandle HitStopTimer;
			FTimerDelegate TimerDel;
			TimerDel.BindUFunction(this, FName("RestoreTimeDilation"), Attacker, HitActor);
			GetWorld()->GetTimerManager().SetTimer(HitStopTimer, TimerDel, HitStopDuration, false);
		}
	}
}

void AWeapon::CameraShake()
{
	if (HitCameraShake && GetInstigatorController())
	{
		if (APlayerController* PlayerController = Cast<APlayerController>(GetInstigatorController()))
		{
			PlayerController->ClientStartCameraShake(HitCameraShake);
		}
	}
}
