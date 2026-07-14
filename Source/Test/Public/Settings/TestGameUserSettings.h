// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "TestGameUserSettings.generated.h"

class USoundClass;
class USoundMix;

/** 配置文件持久化的显示、音频与视角设置。它不属于游戏进度 SaveGame。 */
UCLASS(config = GameUserSettings, defaultconfig)
class TEST_API UTestGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Settings")
	static UTestGameUserSettings* GetTestGameUserSettings();

	UFUNCTION(BlueprintCallable, Category = "Settings|Audio")
	void SetMasterVolume(float NewValue);

	UFUNCTION(BlueprintCallable, Category = "Settings|Audio")
	void SetMusicVolume(float NewValue);

	UFUNCTION(BlueprintCallable, Category = "Settings|Audio")
	void SetSfxVolume(float NewValue);

	UFUNCTION(BlueprintCallable, Category = "Settings|Input")
	void SetLookSensitivity(float NewValue);

	UFUNCTION(BlueprintCallable, Category = "Settings|Input")
	void SetInvertY(bool bNewInvertY);

	UFUNCTION(BlueprintCallable, Category = "Settings|Display", meta = (ToolTip = "应用 WBP_Settings 使用的显示模式标签并立即保存。"))
	bool ApplyDisplayModeOption(const FString& ModeOption);

	UFUNCTION(BlueprintPure, Category = "Settings|Display")
	FString GetDisplayModeOption() const;

	UFUNCTION(BlueprintCallable, Category = "Settings|Display", meta = (ToolTip = "应用格式为“宽 x 高”的分辨率标签并立即保存。"))
	bool ApplyResolutionOption(const FString& ResolutionOption);

	UFUNCTION(BlueprintPure, Category = "Settings|Display")
	FString GetResolutionOption() const;

	UFUNCTION(BlueprintCallable, Category = "Settings|Audio", meta = (ToolTip = "由 WBP_Settings 传入项目的 SoundMix 与 SoundClass 资产并立即应用。"))
	void ApplyAudioSettings(UObject* WorldContextObject, USoundMix* SoundMix, USoundClass* MasterSoundClass,
	                        USoundClass* MusicSoundClass, USoundClass* SfxSoundClass) const;

	UFUNCTION(BlueprintPure, Category = "Settings|Audio")
	FORCEINLINE float GetMasterVolume() const { return MasterVolume; }

	UFUNCTION(BlueprintPure, Category = "Settings|Audio")
	FORCEINLINE float GetMusicVolume() const { return MusicVolume; }

	UFUNCTION(BlueprintPure, Category = "Settings|Audio")
	FORCEINLINE float GetSfxVolume() const { return SfxVolume; }

	UFUNCTION(BlueprintPure, Category = "Settings|Input")
	FORCEINLINE float GetLookSensitivity() const { return LookSensitivity; }

	UFUNCTION(BlueprintPure, Category = "Settings|Input")
	FORCEINLINE bool GetInvertY() const { return bInvertY; }

private:
	UPROPERTY(config)
	float MasterVolume = 1.f;

	UPROPERTY(config)
	float MusicVolume = 1.f;

	UPROPERTY(config)
	float SfxVolume = 1.f;

	UPROPERTY(config)
	float LookSensitivity = 1.f;

	UPROPERTY(config)
	bool bInvertY = false;

	void SaveUserPreferences();
};
