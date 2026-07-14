#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/EncounterTypes.h"
#include "EncounterController.generated.h"

class AEnemy;
class AMyCharacter;
class UBoxComponent;
class UCapsuleComponent;
class UMaterialInterface;
class UPrimitiveComponent;
class USceneComponent;
class USplineComponent;
class UStaticMesh;
class UStaticMeshComponent;

/** 只管理当前地图的遭遇生命周期。奖励、波次和持久化恢复由后续阶段负责。 */
UCLASS()
class TEST_API AEncounterController : public AActor
{
	GENERATED_BODY()

public:
	AEncounterController();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;

	bool TryActivate(AMyCharacter* Player);

	FORCEINLINE EEncounterState GetEncounterState() const { return EncounterState; }
	FORCEINLINE FName GetEncounterId() const { return EncounterId; }

private:
	UFUNCTION()
	void OnCommitVolumeBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	                               UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
	                               const FHitResult& SweepResult);

	UFUNCTION()
	void OnCommitVolumeEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	                             UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	bool ValidateConfiguration() const;
	bool ValidateSplineBoundary() const;
	bool GetSplineBoundaryPoints(TArray<FVector2D>& OutPoints) const;
	bool ConfigureCommitVolumes(AMyCharacter* Player);
	bool ConfigureSplineCommitVolume(AMyCharacter* Player, float CommitVolumeHalfHeight);
	bool HasUsableSplineCommitRegion(const FVector2D& BoundsCenter, const FVector2D& BoundsSize) const;
	bool TryInitializeForPlayer();
	bool InitializeParticipants();
	bool BuildRuntimeBoundaries();
	bool BuildSplineBoundarySegments();
	bool CreateBoundarySegment(const FVector& RelativeLocation, const FRotator& RelativeRotation,
	                           const FVector& BoxExtent);
	bool CreateBoundaryVisualSegment(const FVector& RelativeLocation, const FRotator& RelativeRotation,
	                                 const FVector& BoxExtent);
	bool IsPlayerSafelyInsideCommitRegion(const AMyCharacter* Player) const;
	bool IsPlayerOverlappingCommitVolume(const AMyCharacter* Player) const;
	bool IsPointInsideSplineBoundary(const FVector2D& LocalPoint) const;
	float GetSignedDistanceToSplineBoundary(const FVector2D& LocalPoint) const;
	float GetMinSquaredDistanceToSplineBoundary(const FVector2D& LocalPoint) const;
	UPrimitiveComponent* GetActiveCommitVolume() const;
	void SetCommitVolumeEnabled(UPrimitiveComponent* CommitVolume, bool bShouldEnable);
	void BeginPendingCommit(AMyCharacter* Player);
	void ClearPendingCommit();
	void RefreshTickEnabled();
	void ReleaseParticipants();
	void DestroyRuntimeBoundaries();
	void SetBoundariesClosed(bool bShouldClose);
	void SetEncounterState(EEncounterState NewState);
	void HandleParticipantDied(AEnemy* DefeatedEnemy);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	USceneComponent* Root = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UBoxComponent* RectangularCommitVolume = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UCapsuleComponent* RadialCommitVolume = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true", ToolTip = "Spline 模式的候选查询范围；仅用于启动几何安全判定，不决定遭遇激活。"))
	UBoxComponent* SplineCommitCandidateVolume = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true", ToolTip = "在关卡视口拖点编辑的闭合边界。仅支持本地 Z=0、Linear 点和无自交简单闭环。"))
	USplineComponent* BoundarySpline = nullptr;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Encounter", meta = (AllowPrivateAccess = "true", ToolTip = "存档和未来重载使用的稳定关卡作者 ID；同一地图内必须唯一。"))
	FName EncounterId = NAME_None;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Encounter", meta = (AllowPrivateAccess = "true", ToolTip = "本阶段仅支持预放置参与者；未来波次由 EncounterSpawnPoint 生成。"))
	TArray<AEnemy*> PreplacedParticipants;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Encounter", meta = (AllowPrivateAccess = "true", ToolTip = "边界与同心进入区的配置。Controller 原点必须摆在战斗区中心的地面。"))
	FEncounterBoundaryConfig BoundaryConfig;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter|Boundary", meta = (AllowPrivateAccess = "true", ToolTip = "Active 时应用到运行时边界 Cube 的雾幕材质。为空时遇战仍保留碰撞，但会输出 warning。"))
	UMaterialInterface* BoundaryVisualMaterial = nullptr;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Encounter", meta = (AllowPrivateAccess = "true"))
	EEncounterState EncounterState = EEncounterState::Idle;

	UPROPERTY(Transient)
	TArray<UBoxComponent*> RuntimeBoundarySegments;

	UPROPERTY(Transient)
	TArray<UStaticMeshComponent*> RuntimeBoundaryVisualSegments;

	UPROPERTY(Transient)
	UStaticMesh* BoundaryVisualMesh = nullptr;

	TSet<AEnemy*> RemainingParticipants;
	TWeakObjectPtr<AMyCharacter> PendingCommitPlayer;
	TWeakObjectPtr<AMyCharacter> InitialSafeRegionPlayer;
	TArray<FVector2D> SplineBoundaryPoints;
	FVector2D CommitHalfExtents = FVector2D::ZeroVector;
	float CommitRadius = 0.f;
	float CommitSafetyInset = 0.f;
	bool bStaticConfigurationValid = false;
	bool bConfigurationValid = false;
	bool bAwaitingPlayerSetup = false;
	bool bHasObservedPlayerOutsideCommitRegion = false;
};
