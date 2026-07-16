# valo_AimTrainer fix6 手順書(計測・検証システム)

fix6 = 「fix5が本当にVALORANT実測値を再現できているか」を計測可能にする検証システム。
新規ゲーム機能はなし。既存の操作(視点/WASD/ジャンプ/しゃがみ)は無変更。

## 1. 適用手順

1. zipをプロジェクト直下に展開(ソース4ファイル+Config/DefaultGame.ini)。
   ※DefaultGame.iniは既存内容の末尾にセクション追記のみ。**エディタを閉じてから上書き**。
2. フルビルド(新規クラス追加のためLive Coding不可。エディタ終了→VSビルド)。
3. BP_AimTrainerCharacter を開き、黄色い「↺」が出ているプロパティを既定値へ戻す
   (**BP値 > ini値 の優先順のため、BPに旧値が残っていると設定分離が機能しません**)。

## 2. テストマップの作成(Phase2・初回のみ約2分)

1. File(ファイル) > New Level(新規レベル) > **Basic(ベーシック)** を選択(平坦な床+ライトのみ)。
2. アウトライナで床(Floor)を選択し、Details > Scale を X=20, Y=20 に拡大(移動距離確保)。
3. Place Actors(アクタを配置) から **Player Start** を床の中央へ配置(向きはお好みで固定)。
4. File > Save Current Level As... → `Content/Maps/TestRange` として保存。
5. World Settings > GameMode Override = **BP_AimTrainerGameMode** を確認。
6. (任意)Project Settings > Maps & Modes > Editor Startup Map = TestRange。

再現条件の担保: 平坦床・障害物なし=このマップ / FOV=CameraFOV(103) / 感度=MouseSensitivity(0.5) /
**FPS=セッション開始時に t.MaxFPS を FixedTestFPS(既定120)へ自動固定(終了時に復元)** /
**初期位置=セッション開始時にPlayerStartへ自動テレポート**。

## 3. 計測ワークフロー(Phase1/3/4)

| キー | 動作 |
|---|---|
| **F5** | 計測セッション開始(統計リセット・テレポート・FPS固定) |
| **F6** | セッション終了(差分レポート生成・FPS復元) |
| **F7** | 途中経過レポート(セッション継続) |

1. PIE開始 → コンソール(@キー)で試験名を設定: `at.TestName W_Accel` など(CSVのTestName列)。
2. **F5** → 画面が RECORDING 表示になる。
3. テスト動作を5〜10回繰り返す(下表)。各計測が検出されるたび緑色で値が表示され、
   画面左上に **n / 平均 / 最小 / 最大 / 標準偏差** がリアルタイム更新される。
4. **F6** → レポートが画面・ログ・ファイルに出力される。

### 7テスト項目の推奨手順(1項目=1セッション)

| # | TestName例 | 操作 | 主計測 |
|---|---|---|---|
| 1 | W_Accel | 静止→W長押し(2秒)→離す | Accel 0-95% |
| 2 | AD_Accel | 静止→A(またはD)長押し→離す | Accel 0-95% |
| 3 | AD_Switch | A長押し→離さずD(交互) | Reverse-Stop |
| 4 | Release_Stop | 全力移動→キー解放 | Release-Stop |
| 5 | Reverse_Decel | 全力移動→逆キー | Reverse-Stop |
| 6 | Crouch | 静止→左Ctrl押下(沈み切るまで保持) | Crouch-Sink |
| 7 | Jump | 静止→Space | Jump-Airtime |

## 4. 出力ファイル(Phase3/4)

- **CSV**: `プロジェクト/Saved/AimTrainer/AimTrainerLog_YYYYMMDD.csv`
  列: `Date,TestName,Trial,AccelerationTime_ms,StopTime_ms,ReverseTime_ms,MaxSpeed,JumpTime_ms,CrouchTime_ms`
  (指定8列+しゃがみ用に CrouchTime_ms を末尾追加。移動系は1試行1行に集約、しゃがみ/ジャンプは単独行)
- **レポート**: `Saved/AimTrainer/Report_日時.txt` + 画面(黄色)+Output Log
  形式: `Metric | VALORANT | UE5(avg) | Diff | Judge`
  判定: **±10ms以内=OK / ±10〜30ms=ADJUST / ±30ms超=FIX-PARAM**(しきい値もini変更可)

## 5. 設定分離(Phase5)— fix7以降の調整方法

`Config/DefaultGame.ini` 末尾の2セクションがすべての調整点です(コード変更不要)。

- `[/Script/valo_AimTrainer.AimTrainerCharacter]` … 移動/ジャンプ/しゃがみ/視点の全17項目
- `[/Script/valo_AimTrainer.MovementTestingComponent]` … VALORANT参照値・判定しきい値・固定FPS

手順: **iniの値を変更 → エディタ再起動 → F5計測 → F6レポートで差分確認**。
換算式: 加速0→95%をT秒にしたい→ `MoveAcceleration=513/T`。解放停止→ `MoveBrakingDeceleration=535/T`。
滞空T秒→ `JumpZVelocity=490×T`(GravityScale=1.0時)。

## 6. 注意

- 画面HUDは英語表記です(UEのデバッグフォントに日本語グリフがないため。CSV/レポート/ログはUTF-8)。
- 計測の時間分解能は実行FPSに依存します(120fpsで±8ms)。標準偏差が大きい場合はFixedTestFPSを240へ。
- F5〜F7はテスト専用のレガシーキーバインドで、Enhanced Inputアセットの追加は不要です。
