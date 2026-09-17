// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/InteractableInterface.h"
#include "CheckpointActor.generated.h"

class UArrowComponent;
class UPrimitiveComponent;
class USphereComponent;
class UStaticMeshComponent;

/** 关卡放置的火堆。首次激活自动休息，后续进入火堆服务菜单。 */
UCLASS()
class TEST_API ACheckpointActor : public AActor, public IInteractableInterface
{
	GENERATED_BODY()

public:
	ACheckpointActor();

	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual FText GetInteractionPrompt_Implementation() const override;
	virtual int32 GetInteractionPriority_Implementation() const override;
	virtual void Interact_Implementation(AActor* Interactor) override;

	FORCEINLINE FName GetPersistentId() const { return PersistentId; }
	FORCEINLINE bool IsDefaultCheckpoint() const { return bIsDefaultCheckpoint; }
	FTransform GetSpawnTransform() const;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnInteractionBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	                               UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
	                               const FHitResult& SweepResult);

	UFUNCTION()
	void OnInteractionEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	                             UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	USceneComponent* Root = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* VisualMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	USphereComponent* InteractionSphere = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UArrowComponent* SpawnArrow = nullptr;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Checkpoint", meta = (AllowPrivateAccess = "true", ToolTip = "存档使用的稳定关卡作者 ID，必须在同一地图内唯一。"))
	FName PersistentId = NAME_None;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Checkpoint", meta = (AllowPrivateAccess = "true", ToolTip = "新游戏的默认出生火堆。每张地图最多一个。"))
	bool bIsDefaultCheckpoint = false;
};
