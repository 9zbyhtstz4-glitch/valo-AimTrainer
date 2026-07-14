// AimTrainerCharacter: エイム練習用のFPSキャラクター
// 【実装済みの範囲】
//  - ステップ1: FPS視点カメラ + マウス視点操作(ヨー/ピッチ)
//  - ステップ2: WASD移動
//  - ステップ3: ジャンプ(本ステップ)
// しゃがみ・武器は今後のステップで追加する。
//
// 入力について:
//  - UE5標準の運用に合わせ、InputAction / InputMappingContext は
//    エディタで作成したアセットを参照する(実行時生成はしない)。
//  - 下記の入力アセット参照は、本クラスを親とした
//    Blueprint(BP_AimTrainerCharacter)側で割り当てる。
//
// 責務分離について:
//  - 本クラスは「入力を意図へ変換する」ところまでを担当し、
//    移動・ジャンプの物理計算(加速・重力・速度上限・衝突)は
//    ACharacter / CharacterMovementComponent の標準機能に委譲する。
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
	//~ Begin AActor interface
	/** Blueprint側で編集されたプロパティ値を移動コンポーネントへ反映する */
	virtual void BeginPlay() override;
	//~ End AActor interface

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

	/** 移動アクション(IA_Move / Axis2D を割り当てる。X=右+, Y=前+) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Input")
	TObjectPtr<UInputAction> MoveAction;

	/** ジャンプアクション(IA_Jump / Digital(bool) を割り当てる。Spaceキー) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Input")
	TObjectPtr<UInputAction> JumpAction;

	// ==============================
	// 移動設定
	// ==============================

	/**
	 * 歩行速度(uu/s)。BeginPlay で CharacterMovementComponent の MaxWalkSpeed へ反映される。
	 * 速度の上限管理は CharacterMovementComponent 側の責務のため、
	 * 本クラスは値を渡すだけでフレームレートへの依存はない。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Move", meta = (ClampMin = "1.0"))
	float WalkSpeed = 540.f;

	// ==============================
	// ジャンプ設定
	// ==============================

	/**
	 * ジャンプ初速(uu/s)。BeginPlay で CharacterMovementComponent の JumpZVelocity へ反映される。
	 * 到達高さの目安 ≈ JumpZVelocity^2 / (2 × 980 × GravityScale)。
	 * 既定430では約94uu(GravityScale=1.0時)の控えめなジャンプになる。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Jump", meta = (ClampMin = "1.0"))
	float JumpZVelocity = 430.f;

	/**
	 * 重力倍率。BeginPlay で CharacterMovementComponent の GravityScale へ反映される。
	 * 大きくするほど落下が速くなり、同じ初速でもジャンプは低くなる。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Jump", meta = (ClampMin = "0.1"))
	float GravityScale = 1.0f;

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
	// ジャンプは ACharacter 標準の Jump / StopJumping へ直接バインドするため
	// 本クラスに独自のジャンプ関数は持たない(要件: 独自ジャンプ処理を作らない)。

	/** WASD移動。コントローラのヨーのみを基準に前後左右の移動入力を加える */
	void Input_Move(const FInputActionValue& Value);

	/** マウス視点。X=ヨー(左右), Y=ピッチ(上下) */
	void Input_Look(const FInputActionValue& Value);
};
