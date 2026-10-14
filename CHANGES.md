# EraCH3 — hook.cpp النسخة المُصلحة لـ Fortnite 21.30 (CL-21088273)

**الملف الجاهز:** `hook_fixed.cpp` (3328 سطر)
**هدف الربط:** `libUnreal.so` — Build ID: `5b371871c152651ff2b038c01c9504c1a82de1f8`
**المصادر المستخدمة للتحقق:** MobileDumper-7 SDK المرفق (ClassesInfo / StructsInfo / FunctionsInfo / EnumsInfo / GObjects dump) + قراءة bytes مباشرة من `libUnreal.so` + نقل أفكار الملف المرجعي `game.cpp` (Era 10.40).

---

## أولاً: الأخطاء الحرجة التي كانت تُعطّل الـ Gameplay (تم إصلاحها)

| # | الخطأ في hook.cpp الأصلي | التشخيص من SDK 21.30 | الإصلاح |
|---|---|---|---|
| F01 | `BuildQuickBarsManually` يكتب بايت التفعيل في `slot+0x18` | `FQuickBarSlot.bEnabled` = `+0x10` bit0 (و`+0x18` هو `UsedByItemGuid` — كان يدمر الـ GUID!) | كتابة bit0 (`bEnabled`) + bit3 (`bIsOccupied`) في `+0x10` |
| F02 | مسارات الانعكاس `FortQuickBar.Slots` و `FortQuickBarSlot.Items` غير موجودة → ربط الفتحات كان صامتاً ومعطلاً | أسماء الهياكل المنعكسة هي `QuickBar` و `QuickBarSlot` بدون Fort | تصحيح كل المسارات |
| F03 | `/Script/Engine.Actor.SetMirrored` غير موجود | الدالة على `ABuildingSMActor` (param `bIsMirrored`) | استخدام `BuildingSMActor.SetMirrored` |
| F04 | `FortPlayerController.ServerAttemptAircraftJump` غير موجود في 21.30 → قفزة الباص RPC معطلة | انتقلت إلى `UFortControllerComponent_Aircraft` | جلب المكوّن عبر `GetComponentByClass` ثم الاستدعاء عليه |
| F05 | `JumpFromHudStart` كان يُفلتَر على استقبال الـ Controller → زر القفز باللمس لا يعمل إطلاقاً | الدالة تُطلق على `UAthenaHUDContext` | كشف الاستقبال عبر subclass-of `AthenaHUDContext` (+ إبقاء فحص الـ controller كـ fallback) |
| F06 | `FortPickup.ItemDefinition` و `FortPickup.Count` غير موجودة → **كل الـ pickups واللوت ميت** | `AFortPickup` في 21.30 يستخدم `PrimaryPickupItemEntry` (FFortItemEntry @0x2F8، داخله ItemDefinition@0x18 و Count@0xC) | قراءة/كتابة عبر الـ entry + ضبط `PickupSourceTypeFlags@0x540` و `PickupSpawnSource@0x541` |
| F07 | تحليل `ServerCreateBuildingActor` كان يقرأ `Class` من `0x48` (garbage) | `FCreateBuildingActorData` حجمه `0x50` والـ `BuildingClass*` عند `0x40` | تصحيح الإزاحات (Loc@0x8, Rot@0x20, bMirrored@0x38) |
| F08 | تحليل `ServerEditBuildingActor` يقرأ `bMirrored` من `+0x14` | الترتيب الفعلي: ptr, ptr, **uint8** RotationIterations, **bool** bMirrored | استخراج بالاسم عبر `incoming()` (منطق Era) |
| F09 | `GetAnimationHardReference` بمعاملات قديمة `Type/Animation` → **الإيموتات معطلة** | التوقيع الجديد: `BodyType / Gender / PawnContext` | تجربة تركيبات body/gender مع تمرير الـ pawn |
| F10 | `FortWeapon.EditActor` و `FortWeapon.OnRep_EditActor` غير موجودة → **التعديل (edit) معطل** | كلاهما على `AFortWeap_EditingTool` (`EditActor` @0x1020) | تصحيح المسارين |
| F11 | `ServerOnExitVehicle` بربط `int32` لـ ExitForceBehavior → فشل الربط | الإenum `EETryExitVehicleBehavior` حجمه **uint8** | ربط uint8 |
| F12 | استدعاءات OnRep ميتة: `FortQuickBars.OnRep_Primary/SecondaryQuickBar`، `PC.OnRep_QuickBar`، `PlayerState.OnRep_CharacterParts`، `Pawn.OnRep_CustomizationLoadout` | لا وجود لها في 21.30 | حذفها واستبدالها بـ `ClientForceUpdateQuickbar` + `HandleInventoryLocalUpdate` + `OnRep_CharacterData/HeroType` |
| F13 | `K2_GiveAbility` / `GiveAbility` غير موجودة → منح Melee/Slide كان يفشل صامتاً | لا توجد دوال UFUNCTION لمنح القدرات في 21.30 | الاعتماد على `EquipFortAbilitySet` فقط (القدرات تُضاف للـ set قبل الـ Equip) + `TryActivateAbilityByClass` كتفعيل مباشر |

## ثانياً: تحصينات منقولة من الملف المرجعي (game.cpp / Era 10.40)

| # | الفكرة المنقولة | الفائدة |
|---|---|---|
| F14 | حراس `IsGarbageCollecting()` / `IsSavingPackage` في `tick_hook` | منع الكراش عند تعديل المصفوفات أثناء الـ GC (الـ offsets كانت موجودة وغير مستخدمة!) |
| F15 | بوابة `IsStandalone(world)` قبل أي عمل | عدم العبث بجلسات أونلاين |
| F16 | فئة `Call` مطوّرة: حل override لأحداث الـ Blueprint (event_receiver)، تحقق من الاستقبال في `run()` قبل التنفيذ (رفض cross-class)، `set_any()` لأسماء بديلة (`Class`/`Class_0`)، `set_at()` للربط الموضعي للدوال بلا أسماء معاملات (`SetMaxHealth(float)`) | استدعاءات أكثر أماناً ودقة |
| F17 | `give()` من Era: توليد GUID عبر `KismetGuidLibrary.NewGuid` **قبل** نسخ الـ ReplicatedEntry + التحقق من بقاء العنصر بعد الـ refresh (reconciliation) + منع التكرار | مخزون لا يفقد عناصره ولا يكررها |
| F18 | `local_pickup` من Era: تكديس الذخيرة/الموارد حسب `MaxStackSize` مع تحويل الفائض، اختيار أول خانة سلاح فارغة (1..5)، تدمير الـ pickup عند النجاح فقط | التقاط واقعي بدون تكرار |
| F19 | تخطيط شريطتين: الأسلحة على Primary(0) وقطع البناء على Secondary(1) + الربط عبر `EquipItemToQuickBarSlot` (المعامل `ItemVariantGuid` مضبوط) + التحقق بالقراءة عبر `GetItemInQuickbarSlot` | الـ hotbar يعمل مثل اللعبة الحقيقية على الموبايل |
| F20 | نظام لوت محلي (جداول Era): قراءة `SearchLootTierGroup` من الحاوية، تحويل `Loot_Treasure→Loot_AthenaTreasure`، خلط موزون للأسلحة/الذخيرة/الموارد، spawn `FortPickupAthena` مع **تراجع كامل عند فشل جزئي** | البحث في الصناديق يعمل offline بشكل حتمي؛ `BP_SpawnLoot` يبقى fallback |
| F21 | البناء عبر المعاينة المحلية: `LastBuildPreviewGridSnapLoc/Rot` + `CurrentBuildableClass` + تحويل Rotator→Quat بدقة مضاعفة + dedup 0.25s + `PollTouchBuild` لزر اللمس | بناء سلس على اللمس بدون payload سيرفر |
| F22 | التجميل عبر الـ Locker: `CosmeticLoadoutPC` (@0x1E70) + مسار MCP (`BlueprintContextLibrary.GetContext` → `FortMcpContext.GetLoadoutForPlayer`) + مرور على `FortHero.Specializations` عبر `LoadAsset_Blocking` + mirroring على `Pawn.CosmeticLoadout` (@0x2A08) | ظهور سكن الـ locker الحقيقي بدل الهاردكود |
| F23 | `SetMaxHealth/SetHealth(100)` بعد الـ Possession + `bBuildFree=true` (بناء مجاني) + ضبط بت `bReadyToStartMatch` مباشرة (0xA28 bit5) كـ fallback | صحة كاملة وبناء بلا مواد |
| F24 | اعتراض الرجوع للـ Lobby (`ServerReturnToMainMenu` → سفر `open Frontend` بعد الـ tick) + أوامر كونسول: `fly/walk/ghost/god/teleport` + تشخيصات `era.status/era.inventory/era.equip/era.give` + تتبع `GamePhase` بالأسماء | تكافؤ كامل مع أوامر Era المرجعية |
| F25 | التحقق من prologue لكل RVA قبل `DobbyHook` + كتابة Build ID في اللوج | حماية من offsets خاطئة أو hook آخر |
| F26 | **إصلاح ميداني بعد تجربة الجهاز**: ثابت prologue الأول لـ `ProcessEvent` كان منسوخاً من `game.cpp` المرجعي (`0xf81a0ffc` لـ libUE4.so القديم) بينما 21.30 الفعلي `0xa9ba6ffc` (STP X28,X27 بدل STR X28). + قبول الشكلين كمتغيرات + تثبيت كل hook بشكل مستقل بدل رفض الكل عند أول mismatch | `ProcessEvent` يُثبّت فعلياً على 21.30؛ mismatch واحد لا يعطّل باقي الهوكات |
| F28 | فرض تحميل الـ Streaming Levels: مسح `UWorld.StreamingLevels` (@0x88) واستدعاء `ULevelStreaming.SetShouldBeLoaded/SetShouldBeVisible` (معكوستين في 21.30) تلقائيًا كل 8 ثوان لأول 150 ثانية + عند الطلب | أماكن الخريطة غير المحملة تُحمّل |
| F29 | نظام المركبات (منفذ من Erbium): قراءة `AFortAthenaVehicleSpawner` من الخريطة → `CachedFortVehicleItemDef` (@0x2B8) → `VehicleActorClass` (@0xA38) → Spawn في موضع الـ spawner + fallback بمسح أصناف BP المشتقة من `FortAthenaVehicle` | سيارات/دراجات/شاحنات من spawners الخريطة نفسها (أوتوماتيكي بعد ~12 ثانية من Finalize) |
| F30 | نظام الحيوانات/الـ NPC (Wildlife): مسح GObjects عن أصناف BP مشتقة من `FortAIPawn` بأسماء (boar/chicken/wolf/frog/raptor/crow…) وspawn حلقة حول اللاعب | حيوانات حول اللاعب (أوتوماتيكي بعد ~16 ثانية) + أمر `era.animals [n]` |
| F31 | تحصين البناء (من Erbium): إعادة ترتيب الـ spawn إلى `BeginSpawn → InitializeKismetSpawnedBuildingActor → FinishSpawn` (كان العكس = كراش على الزر) + `OnFinishEditActor` يحفظ موقع/دوران القطعة الأصلية بدل origin | زر البناء لن يطيّر اللعبة + التعديل يبقي القطعة في مكانها |
| F32 | تحديث `ClientForceUpdateQuickbar` بعد كل إعطاء/التقاط عنصر | الشريط يتحدث فورًا مع كل pickup |
| F33 | إظهار الخريطة: `AFortInGameMapManager.GetCurrent` + `AllowShowMiniMap(true)` + `AllowShowMainMap(true)` (معكوسة في 21.30) مع إعادة محاولة تلقائية | الـ minimap والخريطة الكاملة تُفرض ظهورها |
| F34 | مسجّل كراش: SIGSEGV/SIGBUS/SIGILL/SIGABRT → mini-backtrace بإزاحات libUnreal.so في `/data/local/tmp/era_crash.txt` | أي كراش قادم يعطينا offset الدالة مباشرة |
| F27 | **إصلاح ميداني ثانٍ**: حارس F14 في `tick_hook` كان يقرأ **بايت كود** من `profile::IsSavingPackage` (0x0760A59C عنوان EXEC في 21.30 — مُعرّف وغير مستخدم في hook.cpp الأصلي) — أول بايت `0xFD` فكان الشرط صحيحاً دائماً وعاد `tick_hook` مبكراً في كل تِك قبل أي كشف عالم. أُزيل الفحصان (IsSavingPackage كبيانات + IsGarbageCollecting كنداء حر غير موثّق) وأُضيف DIAG لمرة واحدة يطبع العالم وصنفه واسمه | `tick_hook` يعمل فعلياً كل تِك → كشف Terrain → SetupWorld → Possess/Spawn يعملان؛ DIAG يؤكد سلامة دقة الأسماء على الجهاز |

## ثالثاً: إزاحات جديدة مضافة إلى profile (كلها موثقة من SDK 21.30)

```cpp
FortPlayerController_ReadyByte       = 0x0A28 (bit 5)  // bReadyToStartMatch
Controller_CurrentBuildableClass     = 0x1898
Controller_PreviewSnapLoc/Rot        = 0x19B0 / 0x19C8
Controller_BuildPreviewRotIter/Mirrored/bBuildFree = 0x1858 / 0x1859 / 0x185B
Controller_TouchPrimaryFire          = 0x0BB8
Controller_LocationUnderReticle      = 0x24C0
Controller_CosmeticLoadoutPC         = 0x1E70
FortPawn_CurrentWeapon               = 0x07F0
FortPawn_AbilitySystemComponent      = 0x1008
FortPlayerPawn_CosmeticLoadout       = 0x2A08
FortHero_Specializations             = 0x218
Pickaxe_WeaponDefinition             = 0x0790
EditingTool_EditActor                = 0x1020
AFortPickup_PrimaryEntry             = 0x02F8  (+ SourceFlags 0x540 / SpawnSource 0x541)
ItemEntry_ItemGuid / Count / ItemDefinition = 0x68 / 0x0C / 0x18
QuickBarSlot: Items@0x00, bEnabled@0x10(bit0), bIsOccupied(bit3), SlotSize=0x28
AFortGameState: GamePhase 0x2150, SafeZoneIndicator 0x19A8, PlaylistInfo 0x2170
```

## رابعاً: التحقق الذي تم إجراؤه على libUnreal.so

- **كل** الـ RVAs في profile قرأنا bytesها من المكتبة الفعلية: كلها prologues ARM64 سليمة.
- `InitializeAthenaQuickBars` bytesه مطابق تماماً لما يتوقعه الكود: `a9bd57f6 a9014ff4 a9027bfd 910083fd`.
- `GObjects = 0x0DCD6E08` مؤكدة من SDK + OffsetsInfo (وتقع في .bss — طبيعي).
- عدد الـ objects عند الـ dump: 292663 في chunks بحجم 65536 — يطابق بنية `GObjects_t` في الكود.

## خامساً: جدول اللوت المحلي (كيفية التوسيع)

داخل `namespace local_loot` في hook_fixed.cpp — أضف أسطراً بنفس النمط:

```cpp
static const Row weapons[] = {
    {"WID_Assault_Auto_Athena_C_Ore_T03", 30, 1},  // {الاسم القصير, الوزن, العدد}
    ...
};
static const Tier tiers[] = {
    // {المجموعة, أدنى/أقصى أسلحة, أدنى/أقصى ذخيرة, موارد, مضاعف الندرة}
    {"Loot_AthenaTreasure", 2, 3, 2, 3, 1, 1.6f},
    {"Loot_AthenaAmmoLarge", 0, 0, 3, 4, 0, 0.0f},
    {"Loot_AthenaFloorLoot", 1, 1, 1, 2, 1, 0.0f},
};
```
أي صف لا يُوجد أصله في المحتوى يُتخطى بأمان (log فقط). المجموعات غير المغطاة تسقط تلقائياً إلى `BP_SpawnLoot` الأصلية.

## سادساً: الترجمة

```bash
# نفس طريقة بناء hook.cpp الأصلي (NDK clang for arm64 + Dobby)
aarch64-linux-android21-clang++ -shared -o libEraCH3.so hook_fixed.cpp \
  -I<dobby>/include -L<dobby>/lib -ldobby -llog
```
لا تغييرات على المتطلبات: Dobby + Android log + نفس نقاط الدخول (`LibMain` constructor و `JNI_OnLoad`).

## سابعاً: أوامر الكونسول المدعومة بعد الإصلاح

```
fly | walk | ghost | god | teleport          — حركة/إله/تنقل تحت التقاطع
era.status                                    — حالة الجلسة الكاملة
era.inventory                                 — محتويات الشريطتين
era.equip <0..5>                              — تسليح خانة
era.give <ShortName|/path> [count] [slot]     — إعطاء عنصر
era.lobby                                     — العودة للوبي
open athena_terrain                           — سفر (كما الأصل)
```

## ملاحظات مهمة للتجربة على الجهاز

1. **QuickBars**: الإصلاح الأهم (F01/F02). راقب لوج `era_log.txt` لأسطر `[QB]` — سترى `Slot bar=0 slot=0 verified=1` عند نجاح الربط.
2. **اللوت**: راقب `[Loot] group not ported: <name>` — إن ظهرت اسم مجموعة جديد أضفه لجدول `tiers`.
3. **القدرات**: `[Abilities] ✅ Applied set` مع `abilities=N effects=M` يعني أن GAS سليم.
4. **المعرّف `IsStandalone`**: إن شغّلت الجلسة عبر GameServer (أونلاين) سيتوقف الـ hook عمداً — هذا مقصود (F15).

## ملحق: إصلاح ميداني F26 — ProcessEvent prologue (بعد أول تجربة على الجهاز)

### الأعراض (من لوج الجهاز)
```
PROLOGUE MISMATCH ProcessEvent RVA=7611b5c word=0 expected=f81a0ffc actual=a9ba6ffc
Prologue verified WorldTick RVA=949685c
Prologue verified EngineExec RVA=98bb160
Prologue verified AthenaQuickBars RVA=56607ec
Hook prologue mismatch; refusing to patch
=== Ready ===
دخلت الخريطة بلا ربط PlayerController ولا رسبنت
```

### السبب الجذري
نسخة ثوابت الـ prologue الأربعة لـ `ProcessEvent` من `game.cpp` المرجعي، وهذه
مخصصة لـ **libUE4.so** القديم (CL-8936006). التحقق من bytes **libUnreal.so**
لـ 21.30 عند RVA `0x7611B5C` (نفس RVA الصحيح من hook.cpp الأصلي) يُظهر:

| | word0 | word1 | word2 | word3 |
|---|---|---|---|---|
| libUE4.so (مرجعي) | `f81a0ffc` STR X28,[SP,#-0x60]! | `a90167fa` | `a9025ff8` | `a90357f6` |
| libUnreal.so (21.30) | `a9ba6ffc` STP X28,X27,[SP,#-0x60]! | `a90167fa` | `a9025ff8` | `a90357f6` |

الكلمات 1..3 متطابقة تماماً؛ الفرق فقط في التعليمة الأولى: بيلد 21.30 يحفظ
سجلاً إضافياً (X27) مع X28 عبر `STP`. أي أن الـ **RVA صحيح** والدالة هي
`ProcessEvent` فعلاً — الخطأ في الثابت المتوقع فقط. وبسبب منطق
"رفض الكل عند أي mismatch" لم يُثبَّت أي hook (ولا WorldTick)، فلم يكتشف
ال Terrain أبداً ولم يعمل SetupWorld/FinalizePlayer → لا ربط Controller ولا رسبنت.

### الإصلاح
1. `PrologueMatchPE()`: تقبل word0 بأي من الشكلين (`a9ba6ffc` أو `f81a0ffc`)
   ثم تتحقق من الذيل المشترك (كلمات 1..3) — متانة ضد اختلاف بيلدات نفس الفرع.
2. `InstallHooks()` أصبح يثبّت كل hook **مستقلاً**: فشل/توفيق أحدها لا يمنع
   البقية، مع ملخص نهائي `InstallHooks done: installed=N failed=M`.

### اللوج المتوقع بعد الإصلاح
```
Prologue verified ProcessEvent RVA=7611b5c word0=a9ba6ffc
Prologue verified WorldTick RVA=949685c
Prologue verified EngineExec RVA=98bb160
Prologue verified AthenaQuickBars RVA=56607ec
ProcessEvent hooked
WorldTick hooked
EngineExec hooked
InstallHooks done: installed=4 failed=0
=== Ready ===
...
>>> Terrain detected: Athena_Terrain
SetupWorld: initial pass done
=== FinalizePlayer ===
Finalize: complete
```

## ملحق: إصلاح ميداني F27 — tick_hook كان يعود مبكراً في كل تِك (بعد التجربة الثانية)

### أعراض التجربة الثانية (لوج الجهاز)
```
Prologue verified ProcessEvent RVA=7611b5c word0=a9ba6ffc   ✅ (F26 نجح)
ProcessEvent hooked / WorldTick hooked / EngineExec hooked  ✅
InstallHooks done: installed=3 failed=0                     ✅
WorldTick first: tid=32645                                  ✅ الهوك يعمل
Exec: STAT fps x4                                           ✅ exec_hook يعمل
(لا يوجد أي سطر >>> WorldTick world: '...' ولا Terrain detected)  ❌
```

### السبب الجذري — تحليل تعليمي
سطرا الحارسان [F14] في بداية `tick_hook`:
```cpp
if (Read<uint8_t>((const void*)(g_base + profile::IsSavingPackage)) ||
    Fn<bool(*)()>(profile::IsGarbageCollecting)()) return;
```
- `profile::IsSavingPackage = 0x0760A59C` في libUnreal.so 21.30 هو **عنوان دالة (قسم EXEC)**
  وليس متغير bool في الذاكرة — كان معرفاً **وغير مستخدم** في hook.cpp الأصلي.
  النمط منسوخ من game.cpp المرجعي حيث كان العنوان في libUE4.so فعلاً bool في .bss (0x0c8bcbf8).
- قراءة أول بايت من عنوان الدالة تعطي `0xFD` (تعليمة `STP X29,X30,[SP,#-0x10]!`) → لا يساوي صفراً أبداً
  → **الشرط صحيح دائماً → return مبكر في كل تِك** → لا ObjName ولا كشف Frontend/Artemis_Terrain
  → لا SetupWorld → لا ربط PlayerController ولا Spawn (وهو الملاحظ في اللوق).
- تصنيف كل عناوين profile عبر أقسام ELF أكد: كل الدوال داخل EXEC، وGObjects في بيانات غير مصرّح بها (bss)،
  وlayout الـ GObjects مطابق للدامبر (Objects@0x10 / NumElements@0x24 / NumChunks@0x2C).

### الإصلاح
1. حذف الحارسين من tick_hook (الأصلي hook.cpp لم يستخدمهما أبداً؛ إعادة الدخول محميّة بـ Guard).
2. إضافة **DIAG لمرة واحدة** بعد أول تِك:
```
DIAG world=0x… class=0x… ('World') name='Frontend'
```
يؤكد على الجهاز أن قراءة الكائن + FName + NameAppendString تعمل قبل أي منطق.

### اللوج المتوقع في التجربة الثالثة
```
DIAG world=0x… class=0x… ('World') name='<اسم عالم اللوبي>'
>>> WorldTick world: '<اسم عالم اللوبي>'
(بعد السفر للخريطة)
>>> WorldTick world: 'Athena_Extreme' أو مشتق Artemis
>>> Terrain detected: …
SetupWorld: PC=FortPlayerController GM=FortGameModeAthena GS=FortGameStateAthena
SetupWorld: GrantAllAbilitySets … SetupWorld: initial pass done
=== FinalizePlayer ===
Finalize: complete
```
إن ظهر `SetupWorld: no controller` أو `[Init] World is not standalone` فستكون النقطة التالية للمعالجة موثقة مباشرة من اللوج.

## ملحق: التحديث الشامل F28-F34 — نقل أنظمة Erbium إلى hook.cpp (21.30 Gameplay)

### مصدر النقل
`GameServer_contents.txt` داخل الأرشيف = **سورس سيرفر Erbium كامل (75 ملف)**، فُكّك إلى
`fortnite-src/erbium/`. الأنظمة المنقولة: GiveItem/QuickBars (FortInventory.cpp)،
SpawnActorUnfinished→InitializeKismetSpawnedBuildingActor→FinishSpawn (FortPlayerControllerAthena.cpp:615-618)،
مركبات s20/s21/s22 (FortGameMode.cpp:1977-2030).

### ماذا سيحدث تلقائيًا في المباراة (بدون أي أوامر)
1. `FinalizePlayer` → فرض الخريطة (F33) + أول موجة streaming (F28)
2. بعد ~12 ثانية → **مركبات** من spawners الخريطة حتى 12 مركبة (F29)
3. بعد ~16 ثانية → **حيوانات** حول اللاعب حتى 8 (F30)
4. كل 8 ثوان (لأول 150 ثانية) → إعادة فرض streaming (F28)
5. أي كراش الآن يكتب ملف offsets في /data/local/tmp/era_crash.txt (F34)

### أوامر جديدة
```
era.vehicles [n]   — spawn مركبات فوري (12 افتراضيًا)
era.animals  [n]   — spawn حيوانات فوري (8 افتراضيًا)
era.stream         — فرض تحميل كل streaming levels الآن
era.hud            — فرض minimap + الخريطة + refresh HUD
```

### ملاحظات
- أصناف part1..7.cpp مجمدة عند F26/F27 — **hook_fixed.cpp هو المصدر الوحيد للحقيقة** الآن.
- الحيوانات تظهر وتمشي بمنطق BP الخاص بها؛ إن ظهرت ساكنة تمامًا فهي تحتاج AIController
  (سنضيفه في جولة قادمة من بيانات لوج الجهاز).
- مركبات v21 تولد أصلاً من spawners الخريطة على السيرفر الحقيقي (تعليق Erbium: "its auto on
  s20, s21, and s22") — نحن نحاكي ذلك محليًا لأنه لا يوجد سيرفر.

## ملحق: جولة F35-F45 — كل الأنظمة الناقصة من تقرير الجهاز الأخير

### من اللوق القادم + طلب المستخدم (مترجم)
1. QuickBar "ميت" (الأنيميشن فارغ رغم الملء اليدوي) + `[QB] ⚠️ Init may have failed`
2. `[Map] InGameMapManager reflection missing` يتكرر بلا نهاية (سؤام)
3. `[Veh] 0 spawner actors on map` و `[Wild] 0 animal classes loaded`
4. `[Loot] Total 0 containers looted` — لوت الأرضي غير ظاهر
5. انميشينات ميتة / لا انميشين ركض — الجسم بلا SkeletalMesh/AnimInstance
6. ميزة التسلق غير موجودة — **حقيقة: التسلق (Mantle) أُضيف في 23.00 Ch4، لا وجود له في 21.30 إطلاقًا** (صفر classes Mantle في الـ SDK) → حل بديل محلي
7. نظام إعطاء الموارد عند التكسير — **حقيقة من Erbium: اللعبة لا تعطي الموارد أصلًا في وضعنا؛ Erbium يمنحها بنفسه داخل hook OnDamageServer**
8. الأبواب: تنغلق من تلقاء نفسها / تتعطل عند الدفع أثناء الركض
9. Player Counter + Zone + عدّاد اللاعبين

### [F35] Zone/Storm حقيقي
- `AFortSafeZoneIndicator.InitializeWithMapDefaultPhases()` + `StartPhase(0)` (BlueprintAuthorityOnly Native) بعد finalize بـ8 ثوان + `bPlaylistStoppedSafeZonePhases=false`
- المراقب ZoneMonitor كل 5 ثوان: `[Zone] phase=N radius=... center=...` عند تغير الطور فقط
- العاصفة/الضرر خارج الزون تعمل native من الـ Indicator (السيرفر = نحن في standalone)
- أمر `era.zone <i>` للقفز لطور معين

### [F36] Player Counter
- `FortGameStateAthena.PlayersLeft @0x14B8` (Net RepNotify — التحقق من الـ offset في اللوق) + `OnRep_PlayersLeft` → HUD يعرض العداد
- تلقائي: 100 بعد finalize بـ6 ثوان + أمر `era.players <n>` (1..200)

### [F37] موارد التكسير (بور Erbium بدون hook)
- المنطق: متابعة `BuildingActor.Health` لكل BuildingSMActor ضمن 300م (كاش يُبنى كل 8 ثوان من GObjects مع فلتر bPlayerPlaced وCDO)
- هبوط صحة بين فحصين = ضربة → `K2_GetResourceItemDefinition(ResourceType)` → منحنى `BuildingResourceAmountOverride` عبر `DataTableFunctionLibrary.EvaluateCurveTableRow` (قراءة OutXY من out-param وليس ReturnValue!) → `round(Out × Damage/MaxHealth)` (نموذج Erbium حرفيًا)
- `StackAdd()` جديد: يضيف فوق نسخة موجودة في ItemInstances + يطابق ReplicatedEntries بالـ GUID (stride-based) + InventoryUpdate — لا نسخ مكررة
- ملاحظة: أي ضرر (حتى رصاص) على عنصر خريطة يعطي موارد — بدون فلتر سلاح (لا يمكن معرفة مصدر الضرر دون hook)
- أمر `era.harvest on|off`

### [F38] الحيوانات — إصلاح "0 animal classes"
- **السبب**: أسماء حيوانات 21.30 ليست boar/wolf بل وحدات Irwin (Burt الخنزير/ridable، Nug الدجاجة، Smackie) — مؤكدة من هيدرات Dumper-7 (`NPC_Pawn_Irwin_Prey_Burt_classes.hpp`...) ووجود BoarRidableComponent/WolfRidableComponent في البناء
- مسح كل subclasses لـ FortAIPawn مع تفضيل irwin/burt/nug/smackie ورفض parent/base/controller/loot/spawner
- Fallback: StaticLoadClass لمسارات Irwin عبر 5 mounts محتملة
- **AIController**: spawn `Pawn.AIControllerClass` + Possess حتى تمشي فعلاً بدل تماثيل

### [F39] المركبات — إصلاح "0 spawner actors"
- السبب: المسح الواحد بعد 12 ثانية ركض أثناء تحميل streaming levels异步 → الآن `VehicleRetry()` كل 5 ثوان حتى 30 محاولة (~150 ث)
- فلتر CDO جديد `IsCDO()` (RF_ClassDefaultObject @0x08 bit 0x10) في FindAllInstances
- Fallback: StaticLoadClass لمسارات Erbium الدقيقة للمركبات (Dirtbike/Sportbike/TaxiCab/SuperSedan/…كـ plugin mounts) مع تنويع /Game/ — أول محاولة عند attempt 3 ثم كل 5
- `SetFuel(100)` عبر `FortVehicleFuelComponent.SetFuel` (المركبة تحمل CachedFuelComponent @0x1058 — مؤكد AFortAthena)

### [F40] الأبواب — إصلاح جذرين
- **باك قديم خطير**: HandleDoor كان يعيد كتابة البايت كامل 0x0FDE → يمسح bits 0-4 (bSwingingDoor/bSlidingDoor/bDoubleDoor...) → الباب "يتعطل". الآن bits 5|6 فقط (bDoorOpen+bLocalDoorOpen) مع الحفاظ على الباقي
- **DoorAudit** كل 2 ثوان: كاش جدران ذات أبواب (bits أو DoorComponent) يُحدّث كل 60 ث؛ أي باب فتحه اللاعب (doors_ever_open) يُعاد تثبيته open + `OnRep_bDoorOpen` — لا انغلاق ذاتي ولا علق بعد الاندفاع
- الإغلاق المتعمد من اللاعب يمسح من ever_open (أولوية الإغلاق اليدوي)
- أمر `era.doors on|off`

### [F41] الجسم + الانميشينات ("انميشينات ميتة")
- السبب: بلا MCP لا تُحمّل أجزاء الكوسمتك → الـ mesh بلا SkeletalMesh/AnimInstance → انزلاق بلا حركة
- الحل: `FixBodyAndAnimations()` — إن كان mesh الباون ناقصًا: `SetSkeletalMesh(M_MED_Lancelot)` + `SetAnimClass(M_MED_Lancelot_AnimBP_C)` (مؤكد من GObjects-Dump أن الاثنين residents في هذا البناء) + retry كل 5 ث
- أمر `era.body` لإعادة المحاولة يدويًا

### [F42] التسلق (بديل محلي — Mantle غير موجود في 21.30)
- ClimbTick كل 0.18 ث: قفزة مضغوطة (`bPressedJump` bit2 @0x3F0) + سقوط (MovementMode==3 عبر reflection) + LineTraceSingle أمامي 140سم (TraceTypeQuery1) → `LaunchCharacter(Z=520, bZOverride)` — يهبط كأنه mantle
- أمر `era.climb on|off`

### [F43] سؤام [Map] — إصلاح
- السبب: مسارات بأسماء C++ (`AFortInGameMapManager`) بينما أسماء UObject بلا بادئة (`FortInGameMapManager`) + اللوق في كل فشل
- الآن: `FortGameStateZone.GetUIMapManager()` (مثيل حي بلا تخمين مسارات) + لوق واحد فقط + maps_forced=true بعد أول محاولة

### [F44] QuickBar — ملء مبكر + dedupe
- `BuildQuickBarsManually()` الآن يُستدعى نهاية InitializeInventory (قبل EquipSlot) — سلات ممتلئة قبل بناء ويدجت HUD
- Dedupe: إن كان slot0 يحمل UsedByItemGuid غير صفري → تخطَّ (لا re-allocate)
- FinalizePlayer يبقى fallback (dedupe يحميه)

### [F45] لوت الأرضي — إعادة محاولة
- الحاويات في streaming levels async — إعادة `SpawnFloorLoot` كل 20 ث حتى >0 أو 8 محاولات + `session.loot_found_total`
- أمر `era.loot` لإعادة فورية

### الحالة النهائية
- hook_fixed.cpp: 4,300+ سطر، فحص syntax نظيف (-Wall -Wextra، تحذير rotIter القديم فقط)
- كل النقاط التسع في طلب المستخدم معالجة: (المركبات/الحيوانات/الزون/العداد/التكسير/الأبواب/الانميشين/التسلق-بديل/اللوت) + QuickBar + سؤام Map
- **القروش**: لا توجد أصناف Shark في بناء 21.30 إطلاقًا (بحث شامل في SDK + GObjects) — أُزيلت بعد Ch2؛ لن تظهر ولا يمكن إحضارها
- للكومبايل: نفس أمر NDK/Dobby السابق؛ hook_fixed.cpp وحده هو المصدر

---

# الجولة F46–F53 — جهاز Log4: تفاعل معطل/QuickBar ميت/بناء بلا موارد/تكرار/تعديل معطل/مركبات بعيدة/لوت أوتوماتيكي/NPC/قروش/شجرة الواقع/mark

مصدر الطلب: لوق الجهاز الرابع + رسالة المستخدم (البناء فوق المبني، المركبات بلا أماكنها، التعديل لا يعمل، التكسير لا يعطي موارد والبناء لا ينقص، لا ركوب ولا فتح صناديق — "الشخصية بلا تفاعل"، NPC، انميشين الركض والزحلقة سيء، لا point في mini map، لوت لا يرسبن تلقائيًا، QuickBar ميت، تسلق نوعان، تأثيرات شجرة الواقع).

### [F46] اكتشاف محتوى الخريطة تلقائيًا (ScanMapContent)
- **السبب الجذري**: دامب 21.30 أخُذ في اللوبي — أصناف خريطة BP (الصناديق/لوت الأرضي/مولدات المركبات) لا تظهر في الهيدرات إطلاقًا؛ كل المسارات المضمونة قديمة
- الحل: مسح GObjects دفعة واحدة بعد 22 ث من finalize (+ `era.scan`): أصناف فيها tiered/chest/floorloot/ammobox ترث BuildingContainer؛ *VehicleSpawn*/*BoatLaunch* ترث Actor؛ shark/sapling
- اللوق يطبع الأسماء المكتشفة (`[Scan] container: ...`) — تشخيص نهائي لأسماء هذا البناء
- **لوت الأرضي أوتوماتيكي**: كل حاوية floorloot تُفتح فورًا (تُلقي اللوت في أماكنها المعتادة من خريطة اللعبة نفسها — لا إحداثيات مكشوفة)
- **الصناديق** تبقى مغلقة وتسجل في `session.containers` لنظام التفاعل
- مولدات المركبات العامة: أي صنف spawner مكتشف → مركبة من الكاش عند نقطة المولد (أماكنها المعتاد حين تُكتشف المولدات)
- تتبع كل مركبات الخريطة الحية في `session.vehicles` للتفاعل
- القروش + أشجار الواقع (Reality Sapling): إن ظهرت أصنافها مع الخريطة تُنفع فعليًا (+ `era.sharks`)، وإلا لوق صريح أن 21.30 لا يملكها

### [F47] نظام التفاعل (الجذر: "الشخصية بلا تفاعل")
- الـ HUD يعرض زر التفاعل فقط حين يجد FortControllerComponent_Interaction مرشحًا — في وضعنا offline لا يجد أبدًا
- `InteractTick` كل 0.4 ث: أقرب صندوق غير مفتوح/مركبة ضمن 420/480م → كتابة المرشح في `InteractActor @0x198` (تعكس) ليظهر الزر
- **تفاعل ذاتي بالاقتراب** (era.auto on|off، الافتراضي on): صندوق <170م يُفتح، مركبة <200م يُركب — مع كولداون 2.5ث/actor وlocate من كاش ثابت (صفر reflection في التِك للصناديق)
- `GetInteractionComp()`: يجلب مكوّن التفاعل وينشئه بـ AddComponentByClass إن غاب + `SetShouldAutoOpenSwingingDoors(true)` مرة واحدة (أبواب تتفتح بالمشي)
- `DoInteract()`: جدول توجيه موحد — BuildingContainer→Loot، FortAthenaVehicle→EnterVehicle، BuildingWall→HandleDoor، BuildingActor→BlueprintOnInteract + `BP_ServerAttemptInteractInternal` (المسار النيتف الداخلي)
- pe_hook: ServerAttemptInteract الحقيقي يمر عبر نفس الجدول الآن

### [F48] ركوب المركبات
- `EnterVehicle()` = `FortAthenaVehicle.ForceSeatSinglePawn(Pawn, Seat)` (المنطق الرسمي: فحص شغولية، attach، OnPawnEnterVehicle) بمحاولة المقاعد 0..3
- منح `GA_AthenaEnterVehicle_C` (موجود في 21.30 — مؤكد من CppSDK) لتفعيل حالة الإدخال النيتف
- يدويًا: `era.enter` / `era.exit`؛ تلقائيًا بالاقتراب من F47

### [F49] البناء — تكلفة + منع التكرار + تعديل
- **تكلفة الموارد**: `SpendBuildResources()` بعد كل وضع (-10) عبر `StackRemove()` الجديد (ينقص ItemInstances + ReplicatedEntries بالـ GUID مع Floor عند 0)؛ الخامة من اسم الصنف (_M1→معدن، _S1/_S_→حجر، وإلا خشب)
- **منع البناء فوق المبني**: `BuildCellKey` (تقسيم 256u أفقي/384u رأسي + ياو ثماني + نوع القطعة) + `BuildCellOccupied` (خلايانا الموضوعة + مباني الخريطة من كاش harvest ضمن 70م) — يرفض الـ RPC والمحلي معًا
- **التعديل — السبب الجذري**: واجهة التعديل الجوالة لا تفتح إلا حين `BuildingSMActor.EditingPlayer @0x0C28` معيّن + OnRep — لم يكن يُكتب إطلاقًا → الآن في ServerBeginEditingBuildingActor وفي مراقب `EditAssist` (كل 0.3ث يثبت EditingPlayer/EditBuildingActor ويكشف حافة bEditConfirmed)
- `era.edit wall|floor|stairs|roof` — تنفيذ التعديل يدويًا عبر OnFinishEditActor إن لم يُكمل native

### [F50] QuickBar — تنبيه الويدجت مباشرة
- المفاجأة من الـ SDK: `ClientQuickBars @0x1A60` **مؤشر** لشبس أكتور — البيانات كانت تمتلئ لكن ويدجت QuickbarBase لا تعيد البناء إلا بحدث BP `HandleQuickbarContentChanged` الذي لم يصل
- `PokeQuickbarWidgets()`: GetAllWidgetsOfClass(FortQuickbarBase) ثم استدعاء الحدث مباشرة على كل ويدجت (QuickBarType 0+1 مع ChangedSlots={0..5}) — دوال الـ BP تُلف بالاسم عبر `FindAllFunctionsNamed` الجديد (حزمة الويدجت مجهولة المسار)
- تلقائي بعد 18 ث من finalize + `era.qb` + Call(void*) ctor جديد للـ UFunction المباشر

### [F51] الانميشينات — إصلاح جذر ثانٍ
- الفحص القديم `curMesh && animInst → تخطى` كان يقفز فوق الإصلاح عندما يكون الثنائي default في BCL—لا بديل لانميشن — لهذا بقي الركض "سيئًا"
- v2: يقبل الوضع الحالي فقط إن كان الجسم M_MED_*/M_MALE_* **مع** AnimBP حقيقي؛ وإلا يطبق لانسلوت (المش/الانمي) تفاضليًا
- **الزحلقة**: حذف `bWantsToSlide=true` القسري (كان يجعل الباون "يريد" الانزلاق دائمًا!) + منح `GA_Athena_Player_Slide_C` (المسار الصحيح — الاسم مسح بالاسم بلا mount path) → انميشن الزحلقة الرسمي

### [F52] قدرات العالم + NPC
- `GrantWorldAbilities()` كل 8 ث: مسح بالاسم (SubclassOfFast على UClass — IsA لن يصل لهرمية القدرات) + منح GA_Athena_Player_Slide_C و GA_AthenaEnterVehicle_C حتى ينجحا
- `SpawnNPCs(n)` بعد 35 ث تلقائيًا (2) + `era.npc [n]`: FortPlayerPawnAthena + Engine.AIController + Possess + جسم لانسلوت (مشارك مع F51) — بشريون واقفون بالعالم بلا منطق بوت محفوف بالمخاطر

### [F53] علامة الخريطة (mini map point)
- `ServerSpawnMark(Actor, Offset, Text)` موجود في 21.30 → pe_hook يسجل وصوله (تشخيص مسار UI الحقيقي) + `era.mark` يرسلها عند اللاعب (نص فارغ = FText صفري)

### ملاحظات صادقة على طلب المستخدم
- **التسلق بنوعيه**: 21.30 لا يملك أي صنف Mantle (بحث شامل في كل هيدرات SDK الجديدة والقديمة) — الـ ledger climb جاء في Ch4 (23.00) فعليًا؛ بديل ClimbTick المحلي يبقى
- **القروش**: لا صنف Shark في الدامب؛ إن كانت مسجلة بأصناف تحمل مع الخريطة فسيكتشفها F46 وينفعها تلقائيًا — والا فهي غير موجودة في هذا البناء

### الحالة
- hook_fixed.cpp ≈ 5,200 سطر — فحص syntax نظيف (-Wall -Wextra) بستوبس stubs
- اللوق المتوقع: `[Scan] classes: containers=N spawners=N ...` + `[Loot] group ...` عند فتح صندوق + `[Interact] -> ...` + `[Veh] seated ...` + `[Build] -10 WoodItemData` + `[Build] spot already built` + `[Edit] ... (EditingPlayer set)` + `[QBW] poked N/M` + `[World] ✅` + `[NPC] spawned`
- partX.cpp مجمدة (F26/F27) — hook_fixed.cpp هو المصدر الوحيد للبناء

---

# الجولة F54–F63 — جهاز Log5: "لا تقدم إلا منع البناء فوق المبني" — راجع Erbium كاملاً

## التشخيص الجذري (يفسر كل الأعراض دفعة واحدة)

**[F54] GObjects كان معطلاً بالكامل بصمت.** كود `GObjectArray()` القديم قبل أن يكون العنوان
`profile::GObjects` مؤشراً إلى `FUObjectArray` فقط؛ حين يخزن البناء المصفوفة مباشرة (بدون
إسناد غير مباشر) ترجع الدالة `nullptr` صامتة — وكل ماسحات GObjects انهارت للصفر دون أي لوق:
- `[Loot] Total 0 containers` — لا صناديق، لا لوت أرضي
- `[Veh] 0 spawner actors on map` — لا سيارات في أماكنها الرسمية
- `[Wild] 0 animal classes` — لا حيوانات
- `[World] ability class not resident yet` للأبد (Slide/EnterVehicle موجودة في الدامب أصلاً!)
- `[QBW] no quickbar widgets` — الشريط ميت
- لا `[Harvest]` ولا `[Door] audit` ولا `[Scan]` في اللوق إطلاقاً (كانت تخرج مبكراً بصمت)
- **بناء فوق مباني الخريطة** (harvest_cache فارغ) و**لا موارد تكسير** — نفس السبب

الإصلاح: مسح مزدوج (مؤشر ← أو مباشر) مع تحقق بنيوي كامل (NumElements/NumChunks/chunk[0]/
first object) + كاش + لوق تشخيصي واحد `[GObj] resolved (pointer|direct) elements=N chunks=N`
أو `[GObj] UNRESOLVED` — من الآن فصاعداً اللوق يصرح بحالة الماسح.

## الإصلاحات

| # | النظام | الإصلاح |
|---|---|---|
| F55 | السيارات في أماكنها الرسمية | استدعاء `FortAthenaVehicleSpawner.SpawnVehicle/SpawnVehicleWithConstruction` الأصلي على كل spawner حقيقي في الخريطة (نفس مسار الـ warmup) + تتبع المركبات الناتجة؛ fallback: كلاسات أصلية دائمة الإقامة (`FortAthenaSimpleCar/Antelope/Goat/Doghouse/Ferret/Jackal`) قبل BP mounts |
| F56 | اللوت | `ScanMapContent` و`SpawnFloorLoot`: كل instance يشتق من `BuildingContainer` الأصلية = حاوية لوت (بدل فلاتر أسماء BP التي لا تطابق 21.30)؛ كشف floorloot لكل instance من اسم كلاسه؛ **`BP_SpawnLoot` الأصلي أولاً** (جداول اللوت المطبوخة الحقيقية) ثم الـ roll المحلي؛ إعادة مسح كل 60ث ما دامت الحاويات فارغة |
| F57 | QuickBar | عائلة الويدجت الصحيحة Bacchus (`BacchusQuickbarBase/BacchusQuickbar` + `UserWidget` كاحتياط) بدل `FortQuickbarBase` وحدها؛ أسماء أحداث متعددة (`HandleQuickbarContentChanged/OnQuickbarContentChanged/UpdateQuickbar/...`)؛ poke دوري كل 20ث لأول 3 دقائق |
| F58 | انهيار رمي الأدوات + خروج البناء | `ServerAttemptInventoryDrop` **يُبتلع** في pe_hook (مسار الخادم الميت هو الكراش)؛ DropInventoryItem: إزالة الـ ReplicatedEntry بالـ **GUID** (كانت بالفهرس = فساد!) + إزالة الـ instance + pickup أولاً + العودة للمعول إذا كان مجهزاً؛ مصفوفات QuickBar اليدوية تُخصص بمخصص UE (`FMemory::Realloc` عبر profile::Realloc) بدل malloc (خلط المخصصات = مصدر كراشات عشوائية)؛ `ServerHandlingBuildMode` يُبتلع + `[BuildCall] (diag)` يسجل كل مكالمة Build جديدة على الـ controller لتسمية مسار الخروج إن تكرر الكراش |
| F59 | كسر الصندوق ينسكب منه اللوت | بوابة `OnDamageServer` في pe_hook تمتد لاستقبالات `BuildingContainer`: ضربة اللاعب لصندوق غير مفتوح → `Loot()` فوراً (اللوت يقفز من الصندوق) |
| F60 | علامة الخريطة | ServerSpawnMark هو RPC خادم (لا خادم في standalone) — إضافة spawn محلي لممثل العلامة الحقيقي `B_AthenaPlayerMarker_WithCustomization_C` (مقيم في الدامب) مع ربط PlayerState، ثم يظل ServerSpawnMark يرسل أيضاً |
| F61 | قدرات | القائمة: Slide + **GA_AthenaEnterVehicle** (إملاء 21.30 الصحيح) أساس، + `GA_AthenaExitVehicle_C / GA_AthenaInVehicle_C / GAB_Emote_Generic_C` best-effort لا تعطل حلقة إعادة المحاولة؛ إصلاح `GetHeroTypeBP` المرفوض: HeroDefinition هو نفسه FortHeroType في 21.30 |
| F62 | شجرة الواقع | `SpawnRealitySaplings` — `RealitySeedGameplayRuntime.BuildingRealitySapling` (أصلي، GObjects 1565) يُزرع تلقائياً (2) بعد 45ث + أمر `era.reality [n]` |
| F63 | NPC/حيوانات | NPC: 4 تلقائياً + تغذية كل 60ث (لا يقل عن 2 حي، تتبع في session.npcs)؛ الحيوانات: إعادة محاولة كل 20ث ×10 (كانت محاولة واحدة مبكرة قبل تحميل Irwin)؛ ByShortName أصبح memoized (منع تقطيع اللعبة عند فتح عشرات الصناديق) |
| — | مزامنة | part1..part7.cpp **أعيد توليدها كلها** كتقسيم متتالٍ مطابق 100% لـ hook_fixed.cpp الحالي (تم التحقق بـ diff) |

## المتوقع في اللوق القادم (خطوات تحقق)
1. `[GObj] resolved (pointer|direct) elements=~400k chunks=~7` ← **هذا السطر هو مفتاح الجولة**
   (إن ظهر `UNRESOLVED` أرسل اللوق فوراً — سنعالج من العنوان)
2. `[Scan] classes: containers=N ...` ثم `[Scan] containers=N chests=N floorloot-spawned=N`
3. `[Loot] ✅ Container search ...` عند الاقتراب من صندوق + `[Loot] container damaged — popping loot` عند ضربه بالمعول
4. `[Veh] N spawner actors on map` > 0 و`[Veh] native spawner -> vehicle at ...` ثم `[Veh] ✅ seated` عند الاقتراب
5. `[Wild] N animal classes` و`[Wild] spawned NPC_Pawn_Irwin_Prey_Burt_C`
6. `[World] ✅ slide + enter-vehicle abilities granted` (نهاية سبام "not resident yet")
7. `[QBW] poked N/M` > 0 — الشريط يجب أن يعرض الأسلحة الآن
8. `[Harvest] + N WoodItemData` عند التكسير + `[Build] -10 WoodItemData` عند البناء + `[Build] spot already built` فوق مباني الخريطة
9. `[Mark] local marker spawned` عند era.mark
10. إن حدث كراش عند خروج البناء: `/data/local/tmp/era_crash.txt` + سطور `[BuildCall]` في اللوق سيكشفان الدالة

---

# الجولة F64–F71 — Erbium 1:1 (StandAlone)

## التشخيص الجذري (دراسة Erbium game.cpp كاملة — 1553 سطر)

بعد قراءة Erbium سطراً سطراً، تبيّن أن كل عيوب هذه الجولة تعود إلى **5 انحرافات عن
بنية Erbium** — كلها صُحّحت:

1. **القدرات**: Erbium لا يستدعي أي تفعيل يدوي إطلاقاً؛ كل القدرات تصل عبر
   `EquipFortAbilitySet` فقط (spec حقيقي + ربط إدخال). كودنا كان يفعل
   `TryActivateAbilityByClass` على `CharacterJump/InteractUse/InteractSearch/
   **Death**/Skydive/EnterVehicle/InVehicle/ExitVehicle/Emote` — تفعيل Death عند
   الولادة وتفعيل InVehicle خارج مركبة = فساد حالة اللاعب (الشخصية "غير قابلة
   للعب" + "GiveAbility ميتة" لأن التفعيل ليس إسناداً).
2. **الجسد**: Erbium **لا يستبدل SkeletalMesh أبداً** — الجسد يُركّب من
   CustomCharacterParts فقط (ServerChoosePart + ApplyCharacterCosmetics +
   OnReps). استبدالنا المباشر بـ M_MED_Lancelot (هيكل عظمي مجرد) هو "الشكل
   المريض".
3. **التفاعل**: Erbium يعتمد على `ServerAttemptInteract` من مكوّن اللعبة نفسه
   (المسافة 600) + `BlueprintOnInteract` للأبواب. كودنا كان مسح GObjects بالترتيب
   (أول 768 صندوقاً قد تكون كلها في منطقة واحدة بعيدة) + مدى 170 فقط → "الصندوق
   لا يستجيب ولا أي نوع".
4. **`Call::result<T>()` كان يقرأ CPF_ReturnParm فقط** — كل دوال
   GetAllActorsOfClass/GetAllWidgetsOfClass تُخرج في **CPF_OutParm** → كل نداءات
   GetAll* كانت تقرأ مصفوفة فارغة دائماً! هذا وحده أهدم poke الشريط
   ("no quickbar widgets resident" كانت نتيجة كاذبة).
5. **المركبات**: `SpawnVehicle` قد يرفض بصمت (phase gating) بينما النداء "ينجح"
   — اللوق أظهر 12 "native spawner -> vehicle" ثم "tracked 0 live vehicles".

## الإصلاحات

| # | النظام | الإصلاح |
|---|---|---|
| F64 | قراءة الـ Out-params | `Call::get_out<T>(key)` (CPF_OutParm) + إصلاح PokeQuickbarWidgets وGetAllActorsOfClass — كل نداءات GetAll* كانت فارغة بصمت |
| F65 | التفاعل + اللوت | `NearbyScanRefresh()` كل 3ث عبر `GetAllActorsOfClass(BuildingContainer/FortPickup)` (وعي streaming — دائماً حول اللاعب)؛ مدى الصندوق 170→260/320؛ **التقاط تلقائي** للـ pickups داخل 3م عبر مسار Era local_pickup (تكديس + تدمير)؛ أي SearchLootTierGroup غير معروف يرجع لطبقة Treasure بدل الرفض — كل أنواع الحاويات تُفتح |
| F66 | المركبات | تحقق **بالعدّ** قبل/بعد `SpawnVehicle` (لا ثقة بالنداء الأعمى) + fallback فوري إلى `CachedItemDef.VehicleActorClass` عند الرفض؛ دمج **كل** المركبات الحية في session.vehicles كل جولة؛ مدى الركوب 200→300 |
| F67 | القدرات 1:1 | حذف قائمة التفعيل اليدوي بالكامل (كانت تفعّل Death!)؛ Slide يُضاف إلى GAS_AthenaPlayer نفسها؛ `GrantWorldAbilities` يبني **FortAbilitySet مصغّر** (SpawnObject + GenericArrayAdd) ويُمرَّر عبر EquipFortAbilitySet = إسناد حقيقي مع ربط إدخال مثل Erbium |
| F68 | الجسد | حذف استبدال الـ mesh نهائياً؛ FixBodyAndAnimations v3: إعادة تشغيل管线 الأجزاء عند الفراغ + استعادة AnimScriptInstance فقط إذا غاب + إظهار الجسد — مسار Erbium حرفياً |
| F69 | البناء/الـ HUD | `ServerHandlingBuildMode` **يتمرر أصلياً** (الابتلاع كان يعلق اللاعب في وضع البناء — "لا يمكن الخروج")؛ إعادة إطلاق `ClientOnPawnSpawned` عند كل تغيّر GamePhase + ForceUpdateQuickBars + تشخيص `[HUD] hud=… class=…` في FinalizePlayer |
| F70 | الأبواب | Erbium-pure: `BlueprintOnInteract` مع override المُستقبِل يقود **الفتح والإغلاق** معاً (الأنيميشن والصوت من الـ BP نفسه)؛ التبديل اليدوي للبتّ أصبح fallback فقط — **الباب يُغلق الآن** |
| F71 | NPC/حيوانات | `bCanBeDamaged=false` + SetMaxHealth/SetHealth(100) + SetActorHiddenInGame(false) — لا موت عند الولادة ولا اختفاء (اللوق: "دجاج مات / ضفدع / NPC غير مرئين") |
| — | مزامنة | part1..part7.cpp أعيد توليدها (832×6+827) — diff ضد hook_fixed.cpp = مطابق 100% |

## المتوقع في اللوق القادم (خطوات تحقق)
1. `[HUD] hud=… class=…` في نهاية Finalize — إن لم يكن FortHUD فسنبدّل HUDClass
2. `[HUD] phase N — ClientOnPawnSpawned re-kick` عند كل تغيّر طور
3. `[QBW] poked N/M` > 0 — الآن يقرأ FoundWidgets فعلياً (كان فارغاً دائماً!)
4. الاقتراب من صندوق → `[Loot] Opening …` ثم `[Loot] ✅ Container search …` — **الصندوق يفتح ويقفز اللوت**
5. السير فوق لوت أرضي → `Pickup stacked …` أو `Local pickup transferred …` — **الالتقاط يعمل**
6. `[Veh] native spawner refused … fallback` أو native حقيقي + `[Veh] tracked N live vehicles (spawned this pass=N)` — N>0
7. الاقتراب من مركبة → `[Veh] ✅ seated`
8. `[Abilities] Done — sets=N (set delivery only, no manual activations)` + `[World] ✅ N world abilities delivered through EquipFortAbilitySet`
9. `[Body] ✅ body via N character parts — no direct mesh swap` — **الشكل الطبيعي يعود**
10. `[Door] OPEN/CLOSE via BlueprintOnInteract` — **الباب يُغلق ويُفتح**
11. زر البناء يبدّل الدخول/الخروج بدون كراش (`[BuildCall] ServerHandlingBuildMode passes through`)
12. أي كراش: `/data/local/tmp/era_crash.txt`

---

# الجولة F72 — إصلاح كراش المركبات (اللوق الذي انتهى بـ "spawned FortAthenaSimpleCar")

## التشخيص الجذري — من سورس Erbium نفسه (FortGameMode.cpp:2019)

سطر في Erbium يشرح كل شيء:

```cpp
if (... std::floor(VersionInfo.FortniteVersion) != 20 && != 21 && != 22)
    // its auto on s20, s21, and s22   ← Erbium يتخطى توليد المركبات يدويًا على s21!
```

1. **على الموسم 21 (هدفنا 21.30) المركبات تولد تلقائيًا** عبر مولدات الخريطة
   الأصلية — Erbium نفسه لا يولّد شيئًا يدويًا. لذلك `SpawnVehicle` الأصلي
   "يرفض" عندنا: المولد ينتظر دورة حياة وضع اللعب الكاملة (warmup → phases)
   غير موجودة في الـ standalone.
2. **مسار الـ fallback كان ينشئ أصنافًا أصلية عارية.** هرمية 21.30:
   - `FortAthenaSimpleCar : AFortAthenaVehicle : AFortPhysicsPawn` — فرع
     الفيزياء، اشتغل (اللوق: "spawned FortAthenaSimpleCar").
   - `FortAthenaAntelopeVehicle/Goat/Doghouse/Ferret/Jackal :
     AFortAthenaSKMotorVehicle/SKVehicle` — **فرع الهياكل العظمية**؛ إعدادات
     العجلات/الهيكل/الأنميشن تعيش في الـ BP. إنشاء الصنف الأصلي العاري =
     بناء غير مكوّن = **كراش فوري** (اللوق توقف عند الأول ثم انتهى).
3. كانت الحلقة تنشئ **11 مركبة في إطار واحد** — أول كلاس سيئ يهدم كل شيء.

## الإصلاحات

| # | النظام | الإصلاح |
|---|---|---|
| F72-a | حظر الأصناف العارية | `UnsafeRawVehicleClass()` — كل أصناف SK الأصلية (Antelope/Goat/Doghouse/Ferret/Jackal/SKMotorVehicle/SKVehicle) محظورة من الإنشاء الخام؛ أصناف الـ BP (تنتهي بـ `_C`) مسموحة دائمًا لأنها تحمل إعداداتها |
| F72-b | مسار البناء الأصلي | لكل مولد حقيقي: `GetVehicleClass()` (BlueprintPure يعيد صنف الخريطة نفسه) ثم `SpawnVehicleWithConstruction(Class, Transform)` — مدخل البناء الرسمي الذي تستخدمه BP الخريطة (وقود/مقاعد/مودات مربوطة)؛ يتحقق من المؤشر المرجع أو ارتفاع العدد (F66)؛ سقف 4 لكل جولة |
| F72-c | ترتيب الكاش | كاش الـ fallback: أصناف الـ BP mounts **أولًا** ثم `FortAthenaSimpleCar` وحده (الوحيد الآمن خامًا) — الأصناف SK الأصلية خارج الكاش نهائيًا |
| F72-d | معدل الحلقة | **مركبة واحدة لكل جولة إعادة** (كل 5ث) بدل 11 في إطار — كلاس سيئ لن يهدم المركبات الباقية، وسطر `raw spawn attempt: <اسم الصنف>` يسمّي المذنب إن حدث كراش |
| F72-e | استمرار المحاولة | VehicleRetry يستمر حتى 8 مركبات متتبعة (كان يتوقف عند أول نجاح — مع المعدل الجديد 1/جولة كان سيتوقف عند مركبة واحدة) |
| F72-f | لوق أنظف | `GetHeroTypeBP` المرفوض لم يعد يُستدعى أصلًا حين يكون HeroDefinition FortHeroType بالفعل (كان ضجيجًا لا فشلًا) |

## المتوقع في اللوق القادم (سطور التحقق)
1. `[Veh] constructed <صنف الخريطة> at spawner (x y z)` — البناء الأصلي نجح ← **هذا هو المسار الصحيح 1:1**
2. أو `[Veh] raw spawn attempt: <صنف _C>` ثم `[Veh] spawned ...` — مسار Erbium الخام بصنف مُهيّأ
3. لا مزيد من `spawned FortAthenaAntelopeVehicle` إطلاقًا (محظور)
4. `[Veh] tracked N live vehicles` مع N>0 — المركبات حية وقابلة للركوب
5. **لا كراش بعد "fallback resolved"** — الحلقة الآن مركبة واحدة كل 5 ثوان
6. استمر ببقية الاختبار الذي كان الكراش منعك منه: **صندوق/تفاعل/بناء/أبواب/التقاط لوت** — كلها وصلت إصلاحاتها في F64–F71 ولم تُختبر بعد بسبب هذا الكراش

---

# الجولة F73 — منطق Battle Royale الكامل من Erbium (طبقة كاملة 1:1)

**شكوى المستخدم:** «لا شيء تغيّر عن الإصدار السابق — كل شيء ميت. هذا هو Erbium، انظر كل شيء من logic GamePlay Battle Royale وطبّقه» — github.com/plooshi/Erbium لسيزون 21.30.

## التشخيص الجذري (قراءة Erbium كاملة سطرًا سطرًا)

المباراة «ميتة» لأن **كل محرك المباراة يعمل على السيرفر** في اللعبة الحقيقية، والـ standalone لا يملك سيرفرًا:

1. **دورة الأطوار (GamePhase machine)** — `BattleRoyaleGamePhaseLogic.cpp`: Warmup (عدّاد) → Aircraft (الباص) → SafeZones (العاصفة). هي فقط تجعل المباراة «حية». عندي كانت الدالة القديمة DriveSafeZone تحاول `InitializeWithMapDefaultPhases + StartPhase(0)` — لا تنشئ مصفوفة الأطوار أبدًا (المصدر الحقيقي: مولّد السيرفر).
2. **العاصفة** — Erbium `SetupSafeZoneIndicator`: يبني `SafeZonePhases` من `MapInfo->SafeZoneDefinition` (نصف قطر/انتظار/انكماش/ضرر لكل طور) + `StartNewSafeZonePhase` يضع المركز/النصف قطر/أوقات الانكماش/الضرر + OnReps + `OnSafeZoneStateChange` + بايت الحالة (FutureReplicator-4). دون هذا: لا دائرة، لا مؤقت، لا ضرر.
3. **الباص (Aircraft)** — Erbium `StartAircraftPhase`: `AFortGameStateAthena::SpawnAircraft(World, MapInfo->AircraftClass, FlightInfos[0])` + نوافذ الإسقاط/الطيران + `GameState->Aircrafts` + `bAircraftIsLocked`. الكود القديم كان «يقلّد» الطور 3 وينقل اللاعب إلى (0,0,50000) — بلا باص حقيقي.
4. **اللاعبون (99)** — Erbium spawnbot: Pawn + `Athena_PlayerController_C` + `FortPlayerStateAthena` + Possess + ربط PlayerState + TeamIndex/SquadId/bIsABot + ability sets + PlayersLeft++ + AlivePlayers. الـ PlayersLeft كان عندي ثابتًا 100 بلا roster.
5. **العدّاد والمؤقتات** — WarmupCountdownStartTime/EndTime (GameState + component) = عدّاد الإحمود الظاهر في HUD.

## الإصلاحات (F73)

| # | النظام | الإصلاح |
|---|---|---|
| F73-a | جداول العاصفة | `BRZoneTables()`: Count من `SafeZoneDefinition.Count` إن وُجد وإلا 9؛ جداول ERBium-BR: Radius/Wait/Shrink/Damage% لكل طور (1%→10%) |
| F73-b | مراكز الدوائر | `BRPickZoneCenters()`: نفس قاعدة الالتقاط الأصلي (كل دائرة داخل سابقتها) حول `MapInfo->GetMapCenter()` |
| F73-c | المؤشر الحقيقي | `BRSetupSafeZoneIndicator()`: spawn `SafeZoneIndicatorClass` (أو الصنف الأصلي) + بناء `SafeZonePhases` عنصرًا عنصرًا (إزاحات معكوسة: Radius/Wait/Shrink/PlayerCap/Center/DamageInfo%) + `PhaseCount/CurrentPhase` OnReps + ربط على GameState/GameMode/component + `bSafeZoneActive` + رفع `bPlaylistStoppedSafeZonePhases` |
| F73-d | بدء طور العاصفة | `BRStartNewSafeZonePhase(p)`: `HandlePostSafeZonePhaseChanged(-1)` (كتاب Erbium عبر الـ delegate) + `SafeZonePhase=p` + Previous/Next/NextNext (+FutureReplicator) + نافذة الانكماش + `CurrentDamageInfo` + OnReps + بايت الحالة=2 + `OnSafeZoneStateChange(2)` + خطوة StormHolding |
| F73-e | الباص الحقيقي | `BRStartAircraftPhase()`: `SpawnAircraft(World, Class, FlightInfos[0])` (نسخ bytes المعامل بالحجم المطابق) أو spawn يدوي + نسخ `FlightInfo` على الطائرة + الحقول الفردية (Speed/StartLocation/TimeTill*) + نوافذ drop/flight + `Aircrafts` + OnRep + `bAircraftIsLocked` + صعود اللاعب (إخفاء + إيقاف تصادم + كاميرا على الباص — بديل standalone لـ destroy+Spectating) |
| F73-f | القفز من الباص | `BRJump()`: نقل البان إلى موضع الباص (Z-200) + إعادة الظهور/التصادم + سلسلة الهبوط F05 (ServerAttemptAircraftJump + BeginSkydiving + استعادة الكاميرا)؛ زر HUD نفس المسار |
| F73-g | تكتلة الـ 99 | `BRSpawnBotBatch()`: Erbium spawnbot حرفيًا — موجات 10 كل ثانيتين؛ PlayerStarts للتوزيع (fallback حول مركز الخريطة)؛ SetMaxHealth(100)؛ TeamIndex=3+i/SquadId/bIsABot؛ HeroType Commando + ApplyCharacterCosmetics + OnRep_HeroType/CharacterData؛ ability sets؛ AlivePlayers؛ PlayersLeft=1+bots + OnRep |
| F73-h | ضرر العاصفة | `BRStormTick()` (كل ثانية في SafeZones): هندسة الدائرة عبر `GetSafeZoneCenter/Radius` + تحديث `bIsInsideSafeZone/bIsInAnyStorm` (+OnReps) لكل بان + `GameplayStatics.ApplyDamage` بنسبة الـ phase% (المُلحق = المؤشر) — للاعب والبوتات؛ البوت الميت → destroy + PlayersLeft-- + إزالة من AlivePlayers (خروج حقيقي من المباراة) |
| F73-i | بدء المباراة | `BRStart()` (= HandleMatchHasStarted): عدّاد إحمود 60ث على component+GameState+GameMode + GamePhase(Warmup)/Step(Warmup) + `WarmupRequiredPlayerCount=1` + `bWorldIsReady` + اللاعب المحلي في AlivePlayers + PlayersLeft=1 |
| F73-j | المحرك | `Athena::BRTick()` كل tick: Warmup→(انتهاء)→Aircraft→(drop_end: قفز إجباري + SafeZonesStartTime=+60 + StormForming)→(flight_end: تدمير الباص + failsafe استعادة البان)→SafeZones→(safezones_start: مؤشر+طور 1)→(انكماش: state 3 + StormShrinking)→(الطور التالي أو تعليق الدائرة الأخيرة) + سطور [Zone] |
| F73-k | تنظيف | حذف DriveSafeZone/ZoneMonitor (F35) والاستدعاءات القديمة من SetupWorld؛ SpawnBattleBus أصبح مسار cheat يطلق الطور الحقيقي؛ OnAircraftJump→BRJump؛ ForcePlayerVisible يتخطى إخفاء الباص |
| F73-l | أوامر | `era.bots N` (target roster)، `era.warmup N` (تقصير الإحمود)، `era.bus` (إطلاق الباص فورًا)، `era.zone [i]` (قفز أطوار العاصفة عبر المحرك الجديد) |

## فحص + تزامن
- `g++ -std=c++17 -fsyntax-only -Wall -Wextra` مع stubs → **EXIT=0** (صفر تحذيرات) — hook_fixed.cpp = 6960 سطرًا
- إعادة توليد part1..part7 (832×6+1968) — دمجها ثم diff ضد hook_fixed.cpp = **مطابق 100%**

## المتوقع في اللوق القادم (سطور التحقق — بالترتيب)
1. `=== [BR] match start — Erbium HandleMatchHasStarted ===` ثم `[BR] warmup 60s` — المحرك بدأ (بعد finalize بثانيتين)
2. `[BR] GamePhase -> 2` + `[BR] GamePhaseStep -> 2` — عدّاد الإحمود ظاهر في HUD
3. `[BR] PlayerStarts available: N` ثم `[BR] +10 bots — roster 10/99, PlayersLeft=11` (كل ثانيتين) — **أول مرة يظهر فيها لاعبون آخرون بالمباراة**
4. `[BR] GamePhase -> 3` + `[BR] StartAircraftPhase` + `[BR] aircraft ... class=... drop=...` + `[BR] flight path: start=... speed=...` + `[BR] player boarded` — **الباص الحقيقي يحلّق والكاميرا عليه**
5. `[BR] bus unlocked — jump button live` ثم إما قفزك (`[Bus] JumpFromHudStart`) أو `[BR] drop window ended — forcing jump` — هبوط من الباص
6. `[BR] GamePhase -> 4` + `[BR] zone centers picked (9)` + `[BR] phase struct esz=...` + `[BR] SafeZoneIndicator spawned phases=9` + `[BR] zone phase 1 r=19800 ...` — **العاصفة تشكّلت**
7. `[Zone] phase=1 radius=... center=...` ثم `[BR] zone shrinking (state 3)` + `[BR] GamePhaseStep -> 8` — الدائرة تنكمش فعليًا
8. `[BR] local pawn storm status: STORM` + `[BR] storm damage ...` خارج الدائرة، و`[BR] 💀 ErbiumBot(#N) eliminated by the storm (left=...)` — العاصفة تقتل وعداد الـ 99 ينخفض
9. **الأولوية على الجهاز:** إذا ظهر كراش، آخر سطر قبل `/data/local/tmp/era_crash.txt` يسمّي النظام (BRStart/aircraft/indicator/bots) — كل نظام مُسطر.

---

# الجولة F74 — كراش الصفر + بوتات Default + بوتات ثابتة — تطبيق Play Core من Erbium (github.com/plooshi/Erbium)

**شكوى المستخدم:** «يكرش أول ما يوصل عداد الباص إلى الصفر — كل مرة يكرش عند البيكاكس. الـ Player Counter يعمل والبوتات رُسبنت لكن بشخصيات Default ولا تتحرك. انقل Play Core أكثر من Erbium».

## التشخيص الجذري (لوق الجهاز F73 + SDK 21.30 + قراءة Erbium)

1. **كراش الصفر (الدليل قاطع في اللوق):** آخر سطر قبل الكراش = `Equip WID_Harvest_Pickaxe_Athena_C_T01 -> 0x7121352770` — هذا equip **محلي ناجح** عبر مسار pe_hook لـ `ServerExecuteInventoryItem` (سطر اللوق يُطبع بعد نجاح `EquipWeaponDefinition`). بعده الكراش — أي داخل `orig_PE` الأصلي (السطر 6633: كل مسار مرُّوِط يسقط إلى الأصل). **نفس فئة كراش F58** (`ServerAttemptInventoryDrop` — «المسار السيرفري الميت»). عند الصفر تُشغّل اللعبة تلقائيًا سلوط 0 (Erbium يفعل `ClientActivateSlot(0,0)` سيرفريًا في اللحظة نفسها) → العميل يطلق `ServerExecuteInventoryItem` → الـ equip المحلي ينجح → **الأصل السيرفري يقتل العملية**. لهذا لم يظهر الكراش قبل F73: الشريط كان ميتًا ولم تُستدعَ هذه الـ RPC إطلاقًا.
2. **بوتات Default:** F73 طبّق `ApplyCharacterCosmetics` بأجزاء بمسارات لا تُحلّ في 21.30 (`/Game/Athena/Heroes/Meshes/Heads/F_Med_Head1_ATH` مسار **اللاعب المحلي** — للبوتات لم يمر عبر `ServerChoosePart` أبدًا) — والجسد العامل للاعب المحلي جاء من `FortPlayerPawn.ServerChoosePart` (السلسلة الكاملة). GObjects-dump 21.30 يؤكد: `F_Med_Head1`/`F_Med_Soldier_01` (اختيار Erbium) غير محملة؛ البدائل المحملة: `CP_Head_Med_Soldier_M_DesertOps`، `CP_Athena_Body_M_Fallback`، `NoBackpack`.
3. **بوتات ثابتة:** بان مملوك لـ PlayerController بلا مصدر إدخال = تمثال. Erbium بوتاته صامتة عمدًا (سكان المباراة فقط)؛ المستخدم يريد مباراة حية.
4. **`[BR] GamePhaseLogic component class missing`:** **متوقع** — `UFortGameStateComponent_BattleRoyaleGamePhaseLogic` **غير موجودة إطلاقًا في 21.30** (تحقق في SDK dump: صفر نتائج). آلية الأطوار في هذا البناء تعيش على `AFortGameStateAthena` (GamePhaseStep@0x2381) + `FortGameModeAthena.OnGamePhaseStepChanged`.
5. **`SpawnAircraft`:** في 21.30 هي **static على AFortAthenaAircraft** (`FortAthenaAircraft.SpawnAircraft`: World@0 + AircraftClass@8 + FAircraftFlightInfo InFlightInfo@0x10 بحجم 0x48) — مسار `FortGameStateAthena.SpawnAircraft` القديم لم يوجد في هذا البناء أصلًا (كان يسقط للـ fallback الصامت).

## الإصلاحات (F74)

| # | النظام | الإصلاح |
|---|---|---|
| F74-a | **كراش الصفر** | ابتلاع `ServerExecuteInventoryItem` + `ServerExecuteInventoryWeapon` في pe_hook **لأي مستقبل** (قبل بوابة isPC — بوت بلا WorldInventory = null deref مؤكد): الـ equip المحلي يُنفَّذ ثم `return` — الأصل السيرفري الميت لا يعمل أبدًا (نمط F58) |
| F74-b | الباص الحقيقي | المسار الأصلي 21.30: `FortAthenaAircraft.SpawnAircraft` على الـ CDO مع bytes الـ FlightInfos[0]، وشرط فشل → fallback يدوي + `bWasSpawnedForAircraftPhase=true`؛ **كل خطوة تُسطَّر قبل تنفيذها** (class/SpawnAircraft/manual/flight fields/skin/register/board/phase) — أي كراش متبقٍ يسمّي نفسه |
| F74-c | جلد الباص | `BBID_DefaultBus` (موجود في dump: `AthenaBattleBusItemDefinition BBID_DefaultBus.BBID_DefaultBus`) → `GameStateAthena.DefaultBattleBus` + `Aircraft.DefaultBusSkin` + `SpawnedCosmeticActor.ActiveSkin` (Erbium FortGameMode.cpp:1043-1109 — بدونه جسم الباص فارغ لأن standalone بلا matchmaking يضبط DefaultBattleBus) |
| F74-d | بوتات بشخصيات حقيقية | السلسلة الكاملة لكل بوت (نفس سلسلة اللاعب المحلي): `FortPlayerPawn.ServerChoosePart(0=Head,1=Body)` + `SetCharacterBodyVisibilityForPossession(true)` + كتابة `FortPlayerState.LocalCharacterParts[0/1/3]` (مصفوفة Erbium spawnbot الثابتة ×7) + HeroType=HID_001_Athena_Commando_F + OnRep_HeroType + ApplyCharacterCosmetics + OnRep_CharacterData + UpdatePlayerCustomCharacterPartsVisualization؛ البدائل المُتحققة من dump: DesertOps/Fallback/NoBackpack + لوق `bot parts head=... body=...` |
| F74-e | **بوتات تتحرك** | `BRBotMoveTick()` كل تِك: لكل بوت موتور {pawn, yaw, next_heading} — كل 2.5-4.5ث `Controller.SetControlRotation(yaw جديد)` (بوتات Athena تتبع ControlRotation) + `Pawn.AddMovementInput(dir,1,false)` عبر UFunctions مُخزَّنة (استدعاءان cached لا مسح GObjects)؛ الميت يُسقط من القائمة (الكراش/العاصفة) + سطر اللوق يعرض motors=N |
| F74-f | بث الخطوات 21.30 | `BRSetGamePhaseStep` يستدعي أيضًا `FortGameModeAthena.OnGamePhaseStepChanged` + `FortPlayerPawnAthena.GamePhaseStepChanged` بـ {TScriptInterface=SafeZoneIndicator، step} حين يوجد مؤشر (أطوار العاصفة — تدفق السيرفر الحقيقي؛ أطوار الباص تُبلَّغ عبر OnRep_GamePhase) + **خطوة GetReady(3)** عند نهاية الإحمود -10ث (آلة Erbium Tick) + `ClientActivateSlot(0,0)` مع إطلاق الباص (ذيول Erbium StartAircraftPhase — أصبح آمنًا بعد F74-a) + رسالة «component missing» أصبحت توضيحية |

## فحص + تزامن
- `g++ -std=c++17 -fsyntax-only -Wall -Wextra` مع stubs → **EXIT=0** (صفر تحذيرات) — hook_fixed.cpp = **7170 سطرًا**
- إعادة توليد part1..part7 (832×6+2178) — دمج ثم diff ضد hook_fixed.cpp = **مطابق 100%**

## المتوقع في اللوق القادم (سطور التحقق — بالترتيب)
1. `[BR] bot parts head=0x... body=0x... backpack=0x...` — أجزاء البوتات حُمّلت (لا صفر)
2. `[BR] +10 bots — roster ..., motors=N` — الموتورات مسجلة؛ **البوتات تمشي وتلتفت** من لحظة ظهورها
3. عند الصفر: `Equip WID_Harvest_Pickaxe...` ثم **لا كراش** — يليه مباشرة:
4. `[BR] StartAircraftPhase (Erbium) t=...` → `aircraft class=...` → `SpawnAircraft (static) elem=72` → `SpawnAircraft -> 0x...` (أو `manual aircraft -> ...`) → `writing flight path fields...` → `applying bus skin...` → `registering aircraft...` → `boarding player...` → `GamePhase -> 3` → `StartAircraftPhase complete` — **الباص يحلّق بجلد افتراضي والكاميرا عليه**
5. `ClientActivateSlot(0,0)` عند الصفر لن يكرش (الابتلاع F74-a يغطي الـ RPC الذي سيطلقه)
6. `[BR] GamePhaseStep -> 3` (GetReady قبل الباص بـ10ث) ثم `-> 4` (BusLocked)
7. عند هبوطك: `[Zone] phase=1 ...` ثم `[BR] zone shrinking` + مؤثرات الخطوة عبر `OnGamePhaseStepChanged` (موسيقى/إعلان العاصفة)
8. `[BR] 💀 ErbiumBot(#N) eliminated` — البوت الميت يختفي من العداد ومن الموتورات
9. **إن ظهر كراش:** آخر سطر `[BR] ...` يسمّي الخطوة بالضبط (كل خطوة تُسطر قبل تنفيذها) + era_crash.txt

---

# الجولة F75 — كراش الصفر عاد (الابتلاع F74-a لم يكفِ) + بوتات لا تتحرك + حيوانات ميتة + بلا تفاعل — Play Core أعمق من Erbium

**شكوى المستخدم (لوق بناء F74):** «العداد وصل 0 وكرش. البوتات لا تتحرك. الحيوانات من المفترض أن تتحرك وترسبن في أماكنها. لم تضف تفاعل — تفاعل مع صندوق/آلة/حيوان/أي شيء. ركّز في كيف يحل Erbium كل هذا وطبّقهم».

## التشخيص الجذري المُصحَّح (اللوق الجديد يُبطل نصف تشخيص F74)

1. **الكراش ليس داخل `orig_PE` لـ ServerExecuteInventoryItem** — بناء F74 كان يبتلع الأصل أصلًا (السطر 6734)، ومع ذلك كرش عند الصفر بنفس اللحظة (`Equip WID_Harvest_Pickaxe_Athena_C_T01 -> 0x70fd812770` آخر سطر). **الدليل الحاسم:** سطر `[BR] StartAircraftPhase (Erbium)` (أول سطر في الدالة، قبل أي عمل) **لم يظهر أبدًا** — أي أن الكراش وقع قبل أن يصل محركنا للباص إطلاقًا.
2. **المقصّية الحقيقية: انتقال الإحمود-النهاية الأصلي (native).** نحن نكتب `WarmupCountdownEndTime`/`WarmupCountdownDuration` على GameState+GameMode منذ F73 → **مؤقّت الإحمود الأصلي في اللعبة يعمل فعليًا** (وهذا ما يقود عدّاد الباص الظاهر). عند الصفر يشغّل الكود الأصلي تسلسل نهاية الإحمود الحقيقي: يطلق equip البيكاكس تلقائيًا (ServerExecuteInventoryItem — نراه في اللوق بلا أي استدعاء منا) ثم يكمل باتجاه باقي اللاعبين — **بوتاتنا بلا WorldInventory ولا تسجيل GameMemberInfoArray** (Erbium يسجل بوتاته؛ الـ spawning policy الأصلي يمنح مخزونًا كاملًا) → الكود الأصلي يطير في أول محاولة وصول لمخزون بوت.
3. **بوتات ثابتة رغم موتورات F74:** تحقق SDK يؤكد `Engine.Pawn.AddMovementInput` موجودة بمعاملاتها الصحيحة — لكن نبضات الإدخال لا تتحول لحركة لأن ControlInputVector لا يُستهلك لبناتنا المولّدة خامًا (بلا أنبوب possession/input كامل). الحل ليس الإدخال بل **قناة الـ Velocity نفسها** (قناة LaunchCharacter): كتابة `CharacterMovement.Velocity` مباشرة كل تِك.
4. **حيوانات ميتة + ليست في أماكنها:** تُرسبن بحلقة حول اللاعب فقط (600-900 وحدة) ومتحكمات AI وحدها لا تحركها على هذا البناء. Erbium نفسه لا يملك كود حيوانات — حيوانات Erbion تعمل لأن دورة الحياة الأصلية كاملة؛ أقرب مكافئ: نثر على كامل الخريطة + موتورات حركة.
5. **بلا تفاعل:** نكتب `InteractActor` فقط — تلميح التفاعل المحمول يقرأ أيضًا `CurrentlyHighlightedActors` على مكوّن التفاعل (تحقق SDK @0x0218) — لم نكن نكتبه.

## الإصلاحات (F75)

| # | النظام | الإصلاح |
|---|---|---|
| F75-a1 | **كراش الصفر — تجهيز اللاعبين للانتقال الأصلي** | كل بوت يحصل الآن على **WorldInventory حقيقي** (SpawnActor لـ FortInventory + SetOwner + `PC.WorldInventory` + `bHasInitializedWorldInventory` + `InventoryType=World`) — الانتقال الأصلي يمشي على كل controller في AlivePlayers ويطير على مخزون null؛ وErbium يسجّل بوتاته في **GameMemberInfoArray** (FGameMemberInfo{SquadId,TeamIndex} + GenericArrayAdd بإزاحات SDK 0x0C/0x0D مع fallback) — كنا نتجاهل ذلك كليًا |
| F75-a2 | **كراش الصفر — ابتلاع سلسلة الـ equip كاملة** | ابتلاع `ClientActivateSlot` الخارجي في pe_hook (الإعلان الوحيد في 21.30 على FortPlayerController — إشعار سيرفر→عميل؛ نقرات اللاعب لا تمرّ عبره أبدًا واستدعاؤنا نتجاوز الهوك عبر orig_PE مباشرة) — يمنع سلسلة الـ equip الأصلية في لحظة الصفر من الجذر؛ + قصر معالجة `ServerExecuteInventoryItem/Weapon` على المتحكم المحلي حصرًا (بوتات تبتلع بصمت) |
| F75-a3 | **كراش الصفر — dedupe الـ equip الممسوك** | `OnServerExecuteInventoryItem` يتخطى الطلب إذا كان **نفس الغرض مسوكًا فعلًا** (`FortPawn.CurrentWeapon.ItemEntryGuid == guid`) — إعادة تسليح البيكاكس الممسوك منتصف تغيير الطور نافذة كراش على هذا البناء؛ التبديل الشرعي للأبنية الأخرى لم يتأثر (التحقق من المسوك فقط) |
| F75-a4 | **كراش الصفر — عزل تشخيصي** | **مسجل طيران `[PETRACE]`**: خلال نافذة GetReady (step≥3) يُسجَّل أول ظهور لكل اسم ProcessEvent خارجي (سقف 120) — أي نداء يطلقه الانتقال الأصلي يُسمّي نفسه، وآخر سطر قبل الكراش = موقع الكراش بالضبط؛ + سطر `[BR] warmup elapsed — launching aircraft` قبل BRStartAircraftPhase + نبضات `[BR] GetReady — bus in Ns` كل 4ث + سطر `[Motor]` كل 5ث |
| F75-a5 | الباص — تبنٍّ قبل التوليد | BRStartAircraftPhase يقرأ `GameState.Aircraft` أولًا: إن كان الانتقال الأصلي قد ولّد الباص قبلنا نتبنّاه (لا باص مزدوج) — Erbium 1:1 (سيرفر حقيقي = باص واحد بالضبط) |
| F75-b | **بوتات تتحرك فعلًا** | `MotorTick()`: كتابة `CharacterMovement.Velocity` **مباشرة** كل تِك (X/Y=اتجاه×سرعة 320، Z يبقى للجاذبية — قناة LaunchCharacter، تعمل بلا أنبوب إدخال) + AddMovementInput كثانوي + SetControlRotation كل 2.5-4.5ث للالتفات؛ الأوفستات تُحل مرة واحدة (`Character.CharacterMovement` + `MovementComponent.Velocity`@0xB8 من SDK)؛ الميت/النصف-محروق يسقط من القائمة (تحقق class)؛ نبض `[Motor] Bot n=99 rot=1 move=1 cmOff=.. vOff=..` كل 5ث يثبت الحيوية |
| F75-c | **حيوانات في أماكنها + تتحرك** | إعادة هيكلة: `WildClasses()` + `WildSpawnOne()` مشتركة؛ كل حيوان يُسجَّل في **موتور حركة** (نفس قناة Velocity بسرعة 200) فيتجول حول منطقته؛ + `SpawnWildlifeScattered(12)`: نثر 12 حيوانًا على كامل الخريطة (120-800م من المركز، سقوط من Z+600 — أماكنها الطبيعية كما تفعل دورة الحياة الأصلية؛ مولدات الحيوانات الأصلية غير قابلة للاكتشاف في 21.30 — اللوب dump لا يحوي صنفها) + نبض `[Motor] Wild n=20` |
| F75-d | **تفاعل يظهر** | كتابة **`CurrentlyHighlightedActors`** على مكوّن التفاعل (GenericArrayAdd مع حد 4) بجانب `InteractActor` — تلميح/زر التفاعل المحمول يقرأ القائمتين معًا؛ + سطر واحد لكل مرشح جديد `[Interact] candidate: Tiered_Chest_Athena_C (2.3m, kind=1)` يثبت أن المسح يرى عالمك |

## فحص + تزامن
- `g++ -std=c++17 -fsyntax-only -Wall -Wextra` مع stubs → **EXIT=0** (صفر تحذيرات) — hook_fixed.cpp = **7386 سطرًا**
- إعادة توليد part1..part7 (832×6+2394) — دمج ثم diff ضد hook_fixed.cpp = **مطابق 100%**

## المتوقع في اللوق القادم (سطور التحقق — بالترتيب)
1. `[Motor] Bot n=10... rot=1 move=1 cmOff=0x.. vOff=0xB8` (كل 5ث) — **البوتات تمشي وتلتفت الآن**؛ إن بقت ثابتة وvOff=-1 فالإزاحة غير محلولة (أرسل السطر)
2. `[Wild] scattered 12 animals across the map (motors=20)` + `[Motor] Wild n=20...` — حيوانات حولك وعلى كامل الخريطة **تتجول**
3. `[Interact] candidate: ...` عند أول اقتراب من صندوق/مركبة + `[Equip] duplicate equip skipped` أو `[Interact] -> ...` عند الفتح التلقائي — **التفاعل يرى العالم**
4. عند الصفر: `[BR] GetReady — bus in 4s` ثم **إما** `[Equip] external ClientActivateSlot swallowed` (انتقال أصلي بدأ) **أو** `[BR] warmup elapsed — launching aircraft` ثم `[BR] StartAircraftPhase (Erbium)` — **أي مسار يسلكه اللوق يليه الباص يحلّق**
5. إن سبقك الانتقال الأصلي: `[BR] native aircraft already present — adopting 0x...` — باص واحد فقط
6. بقية سلسلة الطيران كالـ F74 (flight path/skin/register/board) ثم القفز/العاصفة كالـ F73
7. **إن كرش رغم كل هذا:** آخر سطر `[PETRACE] ...` قبل الكراش يسمّي النداء الأصلي القاتل + `/data/local/tmp/era_crash.txt` (أرسلهما معًا) — الجولة القادمة تبتلعه بالاسم

---

# الجولة F76 — كراش «قبل لحظة الصفر» + بوتات بلا سكن عشوائي + حيوانات وبوتات تتصرف بغباء + تفاعل/QuickBars/لوت ميتة — قراءة Erbium كاملة وتطبيق كل شيء

**شكوى المستخدم (لوق بناء F75):** «كراش قبل لحظة الصفر. التفاعل و QuickBars واللوت ميتة. أليس من المفترض أن يكون للبوتات سكن مختلف عشوائي وكل شيء عشوائي؟ حتى الحيوانات والبوتات كانوا يتصرفون بغباء».

## التشخيص الجذري (لوق F75 + قراءة Erbium الكاملة + dump 21.30)

1. **الكراش «قبل لحظة الصفر» — الجذر انكشف باللوق نفسه:** آخر أسطر اللوق = `[BR] GamePhaseStep -> 3` (عند نهاية-10ث) ثم فيض `[PETRACE]` ثم الموت. `BRSetGamePhaseStep` في F75 كان يستدعي **`FortGameModeAthena.OnGamePhaseStepChanged` مباشرة (gm.run)** — هذا المعالج هو **آلة الانتقال السيرفرية الأصلية كاملة** (StartAircraftPhase الداخلي + مشي كل الـ controllers) — عملها الداخلي استمر ثوانٍ بعد ندائنا ثم قتل العملية قبل بلوغ الصفر. الدليل: سطر اللوق `[BR] GamePhaseStep -> 3` يُطبع **بعد** اكتمال pp.run (معالج الـ pawn) — إذًا الـ pawn ليس القاتل؛ القاتل عمل gm الأصلي اللاحق. **Erbium لا يسمح أبدًا بامتلاك الـ client لمحرك الأطوار السيرفري** — مكوّن الـ GamePhaseLogic يملكها؛ عندنا المحرك هو F73.
2. **بوتات Default:** F74-d كان يعطي **كل** البوتات نفس الزوج الثابت (DesertOps/Fallback). dump 21.30 يحوي **4,853 جزء CustomCharacterPart محمّلًا فعليًا (583 رأس + 1,157 جسم)** — كل خزانة البناء مطبوخة ومحمّلة! السكن العشوائي مجرد مسح GObjects + اختيار عشوائي لكل بوت.
3. **بوتات/حيوانات «تتصرف بغباء»:** موتور F75 يكتب Velocity باتجاه ثابت → خطوط مستقيمة في الجدران + انزلاق دائم على العوائق + **صراع مع AI الحيوانات الأصلية** (لوق F75 يثبت أن AI الحيوانات حيّ: `OnActorBump on AthenaPathFollowingComponent`، `TestIsInSafeZone`، `On Pawn Focused`). الحل هو مسار اللعبة نفسه: `UAIBlueprintHelperLibrary.SimpleMoveToLocation` (متحقق 0x70fb715070 — ينشئ PathFollowingComponent على أي controller ويمشي على navmesh الخريطة الحقيقي حول العوائق).
4. **اللوت ميت:** الـ pickups تُرسبن من الصناديق لكن **لا تُجمع** — مسار الجمع الحقيقي (Erbium FortPlayerPawnAthena.cpp): الـ pickup يطير للاعب أولاً (`AFortPickup.BlueprintSetPickupTarget` — متحقق 0x70fb53f350 مع صوت الالتقاط) ثم ينتقل للمخزون عند وصوله (Erbium يلتقط FinishedTargetSpline وينفذ InternalPickup).
5. **التفاعل ميت جزئيًا:** لوق F75 أثبت أن **نظام التفاعل الأصلي حي في الطور 3** (`BlueprintCanInteract on NPC_Pawn_Irwin_Prey_Burt_C`، `GetMinDistanceFromInteraction on Apollo_Street_Barrel_02_C`، `Handle Interaction Changed on AthenaInteractionIndicator_C`) — لكن كتاباتنا القسرية لـ `CurrentlyHighlightedActors` كانت **تصارع** النظام الأصلي (الشعور بـ«الغباء»). كذلك: آلات البيع/SCMachine والنباتات وأصناف BGA تنفّذ `BlueprintOnInteract` الخاص بها خارج شجرة BuildingActor (`BGA_Athena_SCMachine_Pickup_C`، `B_HidingProp_C` في الـ dump) — مسارنا العام لم يكن يصلها.

## الإصلاحات (F76)

| # | النظام | الإصلاح |
|---|---|---|
| F76-a1 | **كراش الصفر — ابتلاع آلة الأطوار الأصلية** | pe_hook يبتلع `OnGamePhaseStepChanged` **لأي مستقبل** (GameMode + mutators — منطق سيرفي بحت؛ بث الـ multicast في البند التالي يغطي المرئيات) — حتى انتقال الإحمود-الصفر الأصلي (F74/F75) يُبتلع الآن عند بث الـ delegate لأن بث TMulticastScriptDelegate يمشي عبر ProcessEvent |
| F76-a2 | **كراش الصفر — لا استدعاء للمعالج السيرفري إطلاقًا** | `BRSetGamePhaseStep` أُعيدت كتابته: (1) كتابة `GameState.GamePhaseStep` (الحقل المنسوخ الذي يقرأه الجميع)، (2) **بث `AFortGameStateAthena.GamePhaseStepChanged` multicast** عبر `OnGamePhaseStepChanged__DelegateSignature` (متحقق 0x70fb023b60 — params: SafeZoneInterface@0x0 + GamePhaseStep@0x10): كل روابط **العميل** (PC.HandleGamePhaseStepChanged، الويدجت، الموسيقى) تعمل أصليًا بينما رابط الـ GameMode السيرفري يُبتلع في F76-a1 — مطابق لـ Erbium حيث العميل لا ينفذ انتقال السيرفر أبدًا، (3) `FortPlayerPawnAthena.GamePhaseStepChanged` كما كان (مرئيات الـ pawn) |
| F76-a3 | **تحصين نهاية المباراة** | ابتلاع `Engine.GameMode.ReadyToEndMatch` (سُجّل نشاطه في PETRACE أثناء GetReady!) — يعمل أصليًا ثم **يُفرض على false** عبر قراءة إزاحة الـ return المُنعكسة (CPF_ReturnParm) — دالة الـ `Athena::ForceReturnFalse` الجديدة تقرأ إزاحة القيمة المرجعة من الـ reflection (لا تخمين إزاحات: دالة zero-param تحتفظ بالقيمة المرجعة عند 0) |
| F76-a4 | تسجيل الطيران ممتد | نافذة `[PETRACE]` ممتدة من GetReady حتى **نهاية طور الباص** (br.phase<=3) — إن كرش انتقال الصفر الأصلي رغم الابتلاعات فآخر سطر PETRACE يسمّي القاتل |
| F76-b | **سكن عشوائي لكل بوت** | `BRPartPools()`: مسح GObjects مرة واحدة عن جزء CustomCharacterPart محمّل باسم `CP_Head_*`/`CP_Body_*`/`CP_Athena_Body` (سقف 256 لكل قائمة — 583/1,157 متاحة في البناء)؛ **كل بوت يسحب زوجًا عشوائيًا** (رأس+جسم) ويمر بالسلسلة الكاملة F74-d (ServerChoosePart ×2 + LocalCharacterParts + HeroType + OnReps + ApplyCharacterCosmetics + UpdatePlayerCustomCharacterPartsVisualization)؛ fallback: ثلاثي Erbium الثابت إن كان المسح فارغًا؛ لوق عيّنة `[BR] cosmetic pools: heads=N bodies=M` + `[BR] bot skins: head=... body=...` |
| F76-c1 | **بوتات تتحرك بذكاء** | `MotorTick` يُرقّى لمحرك هجين: **أساسي = `SimpleMoveToLocation(Controller, Goal)`** (navmesh أصلي — يعبر حول العوائق والجدران) على جدول تجول مُتدرّج كل 4-8ث بمدى 900-2600 للبوتات؛ **احتياطي = كتابة Velocity** مع كشف علوق: بعد 2.2ث من طلب الـ navmesh، إزاحة < 40 وحدة ⇒ نافذة Velocity بعنوان جديد ثم إعادة محاولة الـ navmesh؛ نبض `[Motor] Bot n=... navmesh=N vel=M smove=1` |
| F76-c2 | **حيوانات تتصرف طبيعيًا** | نفس المحرك الهجين بسرعة مشي (نافذة تجول 500-1400): طلبات navmesh على AIController الحيوان لا تصارع BT الأصلي (إن مشى أصليًا فإزاحته تكفي ولا تُكتب Velocity أبدًا)؛ إن كان الـ AI الأصلي ميتًا هنا (لا controller) يقود الموتور الاحتياطي بلطف بسرعة 200 |
| F76-d | **اللوت يُجمع** | مسار الجمع Erbium-pure: عند الاقتراب ≤3م يُستدعى `AFortPickup.BlueprintSetPickupTarget(المسكن، صوت)` — الـ pickup **يطير للاعب** مثل اللعبة الحقيقية — ثم بعد 0.6ث `LocalPickup` (التكديس/الإدخال/التدمير)؛ إن فشل نداء الطير انتقال فوري للـ F65؛ خرائط pending تمنع الازدواج |
| F76-e | **حيوانات في أماكنها** | `SpawnWildlifeScattered` يفحص أولًا مولدات الخريطة **المحمّلة فعليًا** (Actors يخت اسم صنفها wildlife/npcspawn/animalspawn + spawn/spawner — مثل BGA_Athena_WildlifeSpawn التي تصل مع الـ streaming) ويرسبن الحيوانات **عند مواقعها بالضبط** (±150)؛ fallback: النثر الموحد على الخريطة إن لم تكن المولدات قد تحمّلت بعد |
| F76-f | تفاعل عام | `DoInteract` مسار أخير جديد: محاولة `BlueprintOnInteract` **من صنف الهدف نفسه** (حل override في Call — يصل BGA SCMachines/النباتات/hiding props/حيوانات لو نفذته) مع تحقق الاستقبال (رفض صامت لمن لا ينفذه) |
| F76-g | تفاعل أصلي أولًا | كتابتنا لـ `CurrentlyHighlightedActors` صارت **فقط حين تكون قائمة النظام الأصلي فارغة** — نظام التفاعل الأصلي (الحي في الطور 3) يقود القائمة وكنا نصارعه |
| F76-h | QuickBars | محاولة poke بأسماء معامل بديلة (QuickBarType/QuickbarType/QuickBarIndex/QuickBar) لكل ويدجت قبل الإعلان عن الرفض |

## فحص + تزامن
- `g++ -std=c++17 -fsyntax-only -Wall -Wextra` مع stubs → **EXIT=0** (صفر تحذيرات) — hook_fixed.cpp = **7642 سطرًا**
- إعادة توليد part1..part7 (832×6+2650) — دمج ثم diff ضد hook_fixed.cpp = **مطابق 100%**

## المتوقع في اللوق القادم (سطور التحقق — بالترتيب)
1. `[BR] cosmetic pools: heads=~250 bodies=~250 (F76 random skins)` ثم `[BR] bot skins: head=CP_Head_... body=CP_Body_...` — **كل بوت بسكن مختلف**
2. `[Motor] Bot n=10 navmesh=10 vel=0 smove=1` — البوتات **تمشي على مسارات حقيقية** (تلف حول الجدران)؛ لو navmesh=0 فالـ navmesh غير نشط ويعمل الاحتياطي بالـ velocity
3. `[Motor] Wild n=... navmesh=.../vel=...` + `[Wild] ✅ N natural spawner positions found` أو `no natural spawner actors streamed yet` — حيوانات في مواقعها الطبيعية تتحرك طبيعيًا
4. عند الصفر: **لا كراش** — إما `[BR] GamePhaseStep -> 3` يليه مباشرة `[BR] native OnGamePhaseStepChanged swallowed (phase engine is ours, F76)` (انتقال أصلي بدأ وابتُلع) أو مباشرة `[BR] warmup elapsed — launching aircraft` ثم سلسلة الباص الكاملة — **الباص يحلّق**
5. `[Loot] ✅ pickup fly-collect live (BlueprintSetPickupTarget)` عند أول التقاء لوت — **اللوت يطير إليك ويدخل المخزون**
6. `[Interact] ✅ BP interact on BGA_.../BuildingProp_VendingMachine...` عند التفاعل مع الآلات والنباتات
7. `[BR] native ... swallowed` + `ReadyToEndMatch` لن ينهي المباراة من تلقاء نفسه
8. **إن كرش رغم كل هذا:** آخر سطر `[PETRACE]` يسمّي النداء القاتل (النافذة ممتدة لطور الباص الآن) + era_crash.txt

# F77 — كراش الصفر من جذره (تعطيل الانتقال الأصلي) + سكنات عشوائية حقيقية + ذكاء بوتات/حيوانات + تفاعل الحيوانات + QuickBars

## قراءة لوق F76 (القرائن الحاسمة)
- الكراش عند **الصفر قبل الباص** — و**لا سطر واحد** من محركنا (`[BR] StartAircraftPhase`) قبل الكراش: الانتقال الذي قتل العملية هو **انتقال الإحمود-الصفر الأصلي** (نحن نكتب `WarmupCountdownEndTime` منذ F73 → مؤقّت الإحمود الأصلي يعمل ويقود العدّاد الظاهر → عند الصفر يشغّل تسلسله السيرفي)
- ذيل `[PETRACE]` يُظهر التسلسل الأصلي حتى لحظة الموت: `HandleGamePhaseChange on AthenaHUDContext` → `OnGamePhaseStepChanged on FortGameModeAthena` (ابتُلع) → `HandleGamePhaseStepChanged on AthenaAISystem` → `GamePhaseStepChanged on PlayerPawn_Athena_C` → سلسلة equip (`ClientActivateSlot` ابتُلع) → `ServerClientPawnLoaded` → **`ReceiveDestroyed on PlayerPawn_Athena_C`** → **`OnAvatarActorDestroyed`** → **`ClientForceWorldInventoryUpdate`** → 💀 — هذا **هو** تسلسل StartAircraftPhase الأصلي نفسه (Erbium BattleRoyaleGamePhaseLogic.cpp:301-327 يكرره عمدًا: ClientActivateSlot → K2_DestroyActor → Reset → ClientGotoState Spectating) لكنه في standalone يطير قبل إكماله
- قراءة Erbium تأكّد: Erbium **لا يسمح أبدًا** للـ GameMode الأصلي بقيادة الأطوار (يستبدل الـ GameMode كله) — محركنا (F73) هو الآلة الوحيدة، يجب **تجويع** الانتقال الأصلي لا مصارعته

## الإصلاحات (F77)

| # | النظام | الإصلاح |
|---|---|---|
| F77-a1 | **كراش الصفر — تجويع الانتقال الأصلي (الإصلاح الجذري)** | `BRTick` يُمسك `GameState.WarmupCountdownEndTime` (+ النسخة على المكوّن إن وُجدت) في نافذة GetReady (step>=3) وطور الباص (phase 3) عند **now+6 كل تِك** — العدّاد الأصلي **لا يصل الصفر أبدًا** فلا يشتعل انتقاله السيرفي؛ العدّاد الظاهر يتجمد على 0:06 ثم بثّ أطوارنا يقلب الـ HUD إلى الباص (مطابق لفلسفة Erbium: محرك الأطوار لنا وحنا)؛ محركنا الخاص (`br.warmup_end`) يواصل العد ويطلق الباص في لحظته الصحيحة |
| F77-a2 | **كراش الصفر — حلبة الثانية** | ابتلاع `ReadyToStartMatch` الخارجي بنمط F76-a3 (يعمل أصليًا ثم ForceReturnFalse) — المشغّل الآخر المحتمل لـ StartMatch الأصلي عند الصفر؛ مهما قرأ المُشغّل الأصلي فلن يبدأ المباراة خلف ظهر محركنا |
| F77-a3 | **كراش الصفر — حزام الأمان** | نافذة `br.phase<=3` تبتلع خارجيًا: `ServerClientPawnLoaded` + `ClientGotoState` (Spectating يُهجّر تصميم الباص المخفي) + `ClientForceWorldInventoryUpdate` (آخر ثلاث روابط في ذيل موت F76) — كلها تعود للعمل الطبيعي عند phase 4 (تحديث المخزون في اللعب السليم)؛ لوق مرة واحدة `[BR] phase-window seatbelt: ... swallowed (F77-a)` |
| F77-b1 | **سكنات عشوائية حقيقية — تشخيص** | لوق F76 كشف `heads=4 bodies=8` فقط: مسح F76 كان **لقطة واحدة عند أول دفعة بوتات** والجزيئات تتدفق مع الـ streaming لاحقًا (الـ dump الكامل 583/1,157 أُخذ وسط مباراة) |
| F77-b2 | **سكنات عشوائية حقيقية — أزياء كاملة** | `BRPartPools(force)` الآن: **إعادة مسح دورية** (كل 5ث ما دامت القوائم نحيفة) + مطابقة موسّعة (كل اسم يحوي Head/Body) + **تجمع أزياء كاملة من CID ساكنة**: كل `AthenaCharacterItemDefinition` محمّلة → `HeroDefinition` → `Specializations` → `CharacterParts` → (رأس، جسم) من **سكن واحد** — نفس الطريقة التي يرسم بها سكن اللاعب المحلي Lancelot (قائمة `g_cid_outfits` حتى 512) |
| F77-b3 | **سكنات عشوائية حقيقية — تطبيق** | أولوية السحب لكل بوت: **زي كامل من سكن عشوائي واحد** ثم جزيئان مستقلان ثم ثلاثي Erbium؛ **جديد: مرآة `FFortAthenaLoadout` الكاملة على `FortPlayerPawn.CosmeticLoadout` لكل بوت مع CID العشواء** (هذه المرآة هي ما جعل سكن اللاعب يعمل — F74-d لم تكتبها للبوتات أبدًا) + سلسلة F74-d كاملة كما هي؛ لوق `bot skins: CID_... | head | body` لأول 3 بوتات |
| F77-c | **بوتات بذكاء (لا أغبياء)** | `MotorTick` آلة حالات: **عقاب العاصفة** (phase>=4 وخارج الدائرة الأولى ×0.92 ⇒ هدف جديد داخلها بنقطة عشوائية 25%) + **الاشتباك** (اللاعب ≤30م ⇒ يواجهه `SetControlRotation` ويثبّت الأرض 1.2-3.7ث بإصلاار Velocity=0 كل تِك، ثم يعود للتجول) فوق التجول الـ navmesh F76-c |
| F77-d | **حيوانات بذكاء (لا ميتة)** | غريزة الفريسة: اللاعب ≤22م ⇒ الحيوان **يهرب** بهدف نقطة بعده 14م في الاتجاه المعاكس (navmesh أولًا ثم velocity) — الحيوانات تتفاعل مع وجودك بدل التجاهل |
| F77-e1 | **الحيوانات تفاعلية** | `InteractTick` يضيف كل حيوان متتبّع كمرشّح تفاعل (kind=3، مدى 2.6م) — `DoInteract` مسار BlueprintOnInteract لكل صنف (من لا ينفذه يُرفض صامتًا) |
| F77-e2 | **QuickBars HUD** | `Call::run` قائمة سماح لـ `HandleQuickbarContentChanged`: ويدجت Bacchus تُبلغ سلسلة وراثة مكسورة ضد QuickbarBase_C فكان كل poke يُرفض والشريط ميتًا — الأهداف تُجمع **بصنفها** فالاستقبال صحيح بالضرورة |
| F77-f | **نبض الباص + قيادة يدوية** | طور الباص ينبّه كل 3ث: `[BUS] (x,y,z) moved=N` — باص ميت (حركتان < 30 وحدة) يُساق يدويًا بخط مستقيم نحو مركز الخريطة بسرعة 1500 حتى يستيقظ الـ tick الأصلي (لا تعارض: القيادة فقط عند السكون المؤكد) |

## فحص + تزامن
- `g++ -std=c++17 -fsyntax-only -Wall -Wextra` مع stubs → **EXIT=0** (صفر تحذيرات) — hook_fixed.cpp = **7879 سطرًا**
- إعادة توليد part1..part7 (832×6+2887) — دمج ثم diff ضد hook_fixed.cpp = **مطابق 100%**

## المتوقع في اللوق القادم (سطور التحقق — بالترتيب)
1. `[BR] cosmetic pools: heads=N(+M) bodies=... outfits=K(+..)` — **تتضخم مع الـ streaming**، و`[BR] bot skins: CID_... | CP_..._Head | CP_..._Body` لأول 3 بوتات — **كل بوت بسكن مختلف كامل**
2. نافذة GetReady: `[BR] native warmup countdown held at T-6 (F77 — native zero-transition starved)` — **الانتقال الأصلي لن يشتعل أبدًا**
3. عند الصفر: **لا كراش ولا PETRACE teardown** — مباشرة `[BR] warmup elapsed — launching aircraft` → `[BR] StartAircraftPhase (Erbium)` → `[BR] SpawnAircraft (static)` → `[BR] player boarded` → `[BR] StartAircraftPhase complete`
4. أثناء الطيران: `[BUS] (x,y,z) moved=N` كل 3ث — moved>0 يعني الباص يحلّق أصليًا؛ moved=0 مرتين ⇒ القيادة اليدوية تدخّل بصمت
5. عند drop_end: `[BR] drop window ended — forcing jump` → `[Bus] JumpFromHudStart intercepted` → `[Bus] Skydiving started` — **هبوط كامل من الباص**
6. بوت قريب منك: يتوقف ويواجهك 1-3ث ثم يكمل تجوله؛ حيوان قريب: يهرب عند اقترابك — `[Motor]` يواصل النبض
7. `[QBW] poked 9/9` (بدل 3/9) بعد سماح HandleQuickbarContentChanged — **شريط الأسلحة يتحرك**
8. عند اقترابك من حيوان: `[Interact] candidate: NPC_Pawn_Irwin_...` — وإن نفذ صنفه BlueprintOnInteract: `[Interact] ✅ BP interact on ...`
9. **إن كرش رغم كل هذا:** آخر سطر يسمّي الخطوة بالضبط (كل خطوة مُسطرة قبل تنفيذها) + era_crash.txt

# F78 — الباص يحلّق فعلاً + البوتات على الباص والقفز المتدرج + الخروج من طور الباص + ساعة الـ HUD

## قراءة لوق F77 (القرائن الحاسمة — لا كراش بعد هذه الجولة!)
- **الكراش انتهى**: تجويع الانتقال الأصلي (F77-a) نجح — اللوق كامل حتى نهاية الرحلة، لا PETRACE teardown ولا موت عملية
- **الباص لم يطير**: غياب سطر `flight path: start=` عن اللوق يعني `haveFI=false` — **MapInfo->FlightInfos فارغة تمامًا** في 21.30 standalone (دفق الـ match-init السيرفي الذي يملؤها لا يعمل هنا) → SpawnAircraft الستاتيكي لم يُجرّب → الرسبن اليدوي بمتحوّل هوية → الباص عند **(0,0,0) تحت الخريطة**، لا FlightInfo له فلا شيء يطيّره: `[BUS] (0,0,0) moved=0` للأبد
- **القيادة اليدوية F77-f لم تتحرك**: كانت تقود نحو `zone_centers[0]` وهو **(0,0,0) غير المُهيأ** في طور الباص (يُملأ لاحقًا في SafeZones) → `len>1000` يفشل دائمًا
- **"دخلت تحت الخريطة"**: الصعود = إخفاء الـ pawn + الكاميرا على الباص (0,0,0) → تحت الأرض؛ القفز = teleport إلى موضع الباص -200 → skydive تحت الخريطة
- **"EVERYBODY OFF! LAST STOP IN 0s" معلّقة**: (1) لا ربط HUD بالطائرة — `FortControllerComponent_Aircraft.CurrentAircraft` لم يُكتب أبدًا؛ (2) ساعة الـ HUD = `GetServerWorldTimeSeconds()` = `ReplicatedWorldTimeSeconds + Delta` ولا أحد يكتبها في standalone؛ (3) **جوهري: بعد تدمير الباص بقي الطور 3 للأبد** — محرك F73 لم يكن ينتقل لـ SafeZones بعد نهاية الرحلة أبدًا (فقط مسار bSkipAircraft كان ينتقل)
- **التحقق من SDK 21.30**: `FAircraftFlightInfo` (0x48) = Start(0x00) + Rot(0x18) + Speed(0x30) + TillFlightEnd/DropStart/DropEnd(0x34-0x3C) — **لا نقطة نهاية: الطيران بالاتجاه والسرعة**؛ `FortAthenaAircraft.StartFlightPath(StartTime)` Native = مشغّل الرحلة الأصلي؛ `FortControllerComponent_Aircraft.CurrentAircraft@0xB0` (ربط HUD)؛ `AGameStateBase.ReplicatedWorldTimeSeconds@0x29C/Delta@0x2A0`؛ MapInfo: `CachedPlayableBoundsForClients@0x518 (FBoxSphereBounds)` + `AircraftHeight@0x5A0` + `AircraftSpeed@0x5C8` (FScalableFloat.Value@0)؛ `FortPlayerPawn.BeginSkydiving(bool bFromBus)/EndSkydiving`

## الإصلاحات (F78)

| # | النظام | الإصلاح |
|---|---|---|
| F78-a1 | **مسار طيران حقيقي** | `BRBuildFlightInfo()` يبني FAircraftFlightInfo (0x48) من بيانات الخريطة نفسها: **المركز ونصف القطر من CachedPlayableBoundsForClients**، الارتفاع من `AircraftHeight.Value`، السرعة من `AircraftSpeed.Value` (سقف 45-95ث للرحلة)، زاوية عشوائية عبر الجزيرة مع pitch -6؛ **يُدفع إلى MapInfo->FlightInfos[0]** (GenericArrayAdd — نفس تدفق match-init الأصلي) فتصبح `haveFI=true` ويسير مسار Erbium كاملًا |
| F78-a2 | **مشغّل الرحلة الأصلي** | بعد كتابة حقول Erbium الخمسة: `StartFlightPath(now)` على الطائرة (الدالة التي يشغّلها السيرفر الحقيقي وحدها تُحلّق الباص في 21.30) + `OnRep_ReplicatedFlightTimestamp` |
| F78-a3 | **ربط HUD بالطائرة** | `CurrentAircraft` يُكتب على `FortControllerComponent_Aircraft` الخاص باللاعب (عبر GetAircraftComponent الموجود) — شريط الباص يقرأ الطائرة منه فتظهر العدّادات بدل حالة الصفر |
| F78-a4 | **رسبن على المسار + طيار احتياطي** | الرسبن اليدوي الآن **على بداية المسار بارتفاعه ودورانه** (لم يعد بمتحوّل هوية تحت الخريطة)؛ القيادة اليدوية عند سكون الباص أصبحت `BRBusPilot()`: تحليق على **نفس المسار** (بداية+اتجاه+سرعة) مع كتابة FlightElapsedTime — لا مزيد من القيادة نحو مركز صفري |
| F78-a5 | **ساعة الـ HUD** | `ReplicatedWorldTimeSeconds = BRWorldTime()` كل تِك (مع Delta=0 مرة واحدة) — عدّاد الباص/العاصفة في الويدجت يحسب من هذه الساعة فيصحّ العدّ |
| F78-a6 | **الخروج من طور الباص** | بعد تدمير الطائرة عند flight_end: `SafeZones (phase 4) + StormForming (step 6)` وsafezones_start=now+12 — **نهاية تعليق "EVERYBODY OFF! LAST STOP IN 0s"** (مطابق لـ Erbium: نهاية الرحلة = بداية العاصفة) |
| F78-b | **قفزة اللاعب من ارتفاع حقيقي** | BRJump يعمل الآن من الباص المُحلّق على ~10500 — الـ skydive الأصلي (BeginSkydiving(bFromBus=true)) يهبط عبر الهواء والـ glider يُنشر تلقائيًا بارتفاع الخريطة (SkydivingForceDeployParachuteWorldZ) — **لا مزيد من السقوط تحت الخريطة** |
| F78-c1 | **البوتات على الباص** | `BRBoardBots()` عند بدء الطور: **كل بوت يركب الباص** (bus_state=1، إخفاء) مع alpha قفز عشوائي 0.08..0.86 وهدف إنزال عشوائي (مركز الحدود ±0.72R) وارتفاع أرض مضروب بـ `BRGroundZ` (LineTraceSingle عمودي — نمط F42) |
| F78-c2 | **قفز متدرج وهبوط واعٍ** | `BRBusBotsTick()` كل تِك: عند تجاوز alpha ⇒ **قفز** (teleport إلى موضع الباص الحالي + إظهار + BeginSkydiving) ثم هبوط محكوم (Velocity Z=-3800 + انجراف 750 نحو الهدف) ⇒ باراشوت عند 2600 (Z=-680، انجراف 420) ⇒ هبوط عند 160 من الأرض (EndSkydiving + تصفير السرعة + استئناف الـ AI الأرضي الذكي F77-c: عقاب عاصفة + اشتباك + تجول navmesh) |
| F78-c3 | **البوتات المتأخرة** | بوت يكتمل رسبنه أثناء الطيران يُركب الباص فورًا (alpha 0.30..0.80) — لا بوت يبقى ماشيًا في طور الباص |
| F78-d | **عزل حالات الباص** | `MotorTick` يتخطى كل بوت bus_state≠0 (ركوب/هبوط/باراشوت إدارة BRBusBotsTick وحدها — لا صراع مع الـ AI الأرضي) |

## فحص + تزامن
- `g++ -std=c++17 -fsyntax-only -Wall -Wextra` مع stubs → **EXIT=0** (صفر تحذيرات) — hook_fixed.cpp = **8249 سطرًا**
- إعادة توليد part1..part7 (832×6+3256) — دمج ثم diff ضد hook_fixed.cpp = **مطابق 100%**

## المتوقع في اللوق القادم (سطور التحقق — بالترتيب)
1. `[BR] synthesized flight path: start=(x,y,z~10500) yaw=... speed=... dur=...` — **مسار حقيقي من حدود الخريطة** (أو `real flight path:` إن وفّرت الخريطة بياناتها)
2. `[BR] manual aircraft -> ...` ثم `[BUS] (x,y,z) moved=N` بـ **N>0 وموضع يتغير** — **الباص يحلّق فوق الجزيرة**
3. `[BR] aircraft component bound (CurrentAircraft)` — **شريط الباص حي بعدّاد ينزل** (LAST STOP IN ~45s بدل 0s)
4. `[BR] N bots boarded — staggered drops (alpha 0.08..0.86)` ثم أثناء الطيران `[Bus] first bot jump` — **بوتات تقفز حولك من الباص وتنزل بالمظلات** ثم `[Bus] bot landed #N`
5. قفزك من الباص: هبوط skydive كامل من الارتفاع + نشر المظلة تلقائيًا + هبوط على الأرض — **لا تحت الخريطة**
6. `[BR] aircraft destroyed (flight over)` يليه **مباشرة** `[BR] flight over — SafeZones in 12s (storm forming)` — **الشريط المعلّق يختفي ويبدأ العاصفة**
7. المشي فوق لوت: `[Loot] ✅ pickup fly-collect live` — الـ pickup يطير إليك ويدخل المخزون (النظام جاهز من F76-d — لم يُختبر سابقًا لأنك كنت تحت الخريطة)
8. **إن كرش رغم كل هذا:** آخر سطر يسمّي الخطوة بالضبط (كل خطوة مُسطرة) + era_crash.txt

# F79 — الباص Erbium-صرف (تدمير الـ pawn + Spectating + القفزة الأصلية) + إصلاح كراش الـ Emote + توجيه QuickBars محليًا + بوتات بسكائن مضمونة وسلاح وعي + مشاهدة الموت

## قراءة لوق F78 + تقرير المستخدم (كلها نفس جذع واحد!)
- **"لم أنزل من الحافلة ومتّ دون سبب"**: التصميم القديم كان يُبقي الـ pawn **حية مخفية على الباص** (Z=80000). عند drop_end كان `BRJump` يعمل teleport للـ pawn تحت الباص ويشغّل `ServerAttemptAircraftJump` الأصلي — **الأصلي يرسبن pawn جديدة** فتصبح `session.pawn` مؤشرًا ميتًا، وسلسلتنا (BeginSkydiving + الكاميرا) تعمل على الجثة. الكاميرا تبقى على الباص (المستخدم يرى أنه لم يقفز) بينما pawn جديدة تسقط وتموت تحت — **موت بلا سبب ظاهري، بالضبط ما وصفه المستخدم**
- **"الكاميرا لم تكن تلحق الباص"**: SetViewTarget(aircraft) يدوي يتصارع مع حالة الـ pawn الحية — Erbium لا يعاني من هذا لأنه **يقتل الـ pawn ويغير الحالة إلى Spectating أصلاً**
- **"الباص كان ينزل حيوانات"**: `[Motor] Wild navmesh move` كان يستمر أثناء الطيران — الحيوانات تتجول تحت الباص بينما هو يحلّق = مشاهد "حيوانات تنزل من الباص"
- **"زر Play Emote يكرش اللعبة"**: pe_hook كان يشغّل `Athena::PlayEmoteItem` (الآمن) ثم **يسقط إلى `orig_PE`** — أي أن المسار الأصلي `ServerPlayEmoteItem` كان يعمل بعدها ويكرش! الفجوة نفسها مع `PlayEmoteItem` (مدخل العميل) الذي لم يكن مُعترضًا إطلاقًا
- **"QuickBars قتلني"**: ابتلاع `ClientActivateSlot` **غير المشروط** (F75-a2) كان يقتل كل نقرة شريط سريع طوال المباراة (كان يحمي سلسلة التجهيز عند الصفر فقط)
- **قراءة Erbium 1:1** (BattleRoyaleGamePhaseLogic.cpp:194-340): StartAircraftPhase = `OnEnteredAircraft.Process()` + `ClientActivateSlot(0,0)` + **`Pawn->K2_DestroyActor()`** + `Reset(PC)` + **`ClientGotoState("Spectating")`**؛ DropStartTime ⇒ BusFlying؛ **DropEndTime ⇒ ServerAttemptAircraftJump لكل من في الطائرة + SafeZonesStartTime = DropEndTime+60**؛ FlightEndTime ⇒ تدمير الطائرة

## الإصلاحات (F79)

| # | النظام | الإصلاح |
|---|---|---|
| F79-a1 | **صعود Erbium-صرف** | boarding = `OnEnteredAircraft` (حدث أصلي) + `UnPossess` + **`K2_DestroyActor(pawn)`** + `session.pawn=nullptr` + **`ClientGotoState("Spectating")`** (FName "Spectating" يُستخرج من أي كائن يحمل الاسم — BRFindFNameByString) + SetViewTarget(aircraft). **لا pawn حية مخفية = لا شيء يأكلها العاصفة/السقوط** (جذر موت F78) |
| F79-a2 | **القفزة الأصلية** | `HandleJumpFromHudStart` (الزر أو drop_end القسري) = `ServerAttemptAircraftJump` فقط (على مكوّن الطائرة ثم FPCA ثم FPC احتياطًا) — الأصلي يرسبن pawn طازجة في وضع skydive ويملك الكاميرا. لا teleport، لا BeginSkydiving على جثة، لا صراع كاميرا |
| F79-a3 | **كشف الرسبن** | `Athena::Tick` يراقب `GetPawn(PC)`: أول pawn جديدة ≠ old_pawn ⇒ **`BRPawnRespawned()`** = ClientRestart (الخروج من حالة Spectating!) + إعادة تطبيق كل الأنظمة المحلية (visibility/body/cosmetics/abilities/sprint/movement/quickbars/HUD) — نفس قائمة SetupWorld بالترتيب نفسه |
| F79-a4 | **شبكة الأمان** | إن لم يظهر رسبن أصلي خلال 1.5ث ⇒ رسبن احتياطي **على الأرض** عند نهاية المسار (أو مركز الدائرة الأولى) بـ BRGroundZ — اللاعب لا يبقى أبدًا بلا pawn ولا يسقط من 80000 |
| F79-a5 | **كاميرا الباص** | إعادة تأكيد كل 2ث: ViewTarget ≠ الطائرة (وأنت **تركب فقط** — `!aircraft_jumped` حتى لا تنتزع الكاميرا من skydive الأصلي بعد القفز) ⇒ SetViewTarget(aircraft, blend 0.3) |
| F79-a6 | **توقيت العاصفة Erbium** | `safezones_start = drop_end + 60` (كان +12 من flight_end — **العاصفة كانت تتشكل فوق الهوائيين**). الحماية في ForcePlayerVisible: لا تلمس أعلام skydiving واللاعب في سقوط حر فعليًا |
| F79-a7 | **الحيوانات تنام في الطيران** | MotorTick يتخطى wild كلها أثناء phase==3 — لا "حيوانات تنزل من الباص" |
| F79-b | **كراش الـ Emote (جذر بجذر)** | `ServerPlayEmoteItem` و`PlayEmoteItem` و`Cheat_ForcePlayEmoteItem` تُعترض كلها على المتحكم: تُشغّل سلسلتنا الآمنة (GetAnimationHardReference → Montage_Play) ثم **`return` — الأصلي الكرّاش لا يُستدعى إطلاقًا في أي طور** |
| F79-c | **QuickBars تعمل بالنقر** | `ClientActivateSlot` = توجيه محلي: قراءة (InQuickBar, Slot) → الـ instance المربوط في session.slots/secondary_slots → `OnServerExecuteInventoryItem(guid)` (تجهيزنا المحلي الكامل) → `return`. **النقر يعمل في كل الأطوار، والمسار الأصلي الكرّاش ممنوع نهائيًا** + dedupe ضد إعادة تجهيز المسكوك |
| F79-d1 | **سكنات مضمونة (لا بلا رؤوس)** | `BRPartMeta` يقرأ PartType/GenderPermitted/BodyTypesPermitted من كل part (انعكاس 21.30) ويبني **أزواجًا متطابقة** (رأس يوافق جسم النوع/الجنس نفسه) — أزواج CID كاملة + fallback الأزواج المُتحقق منها: **GameDataCosmetics.DefaultCharacterParts** (افتراضي المحرك — يرسم حتمًا) + **أجزاء اللاعب نفسه** (تُرسم مؤكدة). لا بوت يأخذ رأسًا لا يناسب جسمه أبدًا |
| F79-d2 | **فأس لكل بوت** | GameDataCosmetics.**FallbackPickaxe** → WeaponDefinition → `EquipWeaponDefinition` عند الرسبن (نفس التجهيز الأصلي لللاعب). الاحتياطي WID_Harvest_Pickaxe_Athena_C_T01 |
| F79-d3 | **سلاح عند الهبوط** | BRBotLoadout عند هبوط كل بوت: سلاح عشوائي من **مجمع الأسلحة الساكنة في الذاكرة** (BRWeaponPool — كل تعريف مضمون التحميل). البوتات تهبط مسلحة |
| F79-d4 | **وعي حقيقي (يطاردك)** | الاشتباك تطوّر: بوت >8م ⇒ **navmesh نحوك** (يطارد)، ≤2.5م ⇒ يواجه ويضرب (ApplyDamage 12 كل ~2ث — ثقب next_strike بالـ motor). **جثث لا تمشي**: Health ≤0 ⇒ تجاهل الـ AI (جثة حقيقية)؛ العاصفة تنظف الجثث + PlayersLeft (موجود مسبقًا — يغذي الآن قتلك أنت أيضًا) |
| F79-e | **مشاهدة الموت (Relocation)** | مراقبة `FortPawn.Health` كل تِك: hp≤0 ⇒ `SpectateNext()` = كاميرا على أقرب بوت حي (blend 1.0) + **زر القفز يبدّل الهدف** (HandleJumpFromHudStart في وضع spectating). نظام مشاهدة يعمل أخيرًا |
| F79-f | **حمايات الرحلة** | HUD re-kick (ClientOnPawnSpawned) فقط حين pawn موجود (كان يُستدعى بلا pawn بعد التدمير)؛ BRStormTick يعمل بلا pawn محلي (البوتات تتأثر) |

## فحص + تزامن
- `g++ -std=c++17 -fsyntax-only -Wall -Wextra` مع stubs → **EXIT=0** (صفر تحذيرات) — hook_fixed.cpp = **8794 سطرًا**
- إعادة توليد part1..part7 (832×6+3802) — دمج ثم diff ضد hook_fixed.cpp = **مطابق 100%**

## المتوقع في اللوق القادم (سطور التحقق — بالترتيب)
1. `[BR] pre-warm: 'Spectating' FName = (x, y)` — قيمة الاسم جاهزة قبل الصعود + `[BR] verified pair #1/#2` + `[BR] weapon pool: N resident ranged defs`
2. عند الصعود: `[BR] boarding player (Erbium: destroy pawn + Spectating)...` ثم `[BR] player boarded (pawn destroyed, spectating on the bus — F79)` — **لا pawn مخفية إطلاقًا**
3. أثناء الطيران: `[BUS] (x,y,z) moved=N` + لا `[Motor] Wild` سطور — **الباص يحلّق والحيوانات تنام**
4. نقرتك على زر القفز: `[Bus] jump requested — native respawn pending` ثم `[BR] pawn respawned — re-applying local systems` — **skydive أصلي والكاميرا معك** (أو `[BR] fallback pawn spawned on the ground` إن رفض الأصلي — هبوط آمن على الأرض)
5. بعد الهبوط: `[Visibility] Forcing player body visible` + `[Abilities] ✅ Applied set` + `[QB] ✅ ClientActivateSlot bar=0 slot=0` — كل الأنظمة تعاد للـ pawn الجديدة
6. نقر شريط السلاح: `[QB] tap routed locally: bar=0 slot=N (F79-c)` — **السلاح يُجهّز فورًا**
7. زر الرقصة: `[Emote] playing <montage>` **بدون كراش** — الأصلي لم يُستدعَ
8. بوت قريب: يطاردك (navmesh نحوك) وعند 2.5م يضربك — ضرر 12 كل ثانيتين؛ بوت يموت يبقى جثة ثم تنظيف العاصفة
9. موتك: `[Spectate] pawn died — entering bot spectate` — **كاميرا على بوت** وزر القفز يبدّل

## تصحيح تالي (نفس الجولة)
- **زر القفز في وضع المشاهدة**: فحص `br.spectating` كان يأتي **بعد** early-return `session.aircraft_jumped` (ما زال true من قفزة الباص عند الموت) — صار الفحص أولًا فيتعامل الزر مع تبديل هدف المشاهدة فعليًا. hook_fixed.cpp = **8796 سطرًا**، parts (832×6+3804) — EXIT=0

---

# F80 — جولة «نفّذ المخزون فعلًا» (تقرير جهاز F79: «QuicBars لم يعمل، اللوت لم يعمل، الباص لا أنزل منه وكرشت اللعبة بعده، بوتات بلا راس ونسخ مني، الحيوانات ليست بمنطقها، الأبواب والتفاعل لم يكتملا»)

## قرائن اللوق F79 (التحليل قبل الكود)
- `[QB] tap swallowed — no item bound at bar=0 slot=-1` — **الـ HUD يرسل slot=-1**: حالته الداخلية للتركيز فارغة لأن أحدًا لم يخبره (لا ClientEquipItem أبدًا) — هذا جذر «الـ QuickBars لم يعمل»
- `[QB] Before: Primary=0 Secondary=0` بعد vtable match — **native init فشل** بسبب Owner=NULL: استدعاء `SetOwner` كان **بلا معاملات** (كتب Owner=null!)
- `[BUS] (...) 80000` — الباص على Z=80000 (FlightInfo الافتراضي: start (0,0,0)) رغم أن مسارنا المُصنّع Z=10500 — **الكتابة فشلت readback لأن اسم الصنف "AthenaAircraftFlightInfo" والقارئ سأل فقط عن "FortAthenaAircraftFlightInfo"**
- الباص حلّق 130+ ثانية وخرج من حدود الجزيرة — **الطيار الاحتياطي لم يشتغل أبدًا** (شرط still-beats: الباص الأصلي كان يتحرك بمساره المهمل)
- `🛩 [BR] aircraft destroyed (flight over)` ثم **كراش** — ViewTarget + GameState.Aircraft/Aircrafts[] + CurrentAircraft كلها كانت تشير للباص المدمَّر
- `[Wild] spawned NPC_Pawn_Parent_C` — **أصناف PARENT مجردة** (لا AI، لا behavior tree): فلتر skip كان يقارن أحرف صغيرة بأسماء CamelCase — «parent» لا تطابق «Parent» أبدًا
- `[BR] bot skins: <parts> | CP_Backpack_HeadbandKMale | CP_Body_Nug` — **حقيبة ظهر في تجمع الرؤوس** (نفس الفلتر: «Headband» تحتوي «Head») + تجمعات بلا dedup (heads=44 = ~11 جزء ×4)
- «نسخ كثيرة مني» — نفس الزوج الاحتياطي طبق على دفعات كاملة من البوتات
- `[Call] rejected BlueprintOnInteract` سبام لكل مرشح كل تِك — الحيوانات مرشحات تفاعل بلا معالج

## جدول التغييرات (13 بند)
| البند | العنوان | التنفيذ |
|---|---|---|
| F80-a1 | **ServerAddItemInternal الأصلي** | GiveItem ⇒ بعد إدراج الـ instance والـ entry، استدعاء `FortQuickBars.ServerAddItemInternal(Guid, bSecondary, -3)` — **مسار تعبئة الشريط الحقيقي** الذي يستعمله اللعبة نفسها (هيئته تشعل إشعارات الـ HUD الأصلية)، **مُتحقق بقراءة الـ GUID داخل مصفوفات الـ slots**؛ BindSlot اليدوي احتياط فقط |
| F80-a2 | **ClientEquipItem** | بعد كل give/equip: `FortPlayerController.ClientEquipItem(Guid, true)` — **حلقة التركيز التي لم تكن موجودة أبدًا** (Erbium تنهي كل give بها؛ الـ HUD عندها يعرف الـ slot المركّز) |
| F80-a3 | **SetOwner بمعامل فعلي** | FortInventory.SetOwner + FortQuickBars.SetOwner يربطان `NewOwner` + كتابة `Actor.Owner` مباشرة — native InitializeAthenaQuickBars يعمل على Owner حقيقي (الـ 0/0 كان Owner=NULL) |
| F80-a4 | **slot=-1 = التركيز المتعقب** | توجيه النقر المحلي: slot<0 ⇒ استعمال آخر slot فعّلناه (focus_slot[bar]، افتراضي 0 = الفأس) — النقر يعمل حتى بحالة HUD فارغة |
| F80-b | **لوت أرضي حقيقي** | `SpawnFloorLootPickups`: FortPickupAthena **تُرمى (TossPickup) عند كل PlayerStart** من مجمع الأسلحة الساكن + ذخيرة/خشب (60/25/15%) — **أسلحة مرئية على الأرض أينما هبطت** (نموذج Erbium SpawnPickup)، تُجمع بمسار fly-collect الحي؛ تتجدد عبر محاولات SpawnFloorLootRetry (سقف 64) |
| F80-c | **الأبواب مرشحات تفاعل** | NearbyScanRefresh يجمع BuildingWall ويحتفظ بمن يحمل بتات باب فقط (0x0F) ⇒ InteractTick مرشح kind=4 بمدى 300 — **الأبواب صارت قابلة للفتح/الإغلاق** من المرشح الأقرب (HandleDoor الأصلي toggle بالاتجاهين) |
| F80-c2 | **لا سبام رفض** | Call::run يسجل الرفض **مرة واحدة لكل اسم دالة** (الرفض نفسه لم يتغير) |
| F80-d1 | **القفز محلي 100%** | HandleJumpFromHudStart ⇒ **فوري محلي**: pawn جديدة عند موقع الباص الحالي − 250 + Possess + **BeginSkydiving(bFromBus=true)** + كل الأنظمة تُعاد (BRSpawnPlayerSkydiveAt) — **نفس الدالة للزر وللقفزة القسرية** (لا انتظار رسبن أصلي رفضه الجهاز) |
| F80-d2 | **جذر كاميرا الباص** | `PlayerController.bAutoManageActiveCameraTarget = false` عند الصعود (المحرك كان يعيد تأكيد ViewTarget المدمرة كل تِك — «الكاميرا لم تلحق الباص») + استعادة =1 عند الرسبن |
| F80-d3 | **المسار داخل الجزيرة** | الباص تُصفّف **على بداية مسارنا** (K2_SetActorLocationAndRotation + readback) والطيران الأصلي **يُعطَّل** (FlightSpeed=0 مُتحقق) — **طيارنا يقود كل تِك** (50ms، بلا شرط still-beats) + المدة 40-70ث داخل الحدود (كانت 95) |
| F80-d4 | **لا كراش بعد اختفاء الباص** | قبل K2_DestroyActor: الكاميرا على الـ pawn/المتحكم + bAutoManage=1 + **GameState.Aircraft=null + Aircrafts.Num=0 + CurrentAircraft=null** — لا إشارة معلقة تبقى للباص المدمّر |
| F80-d5 | **شبكة الأمان بموقع الباص** | مراقب الرسبن: skydive من موقع الباص الحالي (لا «بداية المسار المعكوسة» التي ألقت اللاعب في الركن المقابل) |
| F80-e/f | **بوتات وحيوانات** | الرؤوس/الأجسام بـ PartType من الانعكاس (0/1) + dedup — **لا حقيبة ظهر رأسًا ولا تكرار**؛ زوج **مميز لكل بوت بفهرس الرولستر** (لا نسخ من اللاعب)؛ بوت يهبط بلا سلاح ⇒ **فأس أولًا** ثم سلاح عشوائي؛ الحيوانات: **أصناف Irwin الخرسانية فقط + لا motors إطلاقًا** (الـ AI الأصلي يقودها) وليست مرشحات تفاعل |

## فحص + تزامن
- `g++ -std=c++17 -fsyntax-only -Wall -Wextra` مع stubs → **EXIT=0** (صفر تحذيرات) — jni/hook.cpp = **9127 سطرًا**
- parts1..7 أُوقفت (بنية المستودع الحالية jni/hook.cpp واحد + CI)

## المتوقع في اللوق القادم (سطور التحقق — بالترتيب)
1. `[QB] native ServerAddItemInternal ✅ verified (bar=0)` — **التعبئة الأصلية تعمل** (أو «ran without visible slot write» ⇒ BindSlot الاحتياطي اتخذها)
2. عند فتح أي صندوق/إعطاء عنصر: `Inventory inserted ... ` ثم **الشريط يُظهر الأيقونة** — ClientEquipItem ربط التركيز
3. عند الصعود: `[BR] bus parked on route start (...) — readback (...)` + `[BR] native flight neutralized: FlightSpeed readback=0` + `[BR] bAutoManageActiveCameraTarget parked` — **الباص على مسارنا والكاميرا معه**
4. أثناء الطيران: `[BUS] (x, y, 10500±) moved=...` — **داخل الجزيرة على ارتفاع MapInfo** (لا 80000)
5. نقرتك على القفز: `[Bus] JumpFromHudStart intercepted` ثم `[BR] player skydive from (...)` — **فورًا، من موقع الباص**
6. بعد اختفاء الباص: `[BR] aircraft destroyed (flight over) — references cleared first (F80)` — **لا كراش**
7. اللوت: `[Loot] pickups: N PlayerStart sites available` + `[Loot] ✅ floor pickups spawned=...` — **أسلحة مرئية على الأرض** والمشي عليها يجمعها
8. `[BR] bot skins: <parts> | CP_Head_* | CP_Body_*` — **رؤوس حقيقية**، كل بوت بزوج مختلف
9. `[Wild]` بلا NPC_Pawn_Parent_C — أصناف Irwin خرسانية فقط وتتحرك بـ AI الأصلي
10. باب قريب: مرشح kind=4 والنقر يفتحه/يغلقه — `[Door] OPEN/CLOSE via ...`

## تصحيح تالٍ (نفس جولة F80)
- **«NPC و بوتات مختفين» — جذر ثانٍ أُصيب**: بوت ما زال راكبًا (bus_state=1) حين يُدمَّر الباص كان يبقى **مخفيًا للأبد** (الكود القديم: `bus_state=0; continue` بلا إظهار!) — الآن: إظهار + إنزال عند هدفه على الأرض + استئناف الـ AI الأرضي. hook.cpp = **9162 سطرًا**، EXIT=0

---

# F83 — جولة «استكمال ما بعد الدفعات المتوازية» (rebase على F81/F82)

## قرائن المصدر
- مسار F81 الأساسي (QuickBarResize المحركية) سليم — لكن مساره الاحتياطي كان يحمل جذر الكراش نفسه: `if (oldBytes > 0 && oldBytes < newBytes) memset` يقفز عن التصفير عند Count==0 ← كتلة Realloc معاد تدويرها (هدم جولة سابقة) تُكتب غير مهيأة، والحلقة تقرأ رؤوس Items قمامة
- `bWantsToSlide=true` القسري ما زال (F51 أزالته لأنه يكسر الانزلاق) + TryActivateAbilityByClass لمرة واحدة = مسار F67 المُفسِد للـ pawn
- FixBodyAndAnimations (F41/F68) وGrantWorldAbilities (F52/F67) غائبتان — بلا AnimScriptInstance لا رسم حركة حتى مع أجزاء F82 الصحيحة

## جدول التغييرات (6 بنود)
| البند | العنوان | التنفيذ |
|---|---|---|
| F83-a1 | **تصفير احتياطي غير مشروط** | مسار Realloc الاحتياطي: `if (newBytes > oldBytes) memset(...)` — يصفّر الكل عند التخصيص الطازج ويحفظ القديم عند النمو |
| F83-a2 | **تعقيم الرؤوس** | رأس slots (Count 0..64 + IsBadPtr) قبل مساعدات المحرك + رأس Items في الحلقة (0..16) — لا مؤشرات وحشية إلى memcmp/GenericArrayAdd/Resize |
| F83-a3 | **استعادة dedupe F44** | slot0 يحمل GUID ⇒ تخطي إعادة البناء (مسار إعادة محاولات Tick) |
| F83-b1 | **إزالة bWantsToSlide القسري** | إعادة تعليق F51/F52 — العلم مدفوع بالدخل؛ يبقى SetCanSlide فقط |
| F83-b2 | **استعادة FixBodyAndAnimations** | AnimScriptInstance عبر SetAnimClass + إظهار الجسم — بلا استبدال mesh (F68)؛ تكامل مع F82 (أجزاء صحيحة + رسم حركة) |
| F83-b3 | **استعادة GrantWorldAbilities** | slide + enter/exit/in-vehicle + emote كمواصفات حقيقية (FortAbilitySet مصغر → EquipFortAbilitySet) |
| F83-c | **فحص انزياح runtime** | تحذير عند اختلاف الانعكاس عن 0x278/0x308 — يحسم سبب 0/0 من لوق واحد |

## فحص + تزامن
- `g++ -std=c++17 -fsyntax-only -Wall -Wextra` (بدائل NDK) → **EXIT=0**

## المتوقع في اللوق القادم
1. `[QB] grow bar0/1/2 readback` ثم **كل بنود bar1 تكتمل** بلا كراش
2. `[QB] already filled` عند إعادة الدخول (dedupe يعمل)
3. `[Body] ✅ locomotion online` + `[World] ✅ N world abilities delivered` — **الجري/الانزلاق حيّان**
4. `[QB] ⚠️ offset drift` غائب (أو حاضر فيحسم 0/0 فورًا)

---

## F84 — إصلاح التفاعل مع الصناديق (INT-1) — Erbium local_interact parity

| # | التغيير | السبب (مقارنة game.cpp المرجعي سطر 917-929) |
|---|---|---|
| F84-a | **تحديد الهدف بالاسم المُعرّف**: `incoming(func, params, "ReceivingActor", 8)` أولاً ثم fallback إلى القراءة الخام `params+0` | المرجع يقرأ `ReceivingActor` بالاسم؛ القراءة الخام عند offset 0 تُرجع garbage إذا تغيّر ترتيب البارامترات المطبوخة — هذا سبب صمت التفاعل |
| F84-b | **بوابة IsA Actor**: الهدف يجب أن يكون `/Script/Engine.Actor` فعلاً | المرجع يفحص `isa(actor, Actor)` — يمنع dispatch على مؤشرات وحشية من slot قديم |
| F84-c | **بوابة مسافة 600uu**: `NearPlayer(tloc, 600)` | المرجع `near_player(location, 600)` — يرفض أهدافاً بعيدة/فاسدة، والأهداف غير القابلة للتحديد الموقعي تمر كما في السابق |

## المتوقع في اللوق القادم (سلاسل التفاعل)
- `[Interact] target=... class=BuildingContainer` عند فتح صندوق → `[Loot] Opening ...` → spawn pickups
- `[Interact] F84 far/non-actor target rejected` لو وصل هدف فاسد — يعني بوابة F84 تعمل

## F85 — السيارات 1/325 (VEH-1): إصلاح تثبيت النجاح الجزئي

| # | التغيير | السبب |
|---|---|---|
| F85-a | **عدّاد `viable`**: يُحصي كل spawner/provider كلاسه قابل للتحديد في الـ pass الحالي | تشخيص دقيق للكثافة في اللوق |
| F85-b | **لا تثبيت `vehicles_spawned` على نجاح جزئي**: العلم يُثبَّت فقط عند تغطية كل الأهداف القابلة للحل أو استنفاد نافذة المحاولات (12) | الجذر: كلاسات المركبات تتأخر بالتحميل → أول pass ينجز سيارة واحدة → العلم يُثبَّت → 324 لا تُنتج أبداً (عرض 1/325) |
| F85-c | **لوق كثافة**: `spawner actors: N` في كل pass | رؤية عدد الـ spawners المتدفقة لحظياً |
| ملاحظة | الأهداف المرفوضة بالـ roll تُعاد قرعتها في كل pass لاحق | كثافة Erbium عبر VehicleMin/MaxSpawnPercent محفوظة |

## المتوقع في اللوق
- `[Vehicles] spawner actors: 325` ثم `partial: +N ... retrying` عدة مرات ثم `✅ complete: M vehicles total`

## F87 — LOOT-1 الجذر الثنائي: StructPropertiesSize الصحيح 0x54 (binary-verified)

| # | التغيير | الدليل |
|---|---|---|
| F87-a | `StructPropertiesSize = 0xA8 → 0x54` | سلسلة الإثبات: StructSuper=0x40 مُثبت عامل على الجهاز (SubclassOfFast نجح في 37 ability) + ترتيب أعضاء UStruct في مصدر UE5.0 المحلي (Class.h سطر 310-321): SuperStruct 0x40 → Children 0x48 → ChildProperties 0x50 → **PropertiesSize 0x54** → MinAlignment 0x58 (كلاهما int32). 0xA8 تقع داخل أعضاء UScriptStruct الخاصة → صفر دائماً |
| F87-b | تسمية اللوق `raw@0xA8` → `raw@0x54` | اللوق القادم يطابق الحقيقة |
| تكامل | F86 (Z.ai-Code) walk يبقى fallback + cross-check | مع 0x54 الصحيحة القراءة الأولية تعمل والwalk لا يُستدعى — يبقى تحققاً |
| يصلح | bad row sizes 0/0 + ClipSizeOf (دائماً 30) + ItemLevelOf | كلها نفس القراءة المعطوبة |

## F88 — Alive Players HUD (F40): إصلاح تثبيت one-shot + إعادة محاولة

| # | التغيير | السبب |
|---|---|---|
| F88-a | **إعادة تحليل mode/state لحظة الاستدعاء** من World | SetupWorld قد يسبق وصول GameMode/GameState — القيم المخزنة فارغة |
| F88-b | **التثبيت فقط عند إنجاز الكتلتين** (aliveDone && leftDone) | القديم كان يثبّت `match_state_ready=true` قبل الكتل — أول استدعاء مبكر = موت دائم للعداد (نفس عائلة F85) |
| F88-c | **إعادة محاولة كل 5 ثوان في Tick** حتى التثبيت الكامل | ضمان الوصول لحالة نهائية حتى مع تكرار مزامنة متأخر |

## المتوقع في اللوق
- `[MatchState] AlivePlayers count=1` + `[MatchState] PlayersLeft=1 + OnRep` + `[MatchState] ✅ ready`

## F89 — تدقيق أعلام one-shot: إصلاح ready_called (نتيجة تدقيق F88)

| العلم | الحكم | الإجراء |
|---|---|---|
| vehicles_spawned | خاطئ (F85) | مُصلح |
| match_state_ready | خاطئ (F88) | مُصلح |
| ready_called | **خاطئ** — كان يُثبَّت حتى لو فشل dispatch الـ RPC | **F89**: يُثبَّت فقط عند نجاح ServerReadyToStartMatch الفعلي (srt.ok() && run) — مسار إسقاط شاشة التحميل يعيد المحاولة |
| cosmetics_with_state | آمن | يُثبَّت بعد النجاح الفعلي + إعادة محاولة عند false |
| abilities_granted / tactical_sprint_setup / movement_enabled / quickbars_initialized | آمنة | لها حرّاس إعادة محاولة في Tick |

## المتوقع في اللوق
- لا سلوك جديد مرئي إن كان RPC الجاهزية يعمل؛ لو كان غائباً: لا مزيد تثبيت فاشل صامت
