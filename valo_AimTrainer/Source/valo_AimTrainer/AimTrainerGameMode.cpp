// AimTrainerGameMode の実装
#include "AimTrainerGameMode.h"

#include "AimTrainerCharacter.h"
#include "UObject/ConstructorHelpers.h"

AAimTrainerGameMode::AAimTrainerGameMode()
{
	// UE5テンプレート標準の運用に合わせる:
	// 入力アセット(IMC_Default / IA_Look)を割り当てた Blueprint 版キャラクターを
	// 既定ポーンとして読み込む。パスは固定(Content/Blueprints/BP_AimTrainerCharacter)。
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(
		TEXT("/Game/Blueprints/BP_AimTrainerCharacter"));

	if (PlayerPawnBPClass.Class != nullptr)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
	else
	{
		// Blueprint未作成でも起動だけはできるようC++クラスへフォールバックする。
		// (この場合は入力アセット未割り当てのため視点操作はできない。
		//  ログに警告が出るので BP_AimTrainerCharacter を作成すること)
		DefaultPawnClass = AAimTrainerCharacter::StaticClass();
	}
}
