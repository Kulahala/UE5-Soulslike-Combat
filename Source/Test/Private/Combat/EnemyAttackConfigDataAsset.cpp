// Fill out your copyright notice in the Description page of Project Settings.

#include "Combat/EnemyAttackConfigDataAsset.h"

#include "UObject/UnrealType.h"

int32 UEnemyAttackConfigDataAsset::ChooseAttackIndex(float DistanceToTarget) const
{
	float TotalWeight = 0.f;
	TArray<int32> CandidateIndices;
	CandidateIndices.Reserve(Attacks.Num());

	for (int32 Index = 0; Index < Attacks.Num(); ++Index)
	{
		const FEnemyAttackEntry& Entry = Attacks[Index];
		if (!IsEntrySelectable(Entry))
		{
			continue;
		}

		if (DistanceToTarget < Entry.MinDistance || DistanceToTarget > Entry.MaxDistance)
		{
			continue;
		}

		TotalWeight += Entry.Weight;
		CandidateIndices.Add(Index);
	}

	if (CandidateIndices.IsEmpty() || TotalWeight <= 0.f)
	{
		return INDEX_NONE;
	}

	// Weighted random: choose a point on the accumulated weight line.
	float Pick = FMath::FRandRange(0.f, TotalWeight);
	for (const int32 Index : CandidateIndices)
	{
		Pick -= Attacks[Index].Weight;
		if (Pick <= 0.f)
		{
			return Index;
		}
	}

	return CandidateIndices.Last();
}

int32 UEnemyAttackConfigDataAsset::ChooseAttackIntentIndex(int32 ExcludedAttackIndex) const
{
	float TotalWeight = 0.f;
	TArray<int32> CandidateIndices;
	CandidateIndices.Reserve(Attacks.Num());

	for (int32 Index = 0; Index < Attacks.Num(); ++Index)
	{
		if (Index == ExcludedAttackIndex)
		{
			continue;
		}

		const FEnemyAttackEntry& Entry = Attacks[Index];
		if (!IsEntrySelectable(Entry))
		{
			continue;
		}

		TotalWeight += Entry.Weight;
		CandidateIndices.Add(Index);
	}

	if (CandidateIndices.IsEmpty() || TotalWeight <= 0.f)
	{
		return INDEX_NONE;
	}

	float Pick = FMath::FRandRange(0.f, TotalWeight);
	for (const int32 Index : CandidateIndices)
	{
		Pick -= Attacks[Index].Weight;
		if (Pick <= 0.f)
		{
			return Index;
		}
	}

	return CandidateIndices.Last();
}

bool UEnemyAttackConfigDataAsset::IsEntrySelectable(const FEnemyAttackEntry& Entry) const
{
	if (!Entry.Montage || Entry.Weight <= 0.f)
	{
		return false;
	}

	if (Entry.DeliveryType == EEnemyAttackDeliveryType::Melee)
	{
		return true;
	}

	if (Entry.DeliveryType != EEnemyAttackDeliveryType::Projectile)
	{
		return false;
	}

	return Entry.MaxDistance > 0.f
		&& Entry.MaxDistance >= Entry.MinDistance
		&& Entry.ProjectileClass
		&& Entry.ProjectileDeliveryConfig.IsValid();
}

void UEnemyAttackConfigDataAsset::PostLoad()
{
	Super::PostLoad();

	NormalizeEntries();
	LogConfigWarnings();
}

#if WITH_EDITOR
void UEnemyAttackConfigDataAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	NormalizeEntries();
	LogConfigWarnings();
}
#endif

void UEnemyAttackConfigDataAsset::NormalizeEntries()
{
	for (FEnemyAttackEntry& Entry : Attacks)
	{
		Entry.MinDistance = FMath::Max(0.f, Entry.MinDistance);
		Entry.MaxDistance = FMath::Max(Entry.MinDistance, Entry.MaxDistance);
		Entry.MinCooldown = FMath::Max(0.f, Entry.MinCooldown);
		Entry.MaxCooldown = FMath::Max(Entry.MinCooldown, Entry.MaxCooldown);
		Entry.DamageMultiplier = FMath::Max(0.f, Entry.DamageMultiplier);
		Entry.BlockStaminaDamageMultiplier = FMath::Max(0.f, Entry.BlockStaminaDamageMultiplier);
		Entry.Weight = FMath::Max(0.f, Entry.Weight);
		Entry.WarpStopDistance = FMath::Max(0.f, Entry.WarpStopDistance);
		Entry.MaxWarpDistance = FMath::Max(0.f, Entry.MaxWarpDistance);
	}
}

void UEnemyAttackConfigDataAsset::LogConfigWarnings() const
{
#if WITH_EDITOR
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	if (Attacks.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: Attacks is empty; this enemy will have no DataAsset attack candidates."), *GetName());
	}

	for (int32 Index = 0; Index < Attacks.Num(); ++Index)
	{
		const FEnemyAttackEntry& Entry = Attacks[Index];
		if (!Entry.Montage)
		{
			UE_LOG(LogTemp, Warning, TEXT("%s: Attacks[%d] '%s' has no Montage."),
			       *GetName(), Index, *Entry.AttackName.ToString());
		}

		if (Entry.Weight <= 0.f)
		{
			UE_LOG(LogTemp, Warning, TEXT("%s: Attacks[%d] '%s' has zero weight and will not be selected."),
				       *GetName(), Index, *Entry.AttackName.ToString());
		}

		if (Entry.DeliveryType == EEnemyAttackDeliveryType::Projectile)
		{
			if (!Entry.ProjectileClass)
			{
				UE_LOG(LogTemp, Warning,
					TEXT("%s: Projectile Attacks[%d] '%s' has no ProjectileClass and will not be selected."),
					*GetName(), Index, *Entry.AttackName.ToString());
			}

			if (!Entry.ProjectileDeliveryConfig.IsValid())
			{
				UE_LOG(LogTemp, Warning,
					TEXT("%s: Projectile Attacks[%d] '%s' has an invalid ProjectileDeliveryConfig and will not be selected."),
					*GetName(), Index, *Entry.AttackName.ToString());
			}

			if (Entry.MaxDistance <= 0.f || Entry.MaxDistance < Entry.MinDistance)
			{
				UE_LOG(LogTemp, Warning,
					TEXT("%s: Projectile Attacks[%d] '%s' has an invalid range %.1f-%.1f and will not be selected."),
					*GetName(), Index, *Entry.AttackName.ToString(), Entry.MinDistance, Entry.MaxDistance);
			}

			if (Entry.bUseMotionWarping)
			{
				UE_LOG(LogTemp, Warning,
					TEXT("%s: Projectile Attacks[%d] '%s' enables Motion Warping; Projectile delivery ignores Motion Warping."),
					*GetName(), Index, *Entry.AttackName.ToString());
			}
		}

		if (Entry.bUseMotionWarping && Entry.DeliveryType == EEnemyAttackDeliveryType::Melee)
		{
			if (Entry.WarpTargetName == NAME_None)
			{
				UE_LOG(LogTemp, Warning,
				       TEXT("%s: Attacks[%d] '%s' enables Motion Warping but has no WarpTargetName."),
				       *GetName(), Index, *Entry.AttackName.ToString());
			}

			if (Entry.MaxWarpDistance <= 0.f)
			{
				UE_LOG(LogTemp, Warning,
				       TEXT("%s: Attacks[%d] '%s' enables Motion Warping but MaxWarpDistance is <= 0."),
				       *GetName(), Index, *Entry.AttackName.ToString());
			}

			if (Entry.WarpStopDistance >= Entry.MaxDistance)
			{
				UE_LOG(LogTemp, Warning,
				       TEXT("%s: Attacks[%d] '%s' has WarpStopDistance %.1f >= MaxDistance %.1f; warp landing may be behind or too close to the enemy."),
				       *GetName(), Index, *Entry.AttackName.ToString(), Entry.WarpStopDistance, Entry.MaxDistance);
			}
		}
	}
#endif
}
