// AimTrainerCharacter: エイム練習用のFPSキャラクター
// 【実装済みの範囲】
//  - ステップ1〜4: FPS視点/マウス視点・WASD移動・ジャンプ・しゃがみ
//  - fix2: IMC登録タイミングの是正 + 入力設定不備の可視化
//  - fix3: VALORANT準拠チューニング(移動加減速/スムーズしゃがみ/感度式統一/FOV103)
//  - fix4: VALORANTとの差分分析に基づく再調整(本修正)
//      * 移動: 「ほぼ即時に最高速/解放でもほぼ即停止/逆キーの短縮は数ms程度」という
//              公開証言に合わせ、加速・制動を8192へ引き上げ+減速プロファイルを線形化
//      * しゃがみ: 指数補間(終端が漸近して完了しない)を廃止し、
//                  固定時間0.25秒+SmoothStepの「必ず完了する」遷移へ変更
//      * 計測: コンソール変数 at.MoveDebug 1 で移動応答の実測HUDを表示
//              (0→最高速 / 解放→停止 / 逆キー→停止 の各ミリ秒を画面表示)
//
// 数値の根拠はコメントに明記する。VALORANTの内部値は非公開のため、
// 「確定(公式/確立情報)」と「推測(目標値)」を区別して記載している。
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
	/** しゃがみカメラの固定時間ブレンドと、移動計測HUD(有効時のみ)を更新する */
	virtual void Tick(float DeltaSeconds) override;
	//~ End AActor interface

	//~ Begin APawn interface
	virtual void NotifyControllerChanged() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	//~ End APawn interface

	//~ Begin ACharacter interface
	/** カプセル中心の移動量をカメラ相対Zへ即時打ち消し、固定時間ブレンドを開始する */
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	//~ End ACharacter interface

	/** FPSカメラ取得(今後、武器の射線計算やHUDで使用する) */
	UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }

protected:
	//~ Begin AActor interface
	virtual void BeginPlay() override;
	//~ End AActor interface

	/** チューニング用プロパティをCMCとカメラへ反映する(ctor/BeginPlayの両方から呼ぶ) */
	void ApplyCharacterSettings();

	// ==============================
	// コンポーネント
	// ==============================

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AimTrainer|Camera")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	// ==============================
	// 入力アセット(Blueprint側で割り当てる)
	// ==============================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Input")
	TObjectPtr<UInputAction> CrouchAction;

	// ==============================
	// 移動設定(fix4: 差分分析に基づく再調整)
	// ==============================

	/** 【確定】走行速度 5.4m/s(Riot公表のエージェント基本移動速度)= 540uu/s */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Move", meta = (ClampMin = "1.0"))
	float WalkSpeed = 540.f;

	/** しゃがみ移動速度。【推測】走行の半分(公式数値は非公開) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Move", meta = (ClampMin = "1.0"))
	float CrouchedSpeed = 270.f;

	/**
	 * 最大加速度(uu/s^2)。fix3の4096(0.132秒で最高速)では「もたつき」が残った。
	 * VALORANTは「押した瞬間ほぼ最高速」と形容されるほど加速が速いという
	 * 公開証言(VLR等のプレイヤー/プロの議論)に合わせ、8192へ引き上げる。
	 * 540/8192 ≒ 0.066秒で最高速到達。【推測】厳密な内部値は非公開のため、
	 * 0.066秒は目標値であり at.MoveDebug で実測しながら調整可能にしてある。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Move", meta = (ClampMin = "1.0"))
	float MoveAcceleration = 8192.f;

	/**
	 * キー解放時の制動減速度(uu/s^2)。
	 * VALORANTは「解放だけでもCSより明確に速く止まり、カウンターストレイフの
	 * 短縮効果は数ms〜わずか」という公開情報が複数ある(Dot Esports / VLR)。
	 * これをUEの挙動へ写像すると「解放時の制動 ≒ 逆キー時の制動」が必要なため、
	 * MoveAcceleration と同値の8192とする(解放停止 540/8192 ≒ 0.066秒、
	 * 滑走距離 540^2/(2×8192) ≒ 18cm。逆キー時は旋回摩擦が加わり約0.04〜0.05秒
	 * → 差は約20ms で「数msしか変わらない」証言と整合)。【推測】数値自体は目標値。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Move", meta = (ClampMin = "0.0"))
	float MoveBrakingDeceleration = 8192.f;

	/**
	 * 地上摩擦(移動中の方向転換の鋭さ)。A⇔D切り返しで旧速度を打ち消す強さ。
	 * fix3の10を維持(UE既定8では横滑りが残る)。【推測】
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Move", meta = (ClampMin = "0.0"))
	float MoveGroundFriction = 10.f;

	/**
	 * 制動時専用の摩擦。fix4で 10 → 0 へ変更(重要)。
	 * 摩擦項は速度比例のため、>0 だと「最初だけ強く効き、終端がだらだら残る」
	 * 指数プロファイルになる。VALORANTの停止は「短い一定の滑り→ピタッと停止」に
	 * 見えるため(【推測】映像目視)、摩擦0+一定減速のみの線形プロファイルとし、
	 * 停止完了時刻を明確にする。調整したい場合のためプロパティは残す。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Move", meta = (ClampMin = "0.0"))
	float MoveBrakingFriction = 0.f;

	/** 空中制御(0〜1)。【推測】VALORANTは空中でわずかに軌道修正できる程度 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Move", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AirControlAmount = 0.15f;

	// ==============================
	// ジャンプ設定(fix4では対象外・fix3の値を維持)
	// ==============================

	/** 【推測】高さ約76cm・滞空約0.66秒(GravityScale=1.4との組) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Jump", meta = (ClampMin = "1.0"))
	float JumpZVelocity = 456.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Jump", meta = (ClampMin = "0.1"))
	float GravityScale = 1.4f;

	// ==============================
	// しゃがみ設定(fix4: 固定時間ブレンドへ変更)
	// ==============================

	/** しゃがみ時のカプセル半分高さ。立ち96→60(全高192→120cm)。【推測】半径42は不変 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Crouch", meta = (ClampMin = "30.0"))
	float CrouchedCapsuleHalfHeight = 60.f;

	/** 立ち状態のカメラ高さ(カプセル中心からの相対Z)。目線=地上160uu【推測】 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Crouch")
	float StandingCameraHeight = 64.f;

	/** しゃがみ状態のカメラ高さ。目線=地上110uu【推測】 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Crouch")
	float CrouchedCameraHeight = 50.f;

	/**
	 * しゃがみ/立ちのカメラ遷移にかける時間(秒)。fix4で補間方式を変更:
	 *  fix3: FInterpTo(指数)= 立ち上がりは速いが終端が漸近し「いつ終わったか」が曖昧
	 *  fix4: 固定時間 + SmoothStep = VALORANTのように一定時間で必ず完了し、
	 *        入り/抜きが滑らかなS字カーブ
	 * 0.25秒は映像の目視による【推測】値。途中で反転しても現在位置から再ブレンドする。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Crouch", meta = (ClampMin = "0.01"))
	float CrouchTransitionTime = 0.25f;

	// ==============================
	// 視点設定(fix3で確定済み・変更なし)
	// ==============================

	/** 【確定】VALORANTのゲーム内感度と同一スケール(0.07°/カウント×感度) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Look", meta = (ClampMin = "0.01"))
	float MouseSensitivity = 0.5f;

	/** 上下視点の反転(既定falseで「マウス上=見上げる」) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Look")
	bool bInvertLookY = false;

	/** 【確定】VALORANTは水平FOV=103固定 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Look", meta = (ClampMin = "60.0", ClampMax = "140.0"))
	float CameraFOV = 103.f;

	// ==============================
	// 入力コールバック
	// ==============================

	void Input_Move(const FInputActionValue& Value);
	void Input_Look(const FInputActionValue& Value);

private:
	// --- しゃがみカメラの固定時間ブレンド ---

	/** しゃがみ状態に応じた目標カメラ相対高さ */
	float GetTargetCameraHeight() const;

	/** 現在のカメラ高さを起点にブレンドを開始する(途中反転にも対応) */
	void StartCameraHeightBlend();

	/** ブレンド開始時のカメラ相対Z */
	float CameraBlendStartZ = 0.f;

	/** ブレンド経過秒。負の値は「補間停止中」を表す */
	float CameraBlendElapsed = -1.f;

	// --- 移動計測HUD(at.MoveDebug 1 で有効。Shippingでは何もしない) ---

	/** 0→最高速 / 解放→停止 / 逆キー→停止 の各時間を実測して画面表示する */
	void UpdateMovementDebug();

	float DbgAccelStartTime = -1.f;
	float DbgBrakeStartTime = -1.f;
	float DbgFlipStartTime = -1.f;
	bool bDbgPrevHasInput = false;
};
