// UMovementTestingComponent: fix5検証用の計測・統計・記録コンポーネント
// 【目的】fix5のパラメータが実際にVALORANT実測値どおりの挙動を出しているかを
//         「計測可能」にする(Phase1〜4)。ゲームロジックは持たない検証専用機能。
//
// 【機能】
//  Phase1: 計測セッション(F5開始/F6終了/F7レポート) + 試行回数/平均/最大/最小/標準偏差
//  Phase2: セッション開始時の再現条件適用(初期位置へテレポート・t.MaxFPS固定)
//  Phase3: CSV自動保存(Saved/AimTrainer/AimTrainerLog_日付.csv)
//  Phase4: 参照値(VALORANT実測由来)との差分レポート生成(画面+ログ+txt保存)
//
// 【計測項目】
//  1. 0→95%最高速到達時間  2. キー解放→停止時間  3. 逆キー→停止(速度反転)時間
//  4. しゃがみ開始→最大沈み時間  5. ジャンプ滞空時間  + 試行中の最高速度
//
// 【注意】画面表示(HUD)はUEデバッグフォントに日本語グリフがないため英語表記。
//         ログ・CSV・レポートファイルは日本語を含むUTF-8で出力する。
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MovementTestingComponent.generated.h"

class AAimTrainerCharacter;

/** 計測項目の種別 */
enum class EAimMetric : uint8
{
	Accel = 0,   // 0→95%最高速
	Stop,        // キー解放→停止
	Reverse,     // 逆キー→速度反転
	Crouch,      // しゃがみ開始→最大沈み
	Jump,        // ジャンプ滞空
	COUNT
};

/** 1計測項目ぶんの標本と統計 */
struct FAimMetricStats
{
	TArray<float> Samples;

	void Add(float V) { Samples.Add(V); }
	void Reset() { Samples.Reset(); }
	int32 Num() const { return Samples.Num(); }
	float Avg() const;
	float MinV() const;
	float MaxV() const;
	float StdDev() const;  // 母標準偏差
};

UCLASS(ClassGroup = (AimTrainer), config = Game, meta = (BlueprintSpawnableComponent))
class VALO_AIMTRAINER_API UMovementTestingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMovementTestingComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** F5: 計測セッション開始(統計リセット・初期位置へテレポート・FPS固定) */
	void StartSession();

	/** F6: 計測セッション終了(レポート出力・FPS設定を復元) */
	void EndSession();

	/** F7: 現時点の統計で差分レポートを出力(セッションは継続) */
	void PrintReport();

	// ==============================
	// Phase4: 参照値(VALORANT実測から導出したfix5理論値。Config/ini で変更可能)
	// ==============================

	/** 0→95%最高速の参照値ms (= 0.95×540 / MoveAcceleration3000) */
	UPROPERTY(Config, EditAnywhere, Category = "AimTest|Reference")
	float RefAccelMs = 171.f;

	/** キー解放→停止の参照値ms (= 540 / MoveBrakingDeceleration7500 +停止判定余裕) */
	UPROPERTY(Config, EditAnywhere, Category = "AimTest|Reference")
	float RefStopMs = 71.f;

	/** 逆キー→速度反転の参照値ms (摩擦4.5+加速3000でのフィット値) */
	UPROPERTY(Config, EditAnywhere, Category = "AimTest|Reference")
	float RefReverseMs = 105.f;

	/** しゃがみ沈み時間の参照値ms (VALORANT実測311±8ms→設定値300) */
	UPROPERTY(Config, EditAnywhere, Category = "AimTest|Reference")
	float RefCrouchMs = 300.f;

	/** ジャンプ滞空の参照値ms (VALORANT実測756±8ms) */
	UPROPERTY(Config, EditAnywhere, Category = "AimTest|Reference")
	float RefJumpMs = 755.f;

	/** 判定しきい値: この差分以内なら OK */
	UPROPERTY(Config, EditAnywhere, Category = "AimTest|Reference", meta = (ClampMin = "1.0"))
	float JudgeOkMs = 10.f;

	/** 判定しきい値: この差分以内なら 微調整(ADJUST)。超えたら パラメータ修正(FIX) */
	UPROPERTY(Config, EditAnywhere, Category = "AimTest|Reference", meta = (ClampMin = "1.0"))
	float JudgeAdjustMs = 30.f;

	// ==============================
	// Phase2: 再現条件
	// ==============================

	/** セッション中に固定するFPS(0=固定しない)。t.MaxFPS を書き換え、終了時に復元する */
	UPROPERTY(Config, EditAnywhere, Category = "AimTest|Repro", meta = (ClampMin = "0.0"))
	float FixedTestFPS = 120.f;

	/** セッション開始時にスポーン位置・向きへテレポートして速度をゼロにする */
	UPROPERTY(Config, EditAnywhere, Category = "AimTest|Repro")
	bool bResetToStartOnSession = true;

private:
	// ==============================
	// 内部処理(細分化)
	// ==============================

	/** 移動(加速/停止/逆キー)の検出。fix4のMoveDebugロジックを移設・拡張 */
	void DetectMovementEvents(float Now);

	/** しゃがみ(開始→沈み完了)の検出 */
	void DetectCrouchEvents(float Now);

	/** ジャンプ(離地→着地)の検出 */
	void DetectJumpEvents(float Now);

	/** 標本を登録し、CSV行バッファへ書き込む */
	void RecordSample(EAimMetric Metric, float Ms);

	/** 現在の試行行が完結していればCSVへ書き出す */
	void FlushTrialRowIfComplete(bool bForce = false);

	/** CSVへ1行追記(ヘッダは初回に自動生成) */
	void AppendCsvRow(const FString& Row);

	/** HUD(画面左上)の統計表示を更新 */
	void UpdateHud();

	/** レポート文字列を生成する */
	FString BuildReport() const;

	/** 判定文字列(OK / ADJUST / FIX-PARAM)を返す */
	static const TCHAR* Judge(float DiffAbs, float OkMs, float AdjustMs);

	/** 所有キャラクター(型付き) */
	TWeakObjectPtr<AAimTrainerCharacter> OwnerCharacter;

	// --- セッション状態 ---
	bool bSessionActive = false;
	int32 TrialCounter = 0;
	FAimMetricStats Stats[(int32)EAimMetric::COUNT];

	/** セッション開始時に退避する t.MaxFPS の元値 */
	float PrevMaxFPS = 0.f;

	/** スポーン時の位置・向き(テレポート用) */
	FTransform StartTransform;
	FRotator StartControlRotation = FRotator::ZeroRotator;

	// --- CSV試行行バッファ(1試行=加速/停止or逆キー/最高速を1行に集約) ---
	float RowAccel = -1.f, RowStop = -1.f, RowReverse = -1.f, RowMaxSpeed = -1.f;
	bool RowHasData() const { return RowAccel >= 0 || RowStop >= 0 || RowReverse >= 0; }

	// --- 移動検出の状態(fix4から移設) ---
	float AccelStartTime = -1.f;
	float BrakeStartTime = -1.f;
	float FlipStartTime = -1.f;
	bool bPrevHasInput = false;

	// --- しゃがみ/ジャンプ検出の状態 ---
	bool bPrevCrouched = false;
	bool bPrevFalling = false;
	float CrouchStartTime = -1.f;
	float JumpStartTime = -1.f;
};
