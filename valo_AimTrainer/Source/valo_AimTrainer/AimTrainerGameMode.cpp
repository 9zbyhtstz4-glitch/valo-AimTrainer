// AimTrainerGameMode の実装
#include "AimTrainerGameMode.h"

#include "AimTrainerCharacter.h"

AAimTrainerGameMode::AAimTrainerGameMode()
{
	// 【設計方針】C++ から Content 内の Blueprint をパス検索・ハードコード参照しない。
	//
	// ここでは C++ 側の既定値として C++ キャラクターを指定するだけに留める
	// (C++ → C++ の参照のみ。アセットへの依存なし)。
	//
	// 実際に使用するポーンの差し替えは UE5 標準の運用に従い、エディタ側で行う:
	//   1. 本クラスを親にした BP_AimTrainerGameMode を作成する
	//   2. その Details > Classes > Default Pawn Class に BP_AimTrainerCharacter を割り当てる
	//   3. Project Settings > Maps & Modes > Default GameMode
	//      (またはレベルの World Settings > GameMode Override)に
	//      BP_AimTrainerGameMode を指定する
	//
	// ※C++ クラスのままでは Project Settings / World Settings の
	//   「Selected GameMode > Default Pawn Class」欄は編集不可(グレーアウト)のため、
	//   エディタから差し替えるには GameMode の Blueprint 化が必要になる。
	//
	// ※C++ キャラクター直のままPIEした場合、入力アセットが未割り当てのため
	//   操作はできないが、その旨はキャラクター側の診断機能が画面に赤字で表示する。
	DefaultPawnClass = AAimTrainerCharacter::StaticClass();
}
