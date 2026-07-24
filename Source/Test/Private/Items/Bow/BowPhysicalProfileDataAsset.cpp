#include "Items/Bow/BowPhysicalProfileDataAsset.h"

bool UBowPhysicalProfileDataAsset::ValidateProfile(FString& OutFailureReason) const
{
	if (!BowSkeletalMesh)
	{
		OutFailureReason = TEXT("BowSkeletalMesh is empty.");
		return false;
	}

	if (!BowAnimInstanceClass)
	{
		OutFailureReason = TEXT("BowAnimInstanceClass is empty.");
		return false;
	}

	if (!NockedArrowStaticMesh)
	{
		OutFailureReason = TEXT("NockedArrowStaticMesh is empty.");
		return false;
	}

	return true;
}
