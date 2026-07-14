// Fill out your copyright notice in the Description page of Project Settings.

#include "Settings/TestGameUserSettings.h"

#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"

namespace TestGameUserSettings
{
	bool TryParseResolutionOption(const FString& ResolutionOption, FIntPoint& OutResolution)
	{
		FString WidthText;
		FString HeightText;
		if (!ResolutionOption.Split(TEXT(" x "), &WidthText, &HeightText))
		{
			return false;
		}

		int32 Width = 0;
		int32 Height = 0;
		if (!LexTryParseString(Width, *WidthText) || !LexTryParseString(Height, *HeightText) || Width <= 0 || Height <= 0)
		{
			return false;
		}

		OutResolution = FIntPoint(Width, Height);
		return true;
	}
}

UTestGameUserSettings* UTestGameUserSettings::GetTestGameUserSettings()
{
	return GEngine ? Cast<UTestGameUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
}

void UTestGameUserSettings::SetMasterVolume(float NewValue)
{
	MasterVolume = FMath::Clamp(NewValue, 0.f, 1.f);
	SaveUserPreferences();
}

void UTestGameUserSettings::SetMusicVolume(float NewValue)
{
	MusicVolume = FMath::Clamp(NewValue, 0.f, 1.f);
	SaveUserPreferences();
}

void UTestGameUserSettings::SetSfxVolume(float NewValue)
{
	SfxVolume = FMath::Clamp(NewValue, 0.f, 1.f);
	SaveUserPreferences();
}

void UTestGameUserSettings::SetLookSensitivity(float NewValue)
{
	LookSensitivity = FMath::Clamp(NewValue, 0.1f, 5.f);
	SaveUserPreferences();
}

void UTestGameUserSettings::SetInvertY(bool bNewInvertY)
{
	bInvertY = bNewInvertY;
	SaveUserPreferences();
}

bool UTestGameUserSettings::ApplyDisplayModeOption(const FString& ModeOption)
{
	if (ModeOption == TEXT("Windowed"))
	{
		SetFullscreenMode(EWindowMode::Windowed);
	}
	else if (ModeOption == TEXT("Borderless"))
	{
		SetFullscreenMode(EWindowMode::WindowedFullscreen);
	}
	else if (ModeOption == TEXT("Fullscreen"))
	{
		SetFullscreenMode(EWindowMode::Fullscreen);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Unsupported display mode option: %s"), *ModeOption);
		return false;
	}

	ApplySettings(false);
	return true;
}

FString UTestGameUserSettings::GetDisplayModeOption() const
{
	switch (GetFullscreenMode())
	{
	case EWindowMode::Windowed:
		return TEXT("Windowed");
	case EWindowMode::WindowedFullscreen:
		return TEXT("Borderless");
	case EWindowMode::Fullscreen:
		return TEXT("Fullscreen");
	default:
		return TEXT("Windowed");
	}
}

bool UTestGameUserSettings::ApplyResolutionOption(const FString& ResolutionOption)
{
	FIntPoint Resolution;
	if (!TestGameUserSettings::TryParseResolutionOption(ResolutionOption, Resolution))
	{
		UE_LOG(LogTemp, Warning, TEXT("Unsupported resolution option: %s"), *ResolutionOption);
		return false;
	}

	SetScreenResolution(Resolution);
	ApplySettings(false);
	return true;
}

FString UTestGameUserSettings::GetResolutionOption() const
{
	const FIntPoint Resolution = GetScreenResolution();
	return FString::Printf(TEXT("%d x %d"), Resolution.X, Resolution.Y);
}

void UTestGameUserSettings::ApplyAudioSettings(UObject* WorldContextObject, USoundMix* SoundMix,
	USoundClass* MasterSoundClass, USoundClass* MusicSoundClass, USoundClass* SfxSoundClass) const
{
	if (!WorldContextObject || !SoundMix)
	{
		UE_LOG(LogTemp, Warning, TEXT("ApplyAudioSettings requires a world context and SoundMix asset."));
		return;
	}

	if (MasterSoundClass)
	{
		UGameplayStatics::SetSoundMixClassOverride(WorldContextObject, SoundMix, MasterSoundClass, MasterVolume, 1.f, 0.f, true);
	}
	if (MusicSoundClass)
	{
		UGameplayStatics::SetSoundMixClassOverride(WorldContextObject, SoundMix, MusicSoundClass, MusicVolume, 1.f, 0.f, true);
	}
	if (SfxSoundClass)
	{
		UGameplayStatics::SetSoundMixClassOverride(WorldContextObject, SoundMix, SfxSoundClass, SfxVolume, 1.f, 0.f, true);
	}

	UGameplayStatics::PushSoundMixModifier(WorldContextObject, SoundMix);
}

void UTestGameUserSettings::SaveUserPreferences()
{
	SaveSettings();
}
