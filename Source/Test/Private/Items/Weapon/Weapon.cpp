// Fill out your copyright notice in the Description page of Project Settings.

#include "Items/Weapon/Weapon.h"
#include "Character/BaseCharacter.h"
#include "Character/MyCharacter.h"
#include "Combat/CombatHitResolver.h"
#include "Enemy/Enemy.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Utils/DebugDrawHelper.h"
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

bool AWeapon::AttachMeshToSocket(USceneComponent* Parent, const FName& SocketName)
{
	if (!Parent || SocketName == NAME_None || !Parent->DoesSocketExist(SocketName))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s failed to attach weapon: parent or socket '%s' is invalid."),
			*GetName(), *SocketName.ToString());
		return false;
	}

	FAttachmentTransformRules AttachmentRules(EAttachmentRule::SnapToTarget, true);
	if (!AttachToComponent(Parent, AttachmentRules, SocketName))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s failed to attach weapon to socket '%s'."),
			*GetName(), *SocketName.ToString());
		return false;
	}

	// 叠加装备旋转偏移，修正不同武器模型的本地朝向差异
	if (!EquipRotationOffset.IsNearlyZero())
	{
		SetActorRelativeRotation(EquipRotationOffset);
	}

	return true;
}

bool AWeapon::Equip(USceneComponent* Parent, const FName& SocketName, AActor* NewOwner, APawn* NewInstigator,
	bool bPlayEquipSound)
{
	SetOwner(NewOwner);
	SetInstigator(NewInstigator);

	if (!AttachMeshToSocket(Parent, SocketName))
	{
		return false;
	}

	ItemState = EItemState::EIS_Equipped;
	DisablePickupCollision();

	if (GetEffect())
	{
		GetEffect()->Deactivate();
	}

	// 非常关键：绑定武器的主人，避免砍中自己
	if (Parent && Parent->GetOwner())
	{
		SetOwner(Parent->GetOwner());
	}

	if (bPlayEquipSound)
	{
		PlayEquipSound();
	}

	return true;
}

void AWeapon::PlayEquipSound() const
{
	if (EquipSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, EquipSound, GetActorLocation());
	}
}

void AWeapon::OnPickup_Implementation(AActor* Picker)
{
	if (AMyCharacter* Character = Cast<AMyCharacter>(Picker))
	{
		TryClaimPersistentWorldPickup(Character);
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
	if (const AEnemy* OwnerEnemy = Cast<AEnemy>(GetOwner()); OwnerEnemy && OwnerEnemy->IsEncounterDormant())
	{
		return;
	}

	FVector CurrentCenter = BoxTrace->GetComponentLocation();
	FRotator TraceRotation = BoxTrace->GetComponentRotation();
	FVector BoxHalfExtent = BoxTrace->GetScaledBoxExtent();

	TArray<AActor*> ActorsToIgnore;
	BuildIgnoreList(ActorsToIgnore);

	FHitResult HitPoint;
	const EDrawDebugTrace::Type DrawDebugTrace = FDebugDrawHelper::IsRangesEnabled()
		? EDrawDebugTrace::ForOneFrame
		: EDrawDebugTrace::None;

	// 盒体扫掠：连接相邻两帧路径，防止高速挥砍漏判
	bool bHit = UKismetSystemLibrary::BoxTraceSingle(
		this, TraceCenterOld, CurrentCenter, BoxHalfExtent, TraceRotation,
		UEngineTypes::ConvertToTraceType(ECC_WorldDynamic), false, ActorsToIgnore, DrawDebugTrace,
		HitPoint, true, FLinearColor::Red, FLinearColor::Green, 3.f);

	// 记录本帧位置，留给下帧做参考
	TraceCenterOld = CurrentCenter;
	TraceRotationOld = TraceRotation;

	if (bHit && HitPoint.GetActor())
	{
		AActor* HitActor = HitPoint.GetActor();
		float IncomingDamage = Damage;
		float PoiseDamage = 0.f;
		bool bApplyPoiseDamage = false;
		float BlockStaminaDamageMultiplier = 1.f;
		bool bCanBeParried = true;

		if (ABaseCharacter* Attacker = Cast<ABaseCharacter>(GetOwner()))
		{
			IncomingDamage *= Attacker->GetAttackDamageMultiplier();
			PoiseDamage = Attacker->GetCurrentPoiseDamage();
			bApplyPoiseDamage = true;
			BlockStaminaDamageMultiplier = Attacker->GetBlockStaminaDamageMultiplier();
			bCanBeParried = Attacker->CanCurrentAttackBeParried();
		}

		FCombatHitRequest Request;
		Request.Attacker = GetOwner();
		Request.DamageCauser = this;
		Request.EventInstigator = GetInstigatorController();
		Request.HitActor = HitActor;
		Request.HitResult = HitPoint;
		Request.IncomingDamage = IncomingDamage;
		Request.PoiseDamage = PoiseDamage;
		Request.bApplyPoiseDamage = bApplyPoiseDamage;
		Request.BlockStaminaDamageMultiplier = BlockStaminaDamageMultiplier;
		Request.bCanBeParried = bCanBeParried;

		const FCombatHitResult Result = FCombatHitResolver::ResolveAndApply(Request);
		if (Result.bSuppressed)
		{
			// 预放置参与者在 Controller 激活前不接受本次挥砍，也不产生命中反馈。
			IgnoreActors.AddUnique(HitActor);
			return;
		}

		// 武器专属表现仍由武器拥有，避免投射物继承相机震动、卡肉或挥砍黑名单。
		CameraShake();
		SetEnableHitStop(true);
		HitStop(HitActor);
		IgnoreActors.AddUnique(HitActor);
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
