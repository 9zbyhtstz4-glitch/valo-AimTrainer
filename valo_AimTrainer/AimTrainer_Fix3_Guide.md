# valo_AimTrainer fix3 適用・動作確認手順書

fix3 = VALORANT準拠の操作感チューニング(移動・しゃがみ・視点・ジャンプ)

## 1. 適用手順

1. **エディタを完全に終了する**(重要: 開いたままだとConfigの上書きが起動時に戻されることがあります)。
2. zip をプロジェクト直下(`valo_AimTrainer.uproject` がある場所)に展開して上書き:
   - `Source/valo_AimTrainer/AimTrainerCharacter.h`
   - `Source/valo_AimTrainer/AimTrainerCharacter.cpp`
   - `Config/DefaultInput.ini`(変更は3行のみ: MouseSmoothing / FOVScaling / LegacyInputScales を False)
3. Visual Studio でビルド(`Development Editor | Win64`)。
   - **「Unable to build while Live Coding is active」と出た場合**: エディタ(とゲーム)を終了してからビルドし直すか、エディタ内で **Ctrl+Alt+F11**(Live Codingビルド)を使ってください。Configを変更した今回は**エディタ終了→VSビルド→起動**の順を推奨します。
4. エディタを起動して PIE。

**エディタ側の追加作業はありません**(BP・IA・IMCは既存のまま使えます)。

## 2. 最初に必ずやること: 感度の再設定

fix3で感度の意味が変わりました。**MouseSensitivity = VALORANTのゲーム内感度と同一スケール**です(回転角 = 0.07°×感度×マウスカウント)。

1. `BP_AimTrainerCharacter` を開く → Details → `AimTrainer|Look` → **Mouse Sensitivity に普段のVALORANT感度を入力**(既定0.5)。DPIが同じなら振り向き距離が一致します。
2. 以前 `bInvertLookY` をONにしていた場合は**一度OFFに戻して**確認してください(内部の符号系が変わったため)。既定OFFで「マウス上=見上げる」です。

## 3. 動作確認チェックリスト(フィーリング)

- [ ] **感度**: VALORANTと同じ距離で180°振り向ける(同DPI・同感度値で比較)
- [ ] **FOV**: 視野が以前(90)より広い(=103)。壁に寄ると差が分かりやすい
- [ ] **加速**: 移動開始が機敏(体感0.1秒強で最高速)
- [ ] **停止**: キーを離すと一瞬だけ滑ってすぐ止まる/**逆キーを押すとさらに速く止まる**(カウンターストレイフ)
- [ ] **A⇔D切り返し**: 横滑りせずキレよく反転する
- [ ] **ジャンプ**: 以前より低く・短く・落下が速い(高さ約76cm、滞空約0.66秒)
- [ ] **しゃがみ**: 視点が瞬間移動せず、約0.25秒でスッと沈む/立つ。しゃがみ中の移動が遅い
- [ ] **低天井**: しゃがんだまま低い場所へ入り、キーを離しても頭上が空くまで立たない(従来どおり)
- [ ] 赤字エラーが出ない/Output Log に `[AimTrainer] IMC を登録しました`

## 4. 調整パラメータ早見表(BP_AimTrainerCharacter の Details)

| カテゴリ | プロパティ | 既定値 | 意味・VALORANT対応 |
|---|---|---|---|
| Move | WalkSpeed | 540 | 走行5.4m/s(ライフル所持時) |
| Move | CrouchedSpeed | 270 | しゃがみ移動(走行の半分) |
| Move | MoveAcceleration | 4096 | 最高速まで約0.13秒。大きいほど機敏 |
| Move | MoveBrakingDeceleration | 2800 | キー解放時の制動。大きいほど即停止 |
| Move | MoveGroundFriction | 10 | 切り返しのキレ |
| Move | MoveBrakingFriction | 10 | 停止の立ち上がりの鋭さ |
| Move | AirControlAmount | 0.15 | 空中での軌道修正量 |
| Jump | JumpZVelocity | 456 | 高さ約76cm(GravityScale=1.4時) |
| Jump | GravityScale | 1.4 | 落下の鋭さ |
| Crouch | CrouchedCapsuleHalfHeight | 60 | しゃがみ当たり判定(全高120cm) |
| Crouch | Standing/CrouchedCameraHeight | 64 / 50 | 目線: 地上160→110uu |
| Crouch | CrouchCameraInterpSpeed | 10 | しゃがみ遷移の速さ(約0.25秒) |
| Look | MouseSensitivity | 0.5 | **VALORANT感度と同値でOK** |
| Look | CameraFOV | 103 | VALORANT固定値 |
| Look | bInvertLookY | false | 上下反転(通常はfalse) |

## 5. うまくいかないとき

| 症状 | 対処 |
|---|---|
| 感度が異常に高い/低い | MouseSensitivityにVALORANT感度を再入力。Config/DefaultInput.iniの3行(False×3)が反映されているか確認 |
| 上下が逆 | bInvertLookY を切り替える(fix3で符号系が変わったため以前の設定は一度リセット) |
| しゃがみがまだカクつく | CrouchCameraInterpSpeed を8〜12で調整 |
| 停止が滑りすぎ/止まりすぎ | MoveBrakingDeceleration と MoveBrakingFriction を増減 |
| Configが元に戻る | エディタを閉じてから DefaultInput.ini を上書きする |
