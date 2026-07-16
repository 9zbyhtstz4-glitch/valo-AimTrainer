// AimTrainerCharacter: エイム練習用のFPSキャラクター
// 【実装済みの範囲】
//  - ステップ1〜4: FPS視点/マウス視点・WASD移動・ジャンプ・しゃがみ
//  - fix2: IMC登録タイミングの是正 + 入力設定不備の可視化
//  - fix3: VALORANT準拠チューニング(移動加減速/スムーズしゃがみ/感度式統一/FOV103)
//  - fix4: VALORANTとの差分分析に基づく再調整
//  - fix5: 実機録画の光学解析による実測値でパラメータを確定
//  - fix6: 検証システム化(本修正)
//      * 計測ロジックを UMovementTestingComponent へ分離(統計/CSV/レポート)
//      * F5=計測開始 / F6=終了+レポート / F7=途中レポート
//      * チューニング値を Config 化(Config/DefaultGame.ini から変更可能。
//        以後の調整は「ini変更→計測→比較」だけで完結し、コード変更不要)
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
class UMovementTestingComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

UCLASS(config = Game)
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

	/** しゃがみカメラのブレンド中か(計測コンポーネントが沈み完了判定に使用) */
	bool IsCameraBlending() const { return CameraBlendElapsed >= 0.f; }

	/** しゃがみ状態に応じた目標カメラ相対高さ */
	float GetTargetCameraHeight() const;

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

	/** fix6: 計測・統計・CSV・レポートを担う検証コンポーネント(F5/F6/F7で操作) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AimTrainer|Testing")
	TObjectPtr<UMovementTestingComponent> TestingComponent;

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
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Move", meta = (ClampMin = "1.0"))
	float WalkSpeed = 540.f;

	/** しゃがみ移動速度。【推測】走行の半分(公式数値は非公開) */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Move", meta = (ClampMin = "1.0"))
	float CrouchedSpeed = 270.f;

	/**
	 * 最大加速度(uu/s^2)。
	 * 【実測】VALORANT録画のA/D/AD全11試行を整列平均した加速カーブより
	 *   20-80%到達=106ms, 10-90%=149ms → a = 0.6×540/0.106 ≒ 3057,
	 *   0.8×540/0.149 ≒ 2899。両推定の中間の3000を採用(最高速まで180ms)。
	 * 補足: コミュニティの「押した瞬間最高速」という証言は実測と一致しなかった
	 * (fix4の8192は速すぎた)。実測を優先する。
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Move", meta = (ClampMin = "1.0"))
	float MoveAcceleration = 3000.f;

	/**
	 * キー解放時の制動減速度(uu/s^2)。
	 * 【実測】解放停止カーブ(n=8整列平均)の20-80%減速=43ms
	 *   → D = 0.6×540/0.043 ≒ 7535 → 7500を採用。
	 *   全停止まで約72ms・滑走距離 540^2/(2×7500) ≒ 19cm。
	 * 実測では「解放停止(43ms)は逆キー減速(62ms)より速い」= VALORANTでは
	 * カウンターストレイフの利得がほぼ無い、という通説どおりの結果になった。
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Move", meta = (ClampMin = "0.0"))
	float MoveBrakingDeceleration = 7500.f;

	/**
	 * 地上摩擦(逆キー押下中に旧速度を打ち消す強さ。UEでは減速レート≒2f×v+加速度)。
	 * 【実測】逆キー減速カーブ(n=3)の20-80%=62ms を a=3000 と組み合わせて逆算:
	 *   ∫dv/(2f·v+3000) [108→432] = 0.062 → f ≒ 4〜5 → 4.5を採用。
	 * fix4の10では逆キーが実測より効きすぎていた。
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Move", meta = (ClampMin = "0.0"))
	float MoveGroundFriction = 4.5f;

	/**
	 * 制動時専用の摩擦。fix4で 10 → 0 へ変更(重要)。
	 * 摩擦項は速度比例のため、>0 だと「最初だけ強く効き、終端がだらだら残る」
	 * 指数プロファイルになる。VALORANTの停止は「短い一定の滑り→ピタッと停止」に
	 * 見えるため(【推測】映像目視)、摩擦0+一定減速のみの線形プロファイルとし、
	 * 停止完了時刻を明確にする。調整したい場合のためプロパティは残す。
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Move", meta = (ClampMin = "0.0"))
	float MoveBrakingFriction = 0.f;

	/** 空中制御(0〜1)。【推測】VALORANTは空中でわずかに軌道修正できる程度 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Move", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AirControlAmount = 0.15f;

	// ==============================
	// ジャンプ設定(fix4では対象外・fix3の値を維持)
	// ==============================

	/**
	 * 【実測】VALORANT録画のジャンプ滞空時間 = 756±8ms (n=3)。
	 * t=2v/g より、GravityScale=1.0(g=980) との組で v=370 → 滞空755ms・高さ約70cm。
	 * (fix4の456/1.4=滞空665msは実測より約90ms短かった)
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Jump", meta = (ClampMin = "1.0"))
	float JumpZVelocity = 370.f;

	/** 【実測由来】滞空756msと高さ約70cmを両立する現実重力相当(1.0) */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Jump", meta = (ClampMin = "0.1"))
	float GravityScale = 1.0f;

	// ==============================
	// しゃがみ設定(fix4: 固定時間ブレンドへ変更)
	// ==============================

	/** しゃがみ時のカプセル半分高さ。立ち96→60(全高192→120cm)。【推測】半径42は不変 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Crouch", meta = (ClampMin = "30.0"))
	float CrouchedCapsuleHalfHeight = 60.f;

	/** 立ち状態のカメラ高さ(カプセル中心からの相対Z)。目線=地上160uu【推測】 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Crouch")
	float StandingCameraHeight = 64.f;

	/** しゃがみ状態のカメラ高さ。目線=地上110uu【推測】 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Crouch")
	float CrouchedCameraHeight = 50.f;

	/**
	 * しゃがみ/立ちのカメラ遷移にかける時間(秒)。
	 * 【実測】VALORANT録画: 沈み 311±8ms (n=3) / 立ち 300±0ms (n=3) → 0.30を採用。
	 * 途中で反転しても現在位置から再ブレンドする。
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Crouch", meta = (ClampMin = "0.01"))
	float CrouchTransitionTime = 0.30f;

	/**
	 * しゃがみカメラのイージング指数(1.0=等速, 2.0=強いS字)。
	 * 【実測】遷移中の縦速度カーブのピーク/平均比 = 1.20〜1.24。
	 * SmoothStep相当(比1.5)よりフラットなため、InterpEaseInOutの指数1.3で再現する
	 * (指数eのとき中央速度/平均速度 ≒ e)。
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Crouch", meta = (ClampMin = "1.0", ClampMax = "3.0"))
	float CrouchEaseExponent = 1.3f;

	// ==============================
	// 視点設定(fix3で確定済み・変更なし)
	// ==============================

	/** 【確定】VALORANTのゲーム内感度と同一スケール(0.07°/カウント×感度) */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Look", meta = (ClampMin = "0.01"))
	float MouseSensitivity = 0.5f;

	/** 上下視点の反転(既定falseで「マウス上=見上げる」) */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Look")
	bool bInvertLookY = false;

	/** 【確定】VALORANTは水平FOV=103固定 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "AimTrainer|Look", meta = (ClampMin = "60.0", ClampMax = "140.0"))
	float CameraFOV = 103.f;

	// ==============================
	// 入力コールバック
	// ==============================

	void Input_Move(const FInputActionValue& Value);
	void Input_Look(const FInputActionValue& Value);

private:
	// --- しゃがみカメラの固定時間ブレンド ---

	/** 現在のカメラ高さを起点にブレンドを開始する(途中反転にも対応) */
	void StartCameraHeightBlend();

	/** ブレンド開始時のカメラ相対Z */
	float CameraBlendStartZ = 0.f;

	/** ブレンド経過秒。負の値は「補間停止中」を表す */
	float CameraBlendElapsed = -1.f;

	// 計測HUD・統計・CSV・レポートは UMovementTestingComponent へ移設(fix6)
};

// 既存のインクルード群の下に前方宣言を追加
class AWeaponBase;
class UInputAction; // Enhanced Input用

UCLASS()
class VALO_AIMTRAINER_API AAimTrainerCharacter : public ACharacter
{
	GENERATED_BODY()

	// ... (既存のコンストラクタ等のコード) ...

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

public:
	/* --- 武器システム追加部分 --- */

	// Blueprintで指定する武器のクラス（ARifleBase等のBPを指定）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	TSubclassOf<AWeaponBase> DefaultWeaponClass;

	// 現在装備している武器の参照
	UPROPERTY(BlueprintReadOnly, Category = "Weapon")
	AWeaponBase* CurrentWeapon;

	// 射撃入力用のアクション (Enhanced Input)
	// エディタ側で InputAction をアサインしてください
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* FireAction;

protected:
	// 射撃入力コールバック
	void OnFireStart();
	void OnFireStop();
};