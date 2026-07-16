// UMovementTestingComponent の実装
#include "MovementTestingComponent.h"

#include "AimTrainerCharacter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

// 計測HUDの常時表示(セッション外でも速度等を見たい場合): at.MoveDebug 1
static TAutoConsoleVariable<int32> CVarAimTrainerMoveDebug(
	TEXT("at.MoveDebug"),
	0,
	TEXT("1=移動計測HUDを常時表示(セッション中は自動表示されるため通常は不要)"),
	ECVF_Default);

// CSVのTestName列に入る試験名: at.TestName AD_Switch など
static TAutoConsoleVariable<FString> CVarAimTestName(
	TEXT("at.TestName"),
	TEXT("Manual"),
	TEXT("計測セッションの試験名(CSVのTestName列)。例: at.TestName W_Accel"),
	ECVF_Default);

// ==============================
// 統計
// ==============================

float FAimMetricStats::Avg() const
{
	if (Samples.Num() == 0) return 0.f;
	float S = 0.f;
	for (float V : Samples) S += V;
	return S / Samples.Num();
}

float FAimMetricStats::MinV() const
{
	return Samples.Num() ? FMath::Min(Samples) : 0.f;
}

float FAimMetricStats::MaxV() const
{
	return Samples.Num() ? FMath::Max(Samples) : 0.f;
}

float FAimMetricStats::StdDev() const
{
	const int32 N = Samples.Num();
	if (N < 2) return 0.f;
	const float M = Avg();
	float SS = 0.f;
	for (float V : Samples) SS += FMath::Square(V - M);
	return FMath::Sqrt(SS / N);
}

// 表示名(HUD=英語: デバッグフォントに日本語グリフがないため)
static const TCHAR* MetricName(EAimMetric M)
{
	switch (M)
	{
	case EAimMetric::Accel:   return TEXT("Accel 0-95%");
	case EAimMetric::Stop:    return TEXT("Release-Stop");
	case EAimMetric::Reverse: return TEXT("Reverse-Stop");
	case EAimMetric::Crouch:  return TEXT("Crouch-Sink ");
	case EAimMetric::Jump:    return TEXT("Jump-Airtime");
	default:                  return TEXT("?");
	}
}

// ==============================
// ライフサイクル
// ==============================

UMovementTestingComponent::UMovementTestingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UMovementTestingComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerCharacter = Cast<AAimTrainerCharacter>(GetOwner());

	// Phase2: 再現条件用にスポーン時の位置・向きを記録する
	if (AActor* Owner = GetOwner())
	{
		StartTransform = Owner->GetActorTransform();
		if (const APawn* Pawn = Cast<APawn>(Owner))
		{
			if (const AController* C = Pawn->GetController())
			{
				StartControlRotation = C->GetControlRotation();
			}
		}
	}
}

// ==============================
// セッション制御(Phase1/2)
// ==============================

void UMovementTestingComponent::StartSession()
{
	// 統計と検出状態をリセット
	for (FAimMetricStats& S : Stats) S.Reset();
	TrialCounter = 0;
	RowAccel = RowStop = RowReverse = RowMaxSpeed = -1.f;
	AccelStartTime = BrakeStartTime = FlipStartTime = -1.f;
	CrouchStartTime = JumpStartTime = -1.f;
	bSessionActive = true;

	AAimTrainerCharacter* Ch = OwnerCharacter.Get();

	// Phase2: 初期位置へテレポート(毎回同じ位置・向き・速度0から測る)
	if (bResetToStartOnSession && Ch)
	{
		Ch->SetActorTransform(StartTransform, false, nullptr, ETeleportType::TeleportPhysics);
		Ch->GetCharacterMovement()->StopMovementImmediately();
		if (AController* C = Ch->GetController())
		{
			C->SetControlRotation(StartControlRotation);
		}
	}

	// Phase2: FPS固定(t.MaxFPS を退避してから設定。終了時に復元)
	if (IConsoleVariable* MaxFPS = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS")))
	{
		PrevMaxFPS = MaxFPS->GetFloat();
		if (FixedTestFPS > 0.f)
		{
			MaxFPS->Set(FixedTestFPS, ECVF_SetByConsole);
		}
	}

	const FString TestName = CVarAimTestName.GetValueOnGameThread();
	UE_LOG(LogTemp, Log, TEXT("[AimTest] セッション開始 TestName=%s FixedFPS=%.0f"), *TestName, FixedTestFPS);
#if !UE_BUILD_SHIPPING
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(9100, 4.f, FColor::Green,
			FString::Printf(TEXT("[AimTest] SESSION START (%s)  F6=End  F7=Report"), *TestName));
	}
#endif
}

void UMovementTestingComponent::EndSession()
{
	if (!bSessionActive)
	{
		return;
	}
	FlushTrialRowIfComplete(/*bForce=*/true);
	bSessionActive = false;

	// FPS設定を復元
	if (IConsoleVariable* MaxFPS = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS")))
	{
		MaxFPS->Set(PrevMaxFPS, ECVF_SetByConsole);
	}

	// Phase4: 終了時に差分レポートを自動生成
	PrintReport();
	UE_LOG(LogTemp, Log, TEXT("[AimTest] セッション終了"));
}

// ==============================
// 毎フレーム処理
// ==============================

void UMovementTestingComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!OwnerCharacter.IsValid() || !GetWorld())
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();

	// セッション中のみ計測する(HUDはCVarでも表示可)
	if (bSessionActive)
	{
		DetectMovementEvents(Now);
		DetectCrouchEvents(Now);
		DetectJumpEvents(Now);
		FlushTrialRowIfComplete();
	}

	UpdateHud();
}

// ==============================
// 検出ロジック(Phase1)
// ==============================

void UMovementTestingComponent::DetectMovementEvents(float Now)
{
	AAimTrainerCharacter* Ch = OwnerCharacter.Get();
	const UCharacterMovementComponent* Move = Ch->GetCharacterMovement();
	const FVector Accel = Move->GetCurrentAcceleration();
	FVector Vel = Ch->GetVelocity();
	Vel.Z = 0.f;

	const float Speed = Vel.Size();
	const float TopSpeed = FMath::Max(Move->MaxWalkSpeed, 1.f);
	const bool bHasInput = !Accel.IsNearlyZero();

	// 試行中の最高速度を記録(CSVのMaxSpeed列)
	if (bHasInput || RowHasData())
	{
		RowMaxSpeed = FMath::Max(RowMaxSpeed, Speed);
	}

	// --- 計測1: 入力開始 → 最高速95%到達 ---
	if (bHasInput && !bPrevHasInput && Speed < 30.f)
	{
		AccelStartTime = Now;
	}
	if (!bHasInput)
	{
		AccelStartTime = -1.f;
	}
	if (AccelStartTime >= 0.f && Speed >= TopSpeed * 0.95f)
	{
		RecordSample(EAimMetric::Accel, (Now - AccelStartTime) * 1000.f);
		AccelStartTime = -1.f;
	}

	// --- 計測2: キー解放 → 停止 ---
	if (!bHasInput && bPrevHasInput && Speed > TopSpeed * 0.5f)
	{
		BrakeStartTime = Now;
	}
	if (bHasInput)
	{
		BrakeStartTime = -1.f;
	}
	if (BrakeStartTime >= 0.f && Speed <= 5.f)
	{
		RecordSample(EAimMetric::Stop, (Now - BrakeStartTime) * 1000.f);
		BrakeStartTime = -1.f;
	}

	// --- 計測3: 逆キー入力 → 速度反転点 ---
	const FVector VelDir = Vel.GetSafeNormal();
	const FVector AccelDir = Accel.GetSafeNormal();
	const float MoveDot = FVector::DotProduct(VelDir, AccelDir);

	if (FlipStartTime < 0.f && bHasInput && Speed > TopSpeed * 0.5f && MoveDot < -0.7f)
	{
		FlipStartTime = Now;
	}
	if (FlipStartTime >= 0.f)
	{
		if (!bHasInput)
		{
			FlipStartTime = -1.f;
		}
		else if (Speed <= 20.f || MoveDot > 0.2f)
		{
			RecordSample(EAimMetric::Reverse, (Now - FlipStartTime) * 1000.f);
			FlipStartTime = -1.f;
		}
	}

	bPrevHasInput = bHasInput;
}

void UMovementTestingComponent::DetectCrouchEvents(float Now)
{
	AAimTrainerCharacter* Ch = OwnerCharacter.Get();
	const bool bCrouched = Ch->bIsCrouched;

	// しゃがみ開始
	if (bCrouched && !bPrevCrouched)
	{
		CrouchStartTime = Now;
	}
	// 立ち始めたら計測破棄(沈み途中でのキャンセル)
	if (!bCrouched)
	{
		CrouchStartTime = -1.f;
	}
	// 沈み完了 = カメラブレンド終了(最大沈み位置へ到達)
	if (CrouchStartTime >= 0.f && bCrouched && !Ch->IsCameraBlending())
	{
		RecordSample(EAimMetric::Crouch, (Now - CrouchStartTime) * 1000.f);
		CrouchStartTime = -1.f;
	}

	bPrevCrouched = bCrouched;
}

void UMovementTestingComponent::DetectJumpEvents(float Now)
{
	AAimTrainerCharacter* Ch = OwnerCharacter.Get();
	const bool bFalling = Ch->GetCharacterMovement()->IsFalling();

	// 離地
	if (bFalling && !bPrevFalling)
	{
		JumpStartTime = Now;
	}
	// 着地 = 滞空時間確定
	if (!bFalling && bPrevFalling && JumpStartTime >= 0.f)
	{
		RecordSample(EAimMetric::Jump, (Now - JumpStartTime) * 1000.f);
		JumpStartTime = -1.f;
	}

	bPrevFalling = bFalling;
}

// ==============================
// 記録(Phase3)
// ==============================

void UMovementTestingComponent::RecordSample(EAimMetric Metric, float Ms)
{
	Stats[(int32)Metric].Add(Ms);

#if !UE_BUILD_SHIPPING
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(INDEX_NONE, 3.f, FColor::Green,
			FString::Printf(TEXT("[AimTest] %s : %.0f ms"), MetricName(Metric), Ms));
	}
#endif

	// CSV行の構成: 移動系は1試行行に集約、しゃがみ/ジャンプは即時に単独行
	switch (Metric)
	{
	case EAimMetric::Accel:   RowAccel = Ms; break;
	case EAimMetric::Stop:    RowStop = Ms; break;
	case EAimMetric::Reverse: RowReverse = Ms; break;
	case EAimMetric::Crouch:
	{
		++TrialCounter;
		AppendCsvRow(FString::Printf(TEXT("%s,%s,%02d,,,,,,%.0f"),
			*FDateTime::Now().ToString(TEXT("%Y/%m/%d")),
			*CVarAimTestName.GetValueOnGameThread(), TrialCounter, Ms));
		break;
	}
	case EAimMetric::Jump:
	{
		++TrialCounter;
		AppendCsvRow(FString::Printf(TEXT("%s,%s,%02d,,,,,%.0f,"),
			*FDateTime::Now().ToString(TEXT("%Y/%m/%d")),
			*CVarAimTestName.GetValueOnGameThread(), TrialCounter, Ms));
		break;
	}
	default: break;
	}
}

void UMovementTestingComponent::FlushTrialRowIfComplete(bool bForce)
{
	if (!RowHasData())
	{
		return;
	}

	// 完結条件: 完全停止して入力もない(=1回の移動試行が終わった)
	bool bComplete = bForce;
	if (!bComplete && OwnerCharacter.IsValid())
	{
		FVector Vel = OwnerCharacter->GetVelocity();
		Vel.Z = 0.f;
		const bool bHasInput =
			!OwnerCharacter->GetCharacterMovement()->GetCurrentAcceleration().IsNearlyZero();
		bComplete = (Vel.Size() <= 5.f && !bHasInput);
	}
	if (!bComplete)
	{
		return;
	}

	++TrialCounter;
	auto Cell = [](float V) { return V >= 0.f ? FString::Printf(TEXT("%.0f"), V) : FString(); };
	AppendCsvRow(FString::Printf(TEXT("%s,%s,%02d,%s,%s,%s,%s,,"),
		*FDateTime::Now().ToString(TEXT("%Y/%m/%d")),
		*CVarAimTestName.GetValueOnGameThread(),
		TrialCounter,
		*Cell(RowAccel), *Cell(RowStop), *Cell(RowReverse), *Cell(RowMaxSpeed)));

	RowAccel = RowStop = RowReverse = RowMaxSpeed = -1.f;
}

void UMovementTestingComponent::AppendCsvRow(const FString& Row)
{
	const FString Dir = FPaths::ProjectSavedDir() / TEXT("AimTrainer");
	IFileManager::Get().MakeDirectory(*Dir, /*Tree=*/true);

	const FString File = Dir / FString::Printf(TEXT("AimTrainerLog_%s.csv"),
		*FDateTime::Now().ToString(TEXT("%Y%m%d")));

	// 初回のみヘッダを書く(Phase3指定の列 + CrouchTime_ms を末尾に追加)
	if (!FPaths::FileExists(File))
	{
		const FString Header = TEXT("Date,TestName,Trial,AccelerationTime_ms,StopTime_ms,ReverseTime_ms,MaxSpeed,JumpTime_ms,CrouchTime_ms\r\n");
		FFileHelper::SaveStringToFile(Header, *File, FFileHelper::EEncodingOptions::ForceUTF8);
	}
	FFileHelper::SaveStringToFile(Row + TEXT("\r\n"), *File,
		FFileHelper::EEncodingOptions::ForceUTF8, &IFileManager::Get(), FILEWRITE_Append);
}

// ==============================
// HUD(Phase1)
// ==============================

void UMovementTestingComponent::UpdateHud()
{
#if !UE_BUILD_SHIPPING
	const bool bShow = bSessionActive || CVarAimTrainerMoveDebug.GetValueOnGameThread() != 0;
	if (!bShow || !GEngine || !OwnerCharacter.IsValid())
	{
		return;
	}

	FVector Vel = OwnerCharacter->GetVelocity();
	Vel.Z = 0.f;
	const float Top = FMath::Max(OwnerCharacter->GetCharacterMovement()->MaxWalkSpeed, 1.f);

	// 1行目: セッション状態と速度
	const FString TestName = CVarAimTestName.GetValueOnGameThread();
	GEngine->AddOnScreenDebugMessage(9001, 0.f, bSessionActive ? FColor::Green : FColor::Cyan,
		FString::Printf(TEXT("[AimTest] %s  Test=%s  Speed %4.0f (%3.0f%%)  F5=Start F6=End F7=Report"),
			bSessionActive ? TEXT("RECORDING") : TEXT("idle"), *TestName, Vel.Size(), Vel.Size() / Top * 100.f));

	// 2行目以降: 各計測項目の統計(n / 平均 / 最小 / 最大 / 標準偏差)
	for (int32 i = 0; i < (int32)EAimMetric::COUNT; ++i)
	{
		const FAimMetricStats& S = Stats[i];
		const FString Line = S.Num() == 0
			? FString::Printf(TEXT("%s : n=0"), MetricName((EAimMetric)i))
			: FString::Printf(TEXT("%s : n=%d  avg=%5.0f  min=%5.0f  max=%5.0f  sd=%4.1f"),
				MetricName((EAimMetric)i), S.Num(), S.Avg(), S.MinV(), S.MaxV(), S.StdDev());
		GEngine->AddOnScreenDebugMessage(9002 + i, 0.f, FColor::White, Line);
	}
#endif
}

// ==============================
// レポート(Phase4)
// ==============================

const TCHAR* UMovementTestingComponent::Judge(float DiffAbs, float OkMs, float AdjustMs)
{
	if (DiffAbs <= OkMs)     return TEXT("OK");
	if (DiffAbs <= AdjustMs) return TEXT("ADJUST");
	return TEXT("FIX-PARAM");
}

FString UMovementTestingComponent::BuildReport() const
{
	const float Refs[(int32)EAimMetric::COUNT] =
		{ RefAccelMs, RefStopMs, RefReverseMs, RefCrouchMs, RefJumpMs };

	FString R;
	R += FString::Printf(TEXT("==== AimTrainer Report %s  Test=%s ====\r\n"),
		*FDateTime::Now().ToString(TEXT("%Y/%m/%d %H:%M:%S")),
		*CVarAimTestName.GetValueOnGameThread());
	R += TEXT("Metric        | VALORANT | UE5(avg)      | Diff    | Judge\r\n");

	for (int32 i = 0; i < (int32)EAimMetric::COUNT; ++i)
	{
		const FAimMetricStats& S = Stats[i];
		if (S.Num() == 0)
		{
			R += FString::Printf(TEXT("%s | %6.0fms | (no data)     |         |\r\n"),
				MetricName((EAimMetric)i), Refs[i]);
			continue;
		}
		const float Diff = S.Avg() - Refs[i];
		R += FString::Printf(TEXT("%s | %6.0fms | %5.0fms (n=%2d) | %+5.0fms | %s\r\n"),
			MetricName((EAimMetric)i), Refs[i], S.Avg(), S.Num(), Diff,
			Judge(FMath::Abs(Diff), JudgeOkMs, JudgeAdjustMs));
	}
	R += FString::Printf(TEXT("Judge: <=%.0fms OK / <=%.0fms ADJUST / over FIX-PARAM\r\n"),
		JudgeOkMs, JudgeAdjustMs);
	return R;
}

void UMovementTestingComponent::PrintReport()
{
	const FString Report = BuildReport();

	// ログへ
	UE_LOG(LogTemp, Log, TEXT("\n%s"), *Report);

	// ファイルへ(Saved/AimTrainer/Report_日時.txt)
	const FString Dir = FPaths::ProjectSavedDir() / TEXT("AimTrainer");
	IFileManager::Get().MakeDirectory(*Dir, true);
	const FString File = Dir / FString::Printf(TEXT("Report_%s.txt"),
		*FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
	FFileHelper::SaveStringToFile(Report, *File, FFileHelper::EEncodingOptions::ForceUTF8);

	// 画面へ(行ごとに表示)
#if !UE_BUILD_SHIPPING
	if (GEngine)
	{
		TArray<FString> Lines;
		Report.ParseIntoArrayLines(Lines);
		int32 Key = 9200;
		for (const FString& L : Lines)
		{
			GEngine->AddOnScreenDebugMessage(Key++, 20.f, FColor::Yellow, L);
		}
	}
#endif
	UE_LOG(LogTemp, Log, TEXT("[AimTest] レポートを保存しました: %s"), *File);
}
