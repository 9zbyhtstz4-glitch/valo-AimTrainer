// AimTrainerCharacter: エイム練習用のFPSキャラクター
// 【ステップ1の担当範囲】FPS視点カメラ + マウスによる視点操作(ヨー/ピッチ)のみ。
// 移動・ジャンプ・しゃがみ・武器は今後のステップで追加する。
//
// 入力について:
//  - UE5標準の運用に合わせ、InputAction / InputMappingContext は
//    エディタで作成したアセットを参照する(実行時生成はしない)。
//  - 下記の DefaultMappingContext / LookAction は、本クラスを親とした
//    Blueprint(BP_AimTrainerCharacter)側で割り当てる。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AimTrainerCharacter.generated.h"

class UCameraComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

UCLASS()
class VALO_AIMTRAINER_API AAimTrainerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AAimTrainerCharacter();

	//~ Begin APawn interface
	/** Enhanced Input のマッピングコンテキスト登録と入力バインドを行う */
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	//~ End APawn interface

	/** FPSカメラ取得(今後、武器の射線計算やHUDで使用する) */
	UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }

protected:
	// ==============================
	// コンポーネント
	// ==============================

	/** 一人称視点カメラ。カプセルの目線高さに取り付け、コントローラ回転(ピッチ)に追従させる */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AimTrainer|Camera")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	// ==============================
	// 入力アセット(Blueprint側で割り当てる)
	// ==============================

	/** 既定のマッピングコンテキスト(IMC_Default を割り当てる) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	/** 視点操作アクション(IA_Look / Axis2D を割り当てる) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Input")
	TObjectPtr<UInputAction> LookAction;

	// ==============================
	// 視点設定
	// ==============================

	/** マウス感度(視点回転の倍率) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Look", meta = (ClampMin = "0.01"))
	float MouseSensitivity = 1.0f;

	/**
	 * 上下視点の反転フラグ。
	 * 既定(false)で「マウス上=見上げる」になるよう符号調整している。
	 * もし環境差で上下が逆に感じる場合はこのフラグを切り替えること。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Look")
	bool bInvertLookY = false;

	// ==============================
	// 入力コールバック
	// ==============================

	/** マウス視点。X=ヨー(左右), Y=ピッチ(上下) */
	void Input_Look(const FInputActionValue& Value);
};
