// AimTrainerCharacter の実装
#include "AimTrainerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"

AAimTrainerCharacter::AAimTrainerCharacter()
{
	// 視点操作はイベント駆動のため毎フレーム処理は不要
	PrimaryActorTick.bCanEverTick = false;

	// --- カプセル(当たり判定)の基本サイズ ---
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

	// --- FPS視点カメラ ---
	// 目線高さ(カプセル中心から+64uu)に取り付ける。
	// bUsePawnControlRotation によりピッチ/ヨーがコントローラ回転へ追従する。
	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, 64.f));
	FirstPersonCamera->bUsePawnControlRotation = true;

	// --- 回転設定 ---
	// FPSなので体(アクター)のヨーはコントローラへ追従させる。
	// ピッチ/ロールはカメラ側のみで表現する。
	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
}

void AAimTrainerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// --- マッピングコンテキストの登録 ---
	// BP_AimTrainerCharacter で割り当てた IMC_Default をローカルプレイヤーへ登録する。
	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (DefaultMappingContext)
			{
				Subsystem->AddMappingContext(DefaultMappingContext, /*Priority=*/0);
			}
			else
			{
				// アセット未割り当ての検出用(C++クラスを直接スポーンした場合など)
				UE_LOG(LogTemp, Warning,
					TEXT("[AimTrainer] DefaultMappingContext が未割り当てです。BP_AimTrainerCharacter で IMC_Default を設定してください。"));
			}
		}
	}

	// --- 入力バインド ---
	// DefaultInput.ini で EnhancedInputComponent を既定化済みのため通常はキャスト成功する。
	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[AimTrainer] EnhancedInputComponent が見つかりません。DefaultInput.ini の設定を確認してください。"));
		return;
	}

	// 視点操作: マウス移動中は毎フレーム発火する Triggered にバインドする
	if (LookAction)
	{
		EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &AAimTrainerCharacter::Input_Look);
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[AimTrainer] LookAction が未割り当てです。BP_AimTrainerCharacter で IA_Look を設定してください。"));
	}
}

void AAimTrainerCharacter::Input_Look(const FInputActionValue& Value)
{
	// IA_Look は Axis2D(X=マウス左右, Y=マウス上下)
	const FVector2D LookValue = Value.Get<FVector2D>();

	// ヨー: マウス右で右を向く(そのままの符号でよい)
	AddControllerYawInput(LookValue.X * MouseSensitivity);

	// ピッチ: マウス前方移動は生値が正(+Y)。
	// 本プロジェクトは DefaultInput.ini で bEnableLegacyInputScales=True のため
	// AddControllerPitchInput には旧来の InputPitchScale(負値)が掛かる。
	// そのため既定(非反転)では -1 を掛けることで「マウス上=見上げる」になる。
	// 環境差で上下が逆に感じる場合は bInvertLookY で切り替えられる。
	const float PitchSign = bInvertLookY ? 1.f : -1.f;
	AddControllerPitchInput(LookValue.Y * PitchSign * MouseSensitivity);
}
