// AimTrainerGameMode: エイム練習の既定ゲームモード
// 【ステップ1の担当範囲】既定ポーン(プレイヤーキャラクター)の指定のみ。
// セッション管理(スコア・タイマー)は今後のステップで追加する。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "AimTrainerGameMode.generated.h"

UCLASS()
class VALO_AIMTRAINER_API AAimTrainerGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AAimTrainerGameMode();
};
