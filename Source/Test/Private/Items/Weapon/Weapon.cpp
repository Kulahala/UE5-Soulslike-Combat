// Fill out your copyright notice in the Description page of Project Settings.

#include "Items/Weapon/Weapon.h"
#include "Character/BaseCharacter.h"
#include "Character/MyCharacter.h"
#include "Combat/CombatTeamHelper.h"
#include "Enemy/Enemy.h"
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
	BuildIgnoreList(ActorsToIgnore);

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

		// 命中解析：同阵营判定 + 格挡结算 + 伤害计算
		FWeaponHitResult Result = ResolveHit(HitActor, HitPoint);

		// 非同阵营才扣血
		if (!Result.bSameTeam)
		{
			UGameplayStatics::ApplyDamage(HitActor, Result.FinalDamage, GetInstigatorController(), this,
			                              UDamageType::StaticClass());
		}

		// 反馈派发：上下文写入 + 受击反应 + 相机震动 + 卡肉 + 黑名单
		DispatchHitFeedback(HitActor, HitPoint, Result);
	}
}

void AWeapon::BuildIgnoreList(TArray<AActor*>& OutActors)
{
	OutActors.Add(this);

	if (GetOwner())
	{
		OutActors.AddUnique(GetOwner());
	}

	for (AActor* ToIgnore : IgnoreActors)
	{
		OutActors.AddUnique(ToIgnore);
	}
}

AWeapon::FWeaponHitResult AWeapon::ResolveHit(AActor* HitActor, const FHitResult& HitPoint)
{
	FWeaponHitResult Result;
	float BaseDamage = Damage;

	if (ABaseCharacter* Attacker = Cast<ABaseCharacter>(GetOwner()))
	{
		BaseDamage *= Attacker->GetAttackDamageMultiplier();
	}
	Result.FinalDamage = BaseDamage;

	// 同类豁免：武器持有者和命中目标共享标签
	if (FCombatTeamHelper::ShareTeamTag(GetOwner(), HitActor))
	{
		Result.bSameTeam = true;
		return Result;
	}

	// 格挡判定（仅跨阵营）
	if (IBlockableInterface* Blockable = Cast<IBlockableInterface>(HitActor))
	{
		FBlockResult BlockResult = Blockable->TryBlockHit(
			HitPoint.ImpactPoint, BaseDamage, GetOwner(), this);
		if (BlockResult.bBlocked)
		{
			Result.FinalDamage = BlockResult.DamageAfterBlock;
			Result.bPlayNormalHitReact = BlockResult.bPlayNormalHitReact;
			Result.KnockbackScale = BaseDamage > 0.f ? BlockResult.DamageAfterBlock / BaseDamage : 0.f;
			Result.bApplyStun = BlockResult.bPlayNormalHitReact;
		}
		if (BlockResult.bParried)
		{
			Result.bParried = true;
			Result.ParryStaggerDuration = BlockResult.ParryStaggerDuration;
			Result.ParryStaggerPlayRate = BlockResult.ParryStaggerPlayRate;
		}
	}

	return Result;
}

void AWeapon::DispatchHitFeedback(AActor* HitActor, const FHitResult& HitPoint, const FWeaponHitResult& Result)
{
	// 弹反分支：对攻击方调弹反硬直（在 GetHit 之前，确保敌人先进入 EES_Parried）
	if (Result.bParried)
	{
		if (AEnemy* AttackerEnemy = Cast<AEnemy>(GetOwner()))
		{
			AttackerEnemy->ApplyParried(Result.ParryStaggerDuration, Result.ParryStaggerPlayRate, HitActor);
		}
	}

	// 写入命中上下文（所有命中，含格挡）
	if (ABaseCharacter* HitChar = Cast<ABaseCharacter>(HitActor))
	{
		HitChar->CachePendingHitContext(GetOwner(), Result.KnockbackScale, !Result.bPlayNormalHitReact, Result.bApplyStun);
	}

	// 受击反应+特效：所有命中都走 GetHit（内部按上下文分流）
	if (HitActor->Implements<UHitInterface>())
	{
		IHitInterface::Execute_GetHit(HitActor, HitPoint.ImpactPoint, GetOwner());
	}

	CameraShake();

	SetEnableHitStop(true);
	HitStop(HitActor);

	// 击中一次后加入黑名单，防止同一刀造成多次伤害（同类也加入，避免每帧重复判断）
	IgnoreActors.AddUnique(HitActor);
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
