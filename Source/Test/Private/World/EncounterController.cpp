#include "World/EncounterController.h"

#include "Character/MyCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Enemy/Enemy.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	constexpr float BoundarySegmentOverlap = 2.f;
	constexpr float TransformTolerance = 0.01f;
	constexpr float SplinePlanarTolerance = 0.1f;
	constexpr float SplinePointTolerance = 0.1f;
	constexpr float SplineSafeRegionProbeMinHalfExtent = 1.f;
	constexpr int32 MaxSplineSafeRegionProbeCells = 16384;
	constexpr float SqrtTwo = 1.41421356237f;

	struct FSplineSafeRegionProbeCell
	{
		FVector2D Center = FVector2D::ZeroVector;
		float HalfExtent = 0.f;
		float SignedDistance = 0.f;
		float MaximumSignedDistance = 0.f;
	};

	void PushSplineSafeRegionProbeCell(TArray<FSplineSafeRegionProbeCell>& Heap,
		const FSplineSafeRegionProbeCell& NewCell)
	{
		int32 Index = Heap.Add(NewCell);
		while (Index > 0)
		{
			const int32 ParentIndex = (Index - 1) / 2;
			if (Heap[ParentIndex].MaximumSignedDistance >= Heap[Index].MaximumSignedDistance)
			{
				break;
			}

			Heap.Swap(ParentIndex, Index);
			Index = ParentIndex;
		}
	}

	FSplineSafeRegionProbeCell PopSplineSafeRegionProbeCell(TArray<FSplineSafeRegionProbeCell>& Heap)
	{
		check(!Heap.IsEmpty());

		const FSplineSafeRegionProbeCell Result = Heap[0];
		const FSplineSafeRegionProbeCell LastCell = Heap.Pop(EAllowShrinking::No);
		if (Heap.IsEmpty())
		{
			return Result;
		}

		Heap[0] = LastCell;
		int32 Index = 0;
		while (true)
		{
			const int32 LeftChildIndex = Index * 2 + 1;
			const int32 RightChildIndex = LeftChildIndex + 1;
			int32 LargestIndex = Index;

			if (LeftChildIndex < Heap.Num() &&
				Heap[LeftChildIndex].MaximumSignedDistance > Heap[LargestIndex].MaximumSignedDistance)
			{
				LargestIndex = LeftChildIndex;
			}

			if (RightChildIndex < Heap.Num() &&
				Heap[RightChildIndex].MaximumSignedDistance > Heap[LargestIndex].MaximumSignedDistance)
			{
				LargestIndex = RightChildIndex;
			}

			if (LargestIndex == Index)
			{
				break;
			}

			Heap.Swap(Index, LargestIndex);
			Index = LargestIndex;
		}

		return Result;
	}

	float CrossProduct2D(const FVector2D& A, const FVector2D& B, const FVector2D& C)
	{
		return (B.X - A.X) * (C.Y - A.Y) - (B.Y - A.Y) * (C.X - A.X);
	}

	bool IsPointOnSegment2D(const FVector2D& Point, const FVector2D& SegmentStart, const FVector2D& SegmentEnd)
	{
		if (!FMath::IsNearlyZero(CrossProduct2D(SegmentStart, SegmentEnd, Point), SplinePointTolerance))
		{
			return false;
		}

		return Point.X >= FMath::Min(SegmentStart.X, SegmentEnd.X) - SplinePointTolerance &&
			Point.X <= FMath::Max(SegmentStart.X, SegmentEnd.X) + SplinePointTolerance &&
			Point.Y >= FMath::Min(SegmentStart.Y, SegmentEnd.Y) - SplinePointTolerance &&
			Point.Y <= FMath::Max(SegmentStart.Y, SegmentEnd.Y) + SplinePointTolerance;
	}

	bool DoSegmentsIntersect2D(const FVector2D& FirstStart, const FVector2D& FirstEnd,
		const FVector2D& SecondStart, const FVector2D& SecondEnd)
	{
		const float FirstStartSide = CrossProduct2D(FirstStart, FirstEnd, SecondStart);
		const float FirstEndSide = CrossProduct2D(FirstStart, FirstEnd, SecondEnd);
		const float SecondStartSide = CrossProduct2D(SecondStart, SecondEnd, FirstStart);
		const float SecondEndSide = CrossProduct2D(SecondStart, SecondEnd, FirstEnd);

		const bool bFirstStraddlesSecond =
			(FirstStartSide > SplinePointTolerance && FirstEndSide < -SplinePointTolerance) ||
			(FirstStartSide < -SplinePointTolerance && FirstEndSide > SplinePointTolerance);
		const bool bSecondStraddlesFirst =
			(SecondStartSide > SplinePointTolerance && SecondEndSide < -SplinePointTolerance) ||
			(SecondStartSide < -SplinePointTolerance && SecondEndSide > SplinePointTolerance);
		if (bFirstStraddlesSecond && bSecondStraddlesFirst)
		{
			return true;
		}

		return IsPointOnSegment2D(SecondStart, FirstStart, FirstEnd) ||
			IsPointOnSegment2D(SecondEnd, FirstStart, FirstEnd) ||
			IsPointOnSegment2D(FirstStart, SecondStart, SecondEnd) ||
			IsPointOnSegment2D(FirstEnd, SecondStart, SecondEnd);
	}

	bool AreSplineEdgesAdjacent(int32 FirstEdgeIndex, int32 SecondEdgeIndex, int32 PointCount)
	{
		return FirstEdgeIndex == SecondEdgeIndex ||
			(FirstEdgeIndex + 1) % PointCount == SecondEdgeIndex ||
			(SecondEdgeIndex + 1) % PointCount == FirstEdgeIndex;
	}

	float GetPointToSegmentDistanceSquared2D(const FVector2D& Point, const FVector2D& SegmentStart,
		const FVector2D& SegmentEnd)
	{
		const FVector2D Segment = SegmentEnd - SegmentStart;
		const float SegmentLengthSquared = Segment.SizeSquared();
		if (SegmentLengthSquared <= KINDA_SMALL_NUMBER)
		{
			return FVector2D::DistSquared(Point, SegmentStart);
		}

		const float Projection = FMath::Clamp(FVector2D::DotProduct(Point - SegmentStart, Segment) / SegmentLengthSquared, 0.f, 1.f);
		return FVector2D::DistSquared(Point, SegmentStart + Segment * Projection);
	}

	float GetSignedPolygonArea(const TArray<FVector2D>& Points)
	{
		float TwiceArea = 0.f;
		for (int32 PointIndex = 0; PointIndex < Points.Num(); ++PointIndex)
		{
			const FVector2D& CurrentPoint = Points[PointIndex];
			const FVector2D& NextPoint = Points[(PointIndex + 1) % Points.Num()];
			TwiceArea += CurrentPoint.X * NextPoint.Y - NextPoint.X * CurrentPoint.Y;
		}

		return TwiceArea * 0.5f;
	}
}

AEncounterController::AEncounterController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	RectangularCommitVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("RectangularCommitVolume"));
	RectangularCommitVolume->SetupAttachment(Root);
	RectangularCommitVolume->InitBoxExtent(FVector(1.f, 1.f, 1.f));
	RectangularCommitVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RectangularCommitVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	RectangularCommitVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	RectangularCommitVolume->SetGenerateOverlapEvents(false);

	RadialCommitVolume = CreateDefaultSubobject<UCapsuleComponent>(TEXT("RadialCommitVolume"));
	RadialCommitVolume->SetupAttachment(Root);
	RadialCommitVolume->InitCapsuleSize(1.f, 1.f);
	RadialCommitVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RadialCommitVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	RadialCommitVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	RadialCommitVolume->SetGenerateOverlapEvents(false);

	SplineCommitCandidateVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("SplineCommitCandidateVolume"));
	SplineCommitCandidateVolume->SetupAttachment(Root);
	SplineCommitCandidateVolume->InitBoxExtent(FVector(1.f, 1.f, 1.f));
	SplineCommitCandidateVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SplineCommitCandidateVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	SplineCommitCandidateVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	SplineCommitCandidateVolume->SetGenerateOverlapEvents(false);

	BoundarySpline = CreateDefaultSubobject<USplineComponent>(TEXT("BoundarySpline"));
	BoundarySpline->SetupAttachment(Root);
	BoundarySpline->ClearSplinePoints(false);
	BoundarySpline->AddSplinePoint(FVector(600.f, 600.f, 0.f), ESplineCoordinateSpace::Local, false);
	BoundarySpline->AddSplinePoint(FVector(-600.f, 600.f, 0.f), ESplineCoordinateSpace::Local, false);
	BoundarySpline->AddSplinePoint(FVector(-600.f, -600.f, 0.f), ESplineCoordinateSpace::Local, false);
	BoundarySpline->AddSplinePoint(FVector(600.f, -600.f, 0.f), ESplineCoordinateSpace::Local, false);
	for (int32 PointIndex = 0; PointIndex < BoundarySpline->GetNumberOfSplinePoints(); ++PointIndex)
	{
		BoundarySpline->SetSplinePointType(PointIndex, ESplinePointType::Linear, false);
	}
	BoundarySpline->SetClosedLoop(true, false);
	BoundarySpline->bSplineHasBeenEdited = true;
	BoundarySpline->bDrawDebug = true;
	BoundarySpline->UpdateSpline();

	static ConstructorHelpers::FObjectFinder<UStaticMesh> BoundaryCubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (BoundaryCubeFinder.Succeeded())
	{
		BoundaryVisualMesh = BoundaryCubeFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BoundaryMaterialFinder(
		TEXT("/Game/_GAME/BP/Effects/Materials/Gameplay/M_EncounterBoundary.M_EncounterBoundary"));
	if (BoundaryMaterialFinder.Succeeded())
	{
		BoundaryVisualMaterial = BoundaryMaterialFinder.Object;
	}
}

void AEncounterController::BeginPlay()
{
	Super::BeginPlay();

	bStaticConfigurationValid = ValidateConfiguration();
	if (!bStaticConfigurationValid)
	{
		SetCommitVolumeEnabled(RectangularCommitVolume, false);
		SetCommitVolumeEnabled(RadialCommitVolume, false);
		SetCommitVolumeEnabled(SplineCommitCandidateVolume, false);
		SetBoundariesClosed(false);
		return;
	}

	if (!BuildRuntimeBoundaries())
	{
		UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' could not build its runtime boundaries and is disabled."),
			*EncounterId.ToString());
		bStaticConfigurationValid = false;
		SetBoundariesClosed(false);
		return;
	}

	SetBoundariesClosed(false);
	RectangularCommitVolume->OnComponentBeginOverlap.AddDynamic(this, &AEncounterController::OnCommitVolumeBeginOverlap);
	RectangularCommitVolume->OnComponentEndOverlap.AddDynamic(this, &AEncounterController::OnCommitVolumeEndOverlap);
	RadialCommitVolume->OnComponentBeginOverlap.AddDynamic(this, &AEncounterController::OnCommitVolumeBeginOverlap);
	RadialCommitVolume->OnComponentEndOverlap.AddDynamic(this, &AEncounterController::OnCommitVolumeEndOverlap);
	SplineCommitCandidateVolume->OnComponentBeginOverlap.AddDynamic(this, &AEncounterController::OnCommitVolumeBeginOverlap);
	SplineCommitCandidateVolume->OnComponentEndOverlap.AddDynamic(this, &AEncounterController::OnCommitVolumeEndOverlap);

	bAwaitingPlayerSetup = true;
	RefreshTickEnabled();
	TryInitializeForPlayer();
}

void AEncounterController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (RectangularCommitVolume)
	{
		RectangularCommitVolume->OnComponentBeginOverlap.RemoveDynamic(this, &AEncounterController::OnCommitVolumeBeginOverlap);
		RectangularCommitVolume->OnComponentEndOverlap.RemoveDynamic(this, &AEncounterController::OnCommitVolumeEndOverlap);
	}

	if (RadialCommitVolume)
	{
		RadialCommitVolume->OnComponentBeginOverlap.RemoveDynamic(this, &AEncounterController::OnCommitVolumeBeginOverlap);
		RadialCommitVolume->OnComponentEndOverlap.RemoveDynamic(this, &AEncounterController::OnCommitVolumeEndOverlap);
	}

	if (SplineCommitCandidateVolume)
	{
		SplineCommitCandidateVolume->OnComponentBeginOverlap.RemoveDynamic(this, &AEncounterController::OnCommitVolumeBeginOverlap);
		SplineCommitCandidateVolume->OnComponentEndOverlap.RemoveDynamic(this, &AEncounterController::OnCommitVolumeEndOverlap);
	}

	bAwaitingPlayerSetup = false;
	PendingCommitPlayer.Reset();
	InitialSafeRegionPlayer.Reset();
	SetActorTickEnabled(false);
	SetCommitVolumeEnabled(RectangularCommitVolume, false);
	SetCommitVolumeEnabled(RadialCommitVolume, false);
	SetCommitVolumeEnabled(SplineCommitCandidateVolume, false);
	SetBoundariesClosed(false);
	ReleaseParticipants();
	DestroyRuntimeBoundaries();

	Super::EndPlay(EndPlayReason);
}

void AEncounterController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bAwaitingPlayerSetup)
	{
		TryInitializeForPlayer();
		if (bAwaitingPlayerSetup)
		{
			return;
		}
	}

	if (!bConfigurationValid || EncounterState != EEncounterState::Idle)
	{
		RefreshTickEnabled();
		return;
	}

	if (AMyCharacter* InitialPlayer = InitialSafeRegionPlayer.Get())
	{
		if (IsPlayerSafelyInsideCommitRegion(InitialPlayer))
		{
			return;
		}

		bHasObservedPlayerOutsideCommitRegion = true;
		InitialSafeRegionPlayer.Reset();
		if (IsPlayerOverlappingCommitVolume(InitialPlayer))
		{
			BeginPendingCommit(InitialPlayer);
		}
	}

	AMyCharacter* Player = PendingCommitPlayer.Get();
	if (!Player)
	{
		RefreshTickEnabled();
		return;
	}

	if (!IsPlayerOverlappingCommitVolume(Player))
	{
		bHasObservedPlayerOutsideCommitRegion = true;
		ClearPendingCommit();
		return;
	}

	if (!IsPlayerSafelyInsideCommitRegion(Player))
	{
		bHasObservedPlayerOutsideCommitRegion = true;
		return;
	}

	TryActivate(Player);
	ClearPendingCommit();
}

bool AEncounterController::TryActivate(AMyCharacter* Player)
{
	if (!bConfigurationValid || EncounterState != EEncounterState::Idle || !Player ||
		!bHasObservedPlayerOutsideCommitRegion || !IsPlayerSafelyInsideCommitRegion(Player))
	{
		return false;
	}

	for (AEnemy* Participant : PreplacedParticipants)
	{
		if (!IsValid(Participant) || Participant->GetEnemyState() == EEnemyState::EES_Dead)
		{
			UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' cannot activate because a participant is invalid or dead."),
				*EncounterId.ToString());
			return false;
		}
	}

	RemainingParticipants.Reset();
	for (AEnemy* Participant : PreplacedParticipants)
	{
		if (!Participant->ActivateForEncounter(this, Player))
		{
			UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' failed to activate participant '%s'. Restoring Idle state."),
				*EncounterId.ToString(), *GetNameSafe(Participant));

			for (AEnemy* RollbackParticipant : PreplacedParticipants)
			{
				if (IsValid(RollbackParticipant))
				{
					RollbackParticipant->SetEncounterDormant(this);
				}
			}

			RemainingParticipants.Reset();
			return false;
		}

		RemainingParticipants.Add(Participant);
	}

	SetEncounterState(EEncounterState::Active);
	SetBoundariesClosed(true);
	return true;
}

void AEncounterController::OnCommitVolumeBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OverlappedComponent != GetActiveCommitVolume() || !bConfigurationValid || EncounterState != EEncounterState::Idle)
	{
		return;
	}

	if (AMyCharacter* Player = Cast<AMyCharacter>(OtherActor))
	{
		if (bHasObservedPlayerOutsideCommitRegion)
		{
			BeginPendingCommit(Player);
		}
	}
}

void AEncounterController::OnCommitVolumeEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (OverlappedComponent != GetActiveCommitVolume())
	{
		return;
	}

	if (AMyCharacter* Player = Cast<AMyCharacter>(OtherActor))
	{
		bHasObservedPlayerOutsideCommitRegion = true;
		if (InitialSafeRegionPlayer.Get() == Player)
		{
			InitialSafeRegionPlayer.Reset();
		}

		if (PendingCommitPlayer.Get() == Player)
		{
			ClearPendingCommit();
		}
		else
		{
			RefreshTickEnabled();
		}
	}
}

bool AEncounterController::ValidateConfiguration() const
{
	if (EncounterId == NAME_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("Encounter controller '%s' has no EncounterId and is disabled."), *GetName());
		return false;
	}

	if (!GetActorScale3D().Equals(FVector::OneVector, KINDA_SMALL_NUMBER))
	{
		UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' must keep actor scale at (1, 1, 1); scale the BoundaryConfig or spline points instead."),
			*EncounterId.ToString());
		return false;
	}

	const FRotator ActorRotation = GetActorRotation();
	if (!FMath::IsNearlyZero(ActorRotation.Pitch, TransformTolerance) ||
		!FMath::IsNearlyZero(ActorRotation.Roll, TransformTolerance))
	{
		UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' only supports yaw rotation because its origin defines the arena floor plane."),
			*EncounterId.ToString());
		return false;
	}

	if (BoundaryConfig.WallThickness <= 0.f || BoundaryConfig.WallHeight <= 0.f || BoundaryConfig.SealClearance < 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' has invalid boundary thickness, height, or seal clearance."),
			*EncounterId.ToString());
		return false;
	}

	switch (BoundaryConfig.Shape)
	{
	case EEncounterBoundaryShape::Rectangle:
		if (BoundaryConfig.InteriorHalfExtents.X <= 0.f || BoundaryConfig.InteriorHalfExtents.Y <= 0.f)
		{
			UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' has invalid rectangle interior half extents."), *EncounterId.ToString());
			return false;
		}
		break;

	case EEncounterBoundaryShape::Radial:
		if (BoundaryConfig.InteriorRadius <= 0.f || BoundaryConfig.RadialSegmentCount < 8 || BoundaryConfig.RadialSegmentCount > 32)
		{
			UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' requires a positive radial interior radius and 8-32 boundary segments."),
				*EncounterId.ToString());
			return false;
		}
		break;

	case EEncounterBoundaryShape::Spline:
		if (!ValidateSplineBoundary())
		{
			return false;
		}
		break;

	default:
		UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' has an unsupported boundary shape."), *EncounterId.ToString());
		return false;
	}

	for (TActorIterator<AEncounterController> It(GetWorld()); It; ++It)
	{
		if (*It != this && It->EncounterId == EncounterId)
		{
			UE_LOG(LogTemp, Warning, TEXT("EncounterId '%s' is duplicated by '%s' and '%s'. Both controllers stay disabled."),
				*EncounterId.ToString(), *GetName(), *It->GetName());
			return false;
		}
	}

	if (PreplacedParticipants.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' has no preplaced participants. Wave-only encounters are not available until TODO-02C."),
			*EncounterId.ToString());
		return false;
	}

	TSet<AEnemy*> UniqueParticipants;
	for (AEnemy* Participant : PreplacedParticipants)
	{
		if (!IsValid(Participant))
		{
			UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' has an invalid preplaced participant."), *EncounterId.ToString());
			return false;
		}

		if (UniqueParticipants.Contains(Participant))
		{
			UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' lists participant '%s' more than once."),
				*EncounterId.ToString(), *Participant->GetName());
			return false;
		}

		UniqueParticipants.Add(Participant);
	}

	return true;
}

bool AEncounterController::ValidateSplineBoundary() const
{
	if (!BoundarySpline)
	{
		UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' has no BoundarySpline and is disabled."), *EncounterId.ToString());
		return false;
	}

	if (!BoundarySpline->IsClosedLoop())
	{
		UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' requires BoundarySpline to be a closed loop."), *EncounterId.ToString());
		return false;
	}

	const int32 PointCount = BoundarySpline->GetNumberOfSplinePoints();
	if (PointCount < 3)
	{
		UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' requires at least three spline points."), *EncounterId.ToString());
		return false;
	}

	TArray<FVector2D> Points;
	Points.Reserve(PointCount);
	for (int32 PointIndex = 0; PointIndex < PointCount; ++PointIndex)
	{
		if (BoundarySpline->GetSplinePointType(PointIndex) != ESplinePointType::Linear)
		{
			UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' only supports Linear BoundarySpline points. Point %d is curved."),
				*EncounterId.ToString(), PointIndex);
			return false;
		}

		const FVector SplineLocalPoint = BoundarySpline->GetLocationAtSplinePoint(PointIndex, ESplineCoordinateSpace::Local);
		const FVector WorldPoint = BoundarySpline->GetComponentTransform().TransformPosition(SplineLocalPoint);
		const FVector ControllerLocalPoint = GetActorTransform().InverseTransformPositionNoScale(WorldPoint);
		if (!FMath::IsNearlyZero(ControllerLocalPoint.Z, SplinePlanarTolerance))
		{
			UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' only supports BoundarySpline points on local Z=0. Point %d has Z=%.2f."),
				*EncounterId.ToString(), PointIndex, ControllerLocalPoint.Z);
			return false;
		}

		Points.Emplace(ControllerLocalPoint.X, ControllerLocalPoint.Y);
	}

	for (int32 FirstPointIndex = 0; FirstPointIndex < PointCount; ++FirstPointIndex)
	{
		for (int32 SecondPointIndex = FirstPointIndex + 1; SecondPointIndex < PointCount; ++SecondPointIndex)
		{
			if (FVector2D::DistSquared(Points[FirstPointIndex], Points[SecondPointIndex]) <= FMath::Square(SplinePointTolerance))
			{
				UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' has repeated BoundarySpline points (%d and %d). Remove duplicate or zero-length edges."),
					*EncounterId.ToString(), FirstPointIndex, SecondPointIndex);
				return false;
			}
		}
	}

	for (int32 EdgeIndex = 0; EdgeIndex < PointCount; ++EdgeIndex)
	{
		const FVector2D& EdgeStart = Points[EdgeIndex];
		const FVector2D& EdgeEnd = Points[(EdgeIndex + 1) % PointCount];
		if (FVector2D::DistSquared(EdgeStart, EdgeEnd) <= FMath::Square(SplinePointTolerance))
		{
			UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' has a zero-length BoundarySpline edge at point %d."),
				*EncounterId.ToString(), EdgeIndex);
			return false;
		}

		for (int32 OtherEdgeIndex = EdgeIndex + 1; OtherEdgeIndex < PointCount; ++OtherEdgeIndex)
		{
			if (AreSplineEdgesAdjacent(EdgeIndex, OtherEdgeIndex, PointCount))
			{
				continue;
			}

			const FVector2D& OtherStart = Points[OtherEdgeIndex];
			const FVector2D& OtherEnd = Points[(OtherEdgeIndex + 1) % PointCount];
			if (DoSegmentsIntersect2D(EdgeStart, EdgeEnd, OtherStart, OtherEnd))
			{
				UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' BoundarySpline self-intersects between edges %d and %d."),
					*EncounterId.ToString(), EdgeIndex, OtherEdgeIndex);
				return false;
			}
		}
	}

	if (FMath::IsNearlyZero(GetSignedPolygonArea(Points), FMath::Square(SplinePointTolerance)))
	{
		UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' BoundarySpline encloses no usable area."), *EncounterId.ToString());
		return false;
	}

	return true;
}

bool AEncounterController::GetSplineBoundaryPoints(TArray<FVector2D>& OutPoints) const
{
	OutPoints.Reset();
	if (!BoundarySpline)
	{
		return false;
	}

	const int32 PointCount = BoundarySpline->GetNumberOfSplinePoints();
	if (PointCount < 3)
	{
		return false;
	}

	OutPoints.Reserve(PointCount);
	for (int32 PointIndex = 0; PointIndex < PointCount; ++PointIndex)
	{
		const FVector SplineLocalPoint = BoundarySpline->GetLocationAtSplinePoint(PointIndex, ESplineCoordinateSpace::Local);
		const FVector WorldPoint = BoundarySpline->GetComponentTransform().TransformPosition(SplineLocalPoint);
		const FVector ControllerLocalPoint = GetActorTransform().InverseTransformPositionNoScale(WorldPoint);
		OutPoints.Emplace(ControllerLocalPoint.X, ControllerLocalPoint.Y);
	}

	return true;
}

bool AEncounterController::ConfigureCommitVolumes(AMyCharacter* Player)
{
	if (!Player || !Player->GetCapsuleComponent())
	{
		UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' could not read the player capsule for its safe commit region."),
			*EncounterId.ToString());
		return false;
	}

	const UCapsuleComponent* PlayerCapsule = Player->GetCapsuleComponent();
	const float PlayerRadius = PlayerCapsule->GetScaledCapsuleRadius();
	const float PlayerHalfHeight = PlayerCapsule->GetScaledCapsuleHalfHeight();
	CommitSafetyInset = PlayerRadius + BoundaryConfig.SealClearance;
	const float CommitVolumeHalfHeight = FMath::Max(BoundaryConfig.WallHeight * 0.5f, PlayerHalfHeight);
	if (BoundaryConfig.WallHeight < PlayerHalfHeight * 2.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' wall height must cover the full player capsule to form a valid boundary."),
			*EncounterId.ToString());
		return false;
	}

	CommitHalfExtents = FVector2D::ZeroVector;
	CommitRadius = 0.f;
	SplineBoundaryPoints.Reset();
	SetCommitVolumeEnabled(RectangularCommitVolume, false);
	SetCommitVolumeEnabled(RadialCommitVolume, false);
	SetCommitVolumeEnabled(SplineCommitCandidateVolume, false);

	if (BoundaryConfig.Shape == EEncounterBoundaryShape::Rectangle)
	{
		CommitHalfExtents = BoundaryConfig.InteriorHalfExtents - FVector2D(CommitSafetyInset, CommitSafetyInset);
		if (CommitHalfExtents.X <= 0.f || CommitHalfExtents.Y <= 0.f)
		{
			UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' rectangle is too small for the player capsule plus SealClearance."),
				*EncounterId.ToString());
			return false;
		}

		RectangularCommitVolume->SetBoxExtent(FVector(CommitHalfExtents.X, CommitHalfExtents.Y, CommitVolumeHalfHeight), false);
		RectangularCommitVolume->SetRelativeLocation(FVector(0.f, 0.f, PlayerHalfHeight));
		SetCommitVolumeEnabled(RectangularCommitVolume, true);
		return true;
	}

	if (BoundaryConfig.Shape == EEncounterBoundaryShape::Radial)
	{
		CommitRadius = BoundaryConfig.InteriorRadius - CommitSafetyInset;
		if (CommitRadius <= 0.f)
		{
			UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' radial arena is too small for the player capsule plus SealClearance."),
				*EncounterId.ToString());
			return false;
		}

		RadialCommitVolume->SetCapsuleSize(CommitRadius, FMath::Max(CommitRadius, CommitVolumeHalfHeight), false);
		RadialCommitVolume->SetRelativeLocation(FVector(0.f, 0.f, PlayerHalfHeight));
		SetCommitVolumeEnabled(RadialCommitVolume, true);
		return true;
	}

	return ConfigureSplineCommitVolume(Player, CommitVolumeHalfHeight);
}

bool AEncounterController::ConfigureSplineCommitVolume(AMyCharacter* Player, float CommitVolumeHalfHeight)
{
	if (!GetSplineBoundaryPoints(SplineBoundaryPoints))
	{
		UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' could not read BoundarySpline points for its safe commit region."),
			*EncounterId.ToString());
		return false;
	}

	FVector2D Minimum = SplineBoundaryPoints[0];
	FVector2D Maximum = SplineBoundaryPoints[0];
	for (const FVector2D& Point : SplineBoundaryPoints)
	{
		Minimum.X = FMath::Min(Minimum.X, Point.X);
		Minimum.Y = FMath::Min(Minimum.Y, Point.Y);
		Maximum.X = FMath::Max(Maximum.X, Point.X);
		Maximum.Y = FMath::Max(Maximum.Y, Point.Y);
	}

	const FVector2D BoundsSize = Maximum - Minimum;
	if (BoundsSize.X <= CommitSafetyInset * 2.f || BoundsSize.Y <= CommitSafetyInset * 2.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' BoundarySpline is too narrow for the player capsule plus SealClearance; no usable safe commit region exists."),
			*EncounterId.ToString());
		return false;
	}

	const FVector2D BoundsCenter = (Minimum + Maximum) * 0.5f;
	const FVector2D BoundsHalfExtents = BoundsSize * 0.5f;
	if (!HasUsableSplineCommitRegion(BoundsCenter, BoundsSize))
	{
		UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' BoundarySpline has no verified safe commit region for the current player capsule plus SealClearance; the controller is disabled before it claims participants."),
			*EncounterId.ToString());
		return false;
	}

	SplineCommitCandidateVolume->SetBoxExtent(FVector(BoundsHalfExtents.X, BoundsHalfExtents.Y, CommitVolumeHalfHeight), false);
	SplineCommitCandidateVolume->SetRelativeLocation(FVector(BoundsCenter.X, BoundsCenter.Y, Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
	SetCommitVolumeEnabled(SplineCommitCandidateVolume, true);
	return true;
}

bool AEncounterController::HasUsableSplineCommitRegion(const FVector2D& BoundsCenter, const FVector2D& BoundsSize) const
{
	const float RootHalfExtent = FMath::Max(BoundsSize.X, BoundsSize.Y) * 0.5f;
	if (RootHalfExtent <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	TArray<FSplineSafeRegionProbeCell> ProbeHeap;
	ProbeHeap.Reserve(MaxSplineSafeRegionProbeCells);

	auto MakeProbeCell = [this](const FVector2D& Center, float HalfExtent)
	{
		FSplineSafeRegionProbeCell Cell;
		Cell.Center = Center;
		Cell.HalfExtent = HalfExtent;
		Cell.SignedDistance = GetSignedDistanceToSplineBoundary(Center);
		Cell.MaximumSignedDistance = Cell.SignedDistance + HalfExtent * SqrtTwo;
		return Cell;
	};

	PushSplineSafeRegionProbeCell(ProbeHeap, MakeProbeCell(BoundsCenter, RootHalfExtent));
	int32 CreatedCellCount = 1;

	// Only an actual point satisfying the same clearance rule can validate the configuration.
	while (!ProbeHeap.IsEmpty())
	{
		const FSplineSafeRegionProbeCell Cell = PopSplineSafeRegionProbeCell(ProbeHeap);
		if (Cell.SignedDistance > CommitSafetyInset)
		{
			return true;
		}

		if (Cell.MaximumSignedDistance <= CommitSafetyInset ||
			Cell.HalfExtent <= SplineSafeRegionProbeMinHalfExtent)
		{
			continue;
		}

		if (CreatedCellCount + 4 > MaxSplineSafeRegionProbeCells)
		{
			UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' could not verify a usable Spline safe region within the %d-cell probe budget. Simplify or widen the boundary."),
				*EncounterId.ToString(), MaxSplineSafeRegionProbeCells);
			return false;
		}

		const float ChildHalfExtent = Cell.HalfExtent * 0.5f;
		for (const float XSign : {-1.f, 1.f})
		{
			for (const float YSign : {-1.f, 1.f})
			{
				const FVector2D ChildCenter = Cell.Center + FVector2D(XSign * ChildHalfExtent, YSign * ChildHalfExtent);
				PushSplineSafeRegionProbeCell(ProbeHeap, MakeProbeCell(ChildCenter, ChildHalfExtent));
				++CreatedCellCount;
			}
		}
	}

	return false;
}

bool AEncounterController::TryInitializeForPlayer()
{
	if (!bAwaitingPlayerSetup)
	{
		return bConfigurationValid;
	}

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!PlayerPawn)
	{
		return false;
	}

	bAwaitingPlayerSetup = false;
	AMyCharacter* Player = Cast<AMyCharacter>(PlayerPawn);
	if (!Player)
	{
		UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' requires AMyCharacter as player pawn and is disabled."), *EncounterId.ToString());
		SetBoundariesClosed(false);
		DestroyRuntimeBoundaries();
		RefreshTickEnabled();
		return false;
	}

	if (!ConfigureCommitVolumes(Player))
	{
		SetCommitVolumeEnabled(RectangularCommitVolume, false);
		SetCommitVolumeEnabled(RadialCommitVolume, false);
		SetCommitVolumeEnabled(SplineCommitCandidateVolume, false);
		SetBoundariesClosed(false);
		DestroyRuntimeBoundaries();
		RefreshTickEnabled();
		return false;
	}

	if (!InitializeParticipants())
	{
		SetCommitVolumeEnabled(RectangularCommitVolume, false);
		SetCommitVolumeEnabled(RadialCommitVolume, false);
		SetCommitVolumeEnabled(SplineCommitCandidateVolume, false);
		SetBoundariesClosed(false);
		DestroyRuntimeBoundaries();
		RefreshTickEnabled();
		return false;
	}

	bConfigurationValid = true;
	PendingCommitPlayer.Reset();
	InitialSafeRegionPlayer.Reset();
	bHasObservedPlayerOutsideCommitRegion = !IsPlayerSafelyInsideCommitRegion(Player);
	if (!bHasObservedPlayerOutsideCommitRegion)
	{
		InitialSafeRegionPlayer = Player;
	}
	else if (IsPlayerOverlappingCommitVolume(Player))
	{
		BeginPendingCommit(Player);
	}

	RefreshTickEnabled();
	UE_LOG(LogTemp, Display, TEXT("Encounter '%s' initialized in Idle state with %d preplaced participant(s)."),
		*EncounterId.ToString(), PreplacedParticipants.Num());
	return true;
}

bool AEncounterController::InitializeParticipants()
{
	TArray<AEnemy*> ClaimedParticipants;
	for (AEnemy* Participant : PreplacedParticipants)
	{
		if (!Participant->ClaimEncounterOwner(this))
		{
			UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' could not claim participant '%s'."),
				*EncounterId.ToString(), *GetNameSafe(Participant));

			for (AEnemy* ClaimedParticipant : ClaimedParticipants)
			{
				ClaimedParticipant->ReleaseEncounterOwner(this);
			}

			return false;
		}

		ClaimedParticipants.Add(Participant);
	}

	for (AEnemy* Participant : ClaimedParticipants)
	{
		Participant->GetOnEnemyDied().AddUObject(this, &AEncounterController::HandleParticipantDied);
		Participant->SetEncounterDormant(this);
	}

	return true;
}

bool AEncounterController::BuildRuntimeBoundaries()
{
	DestroyRuntimeBoundaries();

	const float HalfWallThickness = BoundaryConfig.WallThickness * 0.5f;
	const float WallHalfHeight = BoundaryConfig.WallHeight * 0.5f;
	bool bBuilt = false;

	if (BoundaryConfig.Shape == EEncounterBoundaryShape::Rectangle)
	{
		const FVector2D HalfExtents = BoundaryConfig.InteriorHalfExtents;
		bBuilt =
			CreateBoundarySegment(FVector(HalfExtents.X + HalfWallThickness, 0.f, WallHalfHeight), FRotator::ZeroRotator,
				FVector(HalfWallThickness, HalfExtents.Y + HalfWallThickness, WallHalfHeight)) &&
			CreateBoundarySegment(FVector(-HalfExtents.X - HalfWallThickness, 0.f, WallHalfHeight), FRotator::ZeroRotator,
				FVector(HalfWallThickness, HalfExtents.Y + HalfWallThickness, WallHalfHeight)) &&
			CreateBoundarySegment(FVector(0.f, HalfExtents.Y + HalfWallThickness, WallHalfHeight), FRotator::ZeroRotator,
				FVector(HalfExtents.X + HalfWallThickness, HalfWallThickness, WallHalfHeight)) &&
			CreateBoundarySegment(FVector(0.f, -HalfExtents.Y - HalfWallThickness, WallHalfHeight), FRotator::ZeroRotator,
				FVector(HalfExtents.X + HalfWallThickness, HalfWallThickness, WallHalfHeight));
	}
	else if (BoundaryConfig.Shape == EEncounterBoundaryShape::Radial)
	{
		bBuilt = true;
		const float SegmentAngleRadians = 2.f * PI / static_cast<float>(BoundaryConfig.RadialSegmentCount);
		const float SegmentHalfLength = BoundaryConfig.InteriorRadius * FMath::Tan(SegmentAngleRadians * 0.5f) + BoundarySegmentOverlap;

		for (int32 SegmentIndex = 0; SegmentIndex < BoundaryConfig.RadialSegmentCount; ++SegmentIndex)
		{
			const float AngleRadians = SegmentIndex * SegmentAngleRadians;
			const FVector RadialDirection(FMath::Cos(AngleRadians), FMath::Sin(AngleRadians), 0.f);
			const FVector SegmentLocation = RadialDirection * (BoundaryConfig.InteriorRadius + HalfWallThickness) +
				FVector(0.f, 0.f, WallHalfHeight);
			const FRotator SegmentRotation(0.f, FMath::RadiansToDegrees(AngleRadians), 0.f);

			if (!CreateBoundarySegment(SegmentLocation, SegmentRotation,
				FVector(HalfWallThickness, SegmentHalfLength, WallHalfHeight)))
			{
				bBuilt = false;
				break;
			}
		}
	}
	else
	{
		bBuilt = BuildSplineBoundarySegments();
	}

	if (!bBuilt)
	{
		DestroyRuntimeBoundaries();
	}

	return bBuilt;
}

bool AEncounterController::BuildSplineBoundarySegments()
{
	TArray<FVector2D> Points;
	if (!GetSplineBoundaryPoints(Points) || Points.Num() < 3)
	{
		return false;
	}

	const float SignedArea = GetSignedPolygonArea(Points);
	if (FMath::IsNearlyZero(SignedArea, FMath::Square(SplinePointTolerance)))
	{
		return false;
	}

	const bool bCounterClockwise = SignedArea > 0.f;
	const float HalfWallThickness = BoundaryConfig.WallThickness * 0.5f;
	const float WallHalfHeight = BoundaryConfig.WallHeight * 0.5f;
	for (int32 PointIndex = 0; PointIndex < Points.Num(); ++PointIndex)
	{
		const FVector2D& Start = Points[PointIndex];
		const FVector2D& End = Points[(PointIndex + 1) % Points.Num()];
		const FVector2D Edge = End - Start;
		const float EdgeLength = Edge.Size();
		if (EdgeLength <= SplinePointTolerance)
		{
			return false;
		}

		const FVector2D EdgeDirection = Edge / EdgeLength;
		const FVector2D OutwardNormal = bCounterClockwise
			? FVector2D(EdgeDirection.Y, -EdgeDirection.X)
			: FVector2D(-EdgeDirection.Y, EdgeDirection.X);
		const FVector2D SegmentCenter = (Start + End) * 0.5f + OutwardNormal * HalfWallThickness;
		const FRotator SegmentRotation(0.f, FMath::RadiansToDegrees(FMath::Atan2(EdgeDirection.Y, EdgeDirection.X)), 0.f);
		if (!CreateBoundarySegment(FVector(SegmentCenter.X, SegmentCenter.Y, WallHalfHeight), SegmentRotation,
			FVector(EdgeLength * 0.5f + BoundarySegmentOverlap, HalfWallThickness, WallHalfHeight)))
		{
			return false;
		}
	}

	return true;
}

bool AEncounterController::CreateBoundarySegment(const FVector& RelativeLocation, const FRotator& RelativeRotation,
	const FVector& BoxExtent)
{
	if (!Root)
	{
		return false;
	}

	const FName ComponentName = MakeUniqueObjectName(this, UBoxComponent::StaticClass(), TEXT("EncounterBoundarySegment"));
	UBoxComponent* BoundarySegment = NewObject<UBoxComponent>(this, ComponentName, RF_Transient);
	if (!BoundarySegment)
	{
		UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' failed to allocate a runtime boundary segment."), *EncounterId.ToString());
		return false;
	}

	AddInstanceComponent(BoundarySegment);
	BoundarySegment->SetupAttachment(Root);
	BoundarySegment->SetRelativeLocation(RelativeLocation);
	BoundarySegment->SetRelativeRotation(RelativeRotation);
	BoundarySegment->SetBoxExtent(BoxExtent, false);
	BoundarySegment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BoundarySegment->SetCollisionResponseToAllChannels(ECR_Ignore);
	BoundarySegment->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	BoundarySegment->SetGenerateOverlapEvents(false);
	BoundarySegment->RegisterComponent();
	RuntimeBoundarySegments.Add(BoundarySegment);

	if (!CreateBoundaryVisualSegment(RelativeLocation, RelativeRotation, BoxExtent))
	{
		UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' created collision for a boundary segment but could not create its fog visual."),
			*EncounterId.ToString());
	}

	return true;
}

bool AEncounterController::CreateBoundaryVisualSegment(const FVector& RelativeLocation, const FRotator& RelativeRotation,
	const FVector& BoxExtent)
{
	if (!Root || !BoundaryVisualMesh || !BoundaryVisualMaterial)
	{
		UE_LOG(LogTemp, Warning, TEXT("Encounter '%s' is missing the Engine Cube or BoundaryVisualMaterial; fog presentation is disabled while collision remains available."),
			*EncounterId.ToString());
		return false;
	}

	const FName ComponentName = MakeUniqueObjectName(this, UStaticMeshComponent::StaticClass(), TEXT("EncounterBoundaryVisual"));
	UStaticMeshComponent* BoundaryVisual = NewObject<UStaticMeshComponent>(this, ComponentName, RF_Transient);
	if (!BoundaryVisual)
	{
		return false;
	}

	AddInstanceComponent(BoundaryVisual);
	BoundaryVisual->SetupAttachment(Root);
	BoundaryVisual->SetStaticMesh(BoundaryVisualMesh);
	BoundaryVisual->SetMaterial(0, BoundaryVisualMaterial);
	BoundaryVisual->SetRelativeLocation(RelativeLocation);
	BoundaryVisual->SetRelativeRotation(RelativeRotation);
	BoundaryVisual->SetRelativeScale3D(BoxExtent * (2.f / 100.f));
	BoundaryVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BoundaryVisual->SetGenerateOverlapEvents(false);
	BoundaryVisual->SetCastShadow(false);
	BoundaryVisual->SetCastHiddenShadow(false);
	BoundaryVisual->SetVisibility(false, true);
	BoundaryVisual->SetHiddenInGame(true, true);
	BoundaryVisual->RegisterComponent();
	RuntimeBoundaryVisualSegments.Add(BoundaryVisual);
	return true;
}

bool AEncounterController::IsPlayerSafelyInsideCommitRegion(const AMyCharacter* Player) const
{
	if (!Player)
	{
		return false;
	}

	const FVector LocalPlayerLocation = GetActorTransform().InverseTransformPositionNoScale(Player->GetActorLocation());
	if (BoundaryConfig.Shape == EEncounterBoundaryShape::Rectangle)
	{
		return FMath::Abs(LocalPlayerLocation.X) <= CommitHalfExtents.X &&
			FMath::Abs(LocalPlayerLocation.Y) <= CommitHalfExtents.Y;
	}

	if (BoundaryConfig.Shape == EEncounterBoundaryShape::Radial)
	{
		return FVector2D(LocalPlayerLocation.X, LocalPlayerLocation.Y).SizeSquared() <= FMath::Square(CommitRadius);
	}

	const FVector2D LocalPlayerPoint(LocalPlayerLocation.X, LocalPlayerLocation.Y);
	return IsPointInsideSplineBoundary(LocalPlayerPoint) &&
		GetMinSquaredDistanceToSplineBoundary(LocalPlayerPoint) > FMath::Square(CommitSafetyInset);
}

bool AEncounterController::IsPlayerOverlappingCommitVolume(const AMyCharacter* Player) const
{
	const UPrimitiveComponent* CommitVolume = GetActiveCommitVolume();
	return Player && CommitVolume && CommitVolume->IsOverlappingActor(Player);
}

bool AEncounterController::IsPointInsideSplineBoundary(const FVector2D& LocalPoint) const
{
	if (SplineBoundaryPoints.Num() < 3 ||
		GetMinSquaredDistanceToSplineBoundary(LocalPoint) <= FMath::Square(SplinePointTolerance))
	{
		return false;
	}

	bool bIsInside = false;
	for (int32 PointIndex = 0, PreviousPointIndex = SplineBoundaryPoints.Num() - 1;
		PointIndex < SplineBoundaryPoints.Num(); PreviousPointIndex = PointIndex++)
	{
		const FVector2D& CurrentPoint = SplineBoundaryPoints[PointIndex];
		const FVector2D& PreviousPoint = SplineBoundaryPoints[PreviousPointIndex];
		const bool bStraddlesHorizontalRay = (CurrentPoint.Y > LocalPoint.Y) != (PreviousPoint.Y > LocalPoint.Y);
		if (bStraddlesHorizontalRay)
		{
			const float IntersectionX = (PreviousPoint.X - CurrentPoint.X) * (LocalPoint.Y - CurrentPoint.Y) /
				(PreviousPoint.Y - CurrentPoint.Y) + CurrentPoint.X;
			if (LocalPoint.X < IntersectionX)
			{
				bIsInside = !bIsInside;
			}
		}
	}

	return bIsInside;
}

float AEncounterController::GetSignedDistanceToSplineBoundary(const FVector2D& LocalPoint) const
{
	const float MinimumDistanceSquared = GetMinSquaredDistanceToSplineBoundary(LocalPoint);
	if (MinimumDistanceSquared == TNumericLimits<float>::Max())
	{
		return -TNumericLimits<float>::Max();
	}

	const float Distance = FMath::Sqrt(FMath::Max(0.f, MinimumDistanceSquared));
	return IsPointInsideSplineBoundary(LocalPoint) ? Distance : -Distance;
}

float AEncounterController::GetMinSquaredDistanceToSplineBoundary(const FVector2D& LocalPoint) const
{
	if (SplineBoundaryPoints.Num() < 2)
	{
		return TNumericLimits<float>::Max();
	}

	float MinimumDistanceSquared = TNumericLimits<float>::Max();
	for (int32 PointIndex = 0; PointIndex < SplineBoundaryPoints.Num(); ++PointIndex)
	{
		const FVector2D& Start = SplineBoundaryPoints[PointIndex];
		const FVector2D& End = SplineBoundaryPoints[(PointIndex + 1) % SplineBoundaryPoints.Num()];
		MinimumDistanceSquared = FMath::Min(MinimumDistanceSquared,
			GetPointToSegmentDistanceSquared2D(LocalPoint, Start, End));
	}

	return MinimumDistanceSquared;
}

UPrimitiveComponent* AEncounterController::GetActiveCommitVolume() const
{
	switch (BoundaryConfig.Shape)
	{
	case EEncounterBoundaryShape::Rectangle:
		return RectangularCommitVolume;

	case EEncounterBoundaryShape::Radial:
		return RadialCommitVolume;

	case EEncounterBoundaryShape::Spline:
		return SplineCommitCandidateVolume;

	default:
		return nullptr;
	}
}

void AEncounterController::SetCommitVolumeEnabled(UPrimitiveComponent* CommitVolume, bool bShouldEnable)
{
	if (!CommitVolume)
	{
		return;
	}

	CommitVolume->SetCollisionEnabled(bShouldEnable ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	CommitVolume->SetGenerateOverlapEvents(bShouldEnable);
}

void AEncounterController::BeginPendingCommit(AMyCharacter* Player)
{
	if (!Player || !bConfigurationValid || EncounterState != EEncounterState::Idle || !bHasObservedPlayerOutsideCommitRegion)
	{
		return;
	}

	PendingCommitPlayer = Player;
	RefreshTickEnabled();
}

void AEncounterController::ClearPendingCommit()
{
	PendingCommitPlayer.Reset();
	RefreshTickEnabled();
}

void AEncounterController::RefreshTickEnabled()
{
	const bool bShouldTick = bAwaitingPlayerSetup ||
		(bConfigurationValid && EncounterState == EEncounterState::Idle &&
			(PendingCommitPlayer.IsValid() || InitialSafeRegionPlayer.IsValid()));
	SetActorTickEnabled(bShouldTick);
}

void AEncounterController::ReleaseParticipants()
{
	for (AEnemy* Participant : PreplacedParticipants)
	{
		if (!IsValid(Participant))
		{
			continue;
		}

		Participant->GetOnEnemyDied().RemoveAll(this);
		Participant->ReleaseEncounterOwner(this);
	}

	RemainingParticipants.Reset();
}

void AEncounterController::DestroyRuntimeBoundaries()
{
	for (UBoxComponent* BoundarySegment : RuntimeBoundarySegments)
	{
		if (IsValid(BoundarySegment))
		{
			BoundarySegment->DestroyComponent();
		}
	}

	RuntimeBoundarySegments.Reset();

	for (UStaticMeshComponent* BoundaryVisual : RuntimeBoundaryVisualSegments)
	{
		if (IsValid(BoundaryVisual))
		{
			BoundaryVisual->DestroyComponent();
		}
	}

	RuntimeBoundaryVisualSegments.Reset();
}

void AEncounterController::SetBoundariesClosed(bool bShouldClose)
{
	for (UBoxComponent* BoundarySegment : RuntimeBoundarySegments)
	{
		if (IsValid(BoundarySegment))
		{
			BoundarySegment->SetCollisionEnabled(bShouldClose ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		}
	}

	for (UStaticMeshComponent* BoundaryVisual : RuntimeBoundaryVisualSegments)
	{
		if (IsValid(BoundaryVisual))
		{
			BoundaryVisual->SetVisibility(bShouldClose, true);
			BoundaryVisual->SetHiddenInGame(!bShouldClose, true);
		}
	}
}

void AEncounterController::SetEncounterState(EEncounterState NewState)
{
	if (EncounterState == NewState)
	{
		return;
	}

	EncounterState = NewState;
	const TCHAR* StateName = NewState == EEncounterState::Idle ? TEXT("Idle") :
		(NewState == EEncounterState::Active ? TEXT("Active") : TEXT("Cleared"));
	UE_LOG(LogTemp, Display, TEXT("Encounter '%s' entered %s state."), *EncounterId.ToString(), StateName);
}

void AEncounterController::HandleParticipantDied(AEnemy* DefeatedEnemy)
{
	if (EncounterState != EEncounterState::Active || !IsValid(DefeatedEnemy))
	{
		return;
	}

	if (RemainingParticipants.Remove(DefeatedEnemy) == 0)
	{
		return;
	}

	if (!RemainingParticipants.IsEmpty())
	{
		return;
	}

	SetEncounterState(EEncounterState::Cleared);
	SetBoundariesClosed(false);
	SetCommitVolumeEnabled(GetActiveCommitVolume(), false);
	ClearPendingCommit();
	ReleaseParticipants();
}
