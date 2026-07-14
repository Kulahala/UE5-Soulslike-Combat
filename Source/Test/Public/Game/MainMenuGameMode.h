// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MainMenuGameMode.generated.h"

/** MainMenu 使用的无玩家 Pawn GameMode；角色与火堆仅是地图展示 Actor。 */
UCLASS()
class TEST_API AMainMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMainMenuGameMode();
};
