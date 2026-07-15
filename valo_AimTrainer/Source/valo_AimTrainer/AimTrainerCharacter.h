// AimTrainerCharacter: エイム練習用のFPSキャラクター
// 【実装済みの範囲】
//  - ステップ1〜4: FPS視点/マウス視点・WASD移動・ジャンプ・しゃがみ
//  - fix2: IMC登録タイミングの是正 + 入力設定不備の可視化
//  - fix3: VALORANT準拠の操作感チューニング(本修正)
//      * 移動: 加速・減速・摩擦・空中制御をVALORANTの実測値ベースへ調整
//      * しゃがみ: カメラ高さを補間するスムーズしゃがみ(急落下の解消)
//      * 視点: 感度スケールをVALORANTのゲーム内感度と同一化 / FOV103 /
//              マウススムージング・FOVスケーリング無効化(Config側)
//      * ジャンプ: 高さ・滞空時間をVALORANT相当へ再調整
//
// 入力について:
//  - InputAction / InputMappingContext はエディタ作成のアセットを参照する。
//  - 参照の割当は BP_AimTrainerCharacter 側で行う。
//  - IMC登録は NotifyControllerChanged(UE5テンプレート標準タイミング)。
//
// 責務分離について:
//  - 本クラスは「入力→意図の変換」と「チューニング値のCMCへの適用」を担当し、
//    物理計算は ACharacter / CharacterMovementComponent の標準機能に委譲する。
//  - fix3で追加したのは数値適用とカメラ高さ補間のみで、移動計算の独自実装はない。
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

	//~ Begin AActor interface
	/** しゃがみ時のカメラ高さ補間のみを行う(それ以外の毎フレーム処理はない) */
	virtual void Tick(float DeltaSeconds) override;
	//~ End AActor interface

	//~ Begin APawn interface
	/** コントローラ確定後に呼ばれる。IMCをローカルプレイヤーのサブシステムへ登録する */
	virtual void NotifyControllerChanged() override;

	/** Enhanced Input のアクションバインドを行う */
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	//~ End APawn interface

	//~ Begin ACharacter interface
	/**
	 * しゃがみ開始/終了時に呼ばれる(カプセル変形後)。
	 * カプセル中心の上下移動ぶんをカメラの相対位置へ即時打ち消して
	 * 「視点のワールド高さが一瞬で飛ぶ」現象を防ぎ、以後 Tick で滑らかに補間する。
	 */
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	//~ End ACharacter interface

	/** FPSカメラ取得(今後、武器の射線計算やHUDで使用する) */
	UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }

protected:
	//~ Begin AActor interface
	/** Blueprint側で編集されたプロパティ値をカメラ・移動コンポーネントへ反映する */
	virtual void BeginPlay() override;
	//~ End AActor interface

	/**
	 * 本クラスのチューニング用プロパティを CharacterMovementComponent とカメラへ反映する。
	 * コンストラクタ(エディタ表示用の既定値)と BeginPlay(BP編集値の反映)の両方から呼ぶ。
	 */
	void ApplyCharacterSettings();

	// ==============================
	// コンポーネント
	// ==============================

	/** 一人称視点カメラ。カプセルに取り付け、コントローラ回転(ピッチ)に追従させる */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AimTrainer|Camera")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	// ==============================
	// 入力アセット(Blueprint側で割り当てる)
	// ==============================

	/** 既定のマッピングコンテキスト(IMC_Default を割り当てる) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	/** 視点操作アクション(IA_Look / Axis2D) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Input")
	TObjectPtr<UInputAction> LookAction;

	/** 移動アクション(IA_Move / Axis2D。X=右+, Y=前+) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Input")
	TObjectPtr<UInputAction> MoveAction;

	/** ジャンプアクション(IA_Jump / Digital。Spaceキー) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Input")
	TObjectPtr<UInputAction> JumpAction;

	/** しゃがみアクション(IA_Crouch / Digital。左Ctrlホールド) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Input")
	TObjectPtr<UInputAction> CrouchAction;

	// ==============================
	// 移動設定(VALORANT準拠チューニング)
	// ==============================

	/** 立ち歩行速度(uu/s)。VALORANTのライフル所持時の走行速度 5.4m/s に一致させる */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Move", meta = (ClampMin = "1.0"))
	float WalkSpeed = 540.f;

	/** しゃがみ歩行速度(uu/s)。VALORANTのしゃがみ移動(走行の約半分)を想定 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Move", meta = (ClampMin = "1.0"))
	float CrouchedSpeed = 270.f;

	/**
	 * 最大加速度(uu/s^2)。UE既定の2048では最高速まで約0.26秒かかり「滑る」感覚になる。
	 * VALORANTは踏み出しが速い(体感0.1〜0.15秒で最高速)ため4096に引き上げる
	 * (540/4096 ≒ 0.13秒で最高速へ到達)。A/D切り返しのキレにも直結する。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Move", meta = (ClampMin = "1.0"))
	float MoveAcceleration = 4096.f;

	/**
	 * キーを離した際の制動減速度(uu/s^2)。UE既定2048では停止までの滑走が長い。
	 * VALORANTの「離すとすぐ止まるが一瞬だけ滑る」感覚に合わせて2800へ。
	 * なお逆方向キーを押した場合は MoveAcceleration も制動に加わるため、
	 * VALORANT同様「カウンターストレイフの方が速く止まる」挙動が自然に成立する。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Move", meta = (ClampMin = "0.0"))
	float MoveBrakingDeceleration = 2800.f;

	/**
	 * 地上摩擦。方向転換時に旧速度を打ち消す強さで、A⇔Dの切り返しのキレを決める。
	 * UE既定8ではわずかに横滑りが残るため10へ(VALORANTのストレイフは滑らない)。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Move", meta = (ClampMin = "0.0"))
	float MoveGroundFriction = 10.f;

	/**
	 * 制動時専用の摩擦(bUseSeparateBrakingFriction=trueで有効)。
	 * 速度に比例した減速が加わり、停止の立ち上がりが鋭くなる。
	 * BrakingFrictionFactorは1に固定し、この値がそのまま効くようにする。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Move", meta = (ClampMin = "0.0"))
	float MoveBrakingFriction = 10.f;

	/**
	 * 空中制御(0〜1)。VALORANTは空中でわずかに軌道修正できる程度なので0.15。
	 * (UE既定0.05は硬すぎ、1.0はエアストレイフになりすぎる)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Move", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AirControlAmount = 0.15f;

	// ==============================
	// ジャンプ設定(VALORANT準拠チューニング)
	// ==============================

	/**
	 * ジャンプ初速(uu/s)。GravityScale=1.4 との組で
	 * 高さ ≒ 456^2 / (2×980×1.4) ≒ 76uu(≒VALORANTのジャンプ高)、
	 * 滞空時間 ≒ 2×456/1372 ≒ 0.66秒 になる。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Jump", meta = (ClampMin = "1.0"))
	float JumpZVelocity = 456.f;

	/**
	 * 重力倍率。UE既定1.0はふわっと浮く感覚が強い。
	 * VALORANTの「短く鋭い」ジャンプ・落下感に合わせ1.4へ。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Jump", meta = (ClampMin = "0.1"))
	float GravityScale = 1.4f;

	// ==============================
	// しゃがみ設定(スムーズしゃがみ)
	// ==============================

	/**
	 * しゃがみ時のカプセル半分高さ(uu)。立ち96に対し60(全高192cm→120cm)。
	 * UE既定40では潜り込みすぎてVALORANTより視点が低くなるため引き上げる。
	 * ※カプセル半径(42)はVALORANT同様しゃがみでも変更しない。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Crouch", meta = (ClampMin = "30.0"))
	float CrouchedCapsuleHalfHeight = 60.f;

	/** 立ち状態のカメラ高さ(カプセル中心からの相対Z)。目線=地上160uu */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Crouch")
	float StandingCameraHeight = 64.f;

	/** しゃがみ状態のカメラ高さ(しゃがみカプセル中心からの相対Z)。目線=地上110uu */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Crouch")
	float CrouchedCameraHeight = 50.f;

	/**
	 * カメラ高さの補間速度(FInterpTo)。10で体感約0.25秒の遷移になり、
	 * VALORANTのしゃがみモーションの所要時間に近い。
	 * 大きいほど速く、従来の「瞬間移動」は∞に相当する。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Crouch", meta = (ClampMin = "1.0"))
	float CrouchCameraInterpSpeed = 10.f;

	// ==============================
	// 視点設定(VALORANT準拠チューニング)
	// ==============================

	/**
	 * マウス感度。★VALORANTのゲーム内感度と同一スケール★
	 * (DefaultInput.iniのMouse2D係数0.07と合わせ、回転角=0.07°×本値×カウント数。
	 *  これはVALORANTの感度式そのもの)。普段使いの感度をそのまま入力できる。
	 * 既定0.5はVALORANTの初期値相当。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Look", meta = (ClampMin = "0.01"))
	float MouseSensitivity = 0.5f;

	/**
	 * 上下視点の反転フラグ。
	 * fix3でDefaultInput.iniの bEnableLegacyInputScales を False にしたため、
	 * 符号の意味がfix2までと逆になっている点に注意(実装コメント参照)。
	 * 既定(false)で「マウス上=見上げる」。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Look")
	bool bInvertLookY = false;

	/**
	 * 水平視野角(度)。VALORANTは16:9基準の水平FOV=103で固定なので合わせる。
	 * ※DefaultInput.iniで bEnableFOVScaling=False にしており、
	 *   FOVを変えても感度は変化しない(VALORANTと同じ挙動)。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Look", meta = (ClampMin = "60.0", ClampMax = "140.0"))
	float CameraFOV = 103.f;

	// ==============================
	// 入力コールバック
	// ==============================
	// ジャンプ/しゃがみは ACharacter 標準の Jump/StopJumping・Crouch/UnCrouch へ
	// 直接バインドするため、本クラスに独自の処理関数は持たない。

	/** WASD移動。コントローラのヨーのみを基準に前後左右の移動入力を加える */
	void Input_Move(const FInputActionValue& Value);

	/** マウス視点。X=ヨー(左右), Y=ピッチ(上下) */
	void Input_Look(const FInputActionValue& Value);

private:
	/** Tickで目指すカメラ相対高さ(しゃがみ状態から毎フレーム決定する) */
	float GetTargetCameraHeight() const;
};
