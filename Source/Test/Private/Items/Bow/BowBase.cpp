#include "Items/Bow/BowBase.h"

#include "Combat/CombatProjectile.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Items/Bow/BowPhysicalProfileDataAsset.h"

namespace
{
	const FName BowArrowSocketName(TEXT("BowArrowSocket"));
}

ABowBase::ABowBase()
{
	SetDefaultEquipSocketName(FName(TEXT("LeftHandSocket")));

	BowSkeletalVisual = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("BowSkeletalVisual"));
	BowSkeletalVisual->SetupAttachment(GetMesh());
	BowSkeletalVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BowSkeletalVisual->SetCollisionResponseToAllChannels(ECR_Ignore);
	BowSkeletalVisual->SetGenerateOverlapEvents(false);
	BowSkeletalVisual->SetSimulatePhysics(false);
	BowSkeletalVisual->SetEnableGravity(false);
	BowSkeletalVisual->SetCanEverAffectNavigation(false);
	// Profile 是 Skeletal Mesh、Anim Class 与相对 Transform 的唯一作者化入口。
	BowSkeletalVisual->bEditableWhenInherited = false;
	BowSkeletalVisual->SetVisibility(false, true);
	BowSkeletalVisual->SetHiddenInGame(true, true);
}

FName ABowBase::GetBowArrowSocketName()
{
	return BowArrowSocketName;
}

void ABowBase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyPhysicalProfile();
}

void ABowBase::BeginPlay()
{
	Super::BeginPlay();
	ApplyPhysicalProfile();

	FString FailureReason;
	if (!ValidatePhysicalProfile(FailureReason))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: Bow PhysicalProfile is invalid: %s"), *GetName(), *FailureReason);
	}
}

bool ABowBase::TryGetLaunchTransform(FTransform& OutLaunchTransform, FString& OutFailureReason) const
{
	if (!ValidatePhysicalProfile(OutFailureReason))
	{
		return false;
	}

	OutLaunchTransform = BowSkeletalVisual->GetSocketTransform(BowArrowSocketName);
	return true;
}

bool ABowBase::NockPreparedProjectile(ACombatProjectile* PreparedProjectile) const
{
	auto RejectNock = [this, PreparedProjectile](const FString& FailureReason)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: Bow nock failed: %s"), *GetName(), *FailureReason);
		if (IsValid(PreparedProjectile))
		{
			PreparedProjectile->Destroy();
		}
		return false;
	};

	if (!IsValid(PreparedProjectile) || !PreparedProjectile->IsPreparedForActivation())
	{
		return RejectNock(TEXT("prepared projectile is unavailable."));
	}

	FTransform LaunchTransform;
	FString FailureReason;
	if (!TryGetLaunchTransform(LaunchTransform, FailureReason))
	{
		return RejectNock(FailureReason);
	}

	if (!PreparedProjectile->GetRootComponent())
	{
		return RejectNock(TEXT("prepared projectile has no root component."));
	}

	if (!PreparedProjectile->AttachToComponent(BowSkeletalVisual,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale, BowArrowSocketName))
	{
		return RejectNock(TEXT("prepared projectile root could not attach to BowArrowSocket."));
	}

	return true;
}

void ABowBase::SetBowPresentationState(EBowPresentationState NewState)
{
	BowPresentationState = NewState;
}

EBowPresentationState ABowBase::GetBowPresentationState() const
{
	return BowPresentationState;
}

void ABowBase::OnPhysicalProfileApplied()
{
}

void ABowBase::ApplyPhysicalProfile()
{
	FString FailureReason;
	const bool bHasUsableProfile = PhysicalProfile && PhysicalProfile->ValidateProfile(FailureReason);
	if (!bHasUsableProfile)
	{
		if (BowSkeletalVisual)
		{
			// 已装填箭由玩家 Bow 单独管理；不能在 Profile 切换时覆盖子组件状态。
			BowSkeletalVisual->SetVisibility(false, false);
			BowSkeletalVisual->SetHiddenInGame(true, false);
		}
		if (UStaticMeshComponent* StaticMesh = GetMesh())
		{
			StaticMesh->SetVisibility(true, false);
			StaticMesh->SetHiddenInGame(false, false);
		}
		return;
	}

	BowSkeletalVisual->SetSkeletalMesh(PhysicalProfile->GetBowSkeletalMesh());
	BowSkeletalVisual->SetRelativeTransform(PhysicalProfile->GetSkeletalVisualRelativeTransform());
	if (!BowSkeletalVisual->DoesSocketExist(BowArrowSocketName))
	{
		BowSkeletalVisual->SetVisibility(false, false);
		BowSkeletalVisual->SetHiddenInGame(true, false);
		if (UStaticMeshComponent* StaticMesh = GetMesh())
		{
			StaticMesh->SetVisibility(true, false);
			StaticMesh->SetHiddenInGame(false, false);
		}
		return;
	}

	BowSkeletalVisual->SetAnimInstanceClass(PhysicalProfile->GetBowAnimInstanceClass());
	// 不向下传播，避免把 LoadedArrowVisual 从其游戏状态强制改为可见。
	BowSkeletalVisual->SetVisibility(true, false);
	BowSkeletalVisual->SetHiddenInGame(false, false);

	if (UStaticMeshComponent* StaticMesh = GetMesh())
	{
		// BowSkeletalVisual 是该兼容 Mesh 的子组件，不能把父组件显隐传播给它。
		StaticMesh->SetVisibility(false, false);
		StaticMesh->SetHiddenInGame(true, false);
	}

	OnPhysicalProfileApplied();
}

bool ABowBase::ValidatePhysicalProfile(FString& OutFailureReason) const
{
	if (!PhysicalProfile)
	{
		OutFailureReason = TEXT("PhysicalProfile is empty.");
		return false;
	}

	if (!PhysicalProfile->ValidateProfile(OutFailureReason))
	{
		return false;
	}

	if (!BowSkeletalVisual)
	{
		OutFailureReason = TEXT("BowSkeletalVisual is unavailable.");
		return false;
	}

	if (!BowSkeletalVisual->DoesSocketExist(BowArrowSocketName))
	{
		OutFailureReason = TEXT("BowSkeletalVisual does not provide BowArrowSocket.");
		return false;
	}

	return true;
}
