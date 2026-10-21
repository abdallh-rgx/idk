// ============================================================================
// EraCH3 — Fortnite 21.30 CL-21088273 Android  (FIXED & COMPLETED)
// ============================================================================
// Build ID of target libUnreal.so:
//   5b371871c152651ff2b038c01c9504c1a82de1f8
//
// This is the corrected version of hook.cpp. All engine-class offsets and
// function signatures below were verified against the 21.30 MobileDumper-7
// SDK (GObjects dump + ClassesInfo/StructsInfo/FunctionsInfo/EnumsInfo).
//
// FIXES/PORTS (see CHANGES.md for the full list):
//  [F01] FQuickBarSlot.bEnabled lives at +0x10 bit0 (was writing +0x18 =
//        UsedByItemGuid, corrupting the GUID and never enabling the slot).
//  [F02] Reflection paths "FortQuickBar.Slots"/"FortQuickBarSlot.Items" do
//        not exist — the reflected struct names are "QuickBar"/"QuickBarSlot".
//  [F03] "Engine.Actor.SetMirrored" does not exist — use
//        BuildingSMActor.SetMirrored (bIsMirrored).
//  [F04] FortPlayerController.ServerAttemptAircraftJump does not exist in
//        21.30 — it lives on UFortControllerComponent_Aircraft.
//  [F05] JumpFromHudStart is dispatched on UAthenaHUDContext, not on the
//        controller — the old PE filter never matched (touch jump dead).
//  [F06] AFortPickup has no ItemDefinition/Count members in 21.30 — the
//        entry lives in PrimaryPickupItemEntry (FFortItemEntry).
//  [F07] FCreateBuildingActorData: BuildingClass ptr at 0x40 (was 0x48),
//        bMirrored at 0x38, param size 0x50.
//  [F08] ServerEditBuildingActor params: BuildingActorToEdit(ptr),
//        NewBuildingClass(ptr), RotationIterations(uint8), bMirrored(bool)
//        — old struct read bMirrored from the wrong offset.
//  [F09] GetAnimationHardReference in 21.30 takes BodyType/Gender/
//        PawnContext (old "Type/Animation" params are gone) — emotes fixed.
//  [F10] Edit tool: EditActor + OnRep_EditActor live on FortWeap_EditingTool,
//        not FortWeapon.
//  [F11] ServerOnExitVehicle ExitForceBehavior is a uint8 enum (old code
//        bound int32 → bind failed).
//  [F12] Removed dead OnRep calls (FortQuickBars OnRep_Primary/Secondary,
//        PC.OnRep_QuickBar, PlayerState.OnRep_CharacterParts,
//        Pawn.OnRep_CustomizationLoadout) — none exist in 21.30.
//  [F13] Removed K2_GiveAbility/GiveAbility grant paths — no such UFUNCTIONs
//        in 21.30; abilities flow through EquipFortAbilitySet only.
//  [F14] GC / SavePackage guards in WorldTick (offsets existed, never used).
//  [F15] IsStandalone gating before any session work (was never called).
//  [F16] Call: Blueprint event override resolution + receiver validation
//        + named/positional param binding (ported from the 10.40 Era core).
//  [F17] Inventory: native NewGuid + reconciliation verification + duplicate
//        protection (ported from Era's give()).
//  [F18] Pickups: stacking, max-stack transfer, weapon slot selection
//        (ported from Era's local_pickup).
//  [F19] Two-bar quickbar layout: weapons on Primary(0), build pieces on
//        Secondary(1), binding through EquipItemToQuickBarSlot with
//        GetItemInQuickbarSlot read-back verification.
//  [F20] Local loot rolling for containers with rollback (replaces the
//        dead BP-only path; exported-table style from the Era core).
//  [F21] Building through the local preview (LastBuildPreviewGridSnapLoc/
//        Rot + CurrentBuildableClass) with dedup + touch-build polling.
//  [F22] Locker cosmetics via MCP (CosmeticLoadoutPC + GetLoadoutForPlayer)
//        with hero specializations + backpack fallback.
//  [F23] SetMaxHealth/SetHealth(100) after possession; bBuildFree for free
//        building; bReadyToStartMatch direct bit fallback (0xA28 bit5).
//  [F24] Lobby return interception (ServerReturnToMainMenu → open Frontend
//        after tick) + fly/walk/ghost/god/teleport + era.* console commands.
//  [F25] Prologue verification for every hooked RVA before DobbyHook.
//  [F26] ProcessEvent prologue variants (21.30 + 10.40 reference build).
//  [F27] WorldTick re-entrancy fix (dead .bss probe removed).
//  [F28] VEHICLES (Erbium 1:1): AFortAthenaVehicleSpawner spawning with the
//        VehicleMin/MaxSpawnPercent roll + bForceSpawnAlways, SetFuel(100),
//        seat-weapon grant/removal through WeaponSeatDefinitions + PlayerSlots
//        (EquipVehicleWeapon / ServerOnExitVehicle parity).
//  [F29] Shared build dedupe between the ServerCreateBuildingActor RPC path
//        and the touch-poll path (no double placement).
//  [F30] BUILDING (Erbium 1:1): BuildingClass resolved from CurrentBuildable-
//        Class when the client only sends the handle, Erbium spawn order
//        (SpawnActorUnfinished → InitializeKismetSpawnedBuildingActor →
//        FinishSpawnActor), CurrentBuildingLevel + OnRep + Team bookkeeping.
//        EDITS now replace at the OLD actor transform (RotationIterations ×
//        90° yaw) instead of the world origin.
//  [F31] PICKUPS: LoadedAmmo/Level carry-over from the pickup entry, ClipSize
//        read from WeaponStatHandle rows, hotbar swap when full (Erbium
//        GiveOrSwap), consumables take primary slots too.
//  [F32] LOOT (Erbium 1:1): real DataTable roll — AthenaLootTierData_Client /
//        AthenaLootPackages_Client through GetDataTableRowNames/Row with the
//        full ChooseLootForContainer algorithm (weighted tier → DropCount →
//        category min/max bumping → package recursion → ammo companion via
//        FortWeaponPickupSpawnAmmoData → stack merge). The curated table is
//        now only a fallback. GetAllActorsOfClass out-param fixed.
//  [F33] Pickup placement along the container axes (LootSpawnLocation_Athena)
//        + guarded TossPickup (refuses to run with an unbound location).
//  [F34] Standalone cache fix: stale bAlreadySearched reset + floor-loot
//        spawners force-rolled and destroyed (Erbium ≥11 behaviour).
//  [F35] PICKUP PIPELINE (Erbium 1:1) — fixes "loot appears but cannot be
//        picked up / looks weird":
//          • SetPickupTarget emulation (PickupLocationData + OnRep_Pickup-
//            LocationData + bPickedUp/OnRep + IncomingPickups) → the native
//            fly-to-player animation plays.
//          • InternalPickup port (stacking / GiveOrSwapStack / GiveOrSwap /
//            FinalLoc re-toss) with a scheduled transfer after FlyTime.
//          • OnCapsuleBeginOverlap hook on the pawn (the live-server pickup
//            trigger) + ServerHandlePickup* RPCs fully replaced (native half
//            does not exist in this build — loot used to vanish).
//          • Container loot now TossPickups out of the chest (was hovering
//            mid-air), PawnWhoDroppedPickup set on every spawn.
//  [F94] LOOT TABLES ACTIVATED — GetDataTableRow is unresolved on this
//        build ([F93]) which silently failed ALL 2401 row reads ("cache
//        ready=0 tiers=0 packages=0" → curated fallback → random loot).
//        Rows are now read DIRECTLY from UDataTable::RowMap @+0x30
//        (Erbium DataTable.h layout: RowStruct @0x28 — runtime-verified here
//        by every PropOffsetOn hit; TSet element stride 24 with allocation
//        bit-array free-slot skip). The real 21.30 loot distribution
//        (252 tier rows × 2149 package rows) finally rolls.
//  [F94-b] Row size via FField walk (primary) — the raw PropertiesSize@0x54
//        slot reads 0x6F=111 on BOTH loot structs (non-4-aligned, identical
//        across two different row types = not PropertiesSize). Applies to
//        BuildCache + ClipSizeOf + ItemLevelOf. Also: the pkg-row
//        LootPackageCall FString is ENGINE-OWNED row memory — the old
//        Free() on it would have corrupted the table heap the moment row
//        reads started working (latent landmine, removed).
//  [F-loot-batch r2] CRASH FIX — the old SpawnFloorLoot rolled EVERY spawner
//        synchronously inside one WorldTick (1052 + 2958 passes, ~6300
//        pickups × 4-6 native calls each) → multi-second frame stall → phone
//        choke → crash; SearchContainer logged one line per spawner (the
//        ~4000-line log). New model: enqueue-only at SetupWorld, drain ~12
//        spawners per tick, rediscovery every 15s (10 passes) for streamed-in
//        POI sublevel spawners, aggregated logging, 6000-pickup hard cap.
//  [F-loot-place] AUTHENTIC FLOOR-LOOT PLACEMENT — no TossPickup (toss
//        physics was scattering pickups into random spots AND kept all
//        ~6300 of them active physics bodies), no +40 Z, no spread grid,
//        no per-spawner axis UFunction calls: pickups sit EXACTLY on the
//        spawner point (extras on a tight 35cm ring) like the real game.
//        Chests/ammo boxes keep the toss (authentic).
//  [F36] QUICKBARS/INVENTORY (Erbium 1:1) — fixes "QuickBars disabled":
//          • FortQuickBars.ServerAddItemInternal / ServerActivateSlotInternal
//            / ServerRemoveItemInternal / EmptySlot native binding with the
//            manual path as fallback.
//          • GiveItem: OwnerInventory + ParentInventory + TrackerGuid +
//            bIsReplicatedCopy (inventory UI orphan fixes).
//          • UE-allocator growth for QuickBar slot arrays (mixed malloc/UE
//            heap was a latent corruption/crash).
//  [F37] INTERACTION — ServerOnAttemptInteract is also dispatched ON the
//        container (Erbium v3 vft hook parity); the old receiver filter
//        dropped those calls, so chests and most world objects ignored the
//        interact button. Chests/doors/BP-interact now resolve from either
//        receiver shape.
//  [F38] BUILD MODE EXIT CRASH — ServerCreateBuildingActor* native path is
//        fully replaced (Erbium parity) and the OG call skipped: the
//        standalone BuildingClassMap is empty, and the exit-build payload
//        crashed the native implementation. Hidden hand-rolled preview
//        actors (SpawnBuildPreviews) removed — they collided with the native
//        preview teardown when build mode ended.
//  [F39] HARVEST RESOURCES (Erbium ABuildingSMActor::OnDamageServer 1:1) —
//        pickaxe damage on trees/rocks grants the mapped resource
//        (K2_GetResourceItemDefinition + AthenaResourceRates curve +
//        BuildingResourceAmountOverride), stack-clamped with overflow
//        re-toss, + ClientReportDamagedResourceBuilding HUD feedback.
//  [F40] ALIVE PLAYERS — GameMode.AlivePlayers.Add(PC) + GameState.PlayersLeft
//        = 1 + OnRep_PlayersLeft (the HUD counter now shows a number).
//  [F41] VEHICLES retry fix — the spawn pass no longer marks itself done
//        before the map's spawners stream in (12 attempts × 15 s).
//  [F42] era.fix (re-run every pass) + era.debug (dump slots/inventory/state)
//        console commands.
//
// ----------------------------------------------------------------------------
// G-SERIES — "Erbium parity wave 2" (this revision). Everything below was
// ported from the Erbium gameserver source (Finders.cpp / AbilitySystem-
// Component.cpp / FortKismetLibrary.cpp / FortVehicleSeatWeaponComponent.cpp /
// BuildingSMActor.cpp) to Android ARM64, driven by a NEW runtime scanner so
// no new hardcoded RVAs are required:
//  [G01] ARM64 RUNTIME SCANNER (Memcury port): ELF PT_LOAD discovery via
//        dl_iterate_phdr, literal string search in the loaded image,
//        ADRP+ADD xref resolution, validated prologue backscan (STP X29,X30
//        framed by RET/B boundaries) and BL call-target resolution. This is
//        the Android equivalent of Erbium's FindStringRef()/ScanFor() — the
//        engine's own log strings locate the stripped native functions.
//  [G02] NATIVE GAS ENTRY POINTS, discovered at runtime through [G01]:
//          • InitializePlayerGameplayAbilities  ("...with invalid
//            PlayerStateOrProxy!") — THE real Fortnite grant path: resolves
//            the DefaultAbilitySets of the pawn/PlayerState CDOs, builds
//            FGameplayAbilitySpecs WITH their declared InputIDs (the reason
//            GAS-driven locomotion/interaction anims live) and delivers them
//            through the native GiveAbility chain.
//          • GiveAbilityAndActivateOnce ("...called on ability %s on the
//            client, not allowed!") — emotes/sprays as real one-shot specs.
//          • InternalTryActivateAbility ("...called with invalid Handle!
//            ASC: %s. AvatarActor: %s") — authority activation.
//          • ApplyCharacterCustomization ("AFortPlayerState::ApplyCharacter-
//            Customization - Failed initialization, using default parts...")
//            — the native body-assembler, stronger than the visualization-
//            only refresh.
//        Every discovery is validated (exec-range + boundary framing +
//        readable prologue) and every use is layered over the existing
//        reflection fallbacks — a missing native degrades, never crashes.
//  [G03] ABILITY SETS (Erbium 1:1 for season 21): the grant list is now the
//        full Erbium set family — GAS_AthenaPlayer + AS_TacticalSprint +
//        AS_Ascender + AS_DoorBash + AS_HillScramble + AS_SlideImpulse +
//        GAS_Athena_RealitySapling — each loaded, appended with the mobile
//        extras, and delivered through the native InitializePlayerGameplay-
//        Abilities first, then EquipFortAbilitySet per set (granted-flag
//        commits only when specs actually appear in ActivatableAbilities).
//  [G04] InputID VERIFICATION: FGameplayAbilitySpec is read through
//        reflection (Handle/Ability/Level/InputID/SourceObject offsets are
//        resolved from the live UScriptStruct), and every grant pass logs
//        the resulting spec list with ability name + InputID (era.abilities
//        dumps them on demand). "Granted but not bound" is now observable.
//  [G05] MCP PROFILE DISPATCH FIX (Erbium Misc::SendRequestNow): the
//        "MCP-Profile: Dispatching request to %s" function is hooked and
//        re-invoked with context credentials forced to 3 (CXC_Public) —
//        the exact Erbium patch that lets profile/loadout queries leave the
//        box, which is why the "MCP Profile phase" never loaded.
//  [G06] DOORS THROUGH THE REAL API (IOpenableInterface parity): interaction
//        now routes through UFortKismetLibrary::OpenActor /
//        CloseActor (BlueprintAuthorityOnly statics that dispatch the
//        IOpenableInterface::Open/Close implementation on ABuildingWall,
//        with bRequestFastOpen for the slam path), with IsDoorOpen() as the
//        state probe. The manual bit-pack write stays ONLY as fallback —
//        the native path sets bDoorOpen + OnRep + collision + animation
//        with server authority, so doors STAY OPEN exactly like the live
//        game.
//  [G15-b] DOORS ARE NATIVE-ONLY NOW. The device run proved the native
//        interact already toggles doors on its own (log: "CLOSED (was
//        native-open)") — our full toggle on top of it produced a
//        DOUBLE-TOGGLE (user: "the door closes by itself"), and the
//        pin re-assertion fought the DoorBash ability ("breaks when
//        bashing"). Removed: TickDoorPins, TickSprintDoors, door_pins.
//        Kept: an OPEN-ASSIST on closed doors only (idempotent with the
//        native open). Closing an open door is purely the native
//        interact's business — exactly like the live game.
//  [G07] ANIMATIONS: FixBodyAndAnimations now finishes with the native
//        ApplyCharacterCustomization (PlayerState, Pawn) after the part
//        pipeline; PlayEmoteItem first tries GiveAbilityAndActivateOnce
//        with a reflection-built FGameplayAbilitySpec (Erbium ServerPlay-
//        EmoteItem parity, replicating LastReplicatedEmoteExecuted) and
//        falls back to the montage path.
//  [G08] BUILDING EDIT AUTHORITY (Erbium SetEditingPlayer): begin/end edit
//        now set EditingPlayer on the piece + OnRep_EditingPlayer with the
//        SetNetDormancy(Initial)/ForceNetUpdate pair, and the edit spawn
//        chain clears the editor before replacing the piece.
//  [G09] VEHICLES: instant seat-weapon response through the OnPawnEnter/
//        ExitVehicle events (in addition to the 0.5s poll), keeping the
//        Erbium exit parity (remove seat weapon → OG exit → re-equip
//        previous weapon or pickaxe).
//  [G10] era.abilities / era.specs console command + heartbeat flags for
//        the new native paths (gas_init/gaa/ita/acc/mcp).
//  [G11] NATIVE FAULT GUARD (device-crash fix): the run died INSIDE the
//        native InitializePlayerGameplayAbilities — device log ends at
//        "[Stamina] Initialized FortPlayerAttributeSet" and logging is
//        unbuffered (logcat + fflush), so nothing was lost: the interface
//        resolved, the call was made, the process faulted inside the
//        engine. Prime suspect: the client-side PlayerState ASC was never
//        server-initialized (Erbium calls this AFTER a real possession
//        flow on a listen server; EraCH3 has no server). The call now runs
//        under a scoped SIGSEGV/SIGBUS/SIGILL guard (sigsetjmp/siglongjmp
//        + old-handler chaining): a fault LOGS signal+PC+faulting address,
//        permanently disables the native path for the session, and the
//        proven EquipFortAbilitySet reflection path delivers the sets
//        instead of killing the process. The interface pointer is fully
//        validated (readable, readable vtable) and the raw-PlayerState
//        fallback is GONE — the IFortAbilitySystemInterface* is
//        this-adjusted; the UObject pointer means a wrong vtable on the
//        first virtual call. Spec-count success is now measured on BOTH
//        ASCs (pawn + PlayerState) since the native targets the PS's.
//  [G12] SCANNER RELAXATION + LAZY RE-DISCOVERY + PICKAXE: ADRP+ADD string
//        references may materialize the pointer into a different register
//        (ADRP X8,page; ADD X0,X8,#off — clang's argument shape) and up to
//        4 instructions apart. The strict same-register back-to-back form
//        missed GiveAbilityAndActivateOnce, InternalTryActivateAbility,
//        ApplyCharacterCustomization and SendRequestNow on device ("no
//        code ref" lines while the strings WERE resident). Strict stays
//        primary so every already-verified address resolves identically;
//        the relaxed pass only runs on misses. A late re-scan when the
//        terrain world appears + lazy SendRequestNow hook install (Erbium
//        finders are lazy for the same reason) covers paged-in-late
//        .rodata — that hook is the MCP profile enabler. Pickaxe fix
//        (Erbium 1:1): the locker pickaxe resolves its WeaponDefinition
//        (CosmeticLoadoutPC.Pickaxe->WeaponDefinition) before GiveItem,
//        and the locker re-syncs once at finalize (MCP lands async).
//  [G13] CRASH ROOT-CAUSE FIXES (second device run: guard caught the fault,
//        reflection delivered 42 abilities, then the spec dump crashed):
//        (a) BackscanPrologue v2 — the old rule accepted only STP X29,X30
//            prologues, but this build opens big natives with other callee-
//            saved pairs FIRST (ProcessEvent itself = STP X28,X27,[SP,#-N]!,
//            word0=0xA9BA6FFC). The old backscan skipped the real starts and
//            returned EARLIER unrelated functions — that is what faulted at
//            fn+0x24 with a garbage address. v2 accepts any callee-saved-pair
//            pre/post-index writeback STP, PACIASP/PACIBSP, SUB SP,SP,#imm or
//            the classic x29/x30 forms; strong boundaries (RET/B/BR/BLR/NOP)
//            are separated from weak ones (B.cond), candidates with frame
//            confirmation win, and the two prologue words are logged with
//            every find for logcat-side verification.
//        (b) DumpSpecs stride — the dump derived the FGameplayAbilitySpec
//            array stride from hardcoded FField::Inner/Struct guesses (0x70),
//            read misaligned garbage (#1 blank, #2 "inputID=7310658
//            level=510656512") and memcpy'd off the array page at spec #4 —
//            the hard crash. Stride is now the engine's own PropertiesSize
//            (live UScriptStruct), every element readability-probed, dump
//            capped at 128. CountActivatableSpecs got the same engine-truth
//            fallback.
//        (c) UTF-16LE everywhere — FString is { char16_t* Data; int32 Length;
//            int32 Max }, so every TEXT() literal in libUnreal.so is UTF-16LE
//            (device-proven). FindString now falls back to SUBSTRING matching
//            (UTF-16 first) when the exact NUL-terminated literal is absent:
//            "MCP-Profile: Dispatching request to %s" only exists as the
//            prefix of "... - ContextCredentials: %s".
//        (d) Native calls — InitializePlayerGameplayAbilities now tries the
//            Erbium convention (interface slot pointer) first and, only when
//            that FAULTS, retries with the raw PlayerState pointer (member
//            convention); the working convention is remembered for the
//            session. The fault guard is generalized (variadic) and wraps
//            ApplyCharacterCustomization and GiveAbilityAndActivateOnce too —
//            each disables its native path on first fault and falls back.
//            Emotes use the PlayerState ASC (Erbium ServerPlayEmoteItem
//            parity). The MCP hook prologue check accepts the full STP opener
//            set this build emits.
//  [G14] THIRD-RUN ROOT-CAUSE FIXES (5/5 discovery, 43 specs granted, then
//        the dump crashed at spec #3/#4):
//        (a) THE STRIDE — PropertiesSize (111) is the UNPADDED end of the
//            reflected properties (the last one is the InputPressed bitfield
//            byte, so the value is odd); a TArray<FGameplayAbilitySpec>
//            advances by sizeof(), which UHT pads to the struct alignment (8:
//            the spec holds pointers) = 112. Device proof: with 111, spec #1
//            read handle=0x200 — the byte pattern of spec#0's last padding
//            byte (0) followed by spec#1's real Handle (2) — and #2's
//            inputID=0x6F8B62 was the middle of the Ability pointer. The
//            stride is now align8(PropertiesSize) AND auto-validated against
//            3 consecutive slots (small handle/InputID/Level + Ability
//            pointer that is aligned, readable and vtabled inside the
//            module); on validation failure a raw hexdump of slots 0/1 gives
//            ground truth for the next run.
//        (b) THE FATAL CRASH — the misaligned Ability pointer passed ObjName's
//            FName bounds checks and the native NameAppendString walked into
//            unmapped memory (unguarded). Now every Ability pointer is hard
//            pre-validated (alignment + readability + vtable inside
//            libUnreal.so's PT_LOAD ranges) and the name fetch itself runs
//            under the fault guard: diagnostics can never kill the server.
//            CountActivatableSpecs/BuildSpec use the same align8 sizing.
//        (c) THE NATIVE INIT CALL — BOTH attempts (interface x0 AND raw
//            PlayerState x0) faulted at the SAME address (0x2886b5a5ee198c,
//            +8 vs the previous run) — the load at target+0xC reads a STALE
//            ARGUMENT REGISTER, not our this-pointer. The native's own
//            validation string ("...with invalid P[layerStateOrProxy]")
//            names its second parameter, which this variant dereferences
//            first. All calls now go through an 8-arg zeroed prototype
//            (x2..x7 = 0) with a proxy in x1, across a 5-convention matrix
//            (ps-iface+PS / raw PS+PS / pawn-iface+PS / ps-iface+pawn /
//            ps-iface+null — the null trips the function's own guard branch
//            and returns cleanly); the first non-faulting convention is
//            remembered, and the path is only disabled when ALL fault.
//        (d) ANTI-DOUBLE-GRANT — if the native really delivers the CDO
//            DefaultAbilitySets (spec delta >= 20), GAS_AthenaPlayer is NOT
//            re-equipped through EquipFortAbilitySet (would duplicate its 37
//            abilities); the Erbium movement family still equips (not in the
//            CDO).
//        (e) LATE-STREAMING SETS — AS_HillScramble/AS_SlideImpulse were not
//            resident at grant time; they queue and retry from Tick (~8s
//            cadence, ~3 min window) once the main grant has committed.
//  [G15] FOURTH-RUN REGRESSION + ROOT-CAUSE FIXES (user: "the door closes by
//        itself / breaks when bashing; no features; can't build, ride or
//        climb; reverse progress"):
//        (a) [G15-b] DOORS DE-HACKED. The device log proved the native
//            ServerAttemptInteract ALREADY toggles doors ("CLOSED (was
//            native-open)") — our full toggle on top of it was a
//            DOUBLE-TOGGLE ("the door closes by itself"), and the 0.3s pin
//            re-assertion fought the DoorBash slam ("breaks when bashing",
//            "re-asserted 19 pinned doors" every frame). Removed:
//            TickDoorPins, TickSprintDoors, door_pins bookkeeping. Kept:
//            open-ASSIST on closed doors only (idempotent with the native
//            open) — closing is purely native, like the live game.
//        (b) [G15-a] F92 CACHE — GetVehicleActor was name-walked 4×/0.5s
//            from TickVehicleSeats (per-frame log spam + class-chain walks
//            on a phone). Null-receiver Call resolutions now cache
//            (positives forever, negatives 30s TTL so late-streaming
//            UFunctions still resolve).
//        (c) [G15-c] VEHICLE ENTER — Erbium's own vehicle branch is
//            commented out (the OG native interact seats the pawn). We now
//            run the native FIRST, verify via GetVehicleActor, and fall
//            back to the SDK-verified BlueprintCallable
//            AFortAthenaVehicle::ForceSeatSinglePawn(pawn, seat 0) +
//            instant seat-weapon equip when the native declines.
//        (d) [G15-d] VEHICLE SPAWNERS ROLL ONCE — the retry pass re-rolled
//            the spawn percent every 15s (low percents → +2..9 per pass,
//            48/359 after 12 passes, "partial — retrying" forever). Each
//            spawner now rolls exactly once (Erbium world-init parity);
//            completion = every known spawner rolled.
//        (e) [G15-e] ABILITY SETS NEVER GIVE UP — the 22-try (~3 min)
//            window expired before distant POI sublevels streamed in
//            ("giving up on AS_HillScramble" = no climbing, ever). The
//            pump retries forever (StaticLoadObject only 10×/set — pak
//            index walks are measurable on a phone — then the cheap
//            loaded-object scan at a 20s cadence) with a substring-name
//            fallback. GrantWorldAbilities now grants INCREMENTALLY (the
//            old "core pair" gate withheld the slide ability because
//            GA_AthenaEnterVehicle_C never streamed in) and scans BATCHED
//            (one GObjects pass for all five names, was five full passes).
//        (f) [G15-f] LOOT CACHE — the loot tables are UCompositeDataTables:
//            their OWN RowMap is empty and the 252/2149 rows live in
//            ParentTables (@0xB0, SDK-verified) — GetDataTableRowNames
//            merges parents natively, which is why the count resolved while
//            every RowMapFind missed ("cache ready=0" → random loot).
//            RowMapFind now walks the parents (depth ≤3). The UFunction
//            fallback uses the UE5.0 name GetDataTableRowFromName (the old
//            GetDataTableRow no longer exists in 21.30) and handles its
//            OutRow-as-POINTER OutParm.
//        (g) [G15-g] TRUE SPEC STRIDE = 0xE8 (232) — the engine's own array
//            ElementSize (Items→Inner FStructProperty), NOT
//            align8(PropertiesSize=111)=112. Device proofs: the valid #29
//            spec (Default__GA_Athena_Grenade_Rethrow_C) sits at byte
//            3248 = 14×232 exactly, and slot0's bytes only parse under the
//            Erbium FFastArraySerializerItem layout (RepID@0/Key@4/
//            MostRecent@8, Handle@0xC, Ability@0x10, Level@0x18,
//            InputID@0x1C, SourceObject@0x20, ContainerInterface@0x28,
//            Triggers@0x70={ptr,Num,Max}). The 40 "<bad-ability>" slots
//            were a MISREAD, not corruption — the specs were valid all
//            along. BuildSpec now allocates the full 232 bytes (the old
//            112-byte buffer was a 120-byte heap overread in the emote
//            path) and the dump validates the stride against 2-of-4 slots.
//  [G16] WAVE 5 — the run-5 log + the user's src(4).zip reference package
//        (libUnreal.so binary, UE 5.0.0 full source, their 10.40 game.cpp/
//        profile.h, the PC client) drove five root fixes and the multi-
//        version foundation:
//        (a) [G16-a] DOORS FOR EVERY ACTOR — the run-5 "[Interact] target=
//            GasStation_Door_C_C" lines with NO follow-up exposed the gate:
//            BP doors are NOT BuildingWalls, so HandleDoor ignored them,
//            and since standalone DROPS server RPCs (no net connection →
//            the native interact body never runs), nothing ever opened
//            them. HandleOpenable now toggles ANY IOpenableInterface
//            implementer through UFortKismetLibrary::OpenActor/CloseActor
//            (the exact dispatch the native interact uses) with an
//            own-opened set for the toggle direction; TickSprintDoorAssist
//            re-adds the safe bash-through (OPEN-ONLY, no pins — it cannot
//            fight the interact toggle).
//        (b) [G16-b] LOOT — THE REAL TMap LAYOUT from the UE 5.0.0 source
//            the user shipped: FDefaultBitArrayAllocator = TInlineAllocator<4>
//            (inline bits @+0x10, heap ptr @+0x20), NumBits@+0x28,
//            FirstFreeIndex@+0x30, NumFreeIndices@+0x34, TSetAllocator
//            hash = TInlineAllocator<1> → TMap = 0x50 EXACTLY as the SDK
//            dump reports. The old code read flags@+0x18/numBits@+0x20/
//            numFree@+0x14 — garbage offsets. PLUS the UFunction bind fix:
//            the [F93] dump showed GetDataTableRowFromName resolves with
//            RowName@8 declared size 4 — every set()/positional bind
//            demanding sizeof(FName)==8 failed ("RowName bind failed" =
//            the whole cache miss). The 8-byte FName is now written RAW
//            into the slot.
//        (c) [G16-c] VEHICLES — Erbium SKIPS the spawner rollout on s20-22
//            ("its auto") because their listen server's native auto system
//            populates the map; our standalone has none, so WE are it:
//            every resolvable spawner spawns (60/pass, no percent roll —
//            the run-5 roll starved the map to 3/325), pre-s20 builds keep
//            Erbium's exact percent evaluation. The provider map is now
//            Erbium's s21 table 1:1 including the UPGRADE variants.
//        (d) [G16-d] HillScramble/SlideImpulse plugin-mount variants
//            (+ /HillScrambleGame/..., /SlideImpulseGame/... — Erbium needed
//            two mounts for TacticalSprint; the same drift applies).
//        (e) [G16-e] GrantWorldAbilities is now a SPEC-PRESENCE VERIFIER —
//            the run-5 spec dump proved the world abilities were already
//            granted through GAS_AthenaPlayer (#9 EnterVehicle, #36 Slide,
//            #41 DoorBash...): the old mini-set grant could only duplicate
//            them, and wabilities=0 in every heartbeat was a LIE. The
//            heartbeat now reports the verified truth.
//        (f) [G16-f] MULTI-VERSION PROFILE SYSTEM — build-ID detection
//            (PT_NOTE → NT_GNU_BUILD_ID) selects a profile table that
//            drives EVERY RVA and reflection offset: 21.30 CL-21088273
//            (full) and 10.40 CL-8936006 (engine core, from the user's own
//            profile.h + their game.cpp's verified prologue words).
//            Unknown builds fall back to the newest profile with a loud
//            banner. Per-profile: hook prologue words, FField vs pre-FField
//            param walks, raw-offset systems gated to verified builds,
//            pre-s20 vehicle/ability-set semantics.
//  [G17] WAVE 6 — the user's three reports (missing cities/buildings,
//        dead player stays standing, no win banner + frozen alive counter)
//        all traced to the same root: the server half of the match never
//        ran. Four fixes, all Erbium-parity:
//        (a) [G17-a] MISSING CITIES — Erbium sets MeshNetworkSubsystem
//            NodeType=2 for every UE4.27+ build (except v20): Chapter 3
//            geometry (whole POIs, city buildings) streams through the
//            mesh network, and a client node type waits for server mesh
//            data that never comes. Ported with reflection (NodeType
//            offset from the live class) + a 15s×10 retry window, plus
//            Erbium's StreamAdditionalPlaylistLevels fallback
//            (LoadLevelInstanceBySoftObjectPtr) for the playlist's
//            AdditionalLevels / server-only / shared-asset levels.
//        (b) [G17-b] TeamIndex/SquadId on the PlayerState (Erbium
//            CreatePlayer parity: solo team 3, squad 0) — without a team
//            the WinningTeam write can never match the player.
//        (c) [G17-c] DEATH — the client predicts damage into its local
//            attribute set, but death is a SERVER decision: 0 HP just
//            leaves the pawn standing. A 0.5s watcher catches the
//            health→0 EDGE and runs the full Erbium ClientOnPawnDied
//            server flow: inventory dropped as pickups + cleared,
//            DeathInfo + OnRep_DeathInfo, PawnDeathLocation, Place =
//            PlayersLeft (pre-decrement), CharacterMovement DisableMovement,
//            the ClientOnPawnDied client-RPC body executed locally (death
//            UI/camera/spectate), HandleIsDying (ragdoll attempt),
//            PlayersLeft-- + OnRep_PlayersLeft.
//        (d) [G17-d/e] ALIVE COUNTER + WIN — SetupMatchState seeds a
//            virtual solo lobby (playlist MaxPlayers, else 100; era.players
//            N overrides) instead of Erbium's PlayersLeft=1 (which freezes
//            the counter forever with no opponents). A paced elimination
//            curve (fast early game → slow top-10, ~18 min full drain)
//            decrements PlayersLeft + OnRep every step; reaching 1 alive
//            fires the Erbium win flow: Place=1, bIsAWinner,
//            WinningTeam/WinningPlayerState + OnReps, PlayWinEffects →
//            ClientNotifyWon → ClientNotifyTeamWon (the Victory Royale
//            banner). era.die / era.win force either flow for testing.
//  [G18] WAVE 7 — the user's report: "Alive Player is fake — do it like
//        Erbium: alone you never win immediately; alone the win window
//        appears when you die; if someone joins and you stay last the win
//        window appears; from match start alone you simply never win until
//        you die. Plus climbing / building / vehicle / buttons / QuickBars /
//        Inventory / Building are dead."
//        (a) [G18-a] ALIVE PLAYERS ARE REAL — the G17 virtual lobby + paced
//            elimination curve is DELETED. PlayersLeft now counts REAL
//            player states (GameState->PlayerArray walk, dead = Place>0,
//            local-player fallback when the array is empty). Alone the
//            counter reads 1 and stays 1 — no fake curve. Win rules are the
//            game's own: dying alone → Place=1 → the win window fires WITH
//            the death flow; win-while-alive requires players_ever >= 2
//            (a real second player joined and was eliminated). era.players
//            N stays as an explicit FAKE floor for testing (0 = real only),
//            era.alive prints the real state.
//        (b) [G18-b] QUICKBAR SLOTS THROUGH THE CLIENT'S OWN API — the
//            layered binder now leads with AFortPlayerController::
//            AddItemToQuickBars(ItemDefinition, QuickBarType, SlotIndex)
//            (SDK-verified, FortniteGame_functions.cpp:76307 — the exact
//            BlueprintCallable the real client uses), then QuickBars::
//            ServerAddItemInternal (Erbium GiveItem parity), then the
//            manual surgery. Every bind ends with a GetItemInQuickbarSlot
//            read-back so the log states whether the HUD can SEE the slot.
//            A self-healing watch (TickQuickBarWatch, 10s × 12) re-binds
//            any slot the HUD still cannot see; era.qb forces a pass. This
//            is the fix for empty QuickBars + dead slot/build buttons (the
//            build palette reads the Secondary bar the same way).
//        (c) [G18-c] WALL CLIMB v2 — the old assist only boosted while
//            FALLING (vel.Z < 0), so jumping INTO a wall never climbed.
//            v2 boosts in BOTH phases when pressed against an obstacle
//            (rising: hSpeed < 300 collapsed by the wall; falling:
//            hSpeed < 480), chains every 0.45s while jump is held, capped
//            at 4 boosts per air-time (no infinite flight), LaunchCharacter
//            override-mode so fast falls cannot eat the boost.
//        (d) [G18-d] VEHICLE ENTER LADDER — native interact → verify →
//            ForceSeatSinglePawn → ForceSeatPawns({pawn}) (SDK-verified
//            TArray form) → OnPawnEnterVehicle(pawn,0) dispatch, each step
//            verified through GetVehicleActor before the next runs.
//
//  [G19] CRASH + VEHICLE + LOOT + MISSING GEOMETRY (device round 16):
//        (a) [G19-a] DEATH-FLOW HARDENING — the device log ended at
//            "[Death] health reached 0" with NO completion line: one of the
//            native death steps faulted and killed the process. EVERY step
//            (inventory drops, DeathInfo/Place OnReps, DisableMovement,
//            ClientOnPawnDied, HandleIsDying, PlayersLeft, and the whole
//            victory banner chain) now runs under the proven RunGuarded
//            fault guard (the same net that saved the GAS/ACC calls) with a
//            per-step log — a faulting call is abandoned and the remaining
//            steps still run. Pawn-touching steps are additionally gated by
//            PawnStillValid(): when the engine already destroyed the pawn
//            (KillZ / fast eliminate during our 1s debounce) the controller
//            no longer references it, and any native call on it would crash.
//            After the flow a staleness watch clears the cached pawn so no
//            later ticker calls into freed memory.
//        (b) [G19-b] ServerAttemptEnterVehicle HAD NO HANDLER — the enter
//            ability dispatched the Server RPC, the hook ignored it, the net
//            layer dropped it: "button does nothing". It is now caught by
//            NAME on any receiver (before the PC/pawn gate) with named-param
//            extraction (Vehicle/VehicleActor/TargetVehicle + SeatIdx), a
//            one-shot param dump for the device log, and the shared
//            EnterVehicleDirect() ladder (verify → ForceSeatSinglePawn →
//            ForceSeatPawns → OnPawnEnterVehicle → seat weapon). Any
//            unhandled HUD/controller function with "Vehicle"+"Enter" in
//            the name is logged once and routed to the nearest vehicle as
//            the mobile-button fallback.
//        (c) [G19-c] LOOT TIER ROWS LIVED IN PARENT TABLES — device: 252 tier
//            rows read → 1 cached (1717/2149 packages). The composite
//            DataTables keep most rows in PARENT tables; the hard-coded
//            ParentTables@0xB0/0xC0 found nothing on this build. Parents
//            are now DISCOVERED dynamically (scan the object for
//            TArray<UDataTable*> candidates whose elements share our
//            RowStruct — cached per table) and searched first. The UFunction
//            fallback now honors its boolean return (false = miss, not an
//            empty row), and the curated table became a SAFETY NET: any roll
//            that produces zero drops falls back to it, so containers and
//            floor loot are never empty again.
//        (d) [G19-d] MISSING CITIES/BUILDINGS — two causes removed: (1)
//            DestroyLods (a SERVER-side Erbium habit) deleted the client's
//            FortHLODSMActor stand-ins that draw distant POIs — disabled.
//            (2) ForceSublevels(): every ULevelStreaming in the world is now
//            forced bShouldBeLoaded+bShouldBeVisible (applied by the
//            engine's own UpdateLevelStreaming pass, retried 6× as levels
//            appear). (3) FixMeshNetwork rewritten: the GObjects name scan
//            silently never ran — the subsystem is now resolved through
//            UGameplayStatics::GetGameInstanceSubsystem (multiple class
//            path candidates) and NodeType=2 (Erbium POI streaming fix)
//            finally applies.
//  [G20] LOOT-ROLL CRASH (device round 17 — the log died right after "tables:
//        tiers=252 packages=2149", i.e. the FIRST roll with real table data):
//        (a) RESIDENT-FIRST soft resolution — TSoftObjectPtr paths are now
//            parsed to strings (UE5.0 FSoftObjectPath = FString AssetPathName,
//            strictly validated; the 5.1 FName-pair layout fails validation
//            cleanly and falls through) and resolved with StaticFindObject.
//            The batch drain NEVER calls LoadAsset_Blocking anymore: 12 sync
//            disk loads per tick = multi-second frame stall = ANR kill / OOM
//            on a phone, plus the unguarded native-call crash window. The
//            sync load survives ONLY on interactive chest opens (one at a
//            time, under the fault guard).
//        (b) GUARDED DRAIN — every spawner roll (DataTable pipeline + BP
//            fallback + destroy) runs inside ONE RunGuarded scope: a fault
//            skips that spawner and the flow continues. A stage tracker
//            (g_lootStage) names the exact phase that faulted. Circuit
//            breaker: 8 consecutive faults suspend the drain 120 s and put
//            the cache on a 600 s hold (curated fallback covers the gap).
//            8 ms wall-clock budget per tick, batch 12→8.
//        (c) BuildCache: per-loop progress logs every 250 rows (a fault is
//            now pinpointable to a row range) + junk-row counters + the pkg
//            LootPackageCall FString read hardened (count clamped ≤64,
//            kernel-validated readable) + the interactive Athena::Loot path
//            is fault-guarded too (first chest could build the cache).
//        (d) AmmoCountFor: the raw IGameplayTagAssetInterface vtable call
//            (+0x10) — the one unguarded native call inside the roll — now
//            validates both the interface pointer and the code pointer with
//            kernel probes before calling (it already runs under the drain
//            guard; this removes the fault in the common case).
//        (e) Curated minimum: a matched curated tier that rolls 0 of
//            everything (weaponsMin=ammoMin=resources=0, the Warmup group)
//            still yields one weapon + one ammo — "roll produced no drops"
//            can never fire while tables are alive.
//  [G21] VEHICLE ENTRY/EXIT — the enter button STILL never reached a hooked
//        function (no [Vehicle] lines at all in round 17): the mobile touch
//        HUD routes buttons through AthenaHUDContext (the JumpFromHudStart
//        pattern) with names we have not met. (a) Every unique function the
//        HUD context dispatches is now LOGGED once (vocabulary discovery);
//        (b) interact-ish HUD functions dispatch a full interact: nearest
//        BuildingContainer (350uu) first, else nearest vehicle (900uu) →
//        EnterVehicleDirect; (c) 'ehicle'+'xit' names run the new
//        ExitVehicleDirect ladder (OnPawnExitVehicle + seat-weapon restore);
//        (d) every 'ehicle' function name seen anywhere is logged once.
//  [G22] MISSING POIs / WORLD PARTITION (round 17: "0 playlist levels", holes
//        in the terrain where POIs plug in) — Artemis_Terrain is a World
//        Partition map: POIs live in runtime cells streaming around sources.
//        New DiagnoseWorldPartition() (3 passes): (A) World->Levels /
//        StreamingLevels counts (what actually loaded); (B) the UWorldPartition
//        object itself (reflected prop, then a validated memory scan) + its
//        reflected member dump; (C) ADataLayerManager actors + per-layer
//        effective state + activation attempt through any resolvable
//        SetDataLayer* UFunction; plus two safe attempts: bIsStreamingSource
//        forced true on pawn+controller (the flag WP's source iterator keys
//        on, when reflected) and a "FlushLevelStreaming" console pass.
//        StreamPlaylistLevels now logs the resolved playlist name + the size
//        of every levels array it inspects.
//  [G23] ROUND-10 CRASH + UNIVERSAL UFUNCTION GUARD (the log died between
//        "[WP] StreamingLevels: 993" and part B's first line — and loot never
//        built, vehicles never retried, meshnet never resolved, because the
//        crash killed the FIRST finalized tick before those tickers ran):
//        (a) Call::run now dispatches orig_PE under the fault guard — EVERY
//            UFunction call in the module (vehicle retries, WP activation,
//            loot destroy, meshnet, QB updates, death/victory steps) faults
//            into the guard with the function NAMED in the log instead of
//            killing the process;
//        (b) RunGuarded is now NESTABLE — depth counter: only the outermost
//            guard installs the recovery point, nested guards pass through
//            (a fault unwinds to the outermost scope which reports it);
//        (c) DiagnoseWorldPartition: parts A/B/C each run in their own guard;
//            part B's memory scan no longer calls ObjName() on arbitrary
//            readable candidates (a garbage FName index walked GNames out of
//            bounds — THE round-10 crash) — the candidate's class pointer is
//            compared against the real UWorldPartition class object instead
//            (zero native name calls on untrusted memory); part A also logs
//            the streaming-state split (wantLoad / wantVisible / haveLoaded-
//            Level) — the "holes where buildings should be" evidence;
//        (d) ForceSublevels: the round-10 log proved StreamingLevels is EMPTY
//            at ~15 s but holds 993 WP cell streamers by ~30 s — the old
//            "sublevel_passes = 100" permanent quit on empty was a bug. WP
//            maps (count > 128) now force bShouldBeVisible ONLY on streamers
//            that already hold a loaded level (NEVER bShouldBeLoaded on the
//            whole cell set — that would load the entire map = OOM kill);
//            classic maps (≤ 128) keep the loaded+visible force;
//        (e) bIsStreamingSource + FlushLevelStreaming extracted from the WP
//            diagnostic into TickStreamingSources (4 passes × 30 s) — on
//            round 10 the diag crashed BEFORE that block ever ran, so the
//            pawn was never a WP streaming source and cells never streamed
//            around the player;
//        (f) flo::Drain: the spawner K2_DestroyActor moved INSIDE the guarded
//            scope (was the last unguarded UFunction dispatch in the drain).
//  [G24] VOID GUARD — "fell through a hole where the building should be and
//        died at the bottom of the map": TickVoidGuard tracks the last
//        grounded position (sane terrain band, skydive altitudes excluded)
//        and teleports the pawn back above it when it falls below the map
//        floor (Z < -2500, 6 s cooldown, loud log); the third rescue re-arms
//        the streaming-source passes and flushes streaming.
//  [G25] GAS NATIVE CONVENTION (round-10 evidence): conventions 1-4 faulted
//        at garbage pcs that are the low 52 bits of a function's first eight
//        PROLOGUE BYTES read as a pointer (indirect branch through proxy-
//        derived data), while the nullptr-proxy convention reached REAL code
//        at fn+0xC faulting on [proxy+8] — arg2 must be a pointer to a
//        TScriptInterface-shaped pair {ObjectPointer@0, InterfacePointer@8}
//        (Erbium's own GiveAbilitySet builds exactly this pair for the
//        sibling EquipFortAbilitySet call). The matrix now leads with the
//        pair conventions; target address confirmed correct.
//  [G26] VICTORY CROWN (Erbium FortPlayerControllerAthena.cpp:1277-1292,
//        v19+ 1:1): the winner receives AGID_VictoryCrown after the banner
//        chain.
// ============================================================================
#include <jni.h>
#include <pthread.h>
#include <dlfcn.h>
#include <link.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <cstdarg>
#include <cstddef>
#include <cerrno>
#include <atomic>
#include <string>
#include <vector>
#include <array>
#include <map>
#include <set>
#include <algorithm>
#include <android/log.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/ucontext.h>

static FILE* g_logFile = nullptr;
static void InitLog() {
    const char* p[] = {
        "/storage/emulated/0/Android/data/com.epicgames.fortnite/files/era_log.txt",
        "/sdcard/Android/data/com.epicgames.fortnite/files/era_log.txt",
        "/data/local/tmp/era_log.txt",
        "/sdcard/era_log.txt",
        nullptr};
    for (int i = 0; p[i]; ++i) { g_logFile = fopen(p[i], "w"); if (g_logFile) break; }
}
static void LogImpl(const char* fmt, ...) {
    char line[1024]; va_list ap; va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap); va_end(ap);
    __android_log_write(ANDROID_LOG_INFO, "EraCH3", line);
    if (g_logFile) { fprintf(g_logFile, "%s\n", line); fflush(g_logFile); }
}
#define LOG(...) LogImpl(__VA_ARGS__)

using int8=int8_t; using int16=int16_t; using int32=int32_t; using int64=int64_t;
using uint8=uint8_t; using uint16=uint16_t; using uint32=uint32_t; using uint64=uint64_t;
using TCHAR=char16_t;

// ============================================================================
// PROFILE — All offsets for Fortnite 21.30 (libUnreal.so CL-21088273).
// Code RVAs byte-verified against the shipped library; class offsets
// verified against the 21.30 MobileDumper-7 dump.
// ============================================================================
// ============================================================================
// [G16-f] MULTI-VERSION PROFILE SYSTEM — the user's request: one build that
// works on ALL versions. The profile namespace is RUNTIME-INITIALIZED from a
// build-ID-keyed table: at load we read libUnreal.so's .note.gnu.build-id
// (PT_NOTE walk), match it against the known profiles and initialize every
// RVA + structural offset from the match. Unknown builds fall back to the
// newest profile with a loud banner (the [ARM] string scanner already finds
// the GAS natives version-independently — log strings are stable across
// builds; reflection adapts automatically; only these RVAs/offsets differ).
//
// Known profiles:
//   5b371871c152651ff2b038c01c9504c1a82de1f8  — Fortnite 21.30 CL-21088273 (UE 5.0) — FULL support
//   54a4e9956d22bcdc721b4046d62741893cf0f7c1  — Fortnite 10.40 CL-8936006 (UE 4.22-fork) — engine core
//                                             (RVAs from the user's own profile.h; prologue words from
//                                              their game.cpp install_game_hooks)
// ============================================================================
struct BuildProfile {
    const char* buildId;    // 40-hex GNU build id
    const char* name;
    int         fortniteVersion;
    // Core engine RVAs
    uintptr_t ProcessEvent, StaticFindObject, StaticLoadObject, StaticLoadClass,
              NameAppendString, Free, WorldTick, EngineExec, IsStandalone,
              GetPlayerController, GetPawn, Possess, BeginSpawn, FinishSpawn,
              SpawnObject, GenericArrayAdd, GenericArrayRemove, IsGarbageCollecting,
              IsSavingPackage, GObjects, Realloc, CopyScriptStruct, GetInterfaceAddress,
              InitializeAthenaQuickBars, QuickBarResize;
    // Reflection layout offsets
    unsigned ObjectClass, ObjectName, ObjectOuter, FieldNext, StructSuper,
             StructChildren, StructChildProps, StructPropertiesSize, FunctionParamSize,
             PropFlags, PropOffset, PropElementSize, PropArrayDim,
             BoolSize, BoolByteOffset, BoolFieldMask, FFieldName, FFieldNext;
    // Hook prologue words (ARM64, first 4 instructions)
    uint32_t peWords[4], tickWords[4], execWords[4], qbWords[4];
};

static const BuildProfile kProfiles[] = {
    // ---- Fortnite 21.30 CL-21088273 (UE 5.0, Android libUnreal.so) ----
    {
        "5b371871c152651ff2b038c01c9504c1a82de1f8", "21.30 CL-21088273 (UE5.0)", 21,
        0x07611B5C, 0x0762A664, 0x0761C568, 0x0762CFB8,
        0x074937CC, 0x07355370, 0x0949685C, 0x098BB160, 0x0945F840,
        0x093A1D48, 0, 0, 0x093A37E4, 0x0939F1B8,
        0x093A3388, 0x0944A668, 0x0944B2DC, 0x0756E138,
        0x0760A59C, 0x0DCD6E08, 0x0735E520, 0x07535848, 0x0761AA88,
        0x056607EC, 0x056925E0,
        0x10, 0x18, 0x20, 0x28, 0x40,
        0x48, 0x50, 0x54, 0xB6,
        0x38, 0x44, 0x34, 0x30,
        0x80, 0x81, 0x82, 0x28, 0x20,
        {0xa9ba6ffc, 0xa90167fa, 0xa9025ff8, 0xa90357f6},   // ProcessEvent (device-verified word0+tail)
        {0xd10503ff, 0x6d0d23e9, 0xa90e6ffc, 0xa90f67fa},   // WorldTick
        {0xd10183ff, 0xa9025ff8, 0xa90357f6, 0xa9044ff4},   // EngineExec
        {0xa9bd57f6, 0xa9014ff4, 0xa9027bfd, 0x910083fd},   // InitializeAthenaQuickBars
    },
    // ---- Fortnite 10.40 CL-8936006 (UE 4.22-fork, GNU build 54a4e995...) ----
    // RVAs from the user's profile.h; prologue words from their game.cpp
    // install_game_hooks. Engine core only: reflection Call/inventory/build/
    // ability-set paths adapt automatically; the 21.30-specific raw-offset
    // systems (QuickBars manual build, readiness byte, sprint-speed write,
    // door bit-pack, TacticalSprint flags) are DISABLED on this profile.
    {
        "54a4e9956d22bcdc721b4046d62741893cf0f7c1", "10.40 CL-8936006 (UE4.22-fork, engine core)", 10,
        0x05D87F84, 0x05DA14FC, 0x05D90600, 0x05DA3600,
        0x05C2BC18, 0x05AD7908, 0x07212EEC, 0x07620D3C, 0x071EDFA0,
        0x07141934, 0x070779F0, 0x0707653C, 0x07143230, 0x0713ED04,
        0x07142DA4, 0x071DB50C, 0, 0x05CFE284,
        0x0C8BCBF8, 0x0C8BEA00, 0, 0, 0x05D8F3C8,
        0, 0,
        0x10, 0x18, 0x20, 0x38, 0x40,
        0x58, 0 /*no FField layer — pre-FField UProperty chain*/, 0x5C, 0xAE,
        0x48, 0x54, 0x44, 0x30,
        0x80, 0x81, 0x82, 0, 0,
        {0xf81a0ffc, 0xa90167fa, 0xa9025ff8, 0xa90357f6},   // ProcessEvent (user-verified)
        {0xd10503ff, 0x6d0d23e9, 0xa90e6ffc, 0xa90f67fa},   // WorldTick (same family)
        {0xd10243ff, 0xf90023f9, 0xa9055ff8, 0xa90657f6},   // EngineExec (user-verified)
        {0, 0, 0, 0},                                        // no AthenaQuickBars native on 10.40
    },
};

// [G16-f] The live profile — initialized by InitProfile() before ANY use.
// Defaults = the newest (21.30) profile so the legacy constexpr semantics
// hold even if detection never runs.
namespace profile {
inline uintptr_t ProcessEvent         = kProfiles[0].ProcessEvent;
inline uintptr_t StaticFindObject     = kProfiles[0].StaticFindObject;
inline uintptr_t StaticLoadObject     = kProfiles[0].StaticLoadObject;
inline uintptr_t StaticLoadClass      = kProfiles[0].StaticLoadClass;
inline uintptr_t NameAppendString     = kProfiles[0].NameAppendString;
inline uintptr_t Free                 = kProfiles[0].Free;
inline uintptr_t WorldTick            = kProfiles[0].WorldTick;
inline uintptr_t EngineExec           = kProfiles[0].EngineExec;
inline uintptr_t IsStandalone         = kProfiles[0].IsStandalone;
inline uintptr_t GetPlayerController  = kProfiles[0].GetPlayerController;
inline uintptr_t GetPawnRva           = kProfiles[0].GetPawn;         // [G16-f] 10.40 exposes GetPawn
inline uintptr_t PossessRva           = kProfiles[0].Possess;         // [G16-f] 10.40 exposes Possess
inline uintptr_t BeginSpawn           = kProfiles[0].BeginSpawn;
inline uintptr_t FinishSpawn          = kProfiles[0].FinishSpawn;
inline uintptr_t SpawnObject          = kProfiles[0].SpawnObject;
inline uintptr_t GenericArrayAdd      = kProfiles[0].GenericArrayAdd;
inline uintptr_t GenericArrayRemove   = kProfiles[0].GenericArrayRemove;   // 0 = unavailable (guarded)
inline uintptr_t IsGarbageCollecting  = kProfiles[0].IsGarbageCollecting;
inline uintptr_t IsSavingPackage      = kProfiles[0].IsSavingPackage;
inline uintptr_t GObjects             = kProfiles[0].GObjects;
inline uintptr_t Realloc              = kProfiles[0].Realloc;              // 0 = unavailable (guarded)
inline uintptr_t CopyScriptStruct     = kProfiles[0].CopyScriptStruct;     // 0 = unavailable (guarded)
inline uintptr_t GetInterfaceAddress  = kProfiles[0].GetInterfaceAddress;

// [G16-f] Profile metadata (gates the 21.30-specific raw-offset systems)
inline int  FortniteVersion = kProfiles[0].fortniteVersion;   // 21 = full, 10 = engine core
inline bool RawOffsetsVerified = (kProfiles[0].fortniteVersion >= 20);   // 21.30 device-verified offsets only
inline bool HasFField = (kProfiles[0].FFieldNext != 0);       // pre-FField builds walk UProperty chains

// Object layout (per-profile; UE5.0 values = 21.30)
inline unsigned ObjectClass  = kProfiles[0].ObjectClass;
inline unsigned ObjectName   = kProfiles[0].ObjectName;
inline unsigned ObjectOuter  = kProfiles[0].ObjectOuter;
inline unsigned FieldNext    = kProfiles[0].FieldNext;
inline unsigned StructSuper  = kProfiles[0].StructSuper;
inline unsigned StructChildren   = kProfiles[0].StructChildren;
inline unsigned StructChildProps = kProfiles[0].StructChildProps;   // 0 = no FField layer
inline unsigned StructPropertiesSize = kProfiles[0].StructPropertiesSize;
inline unsigned FunctionParamSize = kProfiles[0].FunctionParamSize;
inline unsigned PropFlags       = kProfiles[0].PropFlags;
inline unsigned PropOffset      = kProfiles[0].PropOffset;
inline unsigned PropElementSize = kProfiles[0].PropElementSize;
inline unsigned PropArrayDim    = kProfiles[0].PropArrayDim;
inline unsigned BoolSize        = kProfiles[0].BoolSize;
inline unsigned BoolByteOffset  = kProfiles[0].BoolByteOffset;
inline unsigned BoolFieldMask   = kProfiles[0].BoolFieldMask;
inline unsigned FFieldName = kProfiles[0].FFieldName;   // 0 = no FField layer
inline unsigned FFieldNext = kProfiles[0].FFieldNext;   // 0 = no FField layer

// CPF flags
constexpr uint64_t CPF_Parm       = 0x00000080ULL;
constexpr uint64_t CPF_OutParm    = 0x00000100ULL;
constexpr uint64_t CPF_ConstParm  = 0x00000200ULL;
constexpr uint64_t CPF_ReturnParm = 0x00000400ULL;

// FFortItemEntry (21.30 struct layout — verified)
constexpr unsigned ItemEntry_Count          = 0x0C;
constexpr unsigned ItemEntry_ItemDefinition = 0x18;
constexpr unsigned ItemEntry_Durability     = 0x24;
constexpr unsigned ItemEntry_LoadedAmmo     = 0x2C;
constexpr unsigned ItemEntry_PhantomAmmo    = 0x30;
constexpr unsigned ItemEntry_ItemGuid       = 0x68;
constexpr unsigned ItemEntry_TrackerGuid    = 0x78;
constexpr unsigned ItemEntry_StateValues    = 0xD0;

// Fortnite classes (verified)
constexpr unsigned FortPlayerState_HeroType            = 0x0430;
constexpr unsigned FortPlayerState_CharacterData       = 0x0580;
constexpr unsigned FortPlayerState_AbilitySystemComp   = 0x09F0;
constexpr unsigned FortPlayerController_MyPlayerInfo   = 0x2088;
constexpr unsigned FortPlayerController_ClientQuickBars= 0x1A60;
constexpr unsigned FortPlayerController_QuickBarReady  = 0x1A68;   // byte after ClientQuickBars, checked by native GetItemInQuickbarSlot gate
constexpr unsigned FortPlayerController_ReadyByte      = 0x0A28;   // bReadyToStartMatch bitfield byte
constexpr unsigned FortPlayerController_ReadyBit       = 5;
constexpr unsigned RegisteredPlayerInfo_HeroInstance   = 0x2D0;    // TempAthenaMenuHeroInstance
constexpr unsigned FortHero_CharacterParts             = 0x238;
constexpr unsigned FortHero_Specializations            = 0x218;
constexpr unsigned FortPlayerPawn_bIsSkydiving         = 0x1A1D;
constexpr unsigned FortPlayerPawn_bIsParachuteOpen     = 0x1A1E;
constexpr unsigned FortPlayerPawn_CosmeticLoadout      = 0x2A08;
constexpr unsigned FortPawn_CurrentWeapon              = 0x07F0;
constexpr unsigned FortPawn_AbilitySystemComponent     = 0x1008;
constexpr unsigned BuildingWall_bDoorOpen_Byte         = 0x0FDE;   // plain bool byte (0/1)
constexpr unsigned PlayerPawn_Athena_Speed_Sprint      = 0x53EC;

// ⭐ Movement offsets (from SDK)
constexpr unsigned FortPawn_bWantsToSlide            = 0x1030;
constexpr unsigned FortPlayerPawn_bIsSliding_Byte    = 0x06DB;
constexpr unsigned FortPlayerPawn_bIsSliding_Bit     = 3;
constexpr unsigned FortPlayerPawn_bIsSurfaceSwim_Byte= 0x0770;
constexpr unsigned FortPlayerPawn_bIsSurfaceSwim_Bit = 4;
constexpr unsigned CharacterMovement_MovementMode    = 0x019C;
constexpr unsigned CharacterMovement_CustomMode      = 0x019D;

// TacticalSprint (UFortPlayerControllerComponent_TacticalSprint, verified)
constexpr unsigned TacticalSprint_Flags              = 0xC0;
constexpr uint8_t TacticalSprint_Bit_Enabled   = 0x01;
constexpr uint8_t TacticalSprint_Bit_WantsSprint = 0x02;
constexpr uint8_t TacticalSprint_Bit_IsSprinting = 0x08;

// ⭐ QuickBars (21.30 — libUnreal.so; 0 on other builds = path disabled)
inline uintptr_t InitializeAthenaQuickBars    = kProfiles[0].InitializeAthenaQuickBars;
constexpr unsigned  QuickBars_VtableSlot         = 0x738;
constexpr unsigned  QuickBars_PrimaryOffset      = 0x278;   // FQuickBar.PrimaryQuickBar
constexpr unsigned  QuickBars_SecondaryOffset    = 0x308;   // FQuickBar.SecondaryQuickBar
constexpr unsigned  QuickBars_CreativeOffset     = 0x398;   // FQuickBar.CreativeQuickBar
constexpr unsigned  QuickBar_SlotsOffset         = 0x10;    // FQuickBar.Slots
constexpr unsigned  QuickBarSlot_ItemsOffset     = 0x00;    // FQuickBarSlot.Items
constexpr unsigned  QuickBarSlot_EnabledOffset   = 0x10;    // FQuickBarSlot.bEnabled (bit0)
constexpr unsigned  QuickBarSlot_EnabledBit      = 0x01;
constexpr unsigned  QuickBarSlot_OccupiedBit     = 0x08;    // bIsOccupied (bit3, same byte)
constexpr unsigned  QuickBarSlot_SlotSize        = 0x28;
inline uintptr_t QuickBarResize               = kProfiles[0].QuickBarResize;  // [F81] engine quickbar slot resize+init (0 on other builds = guarded)
constexpr unsigned  Controller_ClientQuickBars   = 0x1a60;
constexpr unsigned  Controller_QuickBarReadyByte = 0x1a68;
constexpr uintptr_t GetItemInQuickbarSlot_Gate   = 0x06673524;
constexpr unsigned  QuickBars_VtableSlot_GetItemInSlot = 0x6f0;
constexpr unsigned  QuickBars_CtxOwner           = 0x138;   // [F94] ctx the native init IsA-casts (0x56607ec: null/fail-cast → silent ret)

// ⭐ Local building / reticle / touch (AFortPlayerController, verified)
constexpr unsigned Controller_CurrentBuildableClass = 0x1898;
constexpr unsigned Controller_BuildPreviewRotIter   = 0x1858;
constexpr unsigned Controller_BuildPreviewMirrored  = 0x1859;
constexpr unsigned Controller_bBuildFree            = 0x185B;
constexpr unsigned Controller_TouchPrimaryFire      = 0x0BB8;
constexpr unsigned Controller_PreviewSnapLoc        = 0x19B0;   // FVector (LWC)
constexpr unsigned Controller_PreviewSnapRot        = 0x19C8;   // FRotator (LWC)
constexpr unsigned Controller_LocationUnderReticle  = 0x24C0;
constexpr unsigned Controller_InteractionComponent  = 0x28D8;
constexpr unsigned Controller_CosmeticLoadoutPC     = 0x1E70;   // FFortAthenaLoadout
constexpr unsigned AFortPickup_PrimaryEntry         = 0x02F8;
constexpr unsigned AFortPickup_SourceFlags          = 0x0540;
constexpr unsigned AFortPickup_SpawnSource          = 0x0541;
constexpr unsigned AFortGameState_GamePhase         = 0x2150;
constexpr unsigned AFortGameState_SafeZoneIndicator = 0x19A8;
constexpr unsigned AFortGameState_PlaylistInfo      = 0x2170;   // FPlaylistPropertyArray
constexpr unsigned EditingTool_EditActor            = 0x1020;
constexpr unsigned Pickaxe_WeaponDefinition         = 0x0790;
}

// [G16-f] Apply a build profile to the live namespace. Called once from the
// worker thread after the build ID is read — BEFORE any hook installation or
// reflection use.
static void InitProfile(const BuildProfile& p) {
    profile::ProcessEvent         = p.ProcessEvent;
    profile::StaticFindObject     = p.StaticFindObject;
    profile::StaticLoadObject     = p.StaticLoadObject;
    profile::StaticLoadClass      = p.StaticLoadClass;
    profile::NameAppendString     = p.NameAppendString;
    profile::Free                 = p.Free;
    profile::WorldTick            = p.WorldTick;
    profile::EngineExec           = p.EngineExec;
    profile::IsStandalone         = p.IsStandalone;
    profile::GetPlayerController  = p.GetPlayerController;
    profile::GetPawnRva           = p.GetPawn;
    profile::PossessRva           = p.Possess;
    profile::BeginSpawn           = p.BeginSpawn;
    profile::FinishSpawn          = p.FinishSpawn;
    profile::SpawnObject          = p.SpawnObject;
    profile::GenericArrayAdd      = p.GenericArrayAdd;
    profile::GenericArrayRemove   = p.GenericArrayRemove;
    profile::IsGarbageCollecting  = p.IsGarbageCollecting;
    profile::IsSavingPackage      = p.IsSavingPackage;
    profile::GObjects             = p.GObjects;
    profile::Realloc              = p.Realloc;
    profile::CopyScriptStruct     = p.CopyScriptStruct;
    profile::GetInterfaceAddress  = p.GetInterfaceAddress;
    profile::InitializeAthenaQuickBars = p.InitializeAthenaQuickBars;
    profile::QuickBarResize       = p.QuickBarResize;
    profile::FortniteVersion      = p.fortniteVersion;
    profile::RawOffsetsVerified   = (p.fortniteVersion >= 20);
    profile::HasFField            = (p.FFieldNext != 0);
    profile::ObjectClass  = p.ObjectClass;
    profile::ObjectName   = p.ObjectName;
    profile::ObjectOuter  = p.ObjectOuter;
    profile::FieldNext    = p.FieldNext;
    profile::StructSuper  = p.StructSuper;
    profile::StructChildren   = p.StructChildren;
    profile::StructChildProps = p.StructChildProps;
    profile::StructPropertiesSize = p.StructPropertiesSize;
    profile::FunctionParamSize = p.FunctionParamSize;
    profile::PropFlags       = p.PropFlags;
    profile::PropOffset      = p.PropOffset;
    profile::PropElementSize = p.PropElementSize;
    profile::PropArrayDim    = p.PropArrayDim;
    profile::BoolSize        = p.BoolSize;
    profile::BoolByteOffset  = p.BoolByteOffset;
    profile::BoolFieldMask   = p.BoolFieldMask;
    profile::FFieldName = p.FFieldName;
    profile::FFieldNext = p.FFieldNext;
}

extern "C" int DobbyHook(void* target, void* replace, void** origin);

// ============================================================================
// MEMORY HELPERS
// ============================================================================
static inline bool IsBadPtr(const void* p) {
    if (!p) return true;
    uintptr_t v = (uintptr_t)p;
    if (v < 0x10000) return true;
    if (v > 0x0000FFFFFFFFFFFFULL) return true;
    return false;
}
template<class T> static T Read(const void* p, size_t off = 0) {
    T r{}; if (p) memcpy(&r, (const char*)p + off, sizeof(T)); return r;
}
template<class T> static void Write(void* p, size_t off, const T& v) {
    if (p) memcpy((char*)p + off, &v, sizeof(T));
}

// [F35] Deep pointer probe — IsBadPtr only validates the VALUE range. On
// Android, dereferencing a freed/unmapped pointer that happens to sit inside
// the valid range range is a SIGSEGV (the "crash on exiting build mode"
// class of bugs). Probing through write(/dev/null) makes the KERNEL validate
// readability without ever touching the memory from our thread.
static int g_devnull_fd = -1;
static bool IsReadable(const void* p, size_t n = 8) {
    if (!p || IsBadPtr(p)) return false;
    if (g_devnull_fd < 0) g_devnull_fd = open("/dev/null", O_WRONLY);
    if (g_devnull_fd < 0) return true;               // cannot probe — assume ok
    ssize_t r = write(g_devnull_fd, p, n);
    return r == (ssize_t)n || (r < 0 && errno != EFAULT);
}

static uintptr_t g_base = 0;
static std::atomic<int> g_status{0};
static std::atomic<int> g_gameTid{0};
static thread_local bool g_internal = false;

struct Guard { bool s = g_internal; Guard() { g_internal = true; } ~Guard() { g_internal = s; } };
template<class F> static F Fn(uintptr_t rva) { return reinterpret_cast<F>(g_base + rva); }

// [G16-f] Read the GNU build ID from the loaded libUnreal.so image: walk the
// program headers for PT_NOTE, then the note records for NT_GNU_BUILD_ID
// (owner "GNU\0", type 3). Returns the 40-char lowercase hex string (or "").
static std::string ReadBuildId() {
    if (!g_base) return {};
    // ELF64 header: e_phoff @0x20, e_phentsize @0x36, e_phnum @0x38
    uint64_t phoff = Read<uint64_t>((void*)g_base, 0x20);
    uint16_t phentsize = Read<uint16_t>((void*)g_base, 0x36);
    uint16_t phnum = Read<uint16_t>((void*)g_base, 0x38);
    if (!phoff || !phnum || phnum > 64) return {};
    if (phentsize && phentsize < sizeof(Elf64_Phdr)) return {};
    for (uint16_t ph = 0; ph < phnum; ++ph) {
        Elf64_Phdr hdr = Read<Elf64_Phdr>((void*)(g_base + phoff), (size_t)ph * sizeof(Elf64_Phdr));
        if (hdr.p_type != PT_NOTE || !hdr.p_memsz || hdr.p_memsz > 0x10000) continue;
        uint8_t* note = (uint8_t*)(g_base + hdr.p_vaddr);
        uint8_t* end  = note + hdr.p_memsz;
        while (note + 12 <= end) {
            uint32_t namesz = Read<uint32_t>(note, 0);
            uint32_t descsz = Read<uint32_t>(note, 4);
            uint32_t type   = Read<uint32_t>(note, 8);
            if (type == 3 && descsz >= 16 && descsz <= 32 &&
                namesz >= 4 && memcmp(note + 12, "GNU", 4) == 0) {
                static const char* hex = "0123456789abcdef";
                std::string out;
                for (uint32_t i = 0; i < descsz; ++i) {
                    uint8_t b = note[12 + ((namesz + 3) & ~3u) + i];
                    out.push_back(hex[b >> 4]);
                    out.push_back(hex[b & 15]);
                }
                return out;
            }
            size_t step = 12 + ((namesz + 3) & ~3u) + ((descsz + 3) & ~3u);
            if (step <= 12) break;
            note += step;
        }
    }
    return {};
}

// [G16-f] The active profile (set by DetectAndApplyProfile before use).
static const BuildProfile* s_activeProfile = &kProfiles[0];

// [G16-f] Detect + apply. Returns the selected profile index (0 = newest).
static int DetectAndApplyProfile() {
    std::string id = ReadBuildId();
    const BuildProfile* sel = nullptr;
    char matched = 0;
    if (!id.empty()) {
        for (const auto& p : kProfiles)
            if (id == p.buildId) { sel = &p; matched = 1; break; }
    }
    if (!sel) sel = &kProfiles[0];   // unknown build → newest profile
    s_activeProfile = sel;
    InitProfile(*sel);
    LOG("[Profile] build-id=%s", id.empty() ? "<unreadable>" : id.c_str());
    if (matched)
        LOG("[Profile] ✅ matched %s — full profile applied", sel->name);
    else
        LOG("[Profile] ⚠️ UNRECOGNIZED build — using %s defaults (string-scanned natives still adapt; report this build-id to add a profile)",
            sel->name);
    LOG("[Profile] FortniteVersion=%d FField=%s raw-offsets=%s",
        profile::FortniteVersion, profile::HasFField ? "yes" : "no",
        profile::RawOffsetsVerified ? "verified" : "DISABLED (engine-core only)");
    return matched ? (int)(sel - kProfiles) : -1;
}


// ============================================================================
// [G01] ARM64 RUNTIME SCANNER — Erbium's Memcury (FindStringRef/ScanFor/
// RelativeOffset) ported to the Android loader model. No new hardcoded RVAs:
// the engine's own log literals locate the stripped native functions.
//   • PT_LOAD discovery through the SAME dl_iterate_phdr walk that finds
//     g_base (exec + data ranges of libUnreal.so).
//   • FindString: literal search in the loaded image, in BOTH encodings UE
//     uses on Android (TCHAR literals are UTF-16; some paths are ASCII).
//   • FindAdrpAddRef: ADRP Xd,page + ADD Xd,Xd,#lo12 xref resolution — the
//     ARM64 equivalent of Erbium's "48 8D 05/0D rel32" LEA scan.
//   • BackscanPrologue: walk back from the reference to the containing
//     function's frame setup (STP X29,X30,[SP..] / SUB SP,SP), validated by
//     a terminator boundary (RET/B/BR/NOP) — Erbium's prologue backscan.
//   • NextBlTarget: BL rel26 resolution for intra-function call discovery.
// Everything is read-only, one-time, and runs on the Worker thread.
// ============================================================================
namespace arm {
struct Range { uintptr_t start; uintptr_t end; bool exec; };
static std::vector<Range> s_ranges;
static bool s_ready = false;

static int PhdrCb(dl_phdr_info* info, size_t, void*) {
    const char* name = strrchr(info->dlpi_name, '/');
    name = name ? name + 1 : info->dlpi_name;
    if (strcmp(name, "libUnreal.so") || !info->dlpi_addr) return 0;
    for (int i = 0; i < info->dlpi_phnum; ++i) {
        const Elf64_Phdr& ph = info->dlpi_phdr[i];
        if (ph.p_type != PT_LOAD) continue;
        uintptr_t start = info->dlpi_addr + ph.p_vaddr;
        uintptr_t end = start + ph.p_memsz;
        if (end <= start || end - start > 0x80000000ULL) continue;
        if (!IsReadable((void*)start, 0x40)) continue;
        s_ranges.push_back({start, end, (ph.p_flags & PF_X) != 0});
    }
    return 1;
}
static bool Init() {
    if (s_ready) return !s_ranges.empty();
    s_ranges.clear();
    dl_iterate_phdr(PhdrCb, nullptr);
    s_ready = true;
    size_t total = 0, execSz = 0;
    for (auto& r : s_ranges) { total += r.end - r.start; if (r.exec) execSz += r.end - r.start; }
    LOG("[ARM] ranges=%zu total=%.1fMB exec=%.1fMB", s_ranges.size(), total / 1048576.0, execSz / 1048576.0);
    return !s_ranges.empty();
}

// Literal search. ASCII first (cheap), then UTF-16 (UE TCHAR literals).
static uintptr_t FindStringAscii(const char* needle, size_t len) {
    for (const Range& r : s_ranges) {
        if (r.end - r.start < len) continue;
        const char* p = (const char*)r.start;
        const char* e = (const char*)r.end - len;
        while (p <= e) {
            const char* hit = (const char*)memchr(p, needle[0], (size_t)(e - p) + 1);
            if (!hit) break;
            if (memcmp(hit, needle, len) == 0) return (uintptr_t)hit;
            p = hit + 1;
        }
    }
    return 0;
}
static uintptr_t FindStringUtf16(const char* needle, size_t len) {
    std::vector<char16_t> wide;
    wide.reserve(len);
    for (size_t i = 0; i < len; ++i) wide.push_back((char16_t)(unsigned char)needle[i]);
    for (const Range& r : s_ranges) {
        if (r.end - r.start < len * 2) continue;
        const char16_t* p = (const char16_t*)r.start;
        const char16_t* e = (const char16_t*)r.end - len;
        while (p <= e) {
            const char16_t* hit = std::find(p, e + 1, wide[0]);
            if (hit == e + 1) break;
            if (memcmp(hit, wide.data(), len * 2) == 0) return (uintptr_t)hit;
            p = hit + 1;
        }
    }
    return 0;
}
static uintptr_t FindString(const char* needle) {
    if (!needle || !*needle) return 0;
    size_t len = strlen(needle);
    // NUL-terminated match (both encodings) so prefixes of longer literals
    // never false-positive.
    uintptr_t a = FindStringAscii(needle, len + 1);
    if (a) return a;
    a = FindStringUtf16(needle, len + 1);
    if (a) return a;
    // [G13] Substring fallback, UTF-16 first. Device-proven with the FString
    // layout { char16_t* Data; int32 Length; int32 Max }: every TEXT()
    // literal in libUnreal.so is UTF-16LE, and "MCP-Profile: Dispatching
    // request to %s" only exists as the PREFIX of the longer
    // "... - ContextCredentials: %s" — the terminator-terminated match
    // refused to see it and reported "string not resident".
    a = FindStringUtf16(needle, len);
    if (a) return a;
    return FindStringAscii(needle, len);
}

// ADRP decode: page-relative immediate. page = (pc & ~0xFFF) + (imm << 12).
static bool DecodeAdrp(uint32_t w, uintptr_t pc, uintptr_t& page) {
    if ((w & 0x9F000000) != 0x90000000) return false;            // ADRP
    int64_t imm = (int64_t)((w >> 29) & 0x3) | ((int64_t)((w >> 5) & 0x7FFFF) << 2);
    if (imm & (1 << 20)) imm -= (int64_t)1 << 21;
    page = (pc & ~0xFFFULL) + ((uintptr_t)imm << 12);
    return true;
}
// [G12] Strict form (original): ADD Xd,Xd,#imm back-to-back with the ADRP,
// same destination register. Kept as the PRIMARY matcher so every address
// the first device run already resolved (init=1) keeps resolving to the
// exact same function — a relaxed-first order could shift a hit to an
// earlier ADRP+ADD belonging to a different (inlined) caller.
static uintptr_t FindAdrpAddRefStrict(uintptr_t target, uintptr_t scanStart, uintptr_t scanEnd) {
    uintptr_t page = target & ~0xFFFULL;
    for (const Range& r : s_ranges) {
        if (!r.exec) continue;
        uintptr_t begin = scanStart > r.start ? scanStart : r.start;
        begin &= ~3ULL;
        uintptr_t end = r.end < scanEnd ? r.end : scanEnd;
        for (uintptr_t pc = begin; pc + 8 <= end; pc += 4) {
            uint32_t adrp = *(uint32_t*)pc;                        // exec range: mapped
            uintptr_t pg = 0;
            if (!DecodeAdrp(adrp, pc, pg) || pg != page) continue;
            uint32_t add = *(uint32_t*)(pc + 4);
            if ((add & 0xFF000000) != 0x91000000) continue;        // ADD Xd,Xn,#imm (64-bit)
            uint32_t rd = adrp & 0x1F;
            if ((add & 0x1F) != rd || ((add >> 5) & 0x1F) != rd) continue;
            uint64_t imm = (add >> 10) & 0xFFF;
            if ((add >> 22) & 1) imm <<= 12;
            if (pg + imm == target) return pc;
        }
    }
    return 0;
}
// [G12] Relaxed form: ADRP Xn,page followed within 4 instructions by
// ADD Xd,Xn,#imm where Rd may differ from the ADRP destination (the
// standard argument-materialization shape). The imm12 still pins the exact
// byte address, so false positives stay vanishingly rare. Device proof it
// is required: gaa/ita/acc/mcp strings were RESIDENT yet "no code ref".
static uintptr_t FindAdrpAddRefRelaxed(uintptr_t target, uintptr_t scanStart, uintptr_t scanEnd) {
    uintptr_t page = target & ~0xFFFULL;
    for (const Range& r : s_ranges) {
        if (!r.exec) continue;
        uintptr_t begin = scanStart > r.start ? scanStart : r.start;
        begin &= ~3ULL;
        uintptr_t end = r.end < scanEnd ? r.end : scanEnd;
        for (uintptr_t pc = begin; pc + 8 <= end; pc += 4) {
            uint32_t adrp = *(uint32_t*)pc;
            uintptr_t pg = 0;
            if (!DecodeAdrp(adrp, pc, pg) || pg != page) continue;
            uint32_t rn = adrp & 0x1F;                            // ADRP destination
            for (int k = 1; k <= 4; ++k) {
                uintptr_t apc = pc + 4 * (size_t)k;
                if (apc + 4 > end) break;
                uint32_t add = *(uint32_t*)apc;
                if ((add & 0xFF000000) != 0x91000000) continue;   // ADD Xd,Xn,#imm
                if (((add >> 5) & 0x1F) != rn) continue;           // source = ADRP dest
                uint64_t imm = (add >> 10) & 0xFFF;
                if ((add >> 22) & 1) imm <<= 12;
                if (pg + imm == target) return pc;
            }
        }
    }
    return 0;
}
static uintptr_t FindAdrpAddRef(uintptr_t target, uintptr_t scanStart, uintptr_t scanEnd) {
    if (uintptr_t strict = FindAdrpAddRefStrict(target, scanStart, scanEnd)) return strict;
    return FindAdrpAddRefRelaxed(target, scanStart, scanEnd);
}

// Frame-setup + boundary detection.
// [G13] v2 — the device run proved the old rule (STP X29,X30 only) wrong for
// this build: ProcessEvent itself opens with STP X28,X27,[SP,#-N]!
// (word0=0xA9BA6FFC), and so do most large Fortnite natives — the frame
// pointer pair is pushed only after the outer register pairs. The old
// backscan skipped those prologues, walked back to an EARLIER unrelated
// function, and that wrong address is what "native call FAULTED at fn+0x24"
// was: a random earlier function executed with a bogus `this`. v2 accepts
// any prologue that opens with a callee-saved-pair pre/post-index writeback
// STP (x19-x30), PACIASP/PACIBSP, SUB SP,SP,#imm, or the classic STP x29,x30
// forms, and separates STRONG boundaries (RET/B/BR/BLR/NOP) from weak ones
// (B.cond — fall-through code follows it, so it is not a function end).
static bool IsStpX29X30Sp(uint32_t w) {
    if ((w & 0x1F) != 29 || ((w >> 5) & 0x1F) != 31 || ((w >> 10) & 0x1F) != 30) return false;
    uint32_t op = (w >> 22) & 0x3FF;
    return op == 0x2A2 || op == 0x2A6 || op == 0x2AA;   // post/pre/signed STP
}
static bool IsSubSpImm(uint32_t w) {
    return (w & 0xFF8003E0) == 0xD10003E0 && (w & 0x1F) == 31;   // SUB SP,SP,#imm
}
static bool IsPacHint(uint32_t w) {                    // PACIASP / PACIBSP
    return w == 0xD503231F || w == 0xD503233F;
}
// STP Xt1,Xt2,[SP,#-imm]! / [SP],#imm — 64-bit pair, SP base, BOTH registers
// callee-saved (x19-x30). Prologue pushes are callee-saved by definition;
// mid-function spills usually hit caller-saved registers, which is what
// keeps this matcher off inner code.
static bool IsCalleeSavedStpSp(uint32_t w) {
    uint32_t top = w & 0xFFC00000;
    if (top != 0xA9800000 && top != 0xA8800000) return false;  // pre/post-index WB
    if (((w >> 5) & 0x1F) != 31) return false;                 // [SP] base
    uint32_t rt1 = w & 0x1F, rt2 = (w >> 10) & 0x1F;
    return rt1 >= 19 && rt1 <= 30 && rt2 >= 19 && rt2 <= 30;
}
static bool IsPrologueOpener(uint32_t w) {
    return IsCalleeSavedStpSp(w) || IsPacHint(w) || IsSubSpImm(w) || IsStpX29X30Sp(w);
}
static bool IsTerminator(uint32_t w) {
    if (w == 0xD65F03C0 || (w & 0xFFFFFC1F) == 0xD65F0000) return true;  // RET
    if ((w & 0xFC000000) == 0x14000000) return true;                     // B imm
    if ((w & 0xFF000010) == 0x54000000) return true;                     // B.cond
    if ((w & 0xFFFFFC1F) == 0xD61F0000) return true;                     // BR
    if ((w & 0xFFFFFC1F) == 0xD63F0000) return true;                     // BLR
    if (w == 0xD503201F) return true;                                    // NOP padding
    return false;
}
static bool IsStrongTerminator(uint32_t w) {
    if ((w & 0xFF000010) == 0x54000000) return false;    // B.cond: weak only
    return IsTerminator(w);
}
// [G13] Non-leaf confirmation: within the 16 words after a candidate start,
// a frame pair (STP x29,x30) or another SP writeback push must appear — the
// string-referencing functions we hunt always set up a frame. Candidates
// with confirmation win over plain openers; leaf-shaped helpers lose.
static bool HasFrameConfirmation(uintptr_t start) {
    for (int i = 0; i < 16; ++i) {
        if (!IsReadable((void*)(start + (size_t)i * 4), 4)) return false;
        uint32_t w = *(uint32_t*)(start + (size_t)i * 4);
        if (IsStpX29X30Sp(w) || IsCalleeSavedStpSp(w)) return true;
    }
    return false;
}
// Walk back to the containing function's start (boundary-validated, v2).
// Scan order (nearest-first from the string ref, one 0x40000 window):
//   1. strong boundary + opener WITH frame confirmation
//   2. strong boundary + opener (unconfirmed)
//   3. weak boundary (B.cond) + opener
static uintptr_t BackscanPrologue(uintptr_t ref) {
    uintptr_t limit = ref > 0x40000 ? ref - 0x40000 : g_base;
    if (limit < g_base + 8) limit = g_base + 8;
    uintptr_t unconfirmed = 0, weak = 0;
    for (uintptr_t pc = (ref & ~3ULL) - 4; pc >= limit; pc -= 4) {
        if (!IsReadable((void*)pc, 8)) break;
        uint32_t w = *(uint32_t*)pc;
        bool strong = IsStrongTerminator(w);
        if (!strong && !weak && IsTerminator(w)) {          // B.cond — weak
            if (IsReadable((void*)(pc + 4), 4) && IsPrologueOpener(*(uint32_t*)(pc + 4)))
                weak = pc + 4;
            continue;
        }
        if (!strong) continue;
        if (!IsReadable((void*)(pc + 4), 4)) continue;
        if (!IsPrologueOpener(*(uint32_t*)(pc + 4))) continue;
        uintptr_t cand = pc + 4;
        if (HasFrameConfirmation(cand)) return cand;        // confirmed — done
        if (!unconfirmed) unconfirmed = cand;
    }
    return unconfirmed ? unconfirmed : weak;
}
// Nth BL target at/after `from` within `window` bytes.
static uintptr_t NextBlTarget(uintptr_t from, size_t window, int nth) {
    for (uintptr_t pc = from; pc < from + window; pc += 4) {
        if (!IsReadable((void*)pc, 4)) break;
        uint32_t w = *(uint32_t*)pc;
        if ((w & 0xFC000000) != 0x94000000) continue;              // BL rel26
        int64_t imm = (int64_t)(w & 0x3FFFFFF);
        if (imm & (1 << 25)) imm -= (int64_t)1 << 26;
        if (--nth <= 0) return pc + ((uintptr_t)imm << 2);
    }
    return 0;
}
// Composite: string → ADRP+ADD ref → containing function start.
static uintptr_t FindFunctionByString(const char* needle, int useRefNum = 0) {
    if (!Init()) return 0;
    uintptr_t str = FindString(needle);
    if (!str) { LOG("[ARM] string not resident: %.60s", needle); return 0; }
    uintptr_t scanStart = 0, scanEnd = UINTPTR_MAX;
    for (const Range& r : s_ranges) if (r.exec) { scanStart = r.start; scanEnd = r.end; break; }
    uintptr_t ref = 0;
    for (int i = 0; i <= useRefNum && scanStart; ++i) {
        ref = FindAdrpAddRef(str, ref ? ref + 4 : scanStart, scanEnd);
        if (!ref) break;
    }
    if (!ref) { LOG("[ARM] no code ref to: %.60s", needle); return 0; }
    uintptr_t fn = BackscanPrologue(ref);
    if (fn) {
        // [G13] Log the first two words so the next device run can be
        // verified from logcat alone (opener shape + following instruction).
        uint32_t w0 = Read<uint32_t>((void*)fn);
        uint32_t w1 = IsReadable((void*)(fn + 4), 4) ? Read<uint32_t>((void*)(fn + 4)) : 0;
        LOG("[ARM] ✅ %.48s -> fn+0x%llX [w0=%08x w1=%08x]",
            needle, (unsigned long long)(fn - g_base), w0, w1);
    }
    else LOG("[ARM] ⚠️ ref found but no prologue for: %.48s", needle);
    return fn;
}
} // namespace arm

// ============================================================================
// UE5 STRUCTS (LWC — 21.30 uses double-precision FVector/FRotator)
// ============================================================================
struct FStringNative { TCHAR* Data; int32 Count; int32 Max; };
struct FName         { uint32 CompIdx; uint32 DispIdx; };
struct Array         { void* Data; int32 Count; int32 Max; };
struct Guid          { uint32 A, B, C, D; };
struct Vector        { double X, Y, Z; };
struct Rotator       { double Pitch, Yaw, Roll; };

struct alignas(16) Transform {
    double Rotation[4]{0, 0, 0, 1};
    double Location[3]{0, 0, 0};
    uint8_t _padL[8]{};
    double Scale[3]{1, 1, 1};
    uint8_t _padS[8]{};
};
static_assert(sizeof(Vector) == 24, "Vector LWC");
static_assert(sizeof(Transform) == 96, "Transform LWC");

struct FUObjectItem { void* Object; int32 Flags; int32 Cluster; int32 Serial; };
struct GObjects_t {
    uint8 _pad0[0x10];
    void** Objects;
    uint8 _pad1[0x8];
    int32 MaxElements;
    int32 NumElements;
    int32 MaxChunks;
    int32 NumChunks;
    void* GetById(uint32 id) {
        if (!Objects) return nullptr;
        uint32 ci = id / 0x10000, wi = id % 0x10000;
        if ((int32)ci >= NumChunks) return nullptr;
        auto* chunk = (FUObjectItem*)Objects[ci];
        if (!chunk) return nullptr;
        return chunk[wi].Object;
    }
    // [F28] Weak-pointer resolution needs the full item (serial check).
    FUObjectItem* GetItem(uint32 id) {
        if (!Objects) return nullptr;
        uint32 ci = id / 0x10000, wi = id % 0x10000;
        if ((int32)ci >= NumChunks) return nullptr;
        auto* chunk = (FUObjectItem*)Objects[ci];
        if (!chunk) return nullptr;
        return &chunk[wi];
    }
};
struct TScriptInterface { void* Object; void* Interface; };

// ============================================================================
// FORWARD DECLS
// ============================================================================
static void* FindObj(const std::string& path);
static std::string ObjName(void* obj);
namespace local_loot { int TableCount(); }   // [F92] heartbeat accessor
static bool IsA(void* obj, void* type);
static bool SubclassOfFast(void* klass, void* targetClass);
static void* GetProp(const char* path);
static void* GetField(void* obj, void* prop);
static void SetBool(void* obj, const char* path, bool enabled);
static std::u16string ToWide(const std::string& s);
static std::string Narrow(const char16_t* s, int max);
static void* GetObjField(void* obj, const char* path);
static void* LoadAsset(const std::string& path, const char* type_path);
static void* LoadClass(const char* path, const char* base_path);
static void* GetPC(void* world, int index);
static void* incoming(void* function, void* data, const char* key, int expected_size = 0);
static GObjects_t* GObjectArray();

// ============================================================================
// TYPE CHECKS
// ============================================================================
static bool SubclassOfFast(void* klass, void* targetClass) {
    if (!klass || !targetClass || IsBadPtr(klass) || IsBadPtr(targetClass)) return false;
    for (int d = 0; d < 64; ++d) {
        if (klass == targetClass) return true;
        void* sup = Read<void*>(klass, profile::StructSuper);
        if (!sup || sup == klass || IsBadPtr(sup)) break;
        klass = sup;
    }
    return false;
}
static bool IsA(void* obj, void* type) {
    if (!obj || !type || IsBadPtr(obj) || IsBadPtr(type)) return false;
    void* klass = Read<void*>(obj, profile::ObjectClass);
    if (!klass || IsBadPtr(klass)) return false;
    return SubclassOfFast(klass, type);
}
// [G15-c] Vehicle target probe for the interact hook: AFortAthenaVehicle
// (Valet/Nevada/Meatball BP vehicles derive from it) or AFortCharacterVehicle.
static bool IsVehicleTarget(void* klass) {
    if (!klass || IsBadPtr(klass)) return false;
    static void* s_athenaVeh = nullptr, *s_charVeh = nullptr;
    static bool s_tried = false;
    if (!s_tried) {
        s_tried = true;
        s_athenaVeh = FindObj("/Script/FortniteGame.FortAthenaVehicle");
        s_charVeh  = FindObj("/Script/FortniteGame.FortCharacterVehicle");
    }
    if (s_athenaVeh && SubclassOfFast(klass, s_athenaVeh)) return true;
    if (s_charVeh && SubclassOfFast(klass, s_charVeh)) return true;
    return false;
}
static GObjects_t* GObjectArray() {
    if (!g_base) return nullptr;
    GObjects_t* go = Read<GObjects_t*>((void*)(g_base + profile::GObjects));
    if (!go || IsBadPtr(go)) return nullptr;
    if (go->NumElements < 0 || go->NumElements > 4000000) return nullptr;
    if (go->NumChunks < 0 || go->NumChunks > 128) return nullptr;
    return go;
}

// ============================================================================
// STRING HELPERS
// ============================================================================
static std::u16string ToWide(const std::string& s) { return std::u16string(s.begin(), s.end()); }
static std::string Narrow(const char16_t* s, int max) {
    std::string out;
    if (s) for (int i = 0; i < max && s[i]; ++i) out.push_back(s[i] < 128 ? (char)s[i] : '?');
    return out;
}
static void* FindObj(const std::string& path) {
    auto w = ToWide(path);
    return Fn<void*(*)(void*, void*, const char16_t*, bool)>(profile::StaticFindObject)(nullptr, nullptr, w.c_str(), false);
}
static bool SafeFieldName(void* field, uint32 nameOff, std::string& out) {
    if (!field || IsBadPtr(field)) return false;
    uint32 compIdx = Read<uint32>(field, nameOff + 0);
    uint32 dispIdx = Read<uint32>(field, nameOff + 4);
    if (compIdx == 0xFFFFFFFF || compIdx > 0x8000000) return false;
    if (dispIdx == 0xFFFFFFFF || dispIdx > 0x8000000) return false;
    FStringNative ns{};
    Fn<void(*)(const void*, FStringNative*)>(profile::NameAppendString)((char*)field + nameOff, &ns);
    out = Narrow(ns.Data, 128);
    if (ns.Data) Fn<void(*)(void*)>(profile::Free)(ns.Data);
    return true;
}
static std::string ObjName(void* obj) {
    if (!obj || IsBadPtr(obj)) return {};
    uint32 compIdx = Read<uint32>(obj, profile::ObjectName + 0);
    uint32 dispIdx = Read<uint32>(obj, profile::ObjectName + 4);
    if (compIdx == 0xFFFFFFFF || compIdx > 0x8000000) return {};
    if (dispIdx == 0xFFFFFFFF || dispIdx > 0x8000000) return {};
    static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
    static std::map<void*, std::string> cache;
    pthread_mutex_lock(&m);
    auto it = cache.find(obj);
    if (it != cache.end()) { std::string r = it->second; pthread_mutex_unlock(&m); return r; }
    pthread_mutex_unlock(&m);
    FStringNative s{};
    Fn<void(*)(const void*, FStringNative*)>(profile::NameAppendString)((char*)obj + profile::ObjectName, &s);
    std::string out = Narrow(s.Data, std::min(std::max(s.Count, 0), 512));
    if (s.Data) Fn<void(*)(void*)>(profile::Free)(s.Data);
    pthread_mutex_lock(&m);
    if (cache.size() < 200000) cache[obj] = out;
    pthread_mutex_unlock(&m);
    return out;
}
// [F17/F20] Find an item definition by its short name through GObjects
// (ported from the Era core by_short_name()).
static void* ByShortName(const std::string& wanted) {
    GObjects_t* go = GObjectArray();
    if (!go) return nullptr;
    void* definition_type = FindObj("/Script/FortniteGame.FortItemDefinition");
    if (!definition_type) return nullptr;
    for (int32 i = 0; i < go->NumElements; i += 1) {
        void* object = go->GetById((uint32)i);
        if (object && !IsBadPtr(object) && IsA(object, definition_type) && ObjName(object) == wanted)
            return object;
    }
    return nullptr;
}

// [F41/F83-b] Find a LOADED object of a type by short name (AnimBPs stream
// in late — a package-path FindObj misses them until they are resident).
static void* FindLoadedObjectOfType(const char* type_path, const std::string& wanted) {
    GObjects_t* go = GObjectArray();
    void* type = FindObj(type_path);
    if (!go || !type) return nullptr;
    for (int32 i = 0; i < go->NumElements; ++i) {
        void* object = go->GetById((uint32)i);
        if (!object || IsBadPtr(object)) continue;
        if (!IsA(object, type)) continue;
        if (ObjName(object) == wanted) return object;
    }
    return nullptr;
}

// [F52/F83-b] Find a GameplayAbility subclass by short name (BP ability
// classes are not FindObj-able by package path before they stream in).
static void* FindAbilityClassNamed(const std::string& wanted) {
    GObjects_t* go = GObjectArray();
    void* gaCls = FindObj("/Script/GameplayAbilities.GameplayAbility");
    void* classClass = FindObj("/Script/CoreUObject.Class");
    if (!go || !gaCls || !classClass) return nullptr;
    for (int32 i = 0; i < go->NumElements; ++i) {
        void* object = go->GetById((uint32)i);
        if (!object || IsBadPtr(object)) continue;
        if (!IsA(object, classClass)) continue;
        if (!SubclassOfFast(object, gaCls)) continue;
        if (ObjName(object) == wanted) return object;
    }
    return nullptr;
}

// ============================================================================
// PROPERTY ACCESS
// ============================================================================
static void* GetProp(const char* path) {
    if (!path) return nullptr;
    std::string s(path);
    if (void* direct = FindObj(s)) return direct;
    auto pos = s.rfind('.');
    if (pos == std::string::npos) return nullptr;
    std::string classPath = s.substr(0, pos);
    std::string propName  = s.substr(pos + 1);
    if (void* alt = FindObj(classPath + ":" + propName)) return alt;
    void* klass = FindObj(classPath);
    if (!klass || IsBadPtr(klass)) return nullptr;
    for (int depth = 0; depth < 32 && klass && !IsBadPtr(klass); ++depth) {
        void* f = (profile::HasFField ? Read<void*>(klass, profile::StructChildProps) : nullptr);
        for (int i = 0; f && !IsBadPtr(f) && i < 1024; ++i) {
            std::string n;
            if (SafeFieldName(f, profile::FFieldName, n) && n == propName) return f;
            f = Read<void*>(f, profile::FFieldNext);
        }
        void* c = Read<void*>(klass, profile::StructChildren);
        for (int i = 0; c && !IsBadPtr(c) && i < 1024; ++i) {
            std::string n;
            if (SafeFieldName(c, profile::ObjectName, n) && n == propName) return c;
            c = Read<void*>(c, profile::FieldNext);
        }
        klass = Read<void*>(klass, profile::StructSuper);
    }
    return nullptr;
}
static void* GetField(void* obj, void* prop) {
    if (!obj || !prop) return nullptr;
    return (char*)obj + Read<int>(prop, profile::PropOffset);
}
static void SetBool(void* obj, const char* path, bool enabled) {
    void* p = GetProp(path);
    if (!p || !obj) return;
    uint8_t size = Read<uint8_t>(p, profile::BoolSize);
    uint8_t byte = Read<uint8_t>(p, profile::BoolByteOffset);
    uint8_t mask = Read<uint8_t>(p, profile::BoolFieldMask);
    if (!size || size > 8 || byte >= size || !mask) return;
    auto dst = (uint8_t*)GetField(obj, p) + byte;
    *dst = enabled ? (*dst | mask) : (*dst & ~mask);
}
static bool GetBool(void* obj, const char* path) {
    void* p = GetProp(path);
    if (!p || !obj) return false;
    uint8_t size = Read<uint8_t>(p, profile::BoolSize);
    uint8_t byte = Read<uint8_t>(p, profile::BoolByteOffset);
    uint8_t mask = Read<uint8_t>(p, profile::BoolFieldMask);
    if (!size || size > 8 || byte >= size || !mask) return false;
    return (Read<uint8_t>(GetField(obj, p), byte) & mask) != 0;
}
static void* GetObjField(void* obj, const char* path) {
    void* p = GetProp(path);
    if (!p || !obj) return nullptr;
    if (Read<int>(p, profile::PropElementSize) != 8) return nullptr;
    return Read<void*>(GetField(obj, p));
}
template<class T> static bool GetFieldValue(void* obj, const char* path, T& out) {
    void* p = GetProp(path);
    if (!p || !obj || Read<int>(p, profile::PropElementSize) != sizeof(T)) return false;
    out = Read<T>(GetField(obj, p));
    return true;
}

// ============================================================================
// HOOK TYPEDEFS
// ============================================================================
using PE_t   = void(*)(void*, void*, void*);
using Tick_t = void(*)(void*, int, float);
using Exec_t = bool(*)(void*, void*, const char16_t*, void*);

static PE_t   orig_PE = nullptr;
static Tick_t orig_Tick = nullptr;
static Exec_t orig_Exec = nullptr;

// [G05] MCP PROFILE DISPATCH — Erbium Misc::SendRequestNow 1:1.
// The standalone client prepares profile/loadout queries but never dispatches
// them with public credentials, so the "MCP Profile phase" never loads. The
// hook forces the context credentials argument to 3 (CXC_Public) on every
// dispatch — exactly Erbium's UE5-era patch (the struct write was only needed
// pre-4.23; 21.30 passes credentials as the third argument). The hook itself
// is installed in InstallHooks only when the [G01] scanner located the native.
using SendRequestNow_t = void* (*)(void*, void*, int);
static SendRequestNow_t orig_SendRequestNow = nullptr;
static void* hook_SendRequestNow(void* Arg1, void* MCPData, int /*ContextCredentials*/) {
    if (orig_SendRequestNow) return orig_SendRequestNow(Arg1, MCPData, 3);   // CXC_Public
    return nullptr;
}
// [G12] Defined near InstallHooks; forward-declared here for the late
// terrain re-scan path in tick_hook.
static bool TryInstallMcpHook();

// ============================================================================
// [G11/G13/G23] NATIVE FAULT GUARD — scoped SIGSEGV/SIGBUS/SIGILL handler
// (sigsetjmp/siglongjmp with old-handler chaining). A fault logs signal+PC+
// faulting address and the caller takes its fallback instead of dying.
// [G23-b] REENTRANCY: Call::run itself is now guarded ([G23-a] — EVERY
// UFunction dispatch in the module goes through the guard), and many guarded
// scopes legitimately call Call::run inside them; a single sigjmp buffer
// would be corrupted by nesting. Depth counter: only the OUTERMOST guard
// installs the recovery point — nested guards run their body directly, and
// any fault longjmps to the outermost setjmp which reports "faulted".
// Semantics preserved: one recovery point per call tree, zero buffer reuse.
// NOTE: lives here (before class Call) because Call::run needs it; the gas
// namespace is reopened for the native-call helpers further down.
// ============================================================================
namespace gas {
static sigjmp_buf g_gasJmp;
static volatile sig_atomic_t g_inNativeCall = 0;
static struct sigaction g_oldSegv, g_oldBus, g_oldIll;
static bool g_gasGuardsUp = false;
static volatile sig_atomic_t g_faultSig = 0;
static volatile uintptr_t g_faultPc = 0, g_faultAddr = 0;
static volatile int g_guardDepth = 0;        // [G23-b] nested-guard depth
static volatile bool g_gasPrevInternal = false;
static pthread_t g_guardOwner = 0;           // [G23-b] thread that owns the recovery point

static void NativeFaultForward(int sig, siginfo_t* info, void* uctx) {
    struct sigaction* old = sig == SIGSEGV ? &g_oldSegv : (sig == SIGBUS ? &g_oldBus : &g_oldIll);
    if (old->sa_flags & SA_SIGINFO) { old->sa_sigaction(sig, info, uctx); return; }
    if (old->sa_handler && old->sa_handler != SIG_IGN && old->sa_handler != SIG_DFL) { old->sa_handler(sig); return; }
    struct sigaction dflt;
    memset(&dflt, 0, sizeof(dflt));
    dflt.sa_handler = SIG_DFL;
    sigaction(sig, &dflt, nullptr);
    raise(sig);
}
static void NativeFaultHandler(int sig, siginfo_t* info, void* uctx) {
    // [G23-b] THREAD-OWNED recovery: with Call::run universally guarded the
    // protected windows are long — a fault on ANY OTHER thread (render/
    // streaming/worker) must be forwarded to the real owner, not longjmp'd
    // onto our game-thread buffer (that would corrupt the faulting thread).
    if (g_inNativeCall && pthread_self() == g_guardOwner) {
        g_faultSig  = sig;
        g_faultPc   = uctx ? (uintptr_t)((ucontext_t*)uctx)->uc_mcontext.pc : 0;
        g_faultAddr = info ? (uintptr_t)info->si_addr : 0;
        g_inNativeCall = 0;
        siglongjmp(g_gasJmp, 1);
    }
    NativeFaultForward(sig, info, uctx);   // not ours — chain to the previous owner
}
static bool InstallFaultGuards() {
    if (g_gasGuardsUp) return true;
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = NativeFaultHandler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    g_gasGuardsUp = sigaction(SIGSEGV, &sa, &g_oldSegv) == 0 &&
                    sigaction(SIGBUS, &sa, &g_oldBus) == 0 &&
                    sigaction(SIGILL, &sa, &g_oldIll) == 0;
    return g_gasGuardsUp;
}
// Runs fn(args...) under the guard. Returns false when the call faulted —
// the caller must treat the native as unusable for that call and take its
// reflection fallback. [G23-b] nested calls are pass-through (see above).
template <typename FnT, typename... ArgsT>
static bool RunGuarded(const char* tag, FnT fn, ArgsT... args) {
    if (!InstallFaultGuards()) {
        // [G23-b] fail-open: guards cannot be installed (sigaction failed —
        // theoretical). Run the call bare rather than disabling every UFunction
        // dispatch in the module (the pre-G23 behavior).
        fn(args...);
        return true;
    }
    if (g_guardDepth > 0) {              // [G23-b] outer guard owns recovery
        fn(args...);
        return true;
    }
    if (sigsetjmp(g_gasJmp, 1) != 0) {
        // Restore the exact pre-call g_internal (captured in a volatile
        // before the call so it survives the longjmp). Blowing it to false
        // here would punch a reentrancy hole through the outer tick Guard.
        g_internal = g_gasPrevInternal;
        g_guardDepth = 0;
        g_inNativeCall = 0;
        LOG("[%s] ⚠️ native call FAULTED — sig=%d pc=0x%llx addr=0x%llx (call abandoned, fallback continues)",
            tag, (int)g_faultSig, (unsigned long long)g_faultPc, (unsigned long long)g_faultAddr);
        return false;
    }
    g_gasPrevInternal = g_internal;
    g_internal = true;        // engine callbacks fired during the native stay internal
    g_inNativeCall = 1;
    g_guardOwner = pthread_self();   // [G23-b] this thread owns the recovery point
    g_guardDepth = 1;
    fn(args...);
    g_guardDepth = 0;
    g_inNativeCall = 0;
    g_internal = g_gasPrevInternal;
    return true;
}
} // namespace gas

// ============================================================================
// CALL SYSTEM  — [F16] upgraded:
//   • Blueprint event override resolution (event_receiver) so BP events such
//     as BuildingActor.BlueprintOnInteract run the receiver's override
//     instead of the empty base event (ported from the Era core).
//   • Receiver validation in run(): refuses to execute a function on an
//     object that is not a subclass of the function's outer class.
//   • set_any(): bind by trying several reflection names (dumper renames
//     duplicate params like "Class" → "Class_0").
//   • set_at(): positional binding for nameless native params
//     (e.g. AFortPawn.SetMaxHealth(float)).
//   • [G23-a] run() dispatches under the gas fault guard — a bad receiver or
//     a wrong param shape faults into the guard (function named in the log)
//     instead of killing the process. This single change covers EVERY tick
//     path: vehicle retries, WP activation, loot spawner destroy, meshnet,
//     quickbar updates, death/victory steps.
// ============================================================================
class Call {
    void* m_func = nullptr;
    struct Param { std::string name; int offset, size; uint64_t flags; };
    std::vector<Param> m_params;
    alignas(16) std::array<uint8_t, 8192> m_data{};
    bool m_valid = false;

    void add_param(void* p, uint64_t flags, int total, bool ffield) {
        if (!(flags & profile::CPF_Parm)) return;
        int off = Read<int>(p, profile::PropOffset);
        int sz  = Read<int>(p, profile::PropElementSize);
        if (off < 0 || sz < 1 || off + sz > total) return;
        std::string pname;
        int nameOff = ffield ? profile::FFieldName : profile::ObjectName;
        if (!SafeFieldName(p, nameOff, pname)) return;
        m_params.push_back({pname, off, sz, flags});
    }
public:
    explicit Call(const char* path, void* event_receiver = nullptr) {
        m_func = FindObj(path);
        // [F92] Name-walk fallback — the device log proved path lookup fails
        // for some UFunctions ('⚠️ OnRep_CosmeticLoadout not present' while
        // GetProp reflection works fine), silently killing OnRep chains
        // (→ dead anims/skin!) and every other call whose StaticFindObject
        // misses. Resolve by NAME over the receiver's class chain — the exact
        // machinery GetProp uses successfully everywhere. Receiver instance →
        // its Class; otherwise the class object named in the path itself.
        if (!m_func) {
            // [G15-a] Resolution cache (NULL-RECEIVER calls only — the
            // receiver-driven walk is class-specific and must not be cached
            // by path). The device log showed the F92 name-walk re-running
            // FOUR TIMES PER 0.5s poll (FortPlayerPawnAthena.GetVehicleActor
            // from TickVehicleSeats + the FortPawn fallback ×2 names) — each
            // walk traverses the full class chain's FField/child lists and
            // logs a line: per-frame log spam + measurable walk cost on a
            // phone. Positive results cache forever; NEGATIVE results expire
            // after 30s so a UFunction whose class streams in late still
            // resolves.
            static std::map<std::string, std::pair<void*, double>> s_f92Cache;
            double nowSec = 0;
            bool skipWalk = false;
            if (!event_receiver) {
                { struct timespec ts{}; clock_gettime(CLOCK_MONOTONIC, &ts); nowSec = (double)ts.tv_sec + 1e-9 * (double)ts.tv_nsec; }
                auto cit = s_f92Cache.find(std::string(path));
                if (cit != s_f92Cache.end()) {
                    if (cit->second.first) m_func = cit->second.first;            // cached hit
                    else if (nowSec - cit->second.second < 30.0) skipWalk = true; // recent negative
                }
            }
            if (!m_func && !skipWalk) {
            std::string p(path);
            auto dot = p.rfind('.');
            std::string clsPath = (dot != std::string::npos) ? p.substr(0, dot) : std::string();
            std::string fname   = (dot != std::string::npos) ? p.substr(dot + 1) : p;
            void* funcType = FindObj("/Script/CoreUObject.Function");
            void* klass = nullptr;
            if (event_receiver && !IsBadPtr(event_receiver))
                klass = Read<void*>(event_receiver, profile::ObjectClass);
            if (!klass && !clsPath.empty()) {
                void* holder = FindObj(clsPath);
                if (holder && !IsBadPtr(holder)) {
                    void* classCls = FindObj("/Script/CoreUObject.Class");
                    klass = (classCls && IsA(holder, classCls)) ? holder
                                                                : Read<void*>(holder, profile::ObjectClass);
                }
            }
            for (int d = 0; d < 64 && !m_func && klass && !IsBadPtr(klass); ++d) {
                void* f = (profile::HasFField ? Read<void*>(klass, profile::StructChildProps) : nullptr);
                for (int i = 0; f && !IsBadPtr(f) && i < 1024; ++i) {
                    std::string n;
                    if (SafeFieldName(f, profile::FFieldName, n) && n == fname && (!funcType || IsA(f, funcType))) { m_func = f; break; }
                    f = Read<void*>(f, profile::FFieldNext);
                }
                if (!m_func) {
                    void* c = Read<void*>(klass, profile::StructChildren);
                    for (int i = 0; c && !IsBadPtr(c) && i < 1024; ++i) {
                        std::string n;
                        if (SafeFieldName(c, profile::ObjectName, n) && n == fname && (!funcType || IsA(c, funcType))) { m_func = c; break; }
                        c = Read<void*>(c, profile::FieldNext);
                    }
                }
                if (!m_func) klass = Read<void*>(klass, profile::StructSuper);
            }
            if (m_func) LOG("[Call] [F92] name-resolved '%s' (StaticFindObject missed it)", fname.c_str());
            if (!event_receiver) s_f92Cache[std::string(path)] = { m_func, nowSec };   // [G15-a] cache pos+neg
            }
        }
        if (!m_func) return;
        // [F16] Blueprint override resolution: walk the receiver's super
        // chain from most-derived to base and prefer the matching Function
        // found there over the (possibly empty) base event we looked up.
        if (event_receiver && !IsBadPtr(event_receiver)) {
            std::string ev = ObjName(m_func);
            void* funcType = FindObj("/Script/CoreUObject.Function");
            void* klass = Read<void*>(event_receiver, profile::ObjectClass);
            void* derived = nullptr;
            for (int d = 0; d < 64 && klass && !IsBadPtr(klass) && funcType; ++d) {
                void* f = (profile::HasFField ? Read<void*>(klass, profile::StructChildProps) : nullptr);
                for (int i = 0; f && !IsBadPtr(f) && i < 1024; ++i) {
                    std::string n;
                    if (SafeFieldName(f, profile::FFieldName, n) && n == ev && IsA(f, funcType)) { derived = f; break; }
                    f = Read<void*>(f, profile::FFieldNext);
                }
                if (!derived) {
                    void* c = Read<void*>(klass, profile::StructChildren);
                    for (int i = 0; c && !IsBadPtr(c) && i < 1024; ++i) {
                        std::string n;
                        if (SafeFieldName(c, profile::ObjectName, n) && n == ev && IsA(c, funcType)) { derived = c; break; }
                        c = Read<void*>(c, profile::FieldNext);
                    }
                }
                if (derived) break;
                klass = Read<void*>(klass, profile::StructSuper);
            }
            if (derived) m_func = derived;
        }
        int total = Read<uint16_t>(m_func, profile::FunctionParamSize);
        if (total < 0 || total > (int)m_data.size()) return;
        void* p = Read<void*>(m_func, profile::StructChildren);
        for (int i = 0; p && !IsBadPtr(p) && i < 128; ++i) {
            add_param(p, Read<uint64_t>(p, profile::PropFlags), total, false);
            p = Read<void*>(p, profile::FieldNext);
        }
        void* fp = (profile::HasFField ? Read<void*>(m_func, profile::StructChildProps) : nullptr);
        for (int i = 0; fp && !IsBadPtr(fp) && i < 128; ++i) {
            add_param(fp, Read<uint64_t>(fp, profile::PropFlags), total, true);
            fp = Read<void*>(fp, profile::FFieldNext);
        }
        m_valid = (total == 0) || !m_params.empty();
    }
    bool ok() const { return m_valid; }
    void* func() const { return m_func; }
    void dump(const char* label) const {
        LOG("[Call %s] valid=%d func=%p params=%zu", label, (int)m_valid, m_func, m_params.size());
        for (auto& p : m_params)
            LOG("   param '%s' off=%d size=%d flags=%llx", p.name.c_str(), p.offset, p.size,
                (unsigned long long)p.flags);
    }
    template<class T> bool set(const char* key, const T& value) {
        for (auto& p : m_params)
            if (p.name == key && p.size == (int)sizeof(T)) {
                memcpy(m_data.data() + p.offset, &value, sizeof(T));
                return true;
            }
        return false;
    }
    // [F16] Try several reflection names (GetContext's "Class" is emitted as
    // "Class_0" by the dumper, and other renames exist across the SDK).
    template<class T> bool set_any(const T& value, const char* k1, const char* k2 = nullptr,
                                   const char* k3 = nullptr, const char* k4 = nullptr) {
        const char* keys[] = {k1, k2, k3, k4};
        for (int i = 0; i < 4; ++i)
            if (keys[i] && set(keys[i], value)) return true;
        return false;
    }
    // [F16] Positional binding for native functions with nameless params.
    template<class T> bool set_at(int index, const T& value) {
        int seen = 0;
        for (auto& p : m_params)
            if (!(p.flags & profile::CPF_ReturnParm) && p.size == (int)sizeof(T))
                if (seen++ == index) { memcpy(m_data.data() + p.offset, &value, sizeof(T)); return true; }
        return false;
    }
    bool set_bytes(const char* key, const void* src, int size) {
        for (auto& p : m_params) {
            if (p.name != key) continue;
            if (p.size == size && src) { memcpy(m_data.data() + p.offset, src, size); return true; }
            if (p.size == 0) return true;              // [F32] wildcard out param — presence only
            if (p.size >= size && src) { memcpy(m_data.data() + p.offset, src, size); return true; }  // [F32] wildcard sized at compile time
        }
        return false;
    }
    // [F32] Raw param access for out params whose contents the native fills
    // (row copies written by GetDataTableRow exceed the reflected size).
    void* param_slot(const char* key) {
        for (auto& p : m_params)
            if (p.name == key) return m_data.data() + p.offset;
        return nullptr;
    }
    int param_size(const char* key) {
        for (auto& p : m_params) if (p.name == key) return p.size;
        return -1;
    }
    // [F93] POSITIONAL/OUT BINDING — layout-stable fallback for K2 functions
    // whose param NAMES drift across builds while the layout does not. Used
    // by DataTableRow when the named binds miss: (a) nth non-ReturnParm param
    // by OFFSET order (memory layout, not linked-list order), (b) first
    // CPF_OutParm slot regardless of name/size.
    bool bind_at(int nonReturnIndex, const void* src, int size) {
        std::vector<const Param*> ordered;
        for (auto& p : m_params) if (!(p.flags & profile::CPF_ReturnParm)) ordered.push_back(&p);
        std::sort(ordered.begin(), ordered.end(), [](const Param* a, const Param* b) { return a->offset < b->offset; });
        if (nonReturnIndex < 0 || nonReturnIndex >= (int)ordered.size()) return false;
        const Param& p = *ordered[(size_t)nonReturnIndex];
        if (p.offset < 0 || size <= 0 || p.offset + size > (int)m_data.size() || !src) return false;
        memcpy(m_data.data() + p.offset, src, (size_t)size);
        return true;
    }
    int param_at_size(int nonReturnIndex) const {
        std::vector<const Param*> ordered;
        for (auto& p : m_params) if (!(p.flags & profile::CPF_ReturnParm)) ordered.push_back(&p);
        std::sort(ordered.begin(), ordered.end(), [](const Param* a, const Param* b) { return a->offset < b->offset; });
        if (nonReturnIndex < 0 || nonReturnIndex >= (int)ordered.size()) return -1;
        return ordered[(size_t)nonReturnIndex]->size;
    }
    void* out_slot() {
        for (auto& p : m_params)
            if (p.flags & profile::CPF_OutParm) return m_data.data() + p.offset;
        return nullptr;
    }
    template<class T> T result() const {
        for (auto& p : m_params)
            if ((p.flags & profile::CPF_ReturnParm) && p.size == (int)sizeof(T))
                return Read<T>(m_data.data(), p.offset);
        return {};
    }
    void* result_data(int size) {
        for (auto& p : m_params)
            if ((p.flags & profile::CPF_ReturnParm) && p.size == size)
                return m_data.data() + p.offset;
        return nullptr;
    }
    template<class T> T get(const char* key) const {
        for (auto& p : m_params)
            if (p.name == key && p.size == (int)sizeof(T))
                return Read<T>(m_data.data(), p.offset);
        return {};
    }
    bool run(void* object) {
        if (!m_valid || !object || !orig_PE) return false;
        // [F16] Receiver validation — refuse cross-class execution.
        void* owner = Read<void*>(m_func, profile::ObjectOuter);
        if (owner && !IsBadPtr(owner) && !IsA(object, owner)) {
            LOG("[Call] rejected %s: receiver=%s owner=%s", ObjName(m_func).c_str(),
                ObjName(Read<void*>(object, profile::ObjectClass)).c_str(), ObjName(owner).c_str());
            return false;
        }
        Guard g;
        // [G23-a] fault-guarded dispatch — the crash-class fix for every
        // tick-driven UFunction call (vehicle/WP/loot/meshnet/QB paths).
        bool clean = gas::RunGuarded("[Call]", [&] {
            orig_PE(object, m_func, m_data.data());
        });
        if (!clean)
            LOG("[Call] ⚠️ '%s' faulted on receiver %p — abandoned [G23]",
                ObjName(m_func).c_str(), object);
        return clean;
    }
};

// [F16] Extract a named incoming PE parameter by reflection metadata
// (ported from the Era core incoming()).
static void* incoming(void* function, void* data, const char* key, int expected_size) {
    if (!function || !data || IsBadPtr(function)) return nullptr;
    int total = Read<uint16_t>(function, profile::FunctionParamSize);
    void* p = Read<void*>(function, profile::StructChildren);
    for (int i = 0; p && !IsBadPtr(p) && i < 128; ++i, p = Read<void*>(p, profile::FieldNext)) {
        if (!(Read<uint64_t>(p, profile::PropFlags) & profile::CPF_Parm)) continue;
        std::string n;
        if (!SafeFieldName(p, profile::ObjectName, n) || n != key) continue;
        int offset = Read<int>(p, profile::PropOffset);
        int size   = Read<int>(p, profile::PropElementSize);
        if (offset < 0 || size < 1 || offset + size > total || (expected_size && size != expected_size))
            return nullptr;
        return (char*)data + offset;
    }
    p = (profile::HasFField ? Read<void*>(function, profile::StructChildProps) : nullptr);
    for (int i = 0; p && !IsBadPtr(p) && i < 128; ++i, p = Read<void*>(p, profile::FFieldNext)) {
        if (!(Read<uint64_t>(p, profile::PropFlags) & profile::CPF_Parm)) continue;
        std::string n;
        if (!SafeFieldName(p, profile::FFieldName, n) || n != key) continue;
        int offset = Read<int>(p, profile::PropOffset);
        int size   = Read<int>(p, profile::PropElementSize);
        if (offset < 0 || size < 1 || offset + size > total || (expected_size && size != expected_size))
            return nullptr;
        return (char*)data + offset;
    }
    return nullptr;
}

// ============================================================================
// REFLECTION HELPERS — [F28/F30/F32] dynamic property resolution used by the
// loot tables, vehicle seats and building bookkeeping. All lookups walk the
// ChildProperties/Children chains and the super classes; every consumer must
// tolerate a -1/nullptr result (feature degrades, never crashes).
// ============================================================================
static void* FindChildProp(void* s, const char* name, int* sizeOut = nullptr) {
    if (!s || IsBadPtr(s) || !name) return nullptr;
    void* f = (profile::HasFField ? Read<void*>(s, profile::StructChildProps) : nullptr);
    for (int i = 0; f && !IsBadPtr(f) && i < 512; ++i) {
        std::string n;
        if (SafeFieldName(f, profile::FFieldName, n) && n == name) {
            if (sizeOut) *sizeOut = Read<int>(f, profile::PropElementSize);
            return f;
        }
        f = Read<void*>(f, profile::FFieldNext);
    }
    void* c = Read<void*>(s, profile::StructChildren);
    for (int i = 0; c && !IsBadPtr(c) && i < 512; ++i) {
        std::string n;
        if (SafeFieldName(c, profile::ObjectName, n) && n == name) {
            if (sizeOut) *sizeOut = Read<int>(c, profile::PropElementSize);
            return c;
        }
        c = Read<void*>(c, profile::FieldNext);
    }
    void* sup = Read<void*>(s, profile::StructSuper);
    for (int d = 0; sup && !IsBadPtr(sup) && sup != s && d < 32; ++d) {
        void* r = FindChildProp(sup, name, sizeOut);
        if (r) return r;
        void* next = Read<void*>(sup, profile::StructSuper);
        if (next == sup) break;
        sup = next;
    }
    return nullptr;
}
static int PropOffsetOn(void* s, const char* name, int* sizeOut = nullptr) {
    void* p = FindChildProp(s, name, sizeOut);
    return p ? Read<int>(p, profile::PropOffset) : -1;
}
// [F86-a/LOOT-1] Resolve a UScriptStruct row size WITHOUT trusting the
// build-broken PropertiesSize@0xA8 read: walk the FField ChildProperties
// chains (same proven machinery as FindChildProp/PropOffsetOn — verified
// working on device by every reflection consumer) and compute
//   size = max(prop.Offset + prop.ElementSize * prop.ArrayDim)
// across the struct and its SuperStruct chain, 8-aligned.
// mustHave/mustHaveCount: required member names — the walk counts hits and
// returns 0 when none match, so a WRONG RowStruct pointer (bad 0x28 on some
// build) is detected instead of producing a garbage size. Pass nullptr to
// skip validation (caller already validates members via PropOffsetOn).
// Never reads, never crashes: every value is range-clamped; a prop with a
// wild offset/size is ignored.
static int StructSizeByFFieldWalk(void* s, const char* const* mustHave, int mustHaveCount) {
    if (!s || IsBadPtr(s)) return 0;
    int best = 0, hits = 0;
    void* u = s;
    for (int d = 0; u && !IsBadPtr(u) && d < 16; ++d) {
        // FField chain (UE4.22+ reflection)
        void* f = (profile::HasFField ? Read<void*>(u, profile::StructChildProps) : nullptr);
        for (int i = 0; f && !IsBadPtr(f) && i < 512; ++i) {
            int off = Read<int>(f, profile::PropOffset);
            int esz = Read<int>(f, profile::PropElementSize);
            int dim = Read<int>(f, profile::PropArrayDim);
            if (dim < 1 || dim > 64) dim = 1;
            if (off >= 0 && off < 4096 && esz > 0 && esz <= 512) {
                int end = off + esz * dim;
                if (end > best) best = end;
            }
            if (mustHave) for (int k = 0; k < mustHaveCount; ++k) {
                std::string n;
                if (SafeFieldName(f, profile::FFieldName, n) && n == mustHave[k]) ++hits;
            }
            f = Read<void*>(f, profile::FFieldNext);
        }
        // Legacy UProperty children chain (pre-FField / mixed builds)
        void* c = Read<void*>(u, profile::StructChildren);
        for (int i = 0; c && !IsBadPtr(c) && i < 512; ++i) {
            int off = Read<int>(c, profile::PropOffset);
            int esz = Read<int>(c, profile::PropElementSize);
            int dim = Read<int>(c, profile::PropArrayDim);
            if (dim < 1 || dim > 64) dim = 1;
            if (off >= 0 && off < 4096 && esz > 0 && esz <= 512) {
                int end = off + esz * dim;
                if (end > best) best = end;
            }
            if (mustHave) for (int k = 0; k < mustHaveCount; ++k) {
                std::string n;
                if (SafeFieldName(c, profile::ObjectName, n) && n == mustHave[k]) ++hits;
            }
            c = Read<void*>(c, profile::FieldNext);
        }
        void* next = Read<void*>(u, profile::StructSuper);
        if (next == u) break;
        u = next;
    }
    if (mustHave && mustHaveCount > 0 && hits < 2) return 0;   // wrong struct pointer — not our row type
    if (best <= 0 || best > 512) return 0;
    return (best + 7) & ~7;   // align to 8 (row copies are struct-aligned)
}
// [F94-b] Row size resolution: the FField walk is AUTHORITATIVE — the raw
// PropertiesSize@0x54 slot reads 0x6F (111) on BOTH loot structs on this
// build (a non-4-aligned impossible size for structs with FName/TArray
// members, and identical across two different row types = that slot is
// not PropertiesSize here). Walk first; raw only as a last resort.
static int TableRowSizeOf(void* rs, const char* const* mustHave, int mustCount) {
    if (!rs || IsBadPtr(rs)) return 0;
    int walk = StructSizeByFFieldWalk(rs, mustHave, mustCount);
    if (walk > 0) return walk;
    int raw = Read<int>(rs, profile::StructPropertiesSize);
    return (raw > 0 && raw <= 512) ? raw : 0;
}
static std::string FNameToString(const void* fnameBytes) {
    if (!fnameBytes) return {};
    FStringNative ns{};
    Fn<void(*)(const void*, FStringNative*)>(profile::NameAppendString)(fnameBytes, &ns);
    std::string out = Narrow(ns.Data, ns.Count > 0 ? std::min(ns.Count, 128) : 0);
    if (ns.Data) Fn<void(*)(void*)>(profile::Free)(ns.Data);
    return out;
}
// [F32] Resolve a TSoftObjectPtr by handing its raw bytes to LoadAsset_Blocking
// (the soft-pointer struct layout is identical on both sides).
static void* LoadSoftPtr(const void* bytes, int stride) {
    if (!bytes || stride < 8 || stride > 64) return nullptr;
    Call load("/Script/Engine.KismetSystemLibrary.LoadAsset_Blocking");
    if (!load.ok()) return nullptr;
    if (!load.set_bytes("Asset", bytes, stride)) return nullptr;
    if (!load.run(FindObj("/Script/Engine.Default__KismetSystemLibrary"))) return nullptr;
    void* out = load.result<void*>();
    return (out && !IsBadPtr(out)) ? out : nullptr;
}
// [G20] TSoftObjectPtr → "/Package/Object" string WITHOUT any native call.
// UE5.0 FSoftObjectPath = { FString AssetPathName; FString SubPathString; }
// (5.1 replaced the FString with an FName pair — on those builds the parse
// fails validation cleanly and the caller falls back, so this is layout-
// agnostic by construction). Every read is kernel-validated: a wrong layout
// yields an empty string, never a crash.
static std::string SoftObjectPathString(const void* soft, int stride) {
    if (!soft || stride < 16 || !IsReadable(soft, 16)) return {};
    FStringNative fs = Read<FStringNative>(soft, 0);
    if (!fs.Data || fs.Count < 3 || fs.Count > 512 || IsBadPtr(fs.Data)) return {};
    if (!IsReadable(fs.Data, (size_t)std::min(fs.Count, 256) * 2)) return {};
    std::string s = Narrow((const char16_t*)fs.Data, fs.Count);
    if (s.size() < 5 || s[0] != '/' || s.find('.') == std::string::npos) return {};
    return s;
}
// [F32] UDataTable row enumeration through the engine Kismet library — works
// regardless of the RowMap key type (FName on stock builds, int on FNE forks).
static bool DataTableRowNames(void* table, std::vector<FName>& out) {
    if (!table || IsBadPtr(table)) return false;
    Call c("/Script/Engine.DataTableFunctionLibrary.GetDataTableRowNames");
    if (!c.ok()) return false;
    Array empty{};
    if (!c.set("Table", table) || !c.set_bytes("OutRowNames", &empty, sizeof(Array))) return false;
    if (!c.run(FindObj("/Script/Engine.Default__DataTableFunctionLibrary"))) return false;
    void* slot = c.param_slot("OutRowNames");
    if (!slot) return false;
    Array res = Read<Array>(slot);
    if (res.Count < 0 || res.Count > 200000) { if (res.Data) Fn<void(*)(void*)>(profile::Free)(res.Data); return false; }
    out.reserve((size_t)res.Count);
    for (int i = 0; i < res.Count; ++i) out.push_back(Read<FName>(res.Data, (size_t)i * sizeof(FName)));
    if (res.Data) Fn<void(*)(void*)>(profile::Free)(res.Data);
    return !out.empty();
}
// [G16-b] THE TRUE UE 5.0 TMap LAYOUT — read straight from the UnrealEngine-
// 5.0.0-release source the user provided (Containers/Set.h + SparseArray.h +
// BitArray.h + ContainerAllocationPolicies.h):
//   FDefaultBitArrayAllocator = TInlineAllocator<4>  → TBitArray allocator is
//   { uint32 InlineData[4](16B) + SecondaryData heap ptr(8B) } = 24B →
//   TBitArray = 24 + NumBits(4) + MaxBits(4) = 0x20
//   TSparseArray = TArray Data(0x10) + TBitArray(0x20) + FirstFreeIndex(4)
//                  + NumFreeIndices(4) = 0x38
//   TSetAllocator default HashAllocator = TInlineAllocator<1> → Hash = 16B
//   TSet = TSparseArray(0x38) + Hash(0x10) + HashSize(4) + pad(4) = 0x50
//   — EXACTLY the 0x50 the 21.30 SDK dump reports for RowMap.
// Offsets (from a TMap at m):
//   +0x00 Elements.Data.Data (element storage ptr)      +0x08 Elements.Data.ArrayNum
//   +0x10 AllocationFlags INLINE bits (4 DWORDs = 128 bits max)
//   +0x20 AllocationFlags HEAP ptr (when set, bits live HERE)
//   +0x28 AllocationFlags.NumBits    +0x2C MaxBits
//   +0x30 Elements.FirstFreeIndex    +0x34 Elements.NumFreeIndices
// Element stride 24: TSetElement<TTuple<FName,uint8*>> = Key(8) + Value(8)
//                    + HashNextId(4) + HashIndex(4).
// The OLD code read flags@+0x18 / numBits@+0x20 / numFree@+0x14 — garbage
// offsets that made every free-slot bit check skip real elements.
static uint8_t* RowMapFindImpl(void* table, const FName& row, int rowSize, int depth);
static uint8_t* RowMapFind(void* table, const FName& row, int rowSize) {
    return RowMapFindImpl(table, row, rowSize, 0);
}
static uint8_t* RowMapFindImpl(void* table, const FName& row, int rowSize, int depth) {
    if (!table || IsBadPtr(table) || !row.CompIdx) return nullptr;
    uint8_t* m = (uint8_t*)table + 0x30;
    void* elems = Read<void*>(m, 0x00);
    int32_t num = Read<int32>(m, 0x08);
    if (elems && !IsBadPtr(elems) && num > 0 && num <= 200000) {
        // [G16-b] Allocation bits: heap storage wins over the inline words
        // (GetAllocation() = IfAThenAElseB(SecondaryData, InlineData)).
        void* heapBits = Read<void*>(m, 0x20);
        uint32_t* bits = (heapBits && !IsBadPtr(heapBits)) ? (uint32_t*)heapBits
                                                           : (uint32_t*)(m + 0x10);
        int32_t numBits = Read<int32>(m, 0x28);
        int32_t numFree = Read<int32>(m, 0x34);
        bool flagsOk = numBits >= 0 && numBits <= num + 256 && IsReadable(bits, 8);
        for (int32_t i = 0; i < num; ++i) {
            if (numFree != 0 && flagsOk) {   // sparse holes — probe the allocation bit
                uint32_t word = Read<uint32>(bits, (size_t)(i >> 5) * 4);
                if (!((word >> (i & 31)) & 1)) continue;
            }
            uint8_t* el = (uint8_t*)elems + (size_t)i * 24;
            if (Read<uint32>(el, 0) != row.CompIdx || Read<uint32>(el, 4) != row.DispIdx) continue;
            uint8_t* val = Read<uint8_t*>(el, 8);
            if (val && IsReadable(val, rowSize > 0 ? (size_t)rowSize : 8)) return val;
        }
    }
    // [G19-c] Composite parent tables — the hard-coded ParentTables@0xB0 /
    // OldParentTables@0xC0 found NOTHING on this build (device: 252 tier rows
    // enumerated → 1 cached; 2149 package rows → 1717 — every parent-only
    // row failed both the RowMap walk AND the UFunction). DISCOVER the parent
    // list dynamically instead: scan the object for TArray candidates whose
    // elements are real UDataTables sharing OUR RowStruct (same row type — a
    // strong filter that rejects every non-table array), then search those.
    // Results are cached per table pointer.
    if (depth < 3) {
        static std::map<void*, std::vector<void*>> s_parents;   // per-table cache
        auto it = s_parents.find(table);
        if (it == s_parents.end()) {
            std::vector<void*> found;
            void* myRowStruct = Read<void*>(table, 0x28);
            static void* s_dtCls = nullptr;
            if (!s_dtCls) s_dtCls = FindObj("/Script/Engine.DataTable");
            if (s_dtCls && myRowStruct && !IsBadPtr(myRowStruct)) {
                for (size_t off = 0x80; off <= 0x1A0; off += 8) {
                    Array cand = Read<Array>(table, off);
                    if (!cand.Data || cand.Count <= 0 || cand.Count > 64) continue;
                    if (!IsReadable(cand.Data, (size_t)cand.Count * 8)) continue;
                    std::vector<void*> tables;
                    for (int i = 0; i < cand.Count; ++i) {
                        void* p = Read<void*>(cand.Data, (size_t)i * 8);
                        if (!p || p == table || IsBadPtr(p)) continue;
                        void* cls = Read<void*>(p, profile::ObjectClass);
                        if (!cls || IsBadPtr(cls) || !IsA(cls, s_dtCls)) continue;
                        if (Read<void*>(p, 0x28) != myRowStruct) continue;   // same row type
                        tables.push_back(p);
                    }
                    if (!tables.empty() && tables.size() >= (size_t)cand.Count / 2)
                        for (void* t : tables)
                            if (std::find(found.begin(), found.end(), t) == found.end())
                                found.push_back(t);
                }
            }
            static bool s_pLogged = false;
            if (!s_pLogged) {
                s_pLogged = true;
                LOG("[Loot][G19] parent-table discovery: %zu parent(s) under %p",
                    found.size(), table);
            }
            it = s_parents.emplace(table, found).first;
        }
        for (void* parent : it->second)
            if (uint8_t* hit = RowMapFindImpl(parent, row, rowSize, depth + 1))
                return hit;
    }
    return nullptr;
}
// [F32] Copy a row into a caller buffer. The native writes Table->RowStruct->
// PropertiesSize bytes at the OutRow slot, so the slot is read back raw.
// [F93] one-shot diagnosis: every historical failure of this function was
// SILENT (all return-false paths unlogged) — the 'cache ready=0 tiers=0
// packages=0' device line with 'tiers=252 rows' one line above proves all
// 2401 calls fail without telling us WHY. This names the path, once.
static bool DataTableRow(void* table, const FName& row, void* dst, int rowSize) {
    if (!table || IsBadPtr(table) || rowSize <= 0 || rowSize > 512 || !dst) return false;
    // [F94] PRIMARY: read the row bytes straight out of RowMap — no
    // UFunction involved (see RowMapFind above for why the old path was
    // dead on this build).
    if (uint8_t* src = RowMapFind(table, row, rowSize)) {
        memcpy(dst, src, (size_t)rowSize);
        return true;
    }
    Call c("/Script/Engine.DataTableFunctionLibrary.GetDataTableRowFromName");   // [G15-f] UE5.0 rename —
    if (!c.ok()) c = Call("/Script/Engine.DataTableFunctionLibrary.GetDataTableRow");   // pre-5.0 name
    static bool dumped = false;   // one diagnosis per process — 2401 failures, 1 log
    auto diagnose = [&](const char* why) {
        if (dumped) return;
        dumped = true;
        LOG("[Loot][F93] DataTableRow first-failure diagnosis: %s", why);
        c.dump("GetDataTableRow");
    };
    if (!c.ok()) { diagnose("c.ok()==false — UFunction unresolved (F92 name-walk also missed)"); return false; }
    // [G16-b] THE BIND FIX. The device dump proved the function resolves and
    // its params are Table@0(8) / RowName@8(size=4!) / OutRow@16(8) /
    // ReturnValue@24(1). The reflected FName ElementSize reads 4 on this
    // build, so every set()/positional bind demanding sizeof(FName)==8
    // failed ("RowName bind failed (named AND positional)" — the entire
    // cache miss in one line). FName IS 8 bytes in memory (CompIdx+Number);
    // the slot area 8..15 is FName-shaped (OutRow starts at 16) — write the
    // full 8 bytes straight into the slot.
    {
        bool tblBound = c.set("Table", table);
        if (!tblBound) {
            void* ts = c.param_slot("Table");
            if (!ts) { diagnose("Table slot unresolved"); return false; }
            memcpy(ts, &table, sizeof(void*));
        }
        void* rs = c.param_slot("RowName");
        if (!rs) { diagnose("RowName slot unresolved"); return false; }
        memcpy(rs, &row, sizeof(FName));   // [G16-b] raw 8-byte FName write
    }
    void* slot = c.param_slot("OutRow");
    if (!slot) {
        slot = c.out_slot();
        if (!slot && c.param_size("OutRow") > 0) slot = c.param_slot("OutRow");
        if (!slot) { diagnose("OutRow slot unresolved (named AND CPF_OutParm scan)"); return false; }
    }
    // zero the out slot before the call (native writes rowSize bytes there)
    {   int zsz = c.param_size("OutRow");
        if (zsz <= 0) zsz = 8;                        // wildcard out param
        memset(slot, 0, (size_t)std::min(zsz, 256));
    }
    void* cdo = FindObj("/Script/Engine.Default__DataTableFunctionLibrary");
    if (!cdo) { diagnose("Default__DataTableFunctionLibrary CDO unresolved"); return false; }
    if (!c.run(cdo)) { diagnose("run() rejected/failed on the CDO"); return false; }
    // [G19-c] the native RETURNS bool ("row found"). A zeroed OutRow with a
    // false return is a MISS, not an empty row — returning true here fed the
    // cache zero bytes that the junk-row guard then silently dropped.
    if (!c.result<bool>()) {
        static bool s_missLogged = false;
        if (!s_missLogged) {
            s_missLogged = true;
            LOG("[Loot][G19] GetDataTableRowFromName returned false (row not in the native map)");
        }
        return false;
    }
    // [G15-f] GetDataTableRowFromName's OutRow is an OutParm POINTER (SDK:
    // "struct FTableRowBase* OutRow (Parm, OutParm)") — the native writes
    // the row pointer INTO the slot. The legacy GetDataTableRow wrote the
    // row bytes inline. Handle both: read the slot as a pointer first; if
    // it is not a sane row pointer, treat the slot itself as inline bytes.
    void* rowPtr = Read<void*>(slot);
    if (rowPtr && IsReadable(rowPtr, (size_t)rowSize) && ((uintptr_t)rowPtr & 7) == 0) {
        memcpy(dst, rowPtr, (size_t)rowSize);
        return true;
    }
    memcpy(dst, slot, (size_t)rowSize);
    return true;
}
// [F31] Weapon clip size from the WeaponStatHandle row (Era GetStats parity).
static int ClipSizeOf(void* weaponDef) {
    void* p = GetProp("/Script/FortniteGame.FortWeaponItemDefinition.WeaponStatHandle");
    if (!p || !weaponDef || IsBadPtr(weaponDef)) return 30;
    void* handle = GetField(weaponDef, p);
    void* table = Read<void*>(handle, 0);
    FName row = Read<FName>(handle, 8);
    if (!table || IsBadPtr(table) || !row.CompIdx) return 30;
    void* rs = Read<void*>(table, 0x28);   // UDataTable::RowStruct
    if (!rs || IsBadPtr(rs)) return 30;
    int rowSize = TableRowSizeOf(rs, nullptr, 0);   // [F94-b] walk-first — ClipSize presence is validated right below
    if (rowSize <= 0 || rowSize > 512) return 30;
    int off = PropOffsetOn(rs, "ClipSize");
    if (off < 0) return 30;
    uint8_t buf[512]{};
    if (!DataTableRow(table, row, buf, rowSize)) return 30;
    int v = Read<int32>(buf, off);
    return (v > 0 && v <= 1000) ? v : 30;
}
// [F31] Erbium IsPrimaryQuickbar parity (class-based approximation — a false
// primary only costs a hotbar slot, never a crash).
static bool IsPrimaryQuickbarItem(void* def) {
    if (!def) return false;
    static const char* secondary[] = {
        "/Script/FortniteGame.FortWeaponMeleeItemDefinition",
        "/Script/FortniteGame.FortAmmoItemDefinition",
        "/Script/FortniteGame.FortBuildingItemDefinition",
        "/Script/FortniteGame.FortEditToolItemDefinition",
        "/Script/FortniteGame.FortResourceItemDefinition",
        "/Script/FortniteGame.FortTrapItemDefinition",
        nullptr
    };
    for (int i = 0; secondary[i]; ++i) {
        void* cls = FindObj(secondary[i]);
        if (cls && IsA(def, cls)) return false;
    }
    return true;
}
static int MaxStackSizeOf(void* def) {
    void* p = GetProp("/Script/FortniteGame.FortItemDefinition.MaxStackSize");
    if (!p || !def) return 0;
    int sz = Read<int>(p, profile::PropElementSize);
    if (sz == 8) { double v = Read<double>(GetField(def, p)); if (v > 1.0 && v <= 9999.0) return (int)v; }
    else if (sz == 4) { float v = Read<float>(GetField(def, p)); if (v > 1.0f && v <= 9999.0f) return (int)v; }
    return 0;
}
// [F28] TWeakObjectPtr → object (GObjects serial check).
static void* ResolveWeak(void* weak) {
    if (!weak || IsBadPtr(weak)) return nullptr;
    int32 idx = Read<int32>(weak, 0);
    int32 serial = Read<int32>(weak, 4);
    GObjects_t* go = GObjectArray();
    if (!go || idx < 0 || idx > 4000000) return nullptr;
    FUObjectItem* item = go->GetItem((uint32)idx);
    if (!item || !item->Object || IsBadPtr(item->Object)) return nullptr;
    if (serial != 0 && item->Serial != serial) return nullptr;
    return item->Object;
}

// [F35] Object → TWeakObjectPtr bytes (the reverse of ResolveWeak). Needed
// for AFortPickup.PickupLocationData.PickupTarget (the fly-to-player
// animation reads it) — writing the RAW pointer into a TWeakObjectPtr would
// corrupt it, so the {ObjectIndex, ObjectSerial} pair is built from GObjects.
static bool MakeWeakRef(void* obj, void* weakDst, bool& cached, int32& cIdx, int32& cSerial) {
    if (!obj || IsBadPtr(obj) || !weakDst) return false;
    if (!cached) {
        cached = true; cIdx = -1; cSerial = 0;
        GObjects_t* go = GObjectArray();
        if (go) {
            // Bounded scan; GC may grow the array between calls, NumElements
            // is read fresh every time. The pawn never moves, so this runs once.
            int32 n = go->NumElements;
            if (n > 0 && n < 8000000) {
                for (int32 i = 0; i < n; ++i) {
                    FUObjectItem* it = go->GetItem((uint32)i);
                    if (!it) continue;
                    if (it->Object == obj) { cIdx = i; cSerial = it->Serial; break; }
                }
            }
        }
        if (cIdx >= 0) LOG("[Pickup] weak ref cached idx=%d serial=%d", cIdx, cSerial);
    }
    if (cIdx < 0) return false;
    Write<int32>(weakDst, 0, cIdx);
    Write<int32>(weakDst, 4, cSerial);
    return true;
}

// ============================================================================
// ASSET LOADING / SPAWN
// ============================================================================
static void* LoadAsset(const std::string& path,
                       const char* type_path = "/Script/FortniteGame.FortItemDefinition") {
    if (void* r = FindObj(path)) return r;
    auto s = ToWide(path);
    void* type = FindObj(type_path);
    if (!type) return nullptr;
    return Fn<void*(*)(void*, void*, const char16_t*, const char16_t*, uint32_t, void*, bool, void*)>(
        profile::StaticLoadObject)(type, nullptr, s.c_str(), nullptr, 0, nullptr, true, nullptr);
}
static void* LoadClass(const char* path, const char* base_path = "/Script/Engine.Actor") {
    if (void* r = FindObj(path)) return r;
    auto s = ToWide(path);
    void* parent = FindObj(base_path);
    if (!parent) return nullptr;
    return Fn<void*(*)(void*, void*, const char16_t*, const char16_t*, uint32_t, void*)>(
        profile::StaticLoadClass)(parent, nullptr, s.c_str(), nullptr, 0, nullptr);
}
static void* GetPC(void* world, int index = 0) {
    if (!world) return nullptr;
    return Fn<void*(*)(void*, int)>(profile::GetPlayerController)(world, index);
}
static void* GetPawn(void* controller) {
    if (!controller) return nullptr;
    static void* pawnProp = nullptr;
    static bool resolved = false;
    if (!resolved) { pawnProp = GetProp("/Script/Engine.Controller.Pawn"); resolved = true; }
    if (!pawnProp) return nullptr;
    return Read<void*>(GetField(controller, pawnProp));
}
static void Possess(void* controller, void* pawn) {
    if (!controller || !pawn) return;
    Call c("/Script/Engine.Controller.Possess");
    if (!c.ok()) { LOG("Possess: binding failed"); return; }
    if (!c.set("InPawn", pawn)) { c.dump("Possess"); return; }
    c.run(controller);
}
static void* SpawnActor(void* world, void* klass, void* owner, const Transform& t) {
    if (!world || !klass) return nullptr;
    void* actor = Fn<void*(*)(void*, void*, const Transform*, uint8_t, void*)>(
        profile::BeginSpawn)(world, klass, &t, 1, owner);
    return actor ? Fn<void*(*)(void*, const Transform*)>(profile::FinishSpawn)(actor, &t) : nullptr;
}
static void* ResolveInterface(void* object, void* ifaceClass) {
    if (!object || !ifaceClass) return nullptr;
    using Fn_t = void*(*)(void*, void*);
    return ((Fn_t)(g_base + profile::GetInterfaceAddress))(object, ifaceClass);
}
// GUID generation through the native Kismet library (verified in 21.30).
static Guid MakeGuid() {
    Guid g{};
    void* klib = FindObj("/Script/Engine.Default__KismetGuidLibrary");
    if (klib) { Call c("/Script/Engine.KismetGuidLibrary.NewGuid"); if (c.ok() && c.run(klib)) g = c.result<Guid>(); }
    return g;
}

// ============================================================================
// SESSION
// ============================================================================
struct Session {
    void* world = nullptr;
    int   world_index = -1;
    void* controller = nullptr;
    void* mode = nullptr;
    void* state = nullptr;
    void* pawn = nullptr;
    void* inventory = nullptr;
    void* quickbars = nullptr;
    void* player_state = nullptr;      // [F43] Erbium FortPlayerStateAthena parity
    void* tactical_sprint = nullptr;
    bool  cosmetics_with_state = false; // [F82] cosmetic flow ran WITH a PlayerState present
    bool  initialized = false;
    bool  inventory_ready = false;
    bool  aircraft_jumped = false;
    bool  attempted = false;
    bool  finalized = false;
    bool  cheats_ready = false;
    bool  safezone_spawned = false;
    bool  sprint_applied = false;
    bool  ready_called = false;
    bool  visibility_forced = false;
    bool  abilities_granted = false;
    bool  melee_granted = false;
    bool  stamina_initialized = false;
    bool  tactical_sprint_setup = false;
    bool  quickbars_initialized = false;
    bool  movement_enabled = false;
    bool  body_fixed = false;           // [F41/F83-b] locomotion AnimInstance restored
    bool  world_abilities_granted = false; // [F52/F83-b] slide + vehicle ability specs
    bool  lobby_requested = false;      // [F24]
    int   last_phase = -1;              // [F24] GamePhase tracking
    // [F21] local build dedup + touch polling
    void*  held_build_class = nullptr;
    void*  last_build_class = nullptr;
    double last_build_time = 0;
    double held_build_time = 0;
    std::array<double,6> held_build_pose{};
    std::array<double,6> last_build_pose{};
    void*  locker_pickaxe = nullptr;      // [F22] resolved by SyncLockerLoadout
    bool   locker_resynced = false;       // [G12] one finalize-time MCP re-sync
    double observed_at = 0;
    double retry_at = 0;
    double finalize_at = 0;
    double next_build_poll = 0;
    std::array<void*, 6> slots{};           // Primary quickbar (weapons)
    std::array<void*, 6> secondary_slots{}; // Secondary quickbar (build pieces) [F19]
    std::vector<void*> character_parts;
    std::vector<void*> granted_ability_sets;
    void* edit_tool = nullptr;
    void* wall = nullptr;
    void* floor = nullptr;
    void* stairs = nullptr;
    void* roof = nullptr;
    // [F28] vehicles
    bool  vehicles_spawned = false;
    int   vehicle_attempts = 0;         // [F41] keep retrying until spawners stream in
    int   qb_retries = 0;               // [F91/BEX-1] per-match QB retry budget (was a Tick function-local static — a second match inherited the exhausted counter and never retried QuickBars)
    Guid  vehicle_weapon_guid{};
    int   vehicle_weapon_seat = -1;
    bool  vehicle_weapon_active = false;
    Guid  previous_weapon_guid{};
    // [F29] shared build dedupe (RPC path records, touch path honors)
    double rpc_build_time = 0;
    // [F35] Erbium pickup pipeline: pickups fly to the player natively, the
    // inventory transfer is scheduled after the FlyTime animation.
    struct PendingPickup { void* pickup; double at; };
    std::vector<PendingPickup> pending_pickups;
    // [F35] cached weak ref of the pawn (PickupTarget)
    bool  pawn_weak_ready = false;
    int32 pawn_weak_idx = -1;
    int32 pawn_weak_serial = 0;
    // [F40] match state (alive players / players-left HUD)
    bool  match_state_ready = false;
    // [G16-a] doors WE opened (toggle direction source — standalone drops
    // the server RPCs, so our open/close is the only door driver)
    std::vector<void*> opened_doors;
    double last_door_scan = 0;
    // [F-cities] building-instruction handler load attempts (streaming window)
    int   building_load_attempts = 0;
    // [G17/G18] death / win / REAL alive-players state
    bool  death_handled = false;        // one-shot death flow latch
    bool  win_handled = false;          // one-shot victory flow latch
    int   players_override = 0;         // era.players N testing floor (0 = REAL counting only)
    int   real_alive = -1;              // last real alive count (-1 = not counted yet)
    int   players_ever = 0;             // highest real player count ever observed (win-while-alive gate)
    int   alive_source = 0;             // 1 = PlayerArray, 2 = local fallback
    float last_known_health = 0.0f;    // death edge detection — only a REAL observed-alive → dead transition counts
    int8_t death_zero_streak = 0;      // debounce: two consecutive zero polls required
    double last_health_poll = 0;        // 0.5s death-watch cadence
    double last_alive_poll = 0;         // 1s real-alive recount cadence
    // [G18-b] quickbar self-healing watch
    int   qb_watch_tries = 0;
    bool  qb_slots_verified = false;
    double last_qb_watch = 0;
    // [G18-c] climb assist chain cap (reset on leaving Falling)
    int   climb_boosts = 0;
    // [G17-a] mesh-network / playlist levels
    bool  mesh_network_fixed = false;
    int   mesh_network_attempts = 0;
    bool  playlist_levels_done = false;
    // [G19-d] sublevel force passes / post-death pawn staleness guard
    int   sublevel_passes = 0;
    bool  pawn_cleared = false;
    // [G22] World Partition diagnostic passes
    int   wp_diag_passes = 0;
    // [G23-e] streaming-source forcing passes (own ticker — no longer tied
    // to the WP diagnostic, whose part-B crash used to kill the attempt)
    int   src_force_passes = 0;
    // [G24] void-guard (fall-through rescue)
    Vector last_safe_loc{};
    bool  has_safe_loc = false;
    Vector spawn_loc{};
    bool  has_spawn_loc = false;
    int   void_rescues = 0;
    double last_void_rescue = 0;
} session;

static double Monotonic() {
    timespec ts{}; clock_gettime(CLOCK_MONOTONIC, &ts);
    return double(ts.tv_sec) + double(ts.tv_nsec) * 1e-9;
}
// [F15] Standalone gating — never touch an online session.
static bool WorldIsStandalone(void* world) {
    if (!g_base || !world) return false;
    using IsStandalone_t = bool(*)(void*);
    return Fn<IsStandalone_t>(profile::IsStandalone)(world);
}
static bool ActorLocation(void* actor, Vector& out) {
    Call c("/Script/Engine.Actor.K2_GetActorLocation");
    if (!c.ok() || !c.run(actor)) return false;
    out = c.result<Vector>();
    return std::isfinite(out.X) && std::isfinite(out.Y) && std::isfinite(out.Z);
}
static bool NearPlayer(const Vector& point, float limit) {
    Vector origin{};
    if (!session.pawn || !ActorLocation(session.pawn, origin)) return false;
    double dx = point.X - origin.X, dy = point.Y - origin.Y, dz = point.Z - origin.Z;
    return dx*dx + dy*dy + dz*dz <= double(limit) * double(limit);
}

// [G17] Find a UScriptStruct by object name with member validation (module
// renames across builds break hard-coded /Script/ paths; the members prove
// identity — used for DeathInfo / FFortPlayerDeathReport layouts).
static void* FindScriptStructNamed(const char* name, const char* mustA, const char* mustB) {
    GObjects_t* go = GObjectArray();
    if (!go) return nullptr;
    for (int32 i = 0; i < go->NumElements; ++i) {
        void* o = go->GetById((uint32)i);
        if (!o || IsBadPtr(o)) continue;
        if (ObjName(o) != name) continue;
        // Must look like a struct with the expected members.
        if (!FindChildProp(o, mustA)) continue;
        if (mustB && !FindChildProp(o, mustB)) continue;
        return o;
    }
    return nullptr;
}

// [G19-a] Post-death pawn sanity. The engine may have already destroyed the
// pawn (KillZ / fast eliminate) while our 1s death debounce was running —
// any native call on a freed actor is a guaranteed crash. The controller's
// own Pawn reference is authoritative: destroyed ⇒ unpossessed ⇒ null or a
// different pawn. Class/vtable readability is the second gate.
static bool PawnStillValid(void* pawn) {
    if (!pawn || IsBadPtr(pawn)) return false;
    void* cls = Read<void*>(pawn, profile::ObjectClass);
    if (!cls || IsBadPtr(cls)) return false;
    void* vt = Read<void*>(pawn, 0);
    if (!vt || IsBadPtr(vt)) return false;
    if (session.controller) {
        void* cur = GetPawn(session.controller);
        if (cur != pawn) return false;   // unpossessed / replaced — stale
    }
    return true;
}

// [G17] Read the pawn's current health through the public accessor.
static float PawnHealth(void* pawn) {
    if (!pawn || IsBadPtr(pawn)) return -1.0f;
    Call h("/Script/FortniteGame.FortPawn.GetHealth");
    if (!h.ok() || !h.run(pawn)) return -1.0f;
    return h.result<float>();
}

// ============================================================================
// ATHENA NAMESPACE
// ============================================================================
namespace Athena {
    void SetupWorld(void* world);
    void Tick();
    bool TryInitializePlayer(void* world, void* mode, void* controller);
    void SetupCheats();
    void FinalizePlayer();
    void ShowSkin();
    void SyncLockerLoadout();          // [F22] MCP locker sync
    void LoadProfileCosmetics();
    void LoadDefaultCosmetics();
    void ForcePlayerVisible();
    void BeginPlayerSkydiving();
    void EquipSlot(int slot);
    void DestroyLods();
    void DropLoadingScreen();
    void FixBuildingFoundations();
    void FixMissingBuildings();        // [F-cities] standalone building-load driver
    void TickBuildingLoads();          // [F-cities] streaming retry window
    void TickFloorLootStreaming();     // [F-loot-auto] floor loot while streaming
    void FixBus();
    void InitializeInventory();
    bool GiveItem(void* def, int count, int slot, uint8_t bar, bool require_new, int loadedAmmo = -1, int level = 1); // [F17/F31]
    bool AddToInventory(void* def, int count, int slot);
    void InventoryUpdate();
    void GrantDefaultAbilities();
    void ActivateAbilityByClass(void* klass);   // [G03] activation helper — granting flows through the sets
    void GrantAbility(void* klass);
    void GrantAllAbilitySets();
    void GrantWorldAbilities();        // [F52/F83-b] slide + vehicle abilities as real specs
    void FixBodyAndAnimations();       // [F41/F68/F83-b] restore missing locomotion AnimInstance
    void EnsurePlayerState();          // [F43] Erbium standalone PlayerState 1:1
    void InitStaminaAttributes();
    void RefreshHUD();
    void ApplySprintSpeed();
    void SetupTacticalSprint();
    void SetTacticalSprinting(bool sprinting);
    void SpawnSafeZoneIndicator();
    void SpawnPickup(void* def, int count, Vector loc, uint8_t sourceFlags, uint8_t spawnSource, bool toss, int level = -1, int loadedAmmo = -1);
    void DropInventoryItem(Guid guid, int count);
    void OnServerExecuteInventoryItem(Guid guid);
    void OnServerExecuteInventoryWeapon(void* weapon);
    void HandleJumpFromHudStart();
    void SpawnBattleBus();
    void OnAircraftJump();
    void TeleportToSpawnIsland();
    void RemoveNetDebugUI();
    void SpawnBuildPreviews();
    bool LocalPickup(void* pickup);    // [F18]
    void Loot(void* actor);
    void HandleOpenable(void* target);   // [G16-a] ANY IOpenableInterface actor
    void TickSprintDoorAssist();         // [G16-a] safe bash-through (open-only, no pins)
    void GrantWorldAbilities();       // [F52/F67/G15-e] slide/vehicle/emote abilities
    void OnServerCreateBuildingActor(void* params);
    bool LocalBuild();                 // [F21]
    bool SpawnBuildingActor(void* klass, const Vector& loc, const Rotator& rot, bool mirrored, int level); // [F30]
    void PollTouchBuild();             // [F21/F29] dedupe shared with the RPC path

    // [F28] Vehicles (Erbium AFortAthenaVehicleSpawner path, 1:1)
    void SpawnVehicles();
    void TickVehicleSeats(bool force = false);   // [G09] force bypasses the 0.5s poll throttle
    void GiveVehicleWeapon(void* weaponDef, int seatIdx);
    void RemoveVehicleWeapon(bool reequip);
    void TickMantleAssist();           // [F45] 21.30 wall climb / mantle assist
    void TickRealityProximity();       // [F44] reality tree proximity effect
    void OnBeginEditActor(void* piece);
    void OnFinishEditActor(void* actor, void* newClass, int rotIter, bool mirrored);
    void PlayEmoteItem(void* item);
    void OnExitVehicle();
    bool EnterVehicleDirect(void* vehicle, int seatIdx);   // [G19-b] shared verified seat ladder
    bool ExitVehicleDirect();                              // [G21] exit ladder (OnPawnExitVehicle + weapon restore)
    void* NearestVehicle(float radius);                    // [G19-b] HUD-button fallback target
    void DispatchHudInteract();                            // [G21] HUD interact button → nearest target dispatch
    void DiagnoseWorldPartition();                         // [G22] WP probes + activation attempts
    void TickStreamingSources();                            // [G23-e] bIsStreamingSource forcing + FlushLevelStreaming
    void TickVoidGuard();                                   // [G24] fall-through rescue (below-map teleport)
    void ForceSublevels();                                 // [G19-d] sublevel geometry (missing cities)
    void ReturnToLobby(void* world);   // [F24]
    void TeleportToReticle();          // [F24]
    void ApplyCheats(const std::string& cmd); // [F24] fly/walk/ghost/god
    void CheatScript(const char* script);

    // QuickBars [F19]
    void InitializeQuickBarsNative();
    void SetQuickBarReady();
    void VerifyHUDReadPath();   // [F92] replay the exact native HUD read chain
    bool BindSlot(void* item, int slot, uint8_t bar);
    void ActivateQuickBarSlot(uint8_t bar, int slot);
    void ForceUpdateQuickBars();
    void BuildQuickBarsManually();

    // Loot [F20/F32]
    void SpawnFloorLoot();
    void ResetStaleContainers();       // [F34] standalone cache fix
    bool SearchContainer(void* actor, const Vector& location, bool force = false, bool floorLoot = false);   // [F-loot-place] floorLoot: authentic static placement

    // [F18] pickups / [movement]
    void ServerHandlePickup(void* pickup);
    void EnableMovementFeatures();

    // [F35] Erbium pickup pipeline (SetPickupTarget / InternalPickup parity)
    void HandlePickupRPC(void* pickup, float flyTime);
    void ProcessPendingPickups();
    void SetPickupTargetAnim(void* pickup, float flyTime);
    bool InternalPickup(void* entry, void* pickupActor);

    // [F36] QuickBars native binding (ServerAddItemInternal parity)
    bool QBAddItem(Guid guid, bool bSecondary, int slot);
    bool QBBindController(void* def, uint8_t bar, int slot);   // [G18-b] AddItemToQuickBars
    void QBActivateSlot(bool bSecondary, int slot);
    void QBRemoveItem(Guid guid, int slot);

    // [F39] harvest resources on building damage (Erbium OnDamageServer 1:1)
    void HandleDamageServer(void* actor, void* func, void* params);

    // [F40] alive-players / players-left HUD state
    void SetupMatchState();

    // [G17-a] Erbium UE4.27+ mesh-network parity + additional playlist levels
    void FixMeshNetwork();
    void TickMeshNetwork();
    void StreamPlaylistLevels();

    // [G17-c] death flow — Erbium AFortPlayerControllerAthena::ClientOnPawnDied
    // server-side parity, driven from a local health watcher
    void TickDeathWatch();
    void HandlePlayerDeath(bool forced);

    // [G18-a] REAL alive-players system (Erbium PlayersLeft parity)
    void TickRealAlive();
    int  CountRealAlivePlayers(int* totalOut);
    void TickQuickBarWatch();   // [G18-b] self-healing slot verification

    // [G17-e] Erbium win parity — Victory Royale banner
    void HandleVictory(bool onDeath = false);

    void DebugDump();
}

// ============================================================================
// ABILITY SYSTEM
// ============================================================================
static void* GetASC(void* actor) {
    if (!actor) return nullptr;
    void* asc = GetObjField(actor, "/Script/FortniteGame.FortPawn.AbilitySystemComponent");
    if (asc) return asc;
    asc = GetObjField(actor, "/Script/FortniteGame.FortPlayerState.AbilitySystemComponent");
    if (asc) return asc;
    asc = GetObjField(actor, "/Script/GameplayAbilities.AbilitySystemComponent");
    return asc;
}

// ============================================================================
// [G02] NATIVE GAS ENTRY POINTS — Erbium Finders.cpp ported through the
// [G01] scanner. The engine's own log literals are the anchors; every entry
// degrades gracefully to the reflection path when not found.
// ============================================================================
namespace gas {
// void AFortPlayerState::InitializePlayerGameplayAbilities(const IFortAbilitySystemInterface*)
static uintptr_t InitializePlayerGameplayAbilities = 0;
// void UAbilitySystemComponent::GiveAbilityAndActivateOnce(this, FGameplayAbilitySpecHandle* out, FGameplayAbilitySpec* spec, void* predictionKey)
static uintptr_t GiveAbilityAndActivateOnce = 0;
// bool UAbilitySystemComponent::InternalTryActivateAbility(this, handle, predictionKey, UFortGameplayAbility** outInstanced, void*, void* triggerEventData)
static uintptr_t InternalTryActivateAbility = 0;
// void AFortPlayerState::ApplyCharacterCustomization(AFortPlayerState*, APawn*)
static uintptr_t ApplyCharacterCustomization = 0;
// [G05] void SendRequestNow(void* Arg1, void* MCPData, int ContextCredentials)
static uintptr_t SendRequestNow = 0;
static bool discovered = false;
static bool gas_native_failed = false;   // [G11] one fault → native grant path off for the session

// [G04] Reflection-driven FGameplayAbilitySpec layout — offsets are resolved
// from the live UScriptStruct, so "print Spec.InputID before/after grant"
// works without a single hardcoded struct offset.
struct SpecLayout {
    int OffHandle = -1, OffAbility = -1, OffLevel = -1, OffInputID = -1, OffSourceObject = -1;
    int Size = 0;
    bool ok() const { return OffAbility >= 0 && OffInputID >= 0 && Size >= 0x20; }
};
static bool ResolveSpecLayout(SpecLayout& out) {
    void* propAbility = GetProp("/Script/GameplayAbilities.GameplayAbilitySpec.Ability");
    void* propInput   = GetProp("/Script/GameplayAbilities.GameplayAbilitySpec.InputID");
    if (!propAbility || !propInput) return false;
    void* propHandle = GetProp("/Script/GameplayAbilities.GameplayAbilitySpec.Handle");
    void* propLevel  = GetProp("/Script/GameplayAbilities.GameplayAbilitySpec.Level");
    void* propSrc    = GetProp("/Script/GameplayAbilities.GameplayAbilitySpec.SourceObject");
    void* stru = FindObj("/Script/GameplayAbilities.GameplayAbilitySpec");
    if (!stru || IsBadPtr(stru)) return false;
    out.OffAbility   = Read<int>(propAbility, profile::PropOffset);
    out.OffInputID   = Read<int>(propInput,   profile::PropOffset);
    out.OffHandle    = propHandle ? Read<int>(propHandle, profile::PropOffset) : 0x10;
    out.OffLevel     = propLevel  ? Read<int>(propLevel,  profile::PropOffset) : out.OffInputID + 4;
    out.OffSourceObject = propSrc ? Read<int>(propSrc, profile::PropOffset) : -1;
    out.Size         = Read<int32>(stru, profile::StructPropertiesSize);
    return out.ok();
}

// Erbium's manual spec fill (GiveAbility fallback shape): zeroed buffer,
// FastArraySerializer keys = -1, Handle = rand, Ability = CDO, Level = 1,
// InputID = -1 (input binding comes from the native set paths). Caller frees.
// [G15-g] TRUE sizeof(FGameplayAbilitySpec): the Items property's Inner
// FStructProperty carries the engine's own ElementSize for the array — the
// number TArray<FGameplayAbilitySpec> advances per element. PropertiesSize
// (@0x54 → 111) is only the REFLECTED property end; the native members
// (ContainerInterface/Triggers/Fortnite extras) extend the real size to
// 0xE8 = 232 on this build. Device proofs: the valid #29 spec
// (Default__GA_Athena_Grenade_Rethrow_C) sits at byte 3248 = 14 × 232
// exactly, and slot0's byte pattern only parses under the Erbium
// FFastArraySerializerItem layout (RepID@0/Key@4/MostRecent@8, Handle@0xC,
// Ability@0x10, Level@0x18, InputID@0x1C, SourceObject@0x20,
// ContainerInterface@0x28, Triggers@0x70={ptr,Num,Max}).
static int SpecArrayStride() {
    static int s_stride = -2;
    if (s_stride != -2) return s_stride;
    s_stride = 0;
    void* itemsProp = GetProp("/Script/GameplayAbilities.GameplayAbilitySpecContainer.Items");
    if (itemsProp) {
        void* inner = Read<void*>(itemsProp, 0x70);            // FArrayProperty::Inner
        if (inner && !IsBadPtr(inner)) {
            int es = Read<int>(inner, profile::PropElementSize);
            if (es >= 0x40 && es <= 0x400) s_stride = es;
        }
    }
    return s_stride;
}
static void* BuildSpec(void* abilityCdo, void* sourceObject) {
    static SpecLayout layout; static bool resolved = false;
    if (!resolved) { resolved = true; if (!ResolveSpecLayout(layout)) LOG("[GAS] ⚠️ GameplayAbilitySpec reflection missing"); }
    if (!layout.ok() || !abilityCdo) return nullptr;
    // [G15-g] sizeof = the ENGINE's array ElementSize (232 on this build) —
    // the native GiveAbilityAndActivateOnce reads sizeof(FGameplayAbilitySpec)
    // bytes out of this buffer; the old align8(PropertiesSize)=112 sizing
    // UNDER-ALLOCATED by 120 bytes (heap overread). Fall back to align8(props)
    // only when the Inner ElementSize is unavailable.
    int trueStride = SpecArrayStride();
    size_t specBytes = (size_t)std::max(trueStride > 0 ? trueStride : ((layout.Size + 7) & ~7), 0x40);
    void* spec = malloc(specBytes);
    if (!spec) return nullptr;
    memset(spec, 0, specBytes);
    Write<int32>(spec, 0x0, -1);                              // ReplicationID
    Write<int32>(spec, 0x4, -1);                              // ReplicationKey
    Write<int32>(spec, 0x8, -1);                              // MostRecentArrayReplicationKey
    Write<int32>(spec, layout.OffHandle, (int32)rand());      // Handle
    Write<void*>(spec, layout.OffAbility, abilityCdo);
    Write<int32>(spec, layout.OffLevel, 1);
    Write<int32>(spec, layout.OffInputID, -1);
    if (layout.OffSourceObject >= 0) Write<void*>(spec, layout.OffSourceObject, sourceObject);
    return spec;
}

// CDO resolution by the engine's own "Default__" naming (StaticFindObject
// path first, GObjects name-scan second — UClass::ClassDefaultObject is not
// reflected in this dump).
static void* ClassDefaultOf(void* klass) {
    if (!klass || IsBadPtr(klass)) return nullptr;
    static std::map<void*, void*> cache;
    auto it = cache.find(klass);
    if (it != cache.end()) return it->second;
    void* cdo = nullptr;
    {
        // Build "<outer path>.Default__<name>" from the class's outer chain.
        std::vector<std::string> parts;
        void* cur = klass;
        for (int d = 0; d < 8 && cur && !IsBadPtr(cur); ++d) {
            parts.push_back(ObjName(cur));
            cur = Read<void*>(cur, profile::ObjectOuter);
        }
        if (!parts.empty()) {
            std::string path;
            for (int d = (int)parts.size() - 1; d >= 0; --d) { path += parts[(size_t)d]; if (d) path += "."; }
            size_t dot = path.rfind('.');
            if (dot != std::string::npos)
                cdo = FindObj(path.substr(0, dot) + ".Default__" + parts[0]);
        }
    }
    if (!cdo) {
        GObjects_t* go = GObjectArray();
        if (go) {
            std::string want = "Default__" + ObjName(klass);
            for (int32 i = 0; i < go->NumElements && !cdo; ++i) {
                void* o = go->GetById((uint32)i);
                if (!o || IsBadPtr(o) || Read<void*>(o, profile::ObjectClass) != klass) continue;
                if (ObjName(o) == want) cdo = o;
            }
        }
    }
    if (cdo) cache[klass] = cdo;
    return cdo;
}

// One-time discovery — Worker thread, before InstallHooks.
static void Discover() {
    if (discovered) return;
    discovered = true;
    double t0 = Monotonic();
    InitializePlayerGameplayAbilities = arm::FindFunctionByString(
        "InitializePlayerGameplayAbilities with invalid PlayerStateOrProxy!");
    GiveAbilityAndActivateOnce = arm::FindFunctionByString(
        "GiveAbilityAndActivateOnce called on ability %s on the client, not allowed!");
    InternalTryActivateAbility = arm::FindFunctionByString(
        "InternalTryActivateAbility called with invalid Handle! ASC: %s. AvatarActor: %s");
    ApplyCharacterCustomization = arm::FindFunctionByString(
        "AFortPlayerState::ApplyCharacterCustomization - Failed initialization, using default parts. "
        "Player Controller: %s PlayerState: %s, HeroId: %s");
    SendRequestNow = arm::FindFunctionByString("MCP-Profile: Dispatching request to %s");
    if (!SendRequestNow)
        SendRequestNow = arm::FindFunctionByString(
            "MCP-Profile: Dispatching request to %s - ContextCredentials: %s");
    LOG("[GAS] discovery done in %.1fs — init=%d gaa=%d ita=%d acc=%d mcp=%d",
        Monotonic() - t0,
        InitializePlayerGameplayAbilities ? 1 : 0, GiveAbilityAndActivateOnce ? 1 : 0,
        InternalTryActivateAbility ? 1 : 0, ApplyCharacterCustomization ? 1 : 0,
        SendRequestNow ? 1 : 0);
}

// [G12] Late re-discovery (Erbium finders are lazy for the same reason):
// paged .rodata only becomes resident after its subsystem first touches it,
// so anything the startup pass missed gets another chance once the game
// world is live. Newly-found SendRequestNow is hooked on the spot by the
// caller (TryInstallMcpHook) — that hook is the MCP profile enabler.
static void RediscoverMissing() {
    if (!InitializePlayerGameplayAbilities)
        InitializePlayerGameplayAbilities = arm::FindFunctionByString(
            "InitializePlayerGameplayAbilities with invalid PlayerStateOrProxy!");
    if (!GiveAbilityAndActivateOnce)
        GiveAbilityAndActivateOnce = arm::FindFunctionByString(
            "GiveAbilityAndActivateOnce called on ability %s on the client, not allowed!");
    if (!InternalTryActivateAbility)
        InternalTryActivateAbility = arm::FindFunctionByString(
            "InternalTryActivateAbility called with invalid Handle! ASC: %s. AvatarActor: %s");
    if (!ApplyCharacterCustomization)
        ApplyCharacterCustomization = arm::FindFunctionByString(
            "AFortPlayerState::ApplyCharacterCustomization - Failed initialization, using default parts. "
            "Player Controller: %s PlayerState: %s, HeroId: %s");
    if (!SendRequestNow) {
        SendRequestNow = arm::FindFunctionByString("MCP-Profile: Dispatching request to %s");
        if (!SendRequestNow)
            SendRequestNow = arm::FindFunctionByString(
                "MCP-Profile: Dispatching request to %s - ContextCredentials: %s");
    }
    LOG("[GAS] rediscovery pass — init=%d gaa=%d ita=%d acc=%d mcp=%d",
        InitializePlayerGameplayAbilities ? 1 : 0, GiveAbilityAndActivateOnce ? 1 : 0,
        InternalTryActivateAbility ? 1 : 0, ApplyCharacterCustomization ? 1 : 0,
        SendRequestNow ? 1 : 0);
}

// Spec count through ActivatableAbilities (FGameplayAbilitySpecContainer).
static int CountActivatableSpecs(void* asc = nullptr);

// [G23] The fault guard itself (sigjmp machinery + RunGuarded with the
// nested-depth reentrancy fix) now lives BEFORE class Call — Call::run
// dispatches every UFunction through it. See the [G11/G13/G23] block above.
static void NativeInitGasCall(void* thisPtr, void* proxyArg) {
    // [G14] 8-arg prototype: the compiler materializes x2..x7 = 0 explicitly.
    // The twin device faults (identical addr, two DIFFERENT x0 values) prove
    // the faulting load at target+0xC reads a stale argument register — the
    // 1-arg cast left x1..x7 holding whatever the previous call left behind.
    // Arg2 carries the PlayerStateOrProxy the native validates ("with invalid
    // P..."); nullptr trips its own null-guard branch (logs, returns clean).
    ((void (*)(void*, void*, void*, void*, void*, void*, void*, void*))
        InitializePlayerGameplayAbilities)(thisPtr, proxyArg,
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
}
// [G13] Wrapped native entries for the other scanner-resolved calls.
static void NativeAccCall(void* playerState, void* pawn) {
    ((void (*)(void*, void*))ApplyCharacterCustomization)(playerState, pawn);
}
static void NativeGaaCall(void* asc, void* outHandle, void* spec) {
    ((void (*)(void*, void*, void*, void*))GiveAbilityAndActivateOnce)(asc, outHandle, spec, nullptr);
}

// [G02/G11/G13] The Erbium primary grant: the native InitializePlayerGameplay-
// Abilities resolves the pawn/PlayerState DefaultAbilitySets from the CDOs,
// builds specs WITH their declared InputIDs (GAS-driven anims), and delivers
// them through the native GiveAbility chain — never a hand-rebuilt spec.
// [G25] CONVENTION ANALYSIS (round-10 log): conventions 1–4 all faulted at
// garbage pcs (0x14ff4a9bd57f6 / 0x157f6f81c0ff7 — the low-52-bits of a
// function's first 8 PROLOGUE BYTES read as a pointer: an indirect branch
// through data loaded from the PROXY), while convention 5 (nullptr proxy)
// reached REAL code at fn+0xC faulting on [proxy+8]. That means:
//   • the target address is CORRECT (fn+0x5E9A304 executes),
//   • arg2 must be a POINTER TO A STRUCT with a pointer at +8 — the exact
//     shape of Erbium's TScriptInterface<IFortAbilitySystemInterface>
//     { UObject* ObjectPointer@0; void* InterfacePointer@8 } (see their
//     AbilitySystemComponent.cpp GiveAbilitySet — they build this pair for
//     the sibling EquipFortAbilitySet call).
// The round-10 matrix therefore leads with TScriptInterface-shaped pairs:
//   1. ps-iface + &pair{PlayerState, ps-iface}   (Erbium GiveAbilitySet shape)
//   2. ps-iface + &pair{ps-iface, PlayerState}   (swapped pair)
//   3. ps-iface + PlayerState (old conv — kept for continuity)
//   4. playerstate + PlayerState (plain member convention)
//   5. pawn-iface + PlayerState
//   6. ps-iface + pawn
//   7. ps-iface + nullptr (guard branch — clean, grants nothing)
// All fault-guarded; first non-faulting convention wins and is remembered.
// gas_native_failed is only set when EVERY self-valid attempt faulted.
static int gas_working_conv = -1;      // [G14] remembered non-faulting convention
struct SIShapeProxy { void* obj; void* iface; };   // [G25] TScriptInterface pair
static bool NativeInitializePlayerGameplayAbilities() {
    if (!InitializePlayerGameplayAbilities) return false;
    if (gas_native_failed) { LOG("[GAS] native path disabled (previous fault)"); return false; }
    void* ifaceType = FindObj("/Script/FortniteGame.FortAbilitySystemInterface");
    if (!ifaceType) ifaceType = FindObj("/Script/FortniteGame.FortPlayerState.FortAbilitySystemInterface");
    void* target = session.player_state ? session.player_state : session.pawn;
    if (!target) return false;
    void* ps = session.player_state;
    void* pawn = session.pawn;
    LOG("[GAS] native init: fn@+0x%llX target=%p (%s) ifaceClass=%s",
        (unsigned long long)(InitializePlayerGameplayAbilities - g_base), target,
        target == session.player_state ? "PlayerState" : "pawn",
        ifaceType ? "found" : "MISSING");
    auto usable = [](void* p) -> void* {
        if (!p || !IsReadable(p, 8)) return nullptr;
        void* vt = Read<void*>(p, 0);
        if (!vt || !IsReadable(vt, 8)) return nullptr;
        return p;
    };
    void* psIface = ifaceType ? usable(ResolveInterface(target, ifaceType)) : nullptr;
    void* pawnIface = (ifaceType && pawn && pawn != target) ? usable(ResolveInterface(pawn, ifaceType)) : nullptr;
    LOG("[GAS] interface pointers: ps=%p pawn=%p (working convention #%d)",
        psIface, pawnIface, gas_working_conv);
    // [G11] The native targets the PlayerState's ASC while the fallback
    // targets the pawn's — measure BOTH so a successful native grant is
    // not reported as a failure (and vice versa).
    void* psAsc = GetObjField(target, "/Script/FortniteGame.FortPlayerState.AbilitySystemComponent");
    int before = CountActivatableSpecs();
    int beforePs = psAsc ? CountActivatableSpecs(psAsc) : -1;
    // [G25] the two TScriptInterface pairs — live for the whole probe
    SIShapeProxy pairA{ ps, psIface };      // { ObjectPointer, InterfacePointer }
    SIShapeProxy pairB{ psIface, ps };
    struct Conv { void* self; void* proxy; const char* name; };
    Conv convs[7] = {
        { psIface,   &pairA,  "ps-iface + TScriptInterface{ps,iface}" },
        { psIface,   &pairB,  "ps-iface + TScriptInterface{iface,ps}" },
        { psIface,   ps,      "ps-iface + PlayerStateOrProxy" },
        { target,    ps,      "playerstate + PlayerStateOrProxy" },
        { pawnIface, ps,      "pawn-iface + PlayerStateOrProxy" },
        { psIface,   pawn,    "ps-iface + pawn" },
        { psIface,   nullptr, "ps-iface + nullptr" },
    };
    int order[7] = { 0, 1, 2, 3, 4, 5, 6 };
    if (gas_working_conv > 0 && gas_working_conv < 7) {
        order[0] = gas_working_conv;
        order[gas_working_conv] = 0;
    }
    int faults = 0, ranClean = -1;
    for (int oi = 0; oi < 7; ++oi) {
        Conv& cv = convs[order[oi]];
        if (!cv.self) continue;
        LOG("[GAS] calling native InitializePlayerGameplayAbilities(%s, this=%p, proxy=%p) — pawn specs=%d ps specs=%d",
            cv.name, cv.self, cv.proxy, before, beforePs);
        if (!RunGuarded("GAS", NativeInitGasCall, cv.self, cv.proxy)) {
            ++faults;
            LOG("[GAS] convention '%s' faulted — trying next", cv.name);
            continue;
        }
        gas_working_conv = order[oi];
        ranClean = order[oi];
        int after = CountActivatableSpecs();
        int afterPs = psAsc ? CountActivatableSpecs(psAsc) : -1;
        LOG("[GAS] InitializePlayerGameplayAbilities: pawn specs %d -> %d, ps specs %d -> %d",
            before, after, beforePs, afterPs);
        if (after > before || (afterPs > beforePs && beforePs >= 0)) return true;
        // Ran clean but granted nothing (null-guard branch or a client-side
        // early-out) — another convention may still deliver.
    }
    if (ranClean < 0 && faults > 0) {
        gas_native_failed = true;
        LOG("[GAS] every native convention faulted — path disabled for the session");
    } else if (ranClean >= 0) {
        LOG("[GAS] native ran clean (convention #%d) but granted nothing — reflection path wins",
            ranClean);
    }
    return false;
}

// Spec count through ActivatableAbilities (FGameplayAbilitySpecContainer).
static int CountActivatableSpecs(void* asc) {
    if (!asc) asc = GetASC(session.pawn);
    if (!asc) return -1;
    static int contOff = -2, itemsOff = -2, stride = -2;
    if (contOff == -2) {
        contOff = -1; itemsOff = -1; stride = -1;
        void* contProp = GetProp("/Script/GameplayAbilities.AbilitySystemComponent.ActivatableAbilities");
        void* itemsProp = GetProp("/Script/GameplayAbilities.GameplayAbilitySpecContainer.Items");
        if (contProp && itemsProp) {
            contOff = Read<int>(contProp, profile::PropOffset);
            itemsOff = Read<int>(itemsProp, profile::PropOffset);
            void* inner = Read<void*>(itemsProp, 0x70);           // FArrayProperty::Inner (guess)
            if (inner && !IsBadPtr(inner)) {
                void* sStruct = Read<void*>(inner, 0x70);         // FStructProperty::Struct (guess)
                if (sStruct && !IsBadPtr(sStruct)) stride = Read<int32>(sStruct, profile::StructPropertiesSize);
            }
            if (stride <= 0) {
                // [G13] Engine truth, not an FField-offset guess: the spec
                // size straight from the live UScriptStruct.
                void* stru = FindObj("/Script/GameplayAbilities.GameplayAbilitySpec");
                if (stru && !IsBadPtr(stru)) stride = Read<int32>(stru, profile::StructPropertiesSize);
            }
            // [G14] PropertiesSize is the unpadded property end (111 on this
            // build) — the TArray element stride is sizeof() = align8(props).
            if (stride > 0) stride = (stride + 7) & ~7;
            if (stride <= 0) stride = 0x70;                        // 21.30 typical spec size fallback
        }
    }
    if (contOff < 0 || itemsOff < 0 || stride <= 0) return -1;
    Array items = Read<Array>((char*)asc + contOff + itemsOff);
    if (!items.Data || IsBadPtr(items.Data) || items.Count < 0 || items.Count > 4096) return 0;
    return items.Count;
}

// [G04/G13/G14] Dump every spec with ability name + InputID (the user-visible
// "granted but bound?" verification). Rate-free — call on demand.
// [G13] The second device crash was HERE: the array stride came from
// hardcoded FField::Inner/Struct offsets (0x70), specs #1+ read misaligned
// garbage ("inputID=7310658 level=510656512"), and spec #4 memcpy'd off the
// end of the array page — a hard, unguarded SIGSEGV.
// [G14-a] The THIRD run proved PropertiesSize (111) is still wrong AS A
// STRIDE: UScriptStruct::PropertiesSize is the UNPADDED end of the reflected
// properties (the last one is the InputPressed bitfield byte, so the value is
// odd) — a TArray<FGameplayAbilitySpec> advances by sizeof(), which UHT pads
// to the struct alignment (8: the spec holds pointers). stride =
// align8(111) = 112. The misaligned Ability pointers (#1-#3 blank names,
// pointer fragments read as ints) were the direct fallout.
// [G14-b] And the FATAL crash: a garbage Ability pointer passed ObjName's
// FName bounds checks and NameAppendString walked into unmapped memory.
// Now: stride auto-validation across 3 consecutive slots, hard pre-validation
// of every Ability pointer (alignment + readability + vtable inside
// libUnreal.so's PT_LOAD ranges), and a fault guard around the name fetch —
// a diagnostic can never kill the server.
static bool PtrInUnrealModule(const void* p) {
    if (!p) return false;
    uintptr_t v = (uintptr_t)p;
    for (const auto& r : arm::s_ranges)
        if (v >= r.start && v < r.end) return true;
    return false;
}
// A slot that looks like a real spec: small engine-assigned handle (1,2,3…),
// sane InputID/Level, and an Ability pointer that is 8-aligned, readable and
// carrying a vtable inside the module (a real UObject). Under the broken 111
// stride this fails by the 2nd/3rd slot — the InputID reads the middle of the
// Ability pointer — which is exactly what makes the auto-validation decisive.
static bool SpecSlotSane(const Array& items, int stride, int idx, const SpecLayout& L) {
    if (idx < 0 || (size_t)idx >= (size_t)items.Count) return false;
    uint8_t* p = (uint8_t*)items.Data + (size_t)idx * (size_t)stride;
    if (!IsReadable(p, (size_t)stride)) return false;
    int32 h = Read<int32>(p, L.OffHandle);
    int32 in = Read<int32>(p, L.OffInputID);
    int32 lv = L.OffLevel >= 0 ? Read<int32>(p, L.OffLevel) : 1;
    void* ab = Read<void*>(p, L.OffAbility);
    if (h < 0 || h > 0xFFFF || in < -1 || in > 0xFFFF || lv < 0 || lv > 0xFFFF) return false;
    if (!ab || ((uintptr_t)ab & 7) != 0 || !IsReadable(ab, 0x28)) return false;
    void* vt = Read<void*>(ab, 0);
    return vt && PtrInUnrealModule(vt);
}
// [G14-b] The guarded name fetch (ObjName → native NameAppendString). Writes
// into out[] only; a fault longjmps past it and leaves out[0] = 0.
static void SpecNameFetch(void* ability, char* out) {
    out[0] = 0;
    std::string n = ObjName(ability);
    if (!n.empty()) { strncpy(out, n.c_str(), 63); out[63] = 0; }
}
static void DumpSpecs(const char* tag) {
    void* asc = GetASC(session.pawn);
    if (!asc) return;
    static SpecLayout layout; static bool resolved = false;
    if (!resolved) { resolved = true; ResolveSpecLayout(layout); }
    void* contProp = GetProp("/Script/GameplayAbilities.AbilitySystemComponent.ActivatableAbilities");
    void* itemsProp = GetProp("/Script/GameplayAbilities.GameplayAbilitySpecContainer.Items");
    if (!contProp || !itemsProp) { LOG("[Specs] reflection missing"); return; }
    int contOff = Read<int>(contProp, profile::PropOffset);
    int itemsOff = Read<int>(itemsProp, profile::PropOffset);
    Array items = Read<Array>((char*)asc + contOff + itemsOff);
    if (!items.Data || IsBadPtr(items.Data) || items.Count < 0 || items.Count > 4096) {
        LOG("[Specs] %s: none", tag); return;
    }
    LOG("[Specs] %s: %d specs", tag, items.Count);
    if (!layout.ok()) return;
    // [G15-g] Stride resolution, priority order:
    //   1. The engine's own array ElementSize (Items→Inner FStructProperty) —
    //      sizeof(FGameplayAbilitySpec) = 0xE8 = 232 on this build. Device
    //      proof: the valid #29 spec sits at byte 3248 = 14 × 232 exactly,
    //      while every 112-strided slot between #1 and #28 read FastArray
    //      garbage ("_Med" UTF-16 fragments, stale pointers) — the array was
    //      NEVER corrupt, the stride was.
    //   2..N. Plausible neighbors, validated by SpecSlotSane. Validation is
    //      RELAXED to "2 of the first 4 slots sane" — a spec may legally
    //      carry a NULL Ability (engine AddDefaulted slack), so demanding 3
    //      consecutive sane slots rejected the true stride too.
    int propsRaw = (layout.Size >= 0x20 && layout.Size <= 0x400) ? layout.Size : 0x70;
    int aligned = (propsRaw + 7) & ~7;
    int engineStride = SpecArrayStride();
    int stride = 0;
    if (items.Count >= 2) {
        int cand[8] = { engineStride, 0xE8, aligned, 0x70, 0x78, 0x80, 0xB0, 0x60 };
        for (int ci = 0; ci < 8 && !stride; ++ci) {
            if (cand[ci] < 0x20 || cand[ci] > 0x400) continue;
            int sane = 0;
            for (int k = 0; k < 4 && k < items.Count; ++k)
                if (SpecSlotSane(items, cand[ci], k, layout)) ++sane;
            if (sane >= 2) stride = cand[ci];
        }
    }
    if (!stride) {
        stride = engineStride > 0 ? engineStride : aligned;
        LOG("[Specs] ⚠️ stride auto-validation failed — using %s=%d, raw slot bytes follow:",
            engineStride > 0 ? "engine ElementSize" : "align8(props)", stride);
        for (int s = 0; s < 2 && s < items.Count; ++s) {   // ground truth for the next run
            uint8_t* p = (uint8_t*)items.Data + (size_t)s * (size_t)stride;
            if (!IsReadable(p, 0x40)) continue;
            char hex[3 * 0x40 + 1]; int o = 0;
            for (int b = 0; b < 0x40; ++b) o += snprintf(hex + o, sizeof(hex) - o, "%02X ", p[b]);
            LOG("[Specs]  slot%d @%p: %s", s, p, hex);
        }
    }
    LOG("[Specs] stride=%d (engine ElementSize=%d, props=%d) data=%p", stride, engineStride, propsRaw, items.Data);
    int cap = items.Count > 128 ? 128 : items.Count;
    for (int i = 0; i < cap; ++i) {
        uint8_t* spec = (uint8_t*)items.Data + (size_t)i * (size_t)stride;
        if (!IsReadable(spec, (size_t)stride)) { LOG("[Specs]  #%d <unreadable spec — stop>", i); break; }
        int32 inputID = Read<int32>(spec, layout.OffInputID);
        int32 level = layout.OffLevel >= 0 ? Read<int32>(spec, layout.OffLevel) : 1;
        int32 handle = layout.OffHandle >= 0 ? Read<int32>(spec, layout.OffHandle) : 0;
        void* ability = Read<void*>(spec, layout.OffAbility);
        char name[64];
        name[0] = 0;
        // [G14-b] Hard pre-validation: alignment + readability + vtable inside
        // the module. Only then is the (still guarded) name fetch allowed.
        if (ability && ((uintptr_t)ability & 7) == 0 && IsReadable(ability, 0x28)
            && PtrInUnrealModule(Read<void*>(ability, 0))) {
            if (!RunGuarded("Specs", SpecNameFetch, ability, name)) {
                LOG("[Specs]  #%d <name fetch faulted — stop>", i);
                break;
            }
        } else {
            strncpy(name, "<bad-ability>", sizeof(name) - 1);
            name[sizeof(name) - 1] = 0;
        }
        LOG("[Specs]  #%d %s inputID=%d level=%d handle=0x%X", i,
            name[0] ? name : "<no-name>", inputID, level, (unsigned)handle);
    }
    if (items.Count > cap) LOG("[Specs]  ... %d more (capped)", items.Count - cap);
}
} // namespace gas

// [G03] ACTIVATION helper (the old "GrantAbility"): TryActivateAbilityByClass
// is the only reflected activation entry point in 21.30 — granting itself
// now flows exclusively through the ability sets above. Never call this at
// grant time (the removed F13 fallback used to fire abilities on spawn).
void Athena::ActivateAbilityByClass(void* klass) {
    if (!klass || !session.pawn) return;
    void* asc = GetASC(session.pawn);
    if (!asc) { LOG("[Ability] no ASC"); return; }
    Call c("/Script/GameplayAbilities.AbilitySystemComponent.TryActivateAbilityByClass");
    if (!c.ok()) return;
    bool bAllow = true;
    if (c.set("InAbilityToActivate", klass) && c.set("bAllowRemoteActivation", bAllow)) {
        c.run(asc);
        LOG("[Ability] Activated: %s", ObjName(klass).c_str());
    }
}

void Athena::GrantAbility(void* klass) { ActivateAbilityByClass(klass); }

// FFortAbilitySetHandle (0x38): TWeakObjectPtr + GrantedAbilityHandles array
// + AppliedEffectHandles array + ItemGuidsForAdditionalItems array. The
// Android wrapper leaves the two handle arrays allocated for us — free them
// so the GAS bookkeeping does not leak (Era core behaviour).
struct FortAbilitySetHandle {
    void*   TargetAbilitySystemComponent;
    Array   GrantedAbilityHandles;
    Array   AppliedEffectHandles;
    Array   ItemGuidsForAdditionalItems;
};
static_assert(sizeof(FortAbilitySetHandle) == 0x38, "FFortAbilitySetHandle 21.30");

static bool ApplyAbilitySet(void* pawn, void* abilitySet) {
    if (!pawn || !abilitySet || IsBadPtr(abilitySet)) return false;
    void* klib = FindObj("/Script/FortniteGame.Default__FortKismetLibrary");
    if (!klib) { LOG("[Abilities] KismetLibrary missing"); return false; }
    Call c("/Script/FortniteGame.FortKismetLibrary.EquipFortAbilitySet");
    if (!c.ok()) { LOG("[Abilities] EquipFortAbilitySet binding FAILED"); c.dump("EquipFortAbilitySet"); return false; }
    void* ifaceType = FindObj("/Script/GameplayAbilities.AbilitySystemInterface");
    if (!ifaceType) { LOG("[Abilities] AbilitySystemInterface class missing"); return false; }
    void* ifaceAddr = ResolveInterface(pawn, ifaceType);
    if (!ifaceAddr) {
        void* state = GetObjField(pawn, "/Script/Engine.Controller.PlayerState");
        if (!state) {
            void* pc = GetObjField(pawn, "/Script/Engine.Pawn.Controller");
            if (pc) state = GetObjField(pc, "/Script/Engine.Controller.PlayerState");
        }
        if (state) {
            ifaceAddr = ResolveInterface(state, ifaceType);
            LOG("[Abilities] Resolved on PlayerState=%p -> %p", state, ifaceAddr);
        }
    }
    LOG("[Abilities] ResolveInterface(pawn=%p) -> iface=%p", pawn, ifaceAddr);
    TScriptInterface iface{pawn, ifaceAddr};
    bool s1 = c.set("AbilitySystemInterfaceActor", iface);
    bool s2 = c.set("AbilitySet", abilitySet);
    bool s3 = c.set("OverrideSourceObject", pawn);
    if (!s1 || !s2 || !s3) { LOG("[Abilities] param bind failed"); c.dump("EquipFortAbilitySet"); return false; }
    if (!c.run(klib)) return false;
    // Free returned handle arrays (Era core: the wrapper destroys them anyway,
    // freeing keeps the GAS bookkeeping consistent).
    FortAbilitySetHandle handle = c.result<FortAbilitySetHandle>();
    if (handle.GrantedAbilityHandles.Data) Fn<void(*)(void*)>(profile::Free)(handle.GrantedAbilityHandles.Data);
    if (handle.AppliedEffectHandles.Data) Fn<void(*)(void*)>(profile::Free)(handle.AppliedEffectHandles.Data);
    LOG("[Abilities] ✅ Applied set: %s (abilities=%d effects=%d)", ObjName(abilitySet).c_str(),
        handle.GrantedAbilityHandles.Count, handle.AppliedEffectHandles.Count);
    return true;
}

// ============================================================================
// [G14-e/G15-e] LATE-STREAMING ABILITY SETS — AS_HillScramble/AS_SlideImpulse
// were not resident at grant time on the device (they load with the terrain's
// streaming levels). The queue below retries them from Tick until the asset
// lands. [G15-e] NEVER GIVES UP: the old 22-try window (~3 min) expired
// before distant POI sublevels streamed in ("giving up on AS_HillScramble"
// in the device log = no climbing, ever). StaticLoadObject is attempted only
// 10 times per set (each miss can walk the pak index — measurable on a
// phone); after that the cheap loaded-object scan carries the retry at a
// relaxed 20s cadence, forever.
// ============================================================================
struct PendingAbilitySet { const char* path; const char* altPath; const char* shortName; int tries; };
static std::vector<PendingAbilitySet> s_pendingSets;
static void QueuePendingAbilitySet(const char* path, const char* shortName) {
    for (const auto& p : s_pendingSets)
        if (strcmp(p.shortName, shortName) == 0) return;   // already queued
    // [G16-d] Plugin-mount variants — Erbium's TacticalSprint needed TWO
    // ("/TacticalSprintGame/" vs "/TacticalSprint/"); the same drift exists
    // for the other content plugins on mobile paks.
    const char* alt = nullptr;
    std::string s(shortName);
    if (s.find("HillScramble") != std::string::npos) alt = "/HillScrambleGame/Gameplay/AS_HillScramble.AS_HillScramble";
    else if (s.find("SlideImpulse") != std::string::npos) alt = "/SlideImpulseGame/Gameplay/AS_SlideImpulse.AS_SlideImpulse";
    s_pendingSets.push_back({ path, alt, shortName, 0 });
}
// [G15-e] Partial-name scan: the set may be resident under a slightly
// different object name on mobile (suffixes/renames) — match by substring.
static void* FindLoadedAbilitySetLike(const std::string& wanted) {
    GObjects_t* go = GObjectArray();
    void* type = FindObj("/Script/FortniteGame.FortAbilitySet");
    if (!go || !type) return nullptr;
    std::string needle = wanted;
    for (auto& ch : needle) ch = (char)::tolower((unsigned char)ch);
    for (int32 i = 0; i < go->NumElements; ++i) {
        void* object = go->GetById((uint32)i);
        if (!object || IsBadPtr(object)) continue;
        if (!IsA(object, type)) continue;
        std::string n = ObjName(object);
        for (auto& ch : n) ch = (char)::tolower((unsigned char)ch);
        if (n.find(needle) != std::string::npos) return object;
    }
    return nullptr;
}
static void PumpPendingAbilitySets() {
    if (s_pendingSets.empty() || !session.pawn) return;
    static double s_lastPump = 0;
    double now = Monotonic();
    if (now - s_lastPump < 20.0) return;   // [G15-e] relaxed cadence — the retry is forever now
    s_lastPump = now;
    for (size_t i = 0; i < s_pendingSets.size(); ) {
        PendingAbilitySet& p = s_pendingSets[i];
        void* set = (p.tries < 10) ? LoadAsset(p.path, "/Script/FortniteGame.FortAbilitySet") : nullptr;
        if (!set && p.altPath && p.tries < 10) set = LoadAsset(p.altPath, "/Script/FortniteGame.FortAbilitySet");   // [G16-d] mount variant
        if (!set) set = FindLoadedObjectOfType("/Script/FortniteGame.FortAbilitySet", p.shortName);
        if (!set) set = FindLoadedAbilitySetLike(p.shortName);   // [G15-e] substring fallback
        if (set && !IsBadPtr(set)) {
            bool already = std::find(session.granted_ability_sets.begin(),
                                     session.granted_ability_sets.end(), set)
                           != session.granted_ability_sets.end();
            if (!already && ApplyAbilitySet(session.pawn, set)) {
                session.granted_ability_sets.push_back(set);
                LOG("[Abilities] ✅ Applied late set: %s (tries=%d)", p.shortName, p.tries);
            }
            s_pendingSets.erase(s_pendingSets.begin() + i);
            continue;
        }
        ++p.tries;   // [G15-e] no give-up: POI sublevels stream in over many minutes
        ++i;
    }
}

void Athena::InitStaminaAttributes() {
    if (!session.pawn) return;
    void* asc = GetASC(session.pawn);
    if (!asc) { LOG("[Stamina] no ASC"); return; }
    void* attrClass = FindObj("/Script/FortniteGame.FortPlayerAttributeSet");
    if (!attrClass) attrClass = FindObj("/Script/GameplayAbilities.AttributeSet");
    if (!attrClass) { LOG("[Stamina] AttributeSet missing"); return; }
    void* dataTable = FindObj("/Game/Athena/Items/Quests/AthenaObjectiveStatXPTable.AthenaObjectiveStatXPTable");
    Call c("/Script/GameplayAbilities.AbilitySystemComponent.K2_InitStats");
    if (!c.ok()) { LOG("[Stamina] K2_InitStats binding FAILED"); return; }
    if (!c.set("Attributes", attrClass)) { LOG("[Stamina] Attributes bind failed"); return; }
    if (dataTable) c.set("DataTable", dataTable);
    if (!c.run(asc)) return;
    session.stamina_initialized = true;
    LOG("[Stamina] Initialized FortPlayerAttributeSet");
}

void Athena::GrantAllAbilitySets() {
    if (!session.pawn || session.abilities_granted) return;
    LOG("[Abilities] Setting up...");
    void* asc = GetASC(session.pawn);
    if (!asc) { LOG("[Abilities] no ASC"); return; }
    if (!session.stamina_initialized) InitStaminaAttributes();

    // [G02/G11] PRIMARY — the native Erbium path, now FAULT-GUARDED: the
    // device run crashed inside this native call (the last log line was the
    // stamina init). A fault no longer kills the process — it logs
    // signal+PC+address, disables the native path for the session, and the
    // proven EquipFortAbilitySet reflection path below takes over.
    // [G14-d] Anti-double-grant: if the native actually delivered the CDO
    // DefaultAbilitySets (GAS_AthenaPlayer & co.), re-equipping the core set
    // through EquipFortAbilitySet would duplicate its 37 abilities.
    int specsPreNative = gas::CountActivatableSpecs(asc);
    bool nativeGranted = gas::NativeInitializePlayerGameplayAbilities();
    int specsPostNative = gas::CountActivatableSpecs(asc);
    bool skipCoreSet = nativeGranted && (specsPostNative - specsPreNative) >= 20;
    if (skipCoreSet)
        LOG("[Abilities] native granted %d specs — skipping GAS_AthenaPlayer re-grant (anti-duplicate)",
            specsPostNative - specsPreNative);

    void* coreSet = LoadAsset(
        "/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GAS_AthenaPlayer.GAS_AthenaPlayer",
        "/Script/FortniteGame.FortAbilitySet");
    if (!coreSet && !nativeGranted) {
        LOG("[Abilities] GAS_AthenaPlayer missing — will retry (asset may stream late)");
        return;   // [F90-a/AUD-1] no premature commit: the 10s Tick retry re-invokes until the set lands
    }

    // [G03] The FULL Erbium season-21 set family (AFortGameMode::AbilitySets
    // in Erbium for Fortnite >= 20): movement/door/slide/ascender/reality sets
    // were missing — exactly why slide/door-bash/climb anims stayed dead with
    // only GAS_AthenaPlayer granted.
    struct SetEntry { const char* path; const char* shortName; };
    static const SetEntry erbiumSets[] = {
        {"/TacticalSprintGame/Gameplay/AS_TacticalSprint.AS_TacticalSprint",                    "AS_TacticalSprint"},
        {"/Ascender/Gameplay/Ascender/AS_Ascender.AS_Ascender",                                  "AS_Ascender"},
        {"/DoorBashContent/Gameplay/AS_DoorBash.AS_DoorBash",                                    "AS_DoorBash"},
        {"/HillScramble/Gameplay/AS_HillScramble.AS_HillScramble",                               "AS_HillScramble"},
        {"/SlideImpulse/Gameplay/AS_SlideImpulse.AS_SlideImpulse",                               "AS_SlideImpulse"},
        {"/RealitySeedGameplay/Environment/Foliage/GAS_Athena_RealitySapling.GAS_Athena_RealitySapling", "GAS_Athena_RealitySapling"},
        {nullptr, nullptr}
    };

    // Add the mobile interaction/harvest/sprint abilities into the core set
    // before EquipFortAbilitySet so the native delivery path initializes their
    // replicated specs (Era core behaviour; offsets verified: GameplayAbilities
    // @0x38 on UFortAbilitySet).
    void* abilities_prop = GetProp("/Script/FortniteGame.FortAbilitySet.GameplayAbilities");
    if (abilities_prop && coreSet) {
        int off = Read<int>(abilities_prop, profile::PropOffset);
        LOG("[Abilities] GameplayAbilities offset = 0x%X (expect 0x38)", off);
        Array* abilities = reinterpret_cast<Array*>(GetField(coreSet, abilities_prop));
        if (abilities && abilities->Count >= 0 && abilities->Count < 256) {
            const char* extra[] = {
                "/Script/FortniteGame.FortGameplayAbility_Sprint",
                "/Script/FortniteGame.FortGameplayAbility_Jump",
                "/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GA_DefaultPlayer_InteractSearch.GA_DefaultPlayer_InteractSearch_C",
                "/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GA_DefaultPlayer_InteractUse.GA_DefaultPlayer_InteractUse_C",
                "/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GA_DefaultPlayer_Harvest.GA_DefaultPlayer_Harvest_C",
                nullptr
            };
            for (int i = 0; extra[i]; ++i) {
                void* klass = LoadClass(extra[i], "/Script/GameplayAbilities.GameplayAbility");
                if (!klass) continue;
                bool present = false;
                for (int j = 0; j < abilities->Count; ++j)
                    if (Read<void*>(abilities->Data, j * 8) == klass) { present = true; break; }
                if (!present) {
                    Fn<int(*)(void*,void*,const void*)>(profile::GenericArrayAdd)(abilities, abilities_prop, &klass);
                    LOG("[Abilities] ✅ Added: %s", extra[i]);
                }
            }
            LOG("[Abilities] Total abilities in set: %d", abilities->Count);
        }
    }

    bool coreApplied = false;
    if (coreSet && !skipCoreSet && ApplyAbilitySet(session.pawn, coreSet)) {
        session.granted_ability_sets.push_back(coreSet);
        coreApplied = true;
    }

    // [G03] Erbium movement/interaction set family — each one equipped through
    // the same native delivery (LoadAsset first, loaded-object fallback second:
    // these stream in late like every other BP asset).
    int extraSets = 0;
    // [G16-f] The Erbium movement family is a season-20+ addition (Erbium:
    // VersionInfo.FortniteVersion >= 20) — the 10.40 profile grants
    // GAS_AthenaPlayer only, exactly like the user's own game.cpp.
    const int setLimit = (profile::FortniteVersion >= 20) ? 64 : 0;
    for (int i = 0; i < setLimit && erbiumSets[i].path; ++i) {
        void* set = LoadAsset(erbiumSets[i].path, "/Script/FortniteGame.FortAbilitySet");
        if (!set) set = FindLoadedObjectOfType("/Script/FortniteGame.FortAbilitySet", erbiumSets[i].shortName);
        if (!set || IsBadPtr(set)) {
            // [G14-e] These stream in late (HillScramble/SlideImpulse on the
            // device run) — queue for the Tick pump instead of dropping.
            LOG("[Abilities] set not resident yet: %s (queued for late retry)", erbiumSets[i].shortName);
            QueuePendingAbilitySet(erbiumSets[i].path, erbiumSets[i].shortName);
            continue;
        }
        if (std::find(session.granted_ability_sets.begin(), session.granted_ability_sets.end(), set) != session.granted_ability_sets.end())
            continue;   // already equipped (retry path)
        if (ApplyAbilitySet(session.pawn, set)) {
            session.granted_ability_sets.push_back(set);
            ++extraSets;
        }
    }

    // [G03/G04] Commit semantics: "granted" is defined by the ASC's own
    // ActivatableAbilities when the reflection read works (specs must exist
    // AND something must have been delivered this pass — the anti-duplicate
    // guard for the 10s Tick retry); when the spec read is unavailable the
    // delivery results alone decide. The spec list (with InputIDs) is logged
    // for verification either way.
    int specsAfter = gas::CountActivatableSpecs(asc);
    bool delivered = nativeGranted || coreApplied || extraSets > 0;
    if (specsAfter >= 0) {
        if (delivered && specsAfter > 0) {
            session.abilities_granted = true;
            session.melee_granted = true; // [F13] melee flows with the pickaxe weapon intrinsics
            gas::DumpSpecs("[Abilities] post-grant");   // [G04] ability + InputID visibility
        }
        // specs == 0 with a delivery attempt → nothing landed: keep retrying
        // (a failed EquipFortAbilitySet does not duplicate anything).
    } else if (delivered) {
        session.abilities_granted = true;
        session.melee_granted = true;
        LOG("[Abilities] spec reflection unavailable — committed on delivery results");
    }
    LOG("[Abilities] Done — sets=%zu native=%d specs=%d", session.granted_ability_sets.size(), (int)nativeGranted, specsAfter);
}

void Athena::GrantDefaultAbilities() { GrantAllAbilitySets(); }

void Athena::RefreshHUD() {
    if (!session.controller) return;
    void* asc = GetASC(session.pawn);
    if (asc) {
        Call c("/Script/GameplayAbilities.AbilitySystemComponent.OnRep_ActivateAbilities");
        if (c.ok()) c.run(asc);
    }
}

// ============================================================================
// QUICKBARS  — [F01/F02/F19] fixed layout + two-bar binding with read-back
// ============================================================================
void Athena::InitializeQuickBarsNative() {
    if (!session.quickbars || IsBadPtr(session.quickbars)) { LOG("[QB] No quickbar"); return; }
    const uint32_t expected[] = {0xa9bd57f6, 0xa9014ff4, 0xa9027bfd, 0x910083fd};
    if (profile::InitializeAthenaQuickBars == 0) return;   // [G16-f] not on this build
    if (memcmp(reinterpret_cast<void*>(g_base + profile::InitializeAthenaQuickBars), expected, sizeof(expected)) != 0)
        LOG("[QB] ⚠️ prologue mismatch at 0x%llX", (unsigned long long)(g_base + profile::InitializeAthenaQuickBars));
    void* vtable = Read<void*>(session.quickbars);
    if (!vtable || IsBadPtr(vtable)) { LOG("[QB] vtable invalid"); return; }
    uintptr_t init_slot = Read<uintptr_t>(vtable, profile::QuickBars_VtableSlot);
    uintptr_t expected_fn = g_base + profile::InitializeAthenaQuickBars;
    LOG("[QB] vtable=%p slot+0x%X=0x%llX expected=0x%llX", vtable, profile::QuickBars_VtableSlot, (unsigned long long)init_slot, (unsigned long long)expected_fn);
    if (init_slot != expected_fn) { LOG("[QB] ❌ vtable mismatch — SKIPPING native call"); return; }
    LOG("[QB] ✅ vtable match — calling native init");
    // [F83-c] runtime cross-check of the F81-verified layout (0x278/0x308 —
    // binary-verified via disassembly) against live reflection. A drift here
    // means every read below is a false 0/0 and the builder would write the
    // wrong bars; the log settles it in one device run.
    {
        void* pProp = GetProp("/Script/FortniteGame.FortQuickBars.PrimaryQuickBar");
        void* sProp = GetProp("/Script/FortniteGame.FortQuickBars.SecondaryQuickBar");
        if (pProp) { int ro = Read<int>(pProp, profile::PropOffset);
            if (ro != (int)profile::QuickBars_PrimaryOffset)
                LOG("[QB] ⚠️ PrimaryQuickBar offset drift: reflect=0x%X const=0x%X", ro, (int)profile::QuickBars_PrimaryOffset); }
        if (sProp) { int ro = Read<int>(sProp, profile::PropOffset);
            if (ro != (int)profile::QuickBars_SecondaryOffset)
                LOG("[QB] ⚠️ SecondaryQuickBar offset drift: reflect=0x%X const=0x%X", ro, (int)profile::QuickBars_SecondaryOffset); }
    }
    void* primary = (char*)session.quickbars + profile::QuickBars_PrimaryOffset;
    void* secondary = (char*)session.quickbars + profile::QuickBars_SecondaryOffset;
    Array p_before = Read<Array>(primary, profile::QuickBar_SlotsOffset);
    Array s_before = Read<Array>(secondary, profile::QuickBar_SlotsOffset);
    LOG("[QB] Before: Primary=%d Secondary=%d", p_before.Count, s_before.Count);
    // [F43] Native init is called with (this, controller) — the 21.30 entry
    // reads the owning controller as its argument (harmless when the param
    // list is empty; without it the call early-outs and the bars stay empty).
    { Guard g; reinterpret_cast<void(*)(void*, void*)>(init_slot)(session.quickbars, session.controller); }
    Array p_after = Read<Array>(primary, profile::QuickBar_SlotsOffset);
    Array s_after = Read<Array>(secondary, profile::QuickBar_SlotsOffset);
    LOG("[QB] After: Primary=%d Secondary=%d", p_after.Count, s_after.Count);
    if (p_after.Count >= 1 && s_after.Count >= 1) { LOG("[QB] ✅ Init succeeded"); session.quickbars_initialized = true; }
    else {
        LOG("[QB] ⚠️ Init may have failed");
        // [F92] disasm 0x56607ec: native init bails SILENTLY when ctx
        // ([this+0x138]) is null OR fails both IsA casts (0x7071ee4 /
        // 0x70975fc probes; sources owner+0x3DA0 | owner+0x31A0). The spawn
        // code already tried ctx=controller. Diagnostic-only retry with
        // ctx=PlayerState — never sets the flag (the manual builder remains
        // the item provider); this just names the real owner for the log.
        void* psCtx = GetObjField(session.controller, "/Script/Engine.Controller.PlayerState");
        if (psCtx && !IsBadPtr(psCtx)) {
            Write<void*>((char*)session.quickbars + profile::QuickBars_CtxOwner, 0, psCtx);
            { Guard g; reinterpret_cast<void(*)(void*, void*)>(init_slot)(session.quickbars, session.controller); }
            Array p2 = Read<Array>(primary, profile::QuickBar_SlotsOffset);
            Array s2 = Read<Array>(secondary, profile::QuickBar_SlotsOffset);
            LOG("[QB] ctx=PlayerState retry: P=%d S=%d%s", p2.Count, s2.Count,
                (p2.Count >= 1 && s2.Count >= 1) ? " — native owner IS the PlayerState" : "");
            Write<void*>((char*)session.quickbars + profile::QuickBars_CtxOwner, 0, session.controller);
        } else LOG("[QB] [F92] no PlayerState for ctx retry");
    }
}

// [F01] Manual QuickBar construction — the enable byte is at slot+0x10
// (bEnabled bit0 | bIsDirty bit1 | bIsReserved bit2 | bIsOccupied bit3);
// the old code wrote slot+0x18 which is UsedByItemGuid.
void Athena::BuildQuickBarsManually() {
    if (!session.quickbars) { LOG("[QB] No quickbars"); return; }
    // [F44/F83-a] Dedupe: if slot 0 of the primary bar already holds a GUID
    // we are already filled — re-running would leak arrays, re-grow stale
    // buffers and duplicate entries (the Tick retry path calls us repeatedly).
    {
        void* barChk = GetProp("/Script/FortniteGame.FortQuickBars.PrimaryQuickBar");
        if (barChk) {
            void* bar = GetField(session.quickbars, barChk);
            if (bar && !IsBadPtr(bar)) {
                Array arr = Read<Array>((char*)bar + profile::QuickBar_SlotsOffset);
                if (arr.Count > 0 && arr.Data && !IsBadPtr(arr.Data)) {
                    Guid g0 = Read<Guid>((char*)arr.Data + 0x18, 0);   // UsedByItemGuid
                    if (g0.A | g0.B | g0.C | g0.D) {
                        LOG("[QB] already filled (slot0 guid) — skipping rebuild");
                        session.quickbars_initialized = true;
                        return;
                    }
                }
            }
        }
    }
    LOG("[QB] Building QuickBars manually...");

    struct SlotEntry { int idx; Guid guid; uint8_t bar; };
    std::vector<SlotEntry> entries;

    void* invProp = GetProp("/Script/FortniteGame.FortInventory.Inventory");
    void* instProp = GetProp("/Script/FortniteGame.FortItemList.ItemInstances");
    if (!invProp || !instProp) return;

    auto collect = [&](std::array<void*,6>& src, uint8_t bar) {
        for (int i = 0; i < 6; ++i) {
            void* inst = src[i];
            if (!inst) continue;
            Call g("/Script/FortniteGame.FortItem.GetItemGuid");
            if (g.ok() && g.run(inst)) entries.push_back({i, g.result<Guid>(), bar});
        }
    };
    collect(session.slots, 0);
    collect(session.secondary_slots, 1);

    LOG("[QB] Collected %zu item GUIDs", entries.size());

    void* barProp = GetProp("/Script/FortniteGame.FortQuickBars.PrimaryQuickBar");
    void* secProp = GetProp("/Script/FortniteGame.FortQuickBars.SecondaryQuickBar");
    void* slotsProp = GetProp("/Script/FortniteGame.QuickBar.Slots");   // [F02] struct name is QuickBar
    if (!barProp || !secProp || !slotsProp) { LOG("[QB] QuickBar reflection missing"); return; }

    // [F81] Grow slot arrays through the ENGINE'S OWN quickbar resize helper
    // (0x56925E0, verified in the binary: ResizeGrow + per-slot init with
    // flags=(old&0xF0)|0x03 and prev=-1). The old raw Realloc+memset left
    // flags=0 and prev=0 — state the native writer never produces.
    auto ensure_slots = [&](void* bar, int want, uint8_t barId) -> Array {
        void* slotsField = (char*)bar + profile::QuickBar_SlotsOffset;
        Array arr = Read<Array>(slotsField);
        // [F83-a] never trust a stale/garbage header — a wild Data pointer
        // must not reach the engine helpers (Resize/Realloc on a freed or
        // wild pointer corrupts the heap; teardown on build exit suspect).
        if (arr.Count < 0 || arr.Count > 64 || (arr.Count > 0 && IsBadPtr(arr.Data))) {
            LOG("[QB] ⚠️ bar%u header sanitized (Count=%d Data=%p)", barId, arr.Count, arr.Data);
            arr = Array{nullptr, 0, 0};
        }
        if (arr.Count < want) {
            LOG("[QB] grow bar%u: Data=%p Count=%d Max=%d want=%d", barId, arr.Data, arr.Count, arr.Max, want);
            if (profile::QuickBarResize) { Guard g; Fn<void(*)(void*, int32, int32)>(profile::QuickBarResize)(slotsField, want, 1); }   // [G16-f] guarded
            arr = Read<Array>(slotsField);
            LOG("[QB] grow bar%u readback: Data=%p Count=%d Max=%d", barId, arr.Data, arr.Count, arr.Max);
            if (arr.Count < want || !arr.Data) {
                // [F36] fallback: UE-allocator growth (FMemory::Realloc 3-arg).
                LOG("[QB] ⚠️ native grow short — manual fallback bar%u", barId);
                int oldBytes = arr.Count > 0 ? profile::QuickBarSlot_SlotSize * arr.Count : 0;
                int newBytes = profile::QuickBarSlot_SlotSize * want;
                if (!profile::Realloc) return arr;   // [G16-f] guarded — cannot grow on this profile
                void* newData = Fn<void*(*)(void*, size_t, uint32_t)>(profile::Realloc)(
                    arr.Count > 0 ? arr.Data : nullptr, (size_t)newBytes, 0u);
                if (!newData) { LOG("[QB] ⚠️ slot growth failed (want=%d)", want); return arr; }
                // [F83-a] CRASH FIX (fallback path): the old `oldBytes > 0`
                // guard skipped this memset exactly when Count==0 — the
                // fresh Realloc'd chunk (recycled from a previous match's
                // teardown) went in UNINITIALIZED and the entries loop read
                // garbage Items headers from it (wild memcmp/GenericArrayAdd
                // → the Bar1-slot1 crash). Zero beyond preserved contents,
                // unconditionally.
                if (newBytes > oldBytes)
                    memset((char*)newData + oldBytes, 0, (size_t)(newBytes - oldBytes));
                Write<void*>(slotsField, 0, newData);
                Write<int32>(slotsField, 8, want);
                Write<int32>(slotsField, 12, want);
                arr = Read<Array>(slotsField);
                LOG("[QB] manual grow bar%u readback: Data=%p Count=%d", barId, arr.Data, arr.Count);
            }
        }
        return arr;
    };

    Array primArr = ensure_slots(GetField(session.quickbars, barProp), 6, 0);
    Array secArr  = ensure_slots(GetField(session.quickbars, secProp), 5, 1);
    // [F43] all 21.30 quickbars: Primary(6) + Secondary(5) + Creative(6).
    {
        void* creatProp = GetProp("/Script/FortniteGame.FortQuickBars.CreativeQuickBar");
        if (creatProp) {
            Array c = ensure_slots(GetField(session.quickbars, creatProp), 6, 2);
            LOG("[QB] Creative bar slots=%d (Data=%p)", c.Count, c.Data);
        }
    }

    // [F81] every path is now logged with readbacks — if anything still dies,
    // the LAST "[QB] bind" line names the exact bar/slot/step.
    for (auto& e : entries) {
        Array& arr = e.bar == 0 ? primArr : secArr;
        if (e.idx < 0 || e.idx >= arr.Count || !arr.Data) {
            LOG("[QB] SKIP bar%u slot%d (Count=%d Data=%p)", e.bar, e.idx, arr.Count, arr.Data);
            continue;
        }
        void* slot = (char*)arr.Data + (size_t)e.idx * profile::QuickBarSlot_SlotSize;
        // Items array (TArray<FGuid>) at slot+0x00 — grown through the UE
        // allocator via GenericArrayAdd so native code can keep mutating it.
        void* itemsField = (char*)slot + profile::QuickBarSlot_ItemsOffset;
        Array values = Read<Array>(itemsField);
        LOG("[QB] bind bar%u slot%d: slot=%p items=%p count=%d max=%d",
            e.bar, e.idx, slot, values.Data, values.Count, values.Max);
        // [F83-a] harden: a garbage Items header must read as empty — never
        // as a wild Data pointer for the memcmp scan below.
        if (values.Count < 0 || values.Count > 16 || (values.Count > 0 && IsBadPtr(values.Data)))
            values = Array{nullptr, 0, 0};
        bool present = false;
        for (int i = 0; i < values.Count; ++i)
            if (!memcmp((char*)values.Data + (size_t)i * sizeof(Guid), &e.guid, sizeof(Guid))) { present = true; break; }
        if (!present) {
            auto add = Fn<int(*)(void*, void*, const void*)>(profile::GenericArrayAdd);
            int rc = add(itemsField, GetProp("/Script/FortniteGame.QuickBarSlot.Items"), &e.guid);
            Array after = Read<Array>(itemsField);
            LOG("[QB] add rc=%d → items=%p count=%d", rc, after.Data, after.Count);
            if (after.Count < 1 || IsBadPtr(after.Data)) { LOG("[QB] ⚠️ add failed bar%u slot%d — skip", e.bar, e.idx); continue; }
        }
        // [F81] native-writer parity: prev slot index = -1, flags keep the
        // high nibble and set Enabled|Dirty|Occupied (native init writes 0x03;
        // occupied marks the slot as holding an item for the HUD).
        Write<int32>(slot, 0x14, -1);
        uint8_t* flags = (uint8_t*)((char*)slot + profile::QuickBarSlot_EnabledOffset);
        *flags = (uint8_t)((*flags & 0xF0) | profile::QuickBarSlot_EnabledBit |
                           profile::QuickBarSlot_OccupiedBit | 0x02);
        LOG("[QB] Bar%u slot %d -> GUID %08X:%08X ✅", e.bar, e.idx, e.guid.A, e.guid.B);
    }

    // [F43] Push the refreshed bars into the UI layer (Erbium calls the
    // OnRep_* pair after manual slot writes; both degrade silently when the
    // UFunctions are absent on this build).
    {
        LOG("[QB] OnRep Primary...");
        Call r1("/Script/FortniteGame.FortQuickBars.OnRep_PrimaryQuickBar");
        if (r1.ok()) r1.run(session.quickbars);
        LOG("[QB] OnRep Secondary...");
        Call r2("/Script/FortniteGame.FortQuickBars.OnRep_SecondaryQuickBar");
        if (r2.ok()) r2.run(session.quickbars);
    }
    LOG("[QB] ClientForceUpdateQuickbar...");
    ForceUpdateQuickBars();
    session.quickbars_initialized = true;
    LOG("[QB] ✅ Manual initialization done");
}

void Athena::SetQuickBarReady() {
    if (!session.controller) return;
    // [F92] the readiness byte is a disasm-verified RAW offset (PC+0x1A68):
    // native GetItemInQuickbarSlot (0x6673508) AND GetQuickBarSlotCount
    // (0x66734cc) both cbz on it as their second gate — 0 means the HUD
    // reads NOTHING. Reflection is informational only; never gate the
    // write on it (the old 3 skip-guards could leave it 0 forever).
    void* qp = GetProp("/Script/FortniteGame.FortPlayerController.ClientQuickBars");
    int propOff = qp ? Read<int>(qp, profile::PropOffset) : -1;
    LOG("[QB] ClientQuickBars reflect offset=0x%X (const 0x%X)", propOff, (int)profile::Controller_ClientQuickBars);
    // HUD check #1 — the raw pointer link (must be OUR quickbars object)
    void* cur = Read<void*>(session.controller, profile::Controller_ClientQuickBars);
    if (cur != session.quickbars) {
        LOG("[QB] ⚠️ PC->ClientQuickBars=%p != quickbars=%p — writing raw link", cur, session.quickbars);
        Write<void*>(session.controller, profile::Controller_ClientQuickBars, session.quickbars);
    }
    if (!session.quickbars_initialized) { LOG("[QB] ❌ QuickBars not initialized — readiness deferred"); return; }
    uint8_t before = Read<uint8_t>(session.controller, profile::Controller_QuickBarReadyByte);
    Write<uint8_t>(session.controller, profile::Controller_QuickBarReadyByte, 1);
    LOG("[QB] ✅ readiness byte: %u -> 1 (raw write — HUD read path is now live)", before);
}

// [F92] Replay the EXACT native HUD read chain (GetItemInQuickbarSlot
// 0x6673508 + GetQuickBarSlotCount 0x66734cc, both fully disassembled):
//   1) PC->0x1A60 (ClientQuickBars) non-null       → else HUD reads nothing
//   2) PC->0x1A68 readiness byte non-zero           → else HUD reads nothing
//   3) quickbars+0x278/0x308 bar Count > slot      (bar+0x18)
//   4) quickbars vtable[0x6f0] GetItemInSlot(type, slot)
// If all four pass, the hotbar HUD WILL show our items; if any fails the
// log names the exact broken link. At most once per minute.
void Athena::VerifyHUDReadPath() {
    static double lastRun = -1e9;
    double now = Monotonic();
    if (now - lastRun < 60.0) return;
    lastRun = now;
    if (!session.controller || !session.quickbars || IsBadPtr(session.quickbars)) return;
    void* qbLink = Read<void*>(session.controller, profile::Controller_ClientQuickBars);
    uint8_t gate = Read<uint8_t>(session.controller, profile::Controller_QuickBarReadyByte);
    LOG("[QB] HUD-verify: PC->ClientQuickBars=%p qb=%p (%s) gate=%u", qbLink, session.quickbars,
        (qbLink == session.quickbars ? "MATCH" : "MISMATCH"), (unsigned)gate);
    void* vt = Read<void*>(session.quickbars);
    if (!vt || IsBadPtr(vt)) { LOG("[QB] HUD-verify: vtable invalid"); return; }
    using GIS_t = void* (*)(void*, int, int);
    GIS_t gis = reinterpret_cast<GIS_t>(Read<uintptr_t>(vt, profile::QuickBars_VtableSlot_GetItemInSlot));
    if (!gis || IsBadPtr((void*)gis)) { LOG("[QB] HUD-verify: GetItemInSlot vtable[0x6f0] invalid"); return; }
    const struct { const char* name; unsigned off; } bars[2] = {
        {"Primary", profile::QuickBars_PrimaryOffset}, {"Secondary", profile::QuickBars_SecondaryOffset} };
    for (int b = 0; b < 2; ++b) {
        Array slots = Read<Array>((char*)session.quickbars + bars[b].off, profile::QuickBar_SlotsOffset);
        LOG("[QB] HUD-verify: %s slots=%d", bars[b].name, slots.Count);
        for (int s = 0; s < slots.Count && s < 6; ++s) {
            void* item = nullptr;
            { Guard g; item = gis(session.quickbars, b, s); }
            LOG("[QB] HUD-verify:   %s[%d] -> %p %s", bars[b].name, s, item, item ? "OK" : "NULL");
        }
    }
}

// [G18-b] GetItemInQuickbarSlot read-back, tolerant of the enum's reflected
// size (UE byte-enums marshal as 1; some builds widen to 4). Returns the
// HUD-visible item for a bar/slot or null — THE definitive "can the HUD see
// this slot?" answer (the mobile hotbar reads slots through exactly this).
static void* QBQuerySlot(int bar, int slot) {
    if (!session.controller) return nullptr;
    Call q("/Script/FortniteGame.FortPlayerController.GetItemInQuickbarSlot");
    if (!q.ok()) return nullptr;
    bool ok = q.set("QuickBarType", bar) || q.set<uint8_t>("QuickBarType", (uint8_t)bar);
    ok = ok && (q.set("SlotIndex", slot) || q.set<int32_t>("SlotIndex", slot));
    if (!ok || !q.run(session.controller)) return nullptr;
    void* item = q.result<void*>();
    return (item && !IsBadPtr(item)) ? item : nullptr;
}

// [F19] Bind through the native Kismet helper, then verify by reading the
// slot back. EquipItemToQuickBarSlot(WorldContextObject, ItemDefinition,
// ItemVariantGuid, QuickBarType, SlotIndex) exists in 21.30.
bool Athena::BindSlot(void* item, int slot, uint8_t bar) {
    if (!item || !session.controller || bar > 1 || slot < 0) return false;
    void* definition = nullptr;
    { Call d("/Script/FortniteGame.FortItem.GetItemDefinitionBP"); if (d.ok() && d.run(item)) definition = d.result<void*>(); }
    // [F43] fallback: read the definition straight from the item's entry
    // (FortWorldItem.ItemEntry.ItemDefinition) instead of failing silently.
    if (!definition) {
        void* entryProp = GetProp("/Script/FortniteGame.FortWorldItem.ItemEntry");
        void* defProp   = GetProp("/Script/FortniteGame.FortItemEntry.ItemDefinition");
        if (entryProp && defProp)
            definition = Read<void*>(GetField(GetField(item, entryProp), defProp));
    }
    if (!definition) { LOG("[QB] BindSlot: no definition (slot=%d bar=%u)", slot, bar); return false; }
    Guid guid{};
    { Call g("/Script/FortniteGame.FortItem.GetItemGuid"); if (g.ok() && g.run(item)) guid = g.result<Guid>(); }
    if (!(guid.A | guid.B | guid.C | guid.D)) return false;

    Call bind("/Script/FortniteGame.FortKismetLibrary.EquipItemToQuickBarSlot");
    if (bind.ok() && bind.set("WorldContextObject", session.world) &&
        bind.set("ItemDefinition", definition) && bind.set("ItemVariantGuid", guid) &&
        bind.set("QuickBarType", bar) && bind.set("SlotIndex", slot))
        bind.run(FindObj("/Script/FortniteGame.Default__FortKismetLibrary"));

    // Verify through reflection layout checks before trusting anything.
    // (21.30: FQuickBarSlot stride 0x28, Items is TArray<FGuid> — the
    // definitive verification is the GetItemInQuickbarSlot read-back below.)
    void* barProp = GetProp(bar == 0 ? "/Script/FortniteGame.FortQuickBars.PrimaryQuickBar"
                                     : "/Script/FortniteGame.FortQuickBars.SecondaryQuickBar");
    void* slotsProp = GetProp("/Script/FortniteGame.QuickBar.Slots");       // [F02]
    void* itemsProp = GetProp("/Script/FortniteGame.QuickBarSlot.Items");   // [F02]
    if (!barProp || !slotsProp || !itemsProp) { LOG("[QB] BindSlot: reflection missing"); return false; }
    void* quickbar = GetObjField(session.controller, "/Script/FortniteGame.FortPlayerController.ClientQuickBars");
    if (!quickbar) { LOG("[QB] BindSlot: ClientQuickBars missing"); return false; }
    void* barField = GetField(quickbar, barProp);
    Array slots = Read<Array>(GetField(barField, slotsProp));
    if (!slots.Data || slots.Count < 1 || slot >= slots.Count) {
        LOG("[QB] BindSlot: slots not built yet (bar=%u count=%d)", bar, slots.Count);
        return false;
    }
    void* target = (char*)slots.Data + (size_t)slot * profile::QuickBarSlot_SlotSize;
    Array values = Read<Array>(GetField(target, itemsProp));
    bool matched = false;
    for (int i = 0; i < values.Count; ++i)
        if (!memcmp((char*)values.Data + (size_t)i * sizeof(Guid), &guid, sizeof(Guid))) matched = true;
    if (!matched && values.Count == 1) {
        Write<Guid>(values.Data, 0, guid);   // stale temporary GUID must not survive reconciliation
        matched = true;
    }
    if (!matched && values.Count == 0) {
        void* itemsField = GetField(target, itemsProp);
        Fn<int(*)(void*,void*,const void*)>(profile::GenericArrayAdd)(itemsField, itemsProp, &guid);
        values = Read<Array>(itemsField);
        matched = values.Count >= 1 && values.Data && !memcmp(values.Data, &guid, sizeof(Guid));
        if (matched) {
            uint8_t* flags = (uint8_t*)((char*)target + profile::QuickBarSlot_EnabledOffset);
            *flags |= profile::QuickBarSlot_EnabledBit | profile::QuickBarSlot_OccupiedBit;
        }
        LOG("[QB] Standalone slot repair bar=%u slot=%d guid_verified=%d", bar, slot, (int)matched);
    }
    // [F19] Read back through the controller API for a definitive answer.
    void* actual = QBQuerySlot(bar, slot);
    if (actual) matched = matched || (actual == item);
    LOG("[QB] Slot bar=%u slot=%d verified=%d", bar, slot, (int)matched);
    return matched;
}

void Athena::ActivateQuickBarSlot(uint8_t bar, int slot) {
    if (!session.controller) return;
    Call act("/Script/FortniteGame.FortPlayerController.ClientActivateSlot");
    if (!act.ok()) { LOG("[QB] ClientActivateSlot binding failed"); return; }
    float delay = 0.f; bool bUpdatePrev = true; bool bForce = true;
    if (!act.set("InQuickBar", bar) || !act.set("Slot", slot) || !act.set("ActivateDelay", delay) ||
        !act.set("bUpdatePreviousFocusedSlot", bUpdatePrev) || !act.set("bForceExecution", bForce)) {
        LOG("[QB] ClientActivateSlot param binding failed"); return;
    }
    if (act.run(session.controller)) LOG("[QB] ✅ ClientActivateSlot bar=%u slot=%d", bar, slot);
}

void Athena::ForceUpdateQuickBars() {
    if (!session.controller) return;
    for (uint8_t bar : {(uint8_t)0, (uint8_t)1}) {
        Call c("/Script/FortniteGame.FortPlayerController.ClientForceUpdateQuickbar");
        if (c.ok() && c.set("QuickbarToRefresh", bar)) c.run(session.controller);
    }
}

// ============================================================================
// [F36] QUICKBARS NATIVE — Erbium binds every item through the QuickBars
// actor itself (GiveItem → ServerAddItemInternal, swap → ServerActivateSlot-
// Internal, Remove → ServerRemoveItemInternal). In standalone those RPCs run
// locally, so the client HUD sees exactly what the native bookkeeping wrote —
// no manual slot surgery needed. Every helper degrades to the manual path
// when the UFunction is absent on this build.
// ============================================================================
// [G18-b] forward declarations (definitions live in the inventory section)
static void* ItemDefinitionOf(void* item);
static Guid  ItemGuidOf(void* item);

bool Athena::QBAddItem(Guid guid, bool bSecondary, int slot) {
    void* qb = session.quickbars;
    if (!qb || IsBadPtr(qb) || !(guid.A | guid.B | guid.C | guid.D)) return false;
    Call add("/Script/FortniteGame.FortQuickBars.ServerAddItemInternal");
    if (!add.ok()) { LOG("[QB] ServerAddItemInternal not present"); return false; }
    bool bound = false;
    bound |= add.set("ItemGuid", guid) || add.set("Guid", guid) || add.set("InItemGuid", guid);
    bool sec = bSecondary;
    bound |= add.set("bSecondary", sec) || add.set("bSecondaryQuickBar", sec) || add.set("Secondary", sec) ||
             add.set_at(1, sec);
    int slotVal = slot;
    if (!add.set("SlotIndex", slotVal) && !add.set("Slot", slotVal) && !add.set("NewSlot", slotVal))
        add.set_at(2, slotVal);
    if (!add.run(qb)) return false;
    LOG("[QB] ✅ ServerAddItemInternal bar=%d slot=%d", (int)bSecondary, slot);
    return true;
}

// [G18-b] PRIMARY SLOT PATH — AFortPlayerController::AddItemToQuickBars(
// UFortItemDefinition* ItemDefinition, EEFortQuickBars QuickBarType, int32
// SlotIndex) — SDK-verified (FortniteGame_functions.cpp:76307, BlueprintCallable
// + Native). This is the CLIENT'S OWN quickbar bookkeeping: it resolves the
// owning QuickBars actor, places the definition in the slot AND refreshes the
// HUD — no raw array surgery, no guessed native conventions. Enum size is tried
// both ways (reflected enums are commonly u8; some builds widen to u32).
bool Athena::QBBindController(void* def, uint8_t bar, int slot) {
    if (!def || !session.controller || slot < 0) return false;
    Call c("/Script/FortniteGame.FortPlayerController.AddItemToQuickBars");
    if (!c.ok()) { LOG("[QB] AddItemToQuickBars not present"); return false; }
    if (!c.set("ItemDefinition", def)) { LOG("[QB] AddItemToQuickBars: ItemDefinition bind failed"); return false; }
    bool typeOk = c.set("QuickBarType", (int)bar) || c.set<uint8_t>("QuickBarType", bar);
    if (!typeOk) {
        // Positional fallback: QuickBarType is the 2nd non-return param.
        int barVal = bar;
        typeOk = c.bind_at(1, &barVal, c.param_at_size(1) > 0 ? c.param_at_size(1) : 1);
    }
    bool slotOk = c.set("SlotIndex", slot) || c.set<int32_t>("SlotIndex", slot);
    if (!slotOk) {
        int slotVal = slot;
        slotOk = c.bind_at(2, &slotVal, c.param_at_size(2) > 0 ? c.param_at_size(2) : 4);
    }
    if (!typeOk || !slotOk) { LOG("[QB] AddItemToQuickBars: param bind failed (type=%d slot=%d)", (int)typeOk, (int)slotOk); return false; }
    if (!c.run(session.controller)) return false;
    LOG("[QB] ✅ AddItemToQuickBars bar=%u slot=%d def=%s", bar, slot, ObjName(def).c_str());
    return true;
}

// [G18-b] SELF-HEALING SLOT WATCH — the mobile HUD reads every hotbar/build
// slot through GetItemInQuickbarSlot; if that returns nothing the slot is
// INVISIBLE no matter what the raw arrays hold. This pass re-checks the whole
// starter layout every 10s (up to 12 passes ≈ 2 min) and re-binds anything the
// HUD still cannot see — covering late HUD init, world-travel re-pairs and
// the client rebuilding its quickbar after a loading screen drop.
void Athena::TickQuickBarWatch() {
    if (!session.finalized || !session.controller || !session.inventory_ready) return;
    if (session.qb_slots_verified) return;
    double now = Monotonic();
    if (now - session.last_qb_watch < 10.0) return;
    session.last_qb_watch = now;
    if (session.qb_watch_tries++ > 12) return;

    // Count HUD-visible slots on both bars through the NATIVE read path.
    int visible[2] = {0, 0};
    {
        Call probe("/Script/FortniteGame.FortPlayerController.GetItemInQuickbarSlot");
        if (!probe.ok()) return;   // no native read path on this build — watch is useless
    }
    for (int bar = 0; bar < 2; ++bar)
        for (int s = 0; s < 6; ++s)
            if (QBQuerySlot(bar, s)) ++visible[bar];
    int expectedPrimary = 0, expectedSecondary = 0;
    for (int s = 0; s < 6; ++s) { if (session.slots[s]) ++expectedPrimary; }
    for (int s = 0; s < 6; ++s) { if (session.secondary_slots[s]) ++expectedSecondary; }
    LOG("[QB] watch pass %d: HUD-visible %d/%d primary, %d/%d secondary",
        session.qb_watch_tries, visible[0], expectedPrimary, visible[1], expectedSecondary);

    if (visible[0] >= expectedPrimary && expectedPrimary > 0 && visible[1] >= expectedSecondary) {
        session.qb_slots_verified = true;
        LOG("[QB] ✅ all starter slots HUD-visible — watch complete");
        return;
    }
    // Re-bind every slot the HUD cannot see through the layered binder.
    int rebound = 0;
    for (int bar = 0; bar < 2; ++bar) {
        auto& src = bar == 0 ? session.slots : session.secondary_slots;
        for (int s = 0; s < 6; ++s) {
            if (!src[s]) continue;
            if (QBQuerySlot(bar, s)) continue;   // already visible
            void* def = ItemDefinitionOf(src[s]);
            if (!def) continue;
            if (QBBindController(def, (uint8_t)bar, s)) ++rebound;
        }
    }
    ForceUpdateQuickBars();
    if (rebound) LOG("[QB] watch re-bound %d slot(s)", rebound);
}

void Athena::QBActivateSlot(bool bSecondary, int slot) {
    void* qb = session.quickbars;
    if (!qb || IsBadPtr(qb) || slot < 0) return;
    // [F36] Erbium parity: QuickBars.ServerActivateSlotInternal(bar, slot, delay, updatePrev)
    Call act("/Script/FortniteGame.FortQuickBars.ServerActivateSlotInternal");
    if (act.ok()) {
        uint8_t bar = bSecondary ? 1 : 0;
        float delay = 0.f; bool upd = true;
        bool bound = act.set("QuickBarType", bar) || act.set("bSecondary", bar) || act.set_at(0, bar);
        bound |= act.set("SlotIndex", slot) || act.set("Slot", slot) || act.set_at(1, slot);
        act.set("ActivateDelay", delay) || act.set_at(2, delay);
        act.set("bUpdatePreviousFocusedSlot", upd) || act.set_at(3, upd);
        if (bound && act.run(qb)) return;
    }
    // Legacy fallback: controller-level activation.
    if (!session.controller) return;
    Call legacy("/Script/FortniteGame.FortPlayerController.ClientActivateSlot");
    if (!legacy.ok()) return;
    float delay = 0.f; bool bUpdatePrev = true; bool bForce = true;
    if (legacy.set("InQuickBar", bSecondary ? (uint8_t)1 : (uint8_t)0) &&
        legacy.set("Slot", slot) && legacy.set("ActivateDelay", delay) &&
        legacy.set("bUpdatePreviousFocusedSlot", bUpdatePrev) && legacy.set("bForceExecution", bForce))
        legacy.run(session.controller);
}

void Athena::QBRemoveItem(Guid guid, int slot) {
    void* qb = session.quickbars;
    if (!qb || IsBadPtr(qb)) return;
    // Erbium Remove(): QuickBars->EmptySlot(bar, i) + ServerRemoveItemInternal(guid, false, true)
    if (slot >= 0 && slot < 6) {
        Call empty("/Script/FortniteGame.FortQuickBars.EmptySlot");
        if (empty.ok()) {
            bool sec = false; int s = slot;
            bool bound = empty.set("bSecondary", sec) || empty.set("Secondary", sec) || empty.set_at(0, sec);
            bound |= empty.set("SlotIndex", s) || empty.set("Slot", s) || empty.set_at(1, s);
            if (bound) empty.run(qb);
        }
    }
    Call rem("/Script/FortniteGame.FortQuickBars.ServerRemoveItemInternal");
    if (!rem.ok()) return;
    bool bound = rem.set("ItemGuid", guid) || rem.set("Guid", guid) || rem.set("InItemGuid", guid);
    bool f1 = false, f2 = true;
    rem.set("bForceRemoval", f1) || rem.set_at(1, f1);
    rem.set("bDestroyItem", f2) || rem.set_at(2, f2);
    if (bound) rem.run(qb);
}

// ============================================================================
// MOVEMENT FEATURES
// ============================================================================
void Athena::EnableMovementFeatures() {
    if (!session.pawn) return;
    LOG("[Movement] Enabling slide & swim...");

    bool canSlideOk = false;   // [F90-b/AUD-1] movement commits only when the critical gate landed
    Call canSlide("/Script/FortniteGame.FortPlayerPawn.SetCanSlide");
    if (canSlide.ok()) {
        bool bTrue = true;
        if (canSlide.set("bCanSlide", bTrue)) {
            if (canSlide.run(session.pawn)) { LOG("[Movement] ✅ SetCanSlide(true)"); canSlideOk = true; }
            else LOG("[Movement] SetCanSlide run failed");
        }
    } else LOG("[Movement] SetCanSlide not found");

    // [F51/F52/F83-b] bWantsToSlide is input/game-driven — forcing it on
    // permanently broke the slide state machine (the F51 fix the rewrite
    // lost). Only SetCanSlide stays forced; the actual SLIDE ability spec is
    // granted by GrantWorldAbilities below.

    void* movement = GetObjField(session.pawn, "/Script/Engine.Character.CharacterMovement");
    if (movement && !IsBadPtr(movement)) {
        const char* swimPaths[] = {
            "/Script/Engine.NavMovementComponent.bCanSwim",
            "/Script/Engine.MovementComponent.bCanSwim",
            "/Script/Engine.CharacterMovementComponent.bCanSwim",
            nullptr
        };
        for (int i = 0; swimPaths[i]; ++i) {
            void* p = GetProp(swimPaths[i]);
            if (p) { SetBool(movement, swimPaths[i], true); LOG("[Movement] ✅ Set %s", swimPaths[i]); break; }
        }
    }

    // [F52/F67/F83-b] Slide is delivered as a REAL ability spec through a
    // mini FortAbilitySet + EquipFortAbilitySet (GrantWorldAbilities retries
    // from Tick until the BP classes stream in). The old direct
    // TryActivateAbilityByClass fire here activated the ability ONCE outside
    // the spec system — that path corrupted pawn state on device (F67).
    GrantWorldAbilities();

    // [F90-b/AUD-1] commit ONLY on SetCanSlide success: on failure the 8s
    // Tick retry re-invokes this function — GrantWorldAbilities is
    // flag-guarded (idempotent) and the comp fields are re-applied by
    // ApplySprintSpeed every 2s, so a re-run is free of side effects.
    session.movement_enabled = canSlideOk;
    LOG("[Movement] %s", canSlideOk ? "Done" : "incomplete — retry scheduled");
}

// ============================================================================
// [F41/F68/F83-b] BODY + LOCOMOTION ANIMS — Erbium assembles the body through
// parts only (F82 cosmetics flow, already present). Here we (a) re-run the
// part pipeline when no parts ever landed, (b) restore a MISSING
// AnimScriptInstance — without it the pawn has NO locomotion graph at all,
// which is exactly "dead run/slide anims" even with correct parts, (c) keep
// the body visible. No direct mesh swap (F68 — that caused the "sick body").
// Complements F82: F82 fixes the PARTS pipeline, this fixes the ANIM GRAPH.
void Athena::FixBodyAndAnimations() {
    if (session.body_fixed || !session.pawn) return;
    void* mesh = GetObjField(session.pawn, "/Script/Engine.Character.Mesh");
    if (!mesh || IsBadPtr(mesh)) return;

    if (session.character_parts.empty()) {
        LoadProfileCosmetics();          // locker -> part pipeline -> defaults
        if (session.character_parts.empty()) return;   // assets not resident yet — retry next pass
    }
    void* animInst = GetObjField(mesh, "/Script/Engine.SkeletalMeshComponent.AnimScriptInstance");
    if (!animInst) {
        void* animBP = FindLoadedObjectOfType("/Script/Engine.AnimBlueprintGeneratedClass", "Fortnite_M_Avg_Player_AnimBlueprint");
        if (!animBP) animBP = FindLoadedObjectOfType("/Script/Engine.AnimBlueprintGeneratedClass", "M_MED_Lancelot_AnimBP_C");
        if (animBP) {
            Call a("/Script/Engine.SkeletalMeshComponent.SetAnimClass");
            if (a.ok() && a.set("NewClass", animBP) && a.run(mesh))
                LOG("[Body] AnimScriptInstance restored (locomotion)");
        }
    }
    animInst = GetObjField(mesh, "/Script/Engine.SkeletalMeshComponent.AnimScriptInstance");
    if (!animInst) return;   // AnimBP not resident yet — retry next pass

    Call body("/Script/FortniteGame.FortPlayerPawnAthena.SetCharacterBodyVisibilityForPossession");
    if (body.ok()) { bool bShow = true; if (body.set("bVisible", bShow)) body.run(session.pawn); }
    Call hid("/Script/Engine.Actor.SetActorHiddenInGame");
    if (hid.ok()) { bool h = false; if (hid.set("bNewHidden", h)) hid.run(session.pawn); }

    // [G07] Finish with the NATIVE body assembler — Erbium's fallback when the
    // visualization refresh is not enough: ApplyCharacterCustomization(Player-
    // State, Pawn) builds the cosmetics component from the CharacterParts the
    // part pipeline just installed (this is the function the live game calls
    // at possession; it also fixes the "sick body" mesh state F68 described).
    if (gas::ApplyCharacterCustomization && session.player_state && session.pawn) {
        // [G13] Fault-guarded — a scanner-resolved native must never be able
        // to kill the process; on fault the native is zeroed for the session
        // and the reflection-installed character parts stand.
        if (gas::RunGuarded("ACC", gas::NativeAccCall, session.player_state, session.pawn))
            LOG("[Body] native ApplyCharacterCustomization dispatched");
        else {
            LOG("[Body] native ApplyCharacterCustomization faulted — disabled for the session");
            gas::ApplyCharacterCustomization = 0;
        }
    }

    session.body_fixed = true;
    LOG("[Body] ✅ locomotion online — body via %zu character parts (Erbium)",
        session.character_parts.size());
}

// ============================================================================
// [F52/F67/F83-b] WORLD ABILITIES — slide + vehicle abilities delivered the
// Erbium way: build a mini FortAbilitySet and Equip it, so the ASC creates
// real ability specs with input bindings.
void Athena::GrantWorldAbilities() {
    // [G16-e] SPEC-PRESENCE VERIFIER. The run-5 spec dump proved the world
    // abilities are ALREADY GRANTED through GAS_AthenaPlayer's 37 (spec #9
    // EnterVehicle, #10 ExitVehicle, #11 InVehicle, #36 Slide, #41 DoorBash,
    // #38/39 TacticalSprint, #40 Ascender_Smash) — the old mini-set grant
    // could only DUPLICATE them, and its "core pair" gate kept wabilities=0
    // in every heartbeat while the abilities were in fact live. This now
    // VERIFIES presence in the ASC's spec array (engine stride) and reports
    // once — the heartbeat metric finally tells the truth.
    static bool s_verified = false;
    if (s_verified || !session.pawn) return;
    void* asc = GetASC(session.pawn);
    if (!asc) return;
    void* contProp = GetProp("/Script/GameplayAbilities.AbilitySystemComponent.ActivatableAbilities");
    void* itemsProp = GetProp("/Script/GameplayAbilities.GameplayAbilitySpecContainer.Items");
    if (!contProp || !itemsProp) return;
    int stride = gas::SpecArrayStride();
    if (stride <= 0) stride = 0xE8;
    gas::SpecLayout layout;
    if (!gas::ResolveSpecLayout(layout)) return;
    Array items = Read<Array>((char*)asc + Read<int>(contProp, profile::PropOffset)
                                       + Read<int>(itemsProp, profile::PropOffset));
    if (!items.Data || IsBadPtr(items.Data) || items.Count <= 0 || items.Count > 4096) return;
    static const char* const names[] = {
        "GA_Athena_Player_Slide_C",
        "GA_AthenaEnterVehicle_C",
        "GA_AthenaExitVehicle_C",
        "GA_AthenaInVehicle_C",
        "GA_Athena_Player_DoorBash_C",
        nullptr
    };
    int present = 0;
    for (int n = 0; names[n]; ++n) {
        for (int i = 0; i < items.Count; ++i) {
            uint8_t* spec = (uint8_t*)items.Data + (size_t)i * (size_t)stride;
            if (!IsReadable(spec, (size_t)stride)) break;
            void* ability = Read<void*>(spec, layout.OffAbility);
            if (!ability || ((uintptr_t)ability & 7) != 0 || !IsReadable(ability, 0x28)) continue;
            if (!gas::PtrInUnrealModule(Read<void*>(ability, 0))) continue;
            std::string nm = ObjName(ability);   // "Default__GA_..." — class CDO name
            if (nm.find(names[n]) != std::string::npos) { ++present; break; }
        }
    }
    s_verified = true;
    session.world_abilities_granted = (present >= 2);   // heartbeat truth
    LOG("[World] ✅ world abilities verified in specs: %d/5 (slide/enter/exit/in-vehicle/doorbash)", present);
}

// ============================================================================
// TACTICAL SPRINT
// ============================================================================
void Athena::SetupTacticalSprint() {
    if (!session.controller) return;
    if (session.tactical_sprint_setup && session.tactical_sprint) return;
    LOG("[TacticalSprint] Setting up component...");
    void* compClass = FindObj("/Script/TacticalSprintRuntime.FortPlayerControllerComponent_TacticalSprint");
    if (!compClass) { LOG("[TacticalSprint] Component class not found"); session.tactical_sprint_setup = true; return; }
    void* comp = nullptr;
    Call get("/Script/Engine.Actor.GetComponentByClass");
    if (get.ok() && get.set("ComponentClass", compClass) && get.run(session.controller)) comp = get.result<void*>();
    if (!comp || IsBadPtr(comp)) {
        LOG("[TacticalSprint] Adding component...");
        Call add("/Script/Engine.Actor.AddComponentByClass");
        if (add.ok()) {
            bool bManual = false; Transform t{}; bool bDeferred = false;
            add.set("Class_0", compClass) || add.set("Class", compClass);
            add.set("bManualAttachment", bManual);
            add.set("RelativeTransform", t); add.set("bDeferredFinish", bDeferred);
            if (add.run(session.controller)) { comp = add.result<void*>(); LOG("[TacticalSprint] Component added: %p", comp); }
        }
    }
    if (!comp || IsBadPtr(comp)) { LOG("[TacticalSprint] Failed to create component"); session.tactical_sprint_setup = true; return; }
    session.tactical_sprint = comp;
    if (!profile::RawOffsetsVerified) { session.tactical_sprint = comp; LOG("[TacticalSprint] component online (raw flags write disabled on this profile)"); return; }   // [G16-f]
    uint8_t* flags = (uint8_t*)comp + profile::TacticalSprint_Flags;
    *flags |= profile::TacticalSprint_Bit_Enabled;
    LOG("[TacticalSprint] Enabled, flags=0x%02X", *flags);
    Call set("/Script/TacticalSprintRuntime.FortPlayerControllerComponent_TacticalSprint.SetIsSprinting");
    if (set.ok()) {
        bool bSprint = true;
        if (set.set("bNewIsSprinting", bSprint)) set.run(comp);
    }
    session.tactical_sprint_setup = true;
    LOG("[TacticalSprint] ✅ Setup complete");
}
void Athena::SetTacticalSprinting(bool sprinting) {
    if (!session.tactical_sprint) return;
    Call set("/Script/TacticalSprintRuntime.FortPlayerControllerComponent_TacticalSprint.SetIsSprinting");
    if (set.ok() && set.set("bNewIsSprinting", sprinting)) set.run(session.tactical_sprint);
}

void Athena::ApplySprintSpeed() {
    if (!session.pawn || IsBadPtr(session.pawn)) return;
    void* pawnCls = FindObj("/Game/Athena/PlayerPawn_Athena.PlayerPawn_Athena_C");
    if (pawnCls && IsA(session.pawn, pawnCls)) {
        if (!profile::RawOffsetsVerified) return;   // [G16-f] raw offset unverified on this profile
        float* sprintSpeed = (float*)((char*)session.pawn + profile::PlayerPawn_Athena_Speed_Sprint);
        if (!IsBadPtr(sprintSpeed)) {
            float old = *sprintSpeed;
            if (std::isfinite(old) && old > 0.0f && old < 5000.0f) {
                if (fabsf(old - 1300.0f) > 1.0f) { *sprintSpeed = 1300.0f; LOG("[Sprint] Speed_Sprint: %.0f -> 1300", old); }
            }
        }
    }
    void* cmc = GetObjField(session.pawn, "/Script/Engine.Character.CharacterMovement");
    if (cmc && !IsBadPtr(cmc)) {
        void* prop = GetProp("/Script/Engine.CharacterMovementComponent.MaxWalkSpeed");
        if (prop) { float* val = (float*)GetField(cmc, prop); if (val && !IsBadPtr(val)) { if (fabsf(*val - 600.f) > 1.0f) *val = 600.f; } }
        void* flyProp = GetProp("/Script/Engine.CharacterMovementComponent.MaxFlySpeed");
        if (flyProp) { float* val = (float*)GetField(cmc, flyProp); if (val && !IsBadPtr(val)) { if (fabsf(*val - 1200.f) > 1.0f) *val = 1200.f; } }
    }
    session.sprint_applied = true;
}

// ============================================================================
// BATTLE BUS / SKYDIVING — [F04] aircraft jump via the 21.30 component
// ============================================================================
static void* GetAircraftComponent(void* controller) {
    if (!controller) return nullptr;
    void* cls = FindObj("/Script/FortniteGame.FortControllerComponent_Aircraft");
    if (!cls) return nullptr;
    Call get("/Script/Engine.Actor.GetComponentByClass");
    if (!get.ok() || !get.set("ComponentClass", cls)) return nullptr;
    if (!get.run(controller)) return nullptr;
    return get.result<void*>();
}

void Athena::SpawnBattleBus() {
    if (!session.mode || !session.state) return;
    LOG("[Bus] Attempting bus...");
    void* gp = GetProp("/Script/FortniteGame.FortGameStateAthena.GamePhase");
    if (gp) {
        uint8_t phase = 3;
        Write<uint8_t>(GetField(session.state, gp), 0, phase);
        Call n("/Script/FortniteGame.FortGameStateAthena.OnRep_GamePhase");
        if (n.ok() && n.set("OldGamePhase", (uint8_t)2)) n.run(session.state);
    }
    if (session.pawn) {
        Vector loc{0.0, 0.0, 50000.0}; Rotator rot{0.0, 0.0, 0.0};
        Call sl("/Script/Engine.Actor.K2_SetActorLocationAndRotation");
        if (sl.ok()) {
            bool bSweep = false, bTeleport = true;
            sl.set("NewLocation", loc); sl.set("NewRotation", rot);
            sl.set("bSweep", bSweep); sl.set("bTeleport", bTeleport);
            sl.run(session.pawn);
        }
        BeginPlayerSkydiving();
    }
    LOG("[Bus] Aircraft phase + skydive dispatched");
}

// [F05] The touch jump button raises UAthenaHUDContext.JumpFromHudStart; the
// standalone build has no authoritative server for the aircraft RPC, so run
// the local skydive fallback and restore the pawn as the view target (Era
// core behaviour, ported with the 21.30 component path added).
void Athena::HandleJumpFromHudStart() {
    if (!session.controller || !session.pawn) return;
    if (session.aircraft_jumped) return;
    Guard g;
    LOG("[Bus] JumpFromHudStart intercepted");
    Vector rot{0, 0, 0};
    void* rotProp = GetProp("/Script/Engine.Controller.ControlRotation");
    if (rotProp && Read<int>(rotProp, profile::PropElementSize) == 24) {
        void* addr = GetField(session.controller, rotProp);
        if (addr) rot = Read<Vector>(addr);
    }
    // [F04] ServerAttemptAircraftJump lives on UFortControllerComponent_Aircraft.
    void* aircraft = GetAircraftComponent(session.controller);
    if (aircraft) {
        Call ac("/Script/FortniteGame.FortControllerComponent_Aircraft.ServerAttemptAircraftJump");
        if (ac.ok()) {
            Rotator r{rot.X, rot.Y, rot.Z};
            if (ac.set("ClientRotation", r)) ac.run(aircraft);
        }
    } else {
        LOG("[Bus] Aircraft component unavailable — local fallback only");
    }
    Call sky("/Script/FortniteGame.FortPlayerPawn.BeginSkydiving");
    if (sky.ok()) { bool bFromBus = true; if (sky.set("bFromBus", bFromBus)) sky.run(session.pawn); }
    Write<uint8_t>(session.pawn, profile::FortPlayerPawn_bIsSkydiving, 1);
    Call cam("/Script/Engine.PlayerController.SetViewTargetWithBlend");
    if (cam.ok()) { cam.set("NewViewTarget", session.pawn); float blend = 0.0f; cam.set("BlendTime", blend); cam.run(session.controller); }
    Call rep("/Script/FortniteGame.FortPlayerPawn.OnRep_IsSkydiving");
    if (rep.ok()) rep.run(session.pawn);
    session.aircraft_jumped = true;
    LOG("[Bus] Skydiving started");
}
void Athena::OnAircraftJump() { HandleJumpFromHudStart(); }
void Athena::BeginPlayerSkydiving() {
    if (!session.pawn || IsBadPtr(session.pawn)) return;
    Call bsd("/Script/FortniteGame.FortPlayerPawn.BeginSkydiving");
    if (bsd.ok()) { bool bFromBus = true; if (bsd.set("bFromBus", bFromBus)) { bsd.run(session.pawn); LOG("[Skydive] BeginSkydiving(true)"); } }
    Write<uint8_t>(session.pawn, profile::FortPlayerPawn_bIsSkydiving, 1);
}

void Athena::SetupCheats() {
    void* pc = session.controller; if (!pc) return;
    void* p = GetProp("/Script/Engine.PlayerController.CheatManager");
    void* klass = FindObj("/Script/FortniteGame.FortCheatManager");
    if (!klass) klass = FindObj("/Script/Engine.CheatManager");
    if (!p || !klass || Read<int>(p, profile::PropElementSize) != 8) return;
    void* cheat = Read<void*>(GetField(pc, p));
    if (!cheat) {
        cheat = Fn<void*(*)(void*, void*)>(profile::SpawnObject)(klass, pc);
        if (cheat) Write<void*>(GetField(pc, p), 0, cheat);
    }
    if (cheat) {
        Call init("/Script/Engine.CheatManager.ReceiveInitCheatManager");
        if (init.ok()) init.run(cheat);
        session.cheats_ready = true;
    }
}

// ============================================================================
// DOORS — [G06] IOpenableInterface parity + [F-door r1] bit-pack fallback
// ============================================================================
// [G06] The 21.30 door API is reflected: UFortKismetLibrary::OpenActor /
// CloseActor (Final, BlueprintAuthorityOnly, Native, Static — SDK-verified)
// dispatch IOpenableInterface::Open(ControllerInstigator, bRequestFastOpen) /
// Close on the actor, i.e. ABuildingWall's own door implementation. In
// standalone the world IS the authority, so the native path runs, sets the
// replicated bDoorOpen through the real SetIsDoorOpen chain (swing anim +
// collision disable + OnRep), and the door STAYS OPEN exactly like the live
// game. The [F-door r1] manual bit-pack write is kept ONLY as the fallback.
//
// 21.30 layout (MobileDumper7 dump, ABuildingWall 0x0FF8):
//   0x0FDE = bSwingingDoor(0)|bSlidingDoor(1)|bAutomaticSlidingDoor(2)|
//            bDoubleDoor(3)|bCreateDoorLink(4)|bDoorOpen(5,RepNotify)|
//            bLocalDoorOpen(6)|bDoorCollisionDisabled(7)
//   0x0FDF = bFastOpenRequested(4, Net|Transient)
static bool DoorOpenNative(void* wall, bool fast) {
    Call c("/Script/FortniteGame.FortKismetLibrary.OpenActor");
    if (!c.ok()) return false;
    void* klib = FindObj("/Script/FortniteGame.Default__FortKismetLibrary");
    if (!klib) return false;
    bool bFast = fast;
    if (!c.set("OpenableInterfaceActor", wall) ||
        !c.set("OptionalControllerInstigator", session.controller) ||
        !c.set("bRequestFastOpen", bFast)) return false;
    if (!c.run(klib)) return false;
    return c.result<bool>();
}
static bool DoorCloseNative(void* wall) {
    Call c("/Script/FortniteGame.FortKismetLibrary.CloseActor");
    if (!c.ok()) return false;
    void* klib = FindObj("/Script/FortniteGame.Default__FortKismetLibrary");
    if (!klib) return false;
    if (!c.set("OpenableInterfaceActor", wall) ||
        !c.set("OptionalControllerInstigator", session.controller)) return false;
    if (!c.run(klib)) return false;
    return c.result<bool>();
}
// State probe: ABuildingWall::IsDoorOpen() (SDK-verified UFunction), with the
// bit read as fallback.
static bool DoorIsOpen(void* wall) {
    if (!wall || IsBadPtr(wall)) return false;
    Call c("/Script/FortniteGame.BuildingWall.IsDoorOpen");
    if (c.ok() && c.run(wall)) return c.result<bool>();
    if (!profile::RawOffsetsVerified) return false;   // [G16-f] bit offsets 21.30-verified only
    uint8_t* pack = (uint8_t*)wall + profile::BuildingWall_bDoorOpen_Byte;
    return (*pack & 0x20) != 0;
}

static void DoorOpenApply(void* wall, bool fast) {
    // [G06] native IOpenableInterface route first — authority, replication,
    // animation and collision all handled by ABuildingWall's own Open().
    if (DoorOpenNative(wall, fast)) {
        Call netup("/Script/Engine.Actor.ForceNetUpdate");
        if (netup.ok()) netup.run(wall);
        return;
    }
    // [F-door r1] fallback: touch only the door bits.
    if (!profile::RawOffsetsVerified) return;   // [G16-f] bit-pack offsets are 21.30-verified only
    uint8_t* pack = (uint8_t*)wall + profile::BuildingWall_bDoorOpen_Byte;
    *pack |= 0x20 | 0x40;                        // bDoorOpen | bLocalDoorOpen
    if (fast) {
        uint8_t* fastPack = (uint8_t*)wall + profile::BuildingWall_bDoorOpen_Byte + 1;
        *fastPack |= 0x10;                       // bFastOpenRequested
    }
    Call r("/Script/FortniteGame.BuildingWall.OnRep_bDoorOpen");
    if (r.ok()) r.run(wall);

    Call netup("/Script/Engine.Actor.ForceNetUpdate");
    if (netup.ok()) netup.run(wall);
}

static void DoorCloseApply(void* wall) {
    // [G06] native Close route first.
    if (DoorCloseNative(wall)) {
        Call netup("/Script/Engine.Actor.ForceNetUpdate");
        if (netup.ok()) netup.run(wall);
        return;
    }
    if (!profile::RawOffsetsVerified) return;   // [G16-f] bit-pack offsets are 21.30-verified only
    uint8_t* pack = (uint8_t*)wall + profile::BuildingWall_bDoorOpen_Byte;
    *pack &= uint8_t(~(0x20 | 0x40));            // clear bDoorOpen|bLocalDoorOpen
    Call r("/Script/FortniteGame.BuildingWall.OnRep_bDoorOpen");
    if (r.ok()) r.run(wall);

    Call netup("/Script/Engine.Actor.ForceNetUpdate");
    if (netup.ok()) netup.run(wall);
}

// [G16-a] OWN-OPENED door bookkeeping — the toggle direction source.
// In standalone the dropped server RPCs mean WE are the only door driver:
// in-set → next interact closes; not-in-set → next interact opens.
static bool DoorSetTake(void* door) {   // remove from set, return whether it was there
    for (auto it = session.opened_doors.begin(); it != session.opened_doors.end(); ++it)
        if (*it == door) { session.opened_doors.erase(it); return true; }
    return false;
}
static void DoorSetAdd(void* door) {
    for (void* d : session.opened_doors) if (d == door) return;
    session.opened_doors.push_back(door);
    if (session.opened_doors.size() > 128) session.opened_doors.erase(session.opened_doors.begin());
}
static bool DoorSetHas(void* door) {
    for (void* d : session.opened_doors) if (d == door) return true;
    return false;
}

void Athena::HandleOpenable(void* target) {
    // [G16-a] THE FIX for "nothing happens" on BP doors: GasStation_Door_C_C
    // / StripMall_Wall_A_Doorway_01_C are NOT BuildingWalls — the old gate
    // ignored them, and since standalone DROPS the server RPC (no net
    // connection → ProcessEvent never runs the native body), nothing ever
    // opened them. Erbium runs the native interact on a real listen server;
    // we ARE the server here, so the toggle goes through the same Kismet
    // statics the native interact uses (UFortKismetLibrary::OpenActor /
    // CloseActor dispatch IOpenableInterface::Open/Close on ANY implementer).
    if (!target || IsBadPtr(target)) return;
    if (DoorSetTake(target)) {
        if (DoorCloseNative(target)) {
            LOG("[Door] ✅ closed %s", ObjName(Read<void*>(target, profile::ObjectClass)).c_str());
            return;
        }
        LOG("[Door] close declined for %s", ObjName(Read<void*>(target, profile::ObjectClass)).c_str());
        return;
    }
    if (DoorOpenNative(target, /*fast=*/false)) {
        DoorSetAdd(target);
        LOG("[Door] ✅ opened %s", ObjName(Read<void*>(target, profile::ObjectClass)).c_str());
        return;
    }
    // Not openable — BP interact event fallback (BuildingActor family).
    Call interact("/Script/FortniteGame.BuildingActor.BlueprintOnInteract", target);
    if (interact.ok() && session.pawn && interact.set("InteractingPawn", session.pawn))
        interact.run(target);
}

static bool GetAllActorsOfClassRaw(void* klass, std::vector<void*>& out);   // [G16-a] defined below

// [G16-a] SAFE BASH-THROUGH — the DoorBash parity for standalone: while
// tactically sprinting, closed BuildingWall doors within reach open with the
// fast flag. OPEN-ONLY: no pins, no re-assertion, no closing — it can never
// fight the interact toggle (a door it opened simply joins the opened-set,
// so the next interact closes it like any other door).
void Athena::TickSprintDoorAssist() {
    if (!session.tactical_sprint || !session.pawn) return;
    if (IsBadPtr(session.tactical_sprint)) return;
    const bool sprinting = (Read<uint8_t>(session.tactical_sprint, profile::TacticalSprint_Flags)
                            & profile::TacticalSprint_Bit_IsSprinting) != 0;
    if (!sprinting) return;
    double now = Monotonic();
    if (now - session.last_door_scan < 2.0) return;
    session.last_door_scan = now;
    static std::vector<void*> s_working;
    s_working.clear();
    {
        void* wallCls = FindObj("/Script/FortniteGame.BuildingWall");
        if (!wallCls) return;
        std::vector<void*> walls;
        if (!GetAllActorsOfClassRaw(wallCls, walls)) return;
        for (void* w : walls) {
            if (!w || IsBadPtr(w)) continue;
            if (DoorSetHas(w)) continue;                       // we opened it — it's open
            Vector loc{};
            if (!ActorLocation(w, loc) || !NearPlayer(loc, 300.0f)) continue;
            s_working.push_back(w);
            if (s_working.size() >= 24) break;
        }
    }
    int opened = 0;
    for (void* w : s_working) {
        Vector loc{};
        if (!ActorLocation(w, loc) || !NearPlayer(loc, 150.0f)) continue;
        if (DoorOpenNative(w, /*fast=*/true)) { DoorSetAdd(w); ++opened; }
    }
    if (opened) LOG("[Door] sprint fast-open %d", opened);
}

// [G15-b] DELETED: TickDoorPins / TickSprintDoors. The pin re-assertion ran
// every 0.3s and fought BOTH the native door state machine and the DoorBash
// ability; the sprint fast-open hack re-implemented what the granted
// AS_DoorBash ability already does natively (AutoOpenDoorInputMagnitude /
// AutoOpenDoorTraceDistance + the slam CVar path). Their tick calls and the
// session bookkeeping (door_pins / sprint_doors / last_door_scan) are gone.

// ============================================================================
// COSMETICS — [F22] locker sync through the MCP context, with the menu-hero
// parts fallback from the original hook (Era core setup_cosmetics ported).
// ============================================================================
static std::vector<void*> SoftAssetArray(void* owner, const char* path) {
    std::vector<void*> result;
    void* p = GetProp(path);
    if (!p || !owner || IsBadPtr(owner)) return result;
    // UE5-safe: the element stride is the property's own ElementSize
    // (TSoftObjectPtr in 21.30 is 24 bytes); no 0x80 inner probing needed.
    int stride = Read<int>(p, profile::PropElementSize);
    Array values = Read<Array>(GetField(owner, p));
    if (stride < 8 || stride > 128 || values.Count < 0 || values.Count > 64 || (values.Count && !values.Data)) return result;
    for (int i = 0; i < values.Count; ++i) {
        Call load("/Script/Engine.KismetSystemLibrary.LoadAsset_Blocking");
        if (load.ok() && load.set_bytes("Asset", (char*)values.Data + (size_t)i * stride, stride) &&
            load.run(FindObj("/Script/Engine.Default__KismetSystemLibrary")))
            if (void* value = load.result<void*>()) result.push_back(value);
    }
    return result;
}

// FFortAthenaLoadout shadow (21.30 size 0x110 — verified: last member
// CachedProfileSource @0x108 + 8).
struct FFortAthenaLoadoutShadow { uint8_t Bytes[0x110]; };

void Athena::SyncLockerLoadout() {
    if (!session.controller || !session.pawn) return;
    // [F22] AFortPlayerController.CosmeticLoadoutPC @0x1E70 (FFortAthenaLoadout).
    void* loadoutProp = GetProp("/Script/FortniteGame.FortPlayerController.CosmeticLoadoutPC");
    if (!loadoutProp) { LOG("[Locker] CosmeticLoadoutPC not present"); return; }
    void* loadout = GetField(session.controller, loadoutProp);

    void* character = GetObjField(loadout, "/Script/FortniteGame.FortAthenaLoadout.Character");
    void* pickaxe   = GetObjField(loadout, "/Script/FortniteGame.FortAthenaLoadout.Pickaxe");
    void* backpack  = GetObjField(loadout, "/Script/FortniteGame.FortAthenaLoadout.Backpack");

    // MCP route: GetContext(FortMcpContext).GetLoadoutForPlayer(PlayerID)
    // [G05] With SendRequestNow patched to CXC_Public the profile queries
    // actually dispatch now, so this route resolves the real locker instead
    // of silently no-oping. [G-d] GetRegisteredPlayerInfo() is the cleaner
    // entry (SDK-verified UFunction); MyPlayerInfo stays as fallback.
    void* info = nullptr;
    {
        Call gpi("/Script/FortniteGame.FortPlayerController.GetRegisteredPlayerInfo");
        if (gpi.ok() && gpi.run(session.controller)) info = gpi.result<void*>();
    }
    if (!info) info = GetObjField(session.controller, "/Script/FortniteGame.FortPlayerController.MyPlayerInfo");
    void* idProp = GetProp("/Script/FortniteGame.FortRegisteredPlayerInfo.PlayerID");
    if (info && idProp && !character) {
        void* ctxType = FindObj("/Script/FortniteGame.FortMcpContext");
        Call context("/Script/BlueprintContext.BlueprintContextLibrary.GetContext");
        if (ctxType && context.ok() &&
            context.set("ContextObject", session.controller) &&
            (context.set("Class", ctxType) || context.set("Class_0", ctxType)) &&
            context.run(FindObj("/Script/BlueprintContext.Default__BlueprintContextLibrary"))) {
            void* mcp = context.result<void*>();
            int idSize = Read<int>(idProp, profile::PropElementSize);
            Call get("/Script/FortniteGame.FortMcpContext.GetLoadoutForPlayer");
            if (mcp && get.ok() && get.set_bytes("MemberUniqueId", GetField(info, idProp), idSize) && get.run(mcp)) {
                void* value = get.result_data(Read<int>(loadoutProp, profile::PropElementSize));
                if (value) {
                    int size = Read<int>(loadoutProp, profile::PropElementSize);
                    memcpy(loadout, value, (size_t)std::min(size, (int)sizeof(FFortAthenaLoadoutShadow)));
                    character = GetObjField(loadout, "/Script/FortniteGame.FortAthenaLoadout.Character");
                    pickaxe   = GetObjField(loadout, "/Script/FortniteGame.FortAthenaLoadout.Pickaxe");
                    LOG("[Locker] loadout resolved through MCP (mcp=%p info=%p)", mcp, info);
                }
            } else if (mcp) {
                LOG("[Locker] MCP context live but loadout query failed (profile may still be loading)");
            }
        }
    }
    LOG("[Locker] character=%s pickaxe=%s", ObjName(character).c_str(), ObjName(pickaxe).c_str());
    session.locker_pickaxe = pickaxe;

    // Hero definition → hero type on the player state.
    void* state = GetObjField(session.controller, "/Script/Engine.Controller.PlayerState");
    void* heroTypeProp = GetProp("/Script/FortniteGame.FortPlayerState.HeroType");
    void* heroType = nullptr;
    if (character) {
        void* heroDef = GetObjField(character, "/Script/FortniteGame.AthenaCharacterItemDefinition.HeroDefinition");
        if (heroDef) {
            Call ght("/Script/FortniteGame.FortHero.GetHeroTypeBP");
            if (ght.ok() && ght.run(heroDef)) heroType = ght.result<void*>();
        }
    }
    if (heroType && state && heroTypeProp && Read<int>(heroTypeProp, profile::PropElementSize) == 8) {
        Write<void*>(GetField(state, heroTypeProp), 0, heroType);
        Call r1("/Script/FortniteGame.FortPlayerState.OnRep_HeroType");
        if (r1.ok()) r1.run(state);
    }

    // Mirror the loadout onto the pawn (CosmeticLoadout @0x2A08).
    void* pawnLoadoutProp = GetProp("/Script/FortniteGame.FortPlayerPawn.CosmeticLoadout");
    if (pawnLoadoutProp && Read<int>(pawnLoadoutProp, profile::PropElementSize) == Read<int>(loadoutProp, profile::PropElementSize)) {
        int size = Read<int>(pawnLoadoutProp, profile::PropElementSize);
        memcpy(GetField(session.pawn, pawnLoadoutProp), loadout, (size_t)size);
        LOG("[Locker] pawn CosmeticLoadout mirrored (%d bytes)", size);
    }

    // Character parts: menu hero first, then hero specializations, then backpack.
    std::vector<void*> equipped_parts;
    void* menu = info ? Read<void*>(info, profile::RegisteredPlayerInfo_HeroInstance) : nullptr;
    if (menu && !IsBadPtr(menu)) {
        Array parts = Read<Array>(menu, profile::FortHero_CharacterParts);
        if (parts.Data && parts.Count > 0 && parts.Count <= 16)
            for (int i = 0; i < parts.Count; ++i) equipped_parts.push_back(Read<void*>(parts.Data, (size_t)i * 8));
    }
    void* heroDef = character ? GetObjField(character, "/Script/FortniteGame.AthenaCharacterItemDefinition.HeroDefinition") : nullptr;
    if (heroDef && !IsBadPtr(heroDef)) {
        for (void* spec : SoftAssetArray(heroDef, "/Script/FortniteGame.FortHeroType.Specializations"))
            for (void* part : SoftAssetArray(spec, "/Script/FortniteGame.FortHeroSpecialization.CharacterParts"))
                equipped_parts.push_back(part);
    }
    if (backpack && !IsBadPtr(backpack)) {
        void* bp = GetProp("/Script/FortniteGame.AthenaCharacterPartItemDefinition.CharacterParts");
        if (bp) {
            Array back_parts = Read<Array>(GetField(backpack, bp));
            if (back_parts.Data && back_parts.Count >= 0 && back_parts.Count <= 16)
                for (int i = 0; i < back_parts.Count; ++i) equipped_parts.push_back(Read<void*>(back_parts.Data, (size_t)i * 8));
        }
    }
    for (void* part : equipped_parts) {
        uint8_t type = 255;
        void* partCls = FindObj("/Script/FortniteGame.CustomCharacterPart");
        if (!part || !partCls || !IsA(part, partCls)) continue;
        if (!GetFieldValue<uint8_t>(part, "/Script/FortniteGame.CustomCharacterPart.CharacterPartType", type) || type > 6) continue;
        Call choose("/Script/FortniteGame.FortPlayerPawn.ServerChoosePart");
        if (choose.ok() && choose.set("Part", type) && choose.set("ChosenCharacterPart", part)) {
            choose.run(session.pawn);
            if (std::find(session.character_parts.begin(), session.character_parts.end(), part) == session.character_parts.end())
                session.character_parts.push_back(part);
        }
    }
    if (!equipped_parts.empty() && state) {
        Array parts{session.character_parts.data(), (int32)session.character_parts.size(), (int32)session.character_parts.size()};
        Call apply("/Script/FortniteGame.FortKismetLibrary.ApplyCharacterCosmetics");
        if (apply.ok() && apply.set("WorldContextObject", session.world) && apply.set("CharacterParts", parts) && apply.set("PlayerState", state))
            apply.run(FindObj("/Script/FortniteGame.Default__FortKismetLibrary"));
    }
    if (state) {
        Call r3("/Script/FortniteGame.FortPlayerState.OnRep_CharacterData"); if (r3.ok()) r3.run(state);
        void* klib = FindObj("/Script/FortniteGame.Default__FortKismetLibrary");
        if (klib) {
            Call refresh("/Script/FortniteGame.FortKismetLibrary.UpdatePlayerCustomCharacterPartsVisualization");
            if (refresh.ok() && refresh.set("PlayerState", state)) refresh.run(klib);
        }
    }
}

void Athena::LoadProfileCosmetics() {
    if (!session.pawn || !session.controller) return;
    LOG("[Profile] Loading cosmetics...");
    SyncLockerLoadout();
    if (!session.character_parts.empty()) { ShowSkin(); return; }
    LoadDefaultCosmetics();
}

void Athena::LoadDefaultCosmetics() {
    if (!session.pawn || !session.controller) return;
    LOG("[Cosmetics] Loading hardcoded defaults...");
    // [F82] ROOT CAUSE of dead run/slide anims: the pawn is spawned with an
    // EMPTY CosmeticLoadout, and on this client build the mesh + animation
    // stack only materialize through the loadout evaluation path
    // (OnRep_CosmeticLoadout — verified present in libUnreal.so; server RPCs
    // like EvaluateCharacterParts are stripped). Give the locker a REAL
    // default AthenaCharacterItemDefinition first, mirror it onto the pawn,
    // then the native OnRep rebuilds parts + anim instance.
    {
        void* loadoutProp = GetProp("/Script/FortniteGame.FortPlayerController.CosmeticLoadoutPC");
        void* charProp    = GetProp("/Script/FortniteGame.FortAthenaLoadout.Character");
        if (loadoutProp && charProp) {
            void* loadout = GetField(session.controller, loadoutProp);
            void* cur = GetObjField(loadout, "/Script/FortniteGame.FortAthenaLoadout.Character");
            if (!cur || IsBadPtr(cur)) {
                void* cid = LoadAsset("/Game/Athena/Items/Cosmetics/Characters/CID_001_Athena_Character_F.CID_001_Athena_Character_F",
                                      "/Script/FortniteGame.AthenaCharacterItemDefinition");
                if (!cid) cid = LoadAsset("/Game/Athena/Items/Cosmetics/Characters/CID_001_Athena_Character_M.CID_001_Athena_Character_M",
                                          "/Script/FortniteGame.AthenaCharacterItemDefinition");
                if (cid) {
                    Write<void*>(GetField(loadout, charProp), 0, cid);
                    LOG("[Cosmetics] default CID written into locker loadout");
                } else LOG("[Cosmetics] ⚠️ default CID asset not found");
            }
            // Mirror onto the pawn and fire the CLIENT-side evaluation.
            int pcSize = Read<int>(loadoutProp, profile::PropElementSize);
            void* pawnLoadoutProp = GetProp("/Script/FortniteGame.FortPlayerPawn.CosmeticLoadout");
            if (pawnLoadoutProp && Read<int>(pawnLoadoutProp, profile::PropElementSize) == pcSize) {
                memcpy(GetField(session.pawn, pawnLoadoutProp), loadout, (size_t)pcSize);
                // [F92] pass the receiver — enables the name-walk fallback
                // when StaticFindObject misses the UFunction (device-proven).
                Call ro("/Script/FortniteGame.FortPlayerPawn.OnRep_CosmeticLoadout", session.pawn);
                if (ro.ok()) { ro.run(session.pawn); LOG("[Cosmetics] ✅ pawn OnRep_CosmeticLoadout"); }
                else LOG("[Cosmetics] ⚠️ OnRep_CosmeticLoadout not present");
            }
        }
    }
    const char* partPaths[] = {
        "/Game/Athena/Heroes/Meshes/Heads/F_Med_Head1_ATH.F_Med_Head1_ATH",
        "/Game/Athena/Heroes/Meshes/Bodies/CP_001_Athena_Body.CP_001_Athena_Body",
    };
    void* loaded[2]{};
    for (int i = 0; i < 2; ++i) {
        void* asset = LoadAsset(partPaths[i], "/Script/FortniteGame.CustomCharacterPart");
        if (!asset) continue;
        void* partCls = FindObj("/Script/FortniteGame.CustomCharacterPart");
        if (!partCls || !IsA(asset, partCls)) continue;
        loaded[i] = asset;
        Call choose("/Script/FortniteGame.FortPlayerPawn.ServerChoosePart");
        if (choose.ok() && choose.set("Part", (uint8_t)i) && choose.set("ChosenCharacterPart", asset)) {
            choose.run(session.pawn);
            if (std::find(session.character_parts.begin(), session.character_parts.end(), asset) == session.character_parts.end())
                session.character_parts.push_back(asset);
        }
    }
    void* state = GetObjField(session.controller, "/Script/Engine.Controller.PlayerState");
    if (!state || IsBadPtr(state)) return;
    if (loaded[0] && loaded[1]) {
        Array partsArr{loaded, 2, 2};
        Call apply("/Script/FortniteGame.FortKismetLibrary.ApplyCharacterCosmetics");
        if (apply.ok() && apply.set("WorldContextObject", session.world) && apply.set("CharacterParts", partsArr) && apply.set("PlayerState", state)) {
            void* klib = FindObj("/Script/FortniteGame.Default__FortKismetLibrary");
            if (klib) apply.run(klib);
        }
    }
    // [F43] HeroType is NOT gated on the parts — it drives the pawn's
    // animation layers (root cause of the dead run/slide animation together
    // with the missing PlayerState).
    {
        void* hero = LoadAsset("/Game/Athena/Heroes/HID_001_Athena_Commando_F.HID_001_Athena_Commando_F", "/Script/FortniteGame.FortHeroType");
        if (!hero) hero = LoadAsset("/Game/Athena/Heroes/HID_001_Athena_Commando_M.HID_001_Athena_Commando_M", "/Script/FortniteGame.FortHeroType");
        if (!hero) hero = LoadAsset("/Game/Athena/Heroes/HID_Commando_Athena_01.HID_Commando_Athena_01", "/Script/FortniteGame.FortHeroType");
        void* heroTypeProp = GetProp("/Script/FortniteGame.FortPlayerState.HeroType");
        if (hero && heroTypeProp) Write<void*>(GetField(state, heroTypeProp), 0, hero);
    }
    // [F92] receivers passed — name-walk fallback covers StaticFindObject misses
    Call r1("/Script/FortniteGame.FortPlayerState.OnRep_HeroType", state); if (r1.ok()) r1.run(state);
    Call r3("/Script/FortniteGame.FortPlayerState.OnRep_CharacterData", state); if (r3.ok()) r3.run(state);
    // [F82] OnRep_CharacterParts exists client-side (verified) and drives the
    // state→pawn part evaluation — the old flow never called it.
    Call rcp("/Script/FortniteGame.FortPlayerState.OnRep_CharacterParts", state);
    if (rcp.ok()) rcp.run(state);
    void* klib = FindObj("/Script/FortniteGame.Default__FortKismetLibrary");
    if (klib) {
        Call upd("/Script/FortniteGame.FortKismetLibrary.UpdatePlayerCustomCharacterPartsVisualization");
        if (upd.ok() && upd.set("PlayerState", state)) upd.run(klib);
    }
    session.cosmetics_with_state = true;
    LOG("[Cosmetics] ✅ full flow with state done");
}
void Athena::ShowSkin() { LoadDefaultCosmetics(); }

void Athena::ForcePlayerVisible() {
    if (!session.pawn || IsBadPtr(session.pawn)) return;
    LOG("[Visibility] Forcing player body visible...");
    SetBool(session.pawn, "/Script/FortniteGame.FortPlayerPawn.bIsSkydiving", false);
    SetBool(session.pawn, "/Script/FortniteGame.FortPlayerPawn.bIsParachuteOpen", false);
    SetBool(session.pawn, "/Script/FortniteGame.FortPlayerPawn.bIsParachuteAvailable", true);
    Call sd("/Script/FortniteGame.FortPlayerPawn.OnRep_IsSkydiving"); if (sd.ok()) sd.run(session.pawn);
    Call po("/Script/FortniteGame.FortPlayerPawn.OnRep_IsParachuteOpen"); if (po.ok()) po.run(session.pawn);
    Call body("/Script/FortniteGame.FortPlayerPawnAthena.SetCharacterBodyVisibilityForPossession");
    if (body.ok()) { bool bShow = true; if (body.set("bVisible", bShow)) { body.run(session.pawn); bool ret = body.result<bool>(); LOG("[Visibility] SetCharacterBodyVisibilityForPossession -> %d", (int)ret); } }
    Call hid("/Script/Engine.Actor.SetActorHiddenInGame");
    if (hid.ok()) { bool bHidden = false; if (hid.set("bNewHidden", bHidden)) hid.run(session.pawn); }
    SetBool(session.pawn, "/Script/Engine.Actor.bHidden", false);
    LoadProfileCosmetics();
    session.visibility_forced = true;
}

// ============================================================================
// PLAYER INITIALIZATION / FINALIZATION
// ============================================================================
static void SetPawnHealth(float value) {
    if (!session.pawn) return;
    // [F23] SetMaxHealth/SetHealth take a single nameless float — positional
    // binding handles them.
    for (const char* method : {"/Script/FortniteGame.FortPawn.SetMaxHealth", "/Script/FortniteGame.FortPawn.SetHealth"}) {
        Call c(method);
        if (c.ok() && c.set_at(0, value)) c.run(session.pawn);
    }
}

bool Athena::TryInitializePlayer(void* world, void* mode, void* controller) {
    // [F15] Standalone gate.
    if (!WorldIsStandalone(world)) { LOG("[Init] World is not standalone — stopping setup"); session.attempted = true; return false; }
    void* pawnCls = LoadClass("/Game/Athena/PlayerPawn_Athena.PlayerPawn_Athena_C", "/Script/Engine.Pawn");
    if (!pawnCls) { LOG("[Init] PlayerPawn_Athena_C missing"); return false; }
    void* existing = GetPawn(controller);
    if (existing && !IsBadPtr(existing) && IsA(existing, pawnCls)) { session.pawn = existing; LOG("[Init] Using existing pawn"); return true; }
    Transform t{};
    t.Location[0] = 0.0; t.Location[1] = 0.0; t.Location[2] = 500.0;
    bool haveSpawn = false;
    Call choose("/Script/Engine.GameModeBase.ChoosePlayerStart");
    if (choose.ok() && choose.set("Player", controller) && choose.run(mode)) {
        void* start = choose.result<void*>();
        if (start && !IsBadPtr(start)) {
            Call loc("/Script/Engine.Actor.K2_GetActorLocation");
            if (loc.ok() && loc.run(start)) {
                Vector v = loc.result<Vector>();
                if (std::isfinite(v.X) && std::isfinite(v.Y) && std::isfinite(v.Z)) {
                    t.Location[0] = v.X; t.Location[1] = v.Y; t.Location[2] = v.Z + 100.0;
                    haveSpawn = true;
                    session.spawn_loc = Vector{v.X, v.Y, v.Z};   // [G24] void-guard fallback target
                    session.has_spawn_loc = true;
                    LOG("[Init] PlayerStart at (%.0f,%.0f,%.0f)", v.X, v.Y, v.Z);
                }
            }
        }
    }
    if (!haveSpawn) { t.Location[0] = 0.0; t.Location[1] = 0.0; t.Location[2] = 3100.0; }
    session.pawn = SpawnActor(world, pawnCls, controller, t);
    if (!session.pawn) { LOG("[Init] Spawn failed"); return false; }
    LOG("[Init] Spawned: %p", session.pawn);
    SetBool(session.pawn, "/Script/FortniteGame.FortPlayerPawn.bIsSkydiving", false);
    SetBool(session.pawn, "/Script/FortniteGame.FortPlayerPawn.bIsParachuteOpen", false);
    Possess(controller, session.pawn);
    if (GetPawn(controller) != session.pawn) { LOG("[Init] Possession did not stick"); return false; }
    LOG("[Init] Possessed OK");
    SetupCheats();
    // [F23] Full health on spawn.
    SetPawnHealth(100.0f);
    Call sd("/Script/FortniteGame.FortPlayerPawn.OnRep_IsSkydiving"); if (sd.ok()) sd.run(session.pawn);
    Call po("/Script/FortniteGame.FortPlayerPawn.OnRep_IsParachuteOpen"); if (po.ok()) po.run(session.pawn);
    Call restart("/Script/Engine.PlayerController.ClientRestart");
    if (restart.ok() && restart.set("NewPawn", session.pawn)) restart.run(controller);
    ForcePlayerVisible();
    return true;
}

// [F43] Erbium 1:1 standalone PlayerState (its CreatePlayer fallback path):
// the pawn was spawned and possessed WITHOUT a FortPlayerStateAthena, so the
// cosmetic/animation stack had no PlayerState to read — locomotion/slide
// animation stayed dead while movement itself worked. Erbium spawns the
// state, wires Controller+Pawn, fires both OnReps, sets owner and HeroType.
void Athena::EnsurePlayerState() {
    if (!session.controller || !session.world) return;
    void* state = GetObjField(session.controller, "/Script/Engine.Controller.PlayerState");
    if (!state || IsBadPtr(state)) {
        void* psCls = FindObj("/Script/FortniteGame.FortPlayerStateAthena");
        if (!psCls) psCls = FindObj("/Script/FortniteGame.FortPlayerState");
        if (!psCls) { LOG("[PS] PlayerState class missing"); return; }
        Transform t{};
        t.Location[0] = 0.0; t.Location[1] = 0.0; t.Location[2] = 5000.0;
        state = SpawnActor(session.world, psCls, session.controller, t);
        LOG("[PS] Spawned FortPlayerStateAthena: %p", state);
    }
    if (!state || IsBadPtr(state)) { LOG("[PS] no PlayerState available"); return; }
    session.player_state = state;

    void* pcPsProp = GetProp("/Script/Engine.Controller.PlayerState");
    if (pcPsProp) Write<void*>(GetField(session.controller, pcPsProp), 0, state);
    if (session.pawn) {
        void* pawnPsProp = GetProp("/Script/Engine.Pawn.PlayerState");
        if (pawnPsProp) Write<void*>(GetField(session.pawn, pawnPsProp), 0, state);
    }
    { Call so("/Script/Engine.Actor.SetOwner");
      if (so.ok() && (so.set("NewOwner", session.controller) || so.set_at(0, session.controller))) so.run(state); }

    // Erbium: PlayerController->OnRep_PlayerState(); Pawn->OnRep_PlayerState();
    { Call r("/Script/Engine.Controller.OnRep_PlayerState"); if (r.ok()) r.run(session.controller); }
    { Call r("/Script/Engine.Pawn.OnRep_PlayerState"); if (r.ok() && session.pawn) r.run(session.pawn); }

    // [G17-b] Erbium team parity (its CreatePlayer flow): TeamIndex = PickTeam
    // (solo first team = 3), SquadId = TeamIndex - 3. Without a team the
    // WinningTeam write in the victory flow can never match the player, and
    // squad HUD widgets stay empty.
    {
        void* tiProp = GetProp("/Script/FortniteGame.FortPlayerStateAthena.TeamIndex");
        void* sqProp = GetProp("/Script/FortniteGame.FortPlayerStateAthena.SquadId");
        if (tiProp) {
            int sz = Read<int>(tiProp, profile::PropElementSize);
            if (sz == 1) Write<uint8_t>(GetField(state, tiProp), 0, 3);
            else if (sz == 4) Write<int>(GetField(state, tiProp), 0, 3);
        }
        if (sqProp) {
            int sz = Read<int>(sqProp, profile::PropElementSize);
            if (sz == 1) Write<uint8_t>(GetField(state, sqProp), 0, 0);
            else if (sz == 4) Write<int>(GetField(state, sqProp), 0, 0);
        }
        { Call r("/Script/FortniteGame.FortPlayerStateAthena.OnRep_TeamIndex"); if (r.ok()) r.run(state); }
        { Call r("/Script/FortniteGame.FortPlayerStateAthena.OnRep_SquadId"); if (r.ok()) r.run(state); }
    }

    // HeroType fallback exactly like Erbium's bot creation.
    void* heroProp = GetProp("/Script/FortniteGame.FortPlayerState.HeroType");
    if (heroProp && !Read<void*>(GetField(state, heroProp))) {
        void* hero = LoadAsset("/Game/Athena/Heroes/HID_001_Athena_Commando_F.HID_001_Athena_Commando_F", "/Script/FortniteGame.FortHeroType");
        if (!hero) hero = LoadAsset("/Game/Athena/Heroes/HID_Commando_Athena_01.HID_Commando_Athena_01", "/Script/FortniteGame.FortHeroType");
        if (hero) Write<void*>(GetField(state, heroProp), 0, hero);
    }
    { Call r("/Script/FortniteGame.FortPlayerState.OnRep_HeroType"); if (r.ok()) r.run(state); }
    { Call r("/Script/FortniteGame.FortPlayerState.OnRep_CharacterData"); if (r.ok()) r.run(state); }
    LOG("[PS] ✅ PlayerState ready: %p", state);
}

void Athena::SetupWorld(void* world) {
    session.world = world;
    session.controller = GetPC(world, 0);
    if (!session.controller) { LOG("SetupWorld: no controller"); return; }
    void* baseCtrl = FindObj("/Script/FortniteGame.FortPlayerController");
    if (!baseCtrl || !IsA(session.controller, baseCtrl)) { LOG("SetupWorld: not FortPlayerController"); return; }
    void* modeProp  = GetProp("/Script/Engine.World.AuthorityGameMode");
    void* stateProp = GetProp("/Script/Engine.World.GameState");
    session.mode  = modeProp  ? Read<void*>(GetField(world, modeProp))  : nullptr;
    session.state = stateProp ? Read<void*>(GetField(world, stateProp)) : nullptr;
    if (!session.mode || !session.state) { LOG("SetupWorld: mode/state missing"); return; }
    LOG("SetupWorld: PC=%s GM=%s GS=%s", ObjName(session.controller).c_str(), ObjName(session.mode).c_str(), ObjName(session.state).c_str());
    if (!TryInitializePlayer(world, session.mode, session.controller)) { LOG("SetupWorld: prerequisites not ready"); return; }
    LOG("SetupWorld: EnsurePlayerState"); EnsurePlayerState();   // [F43] fixes dead anim + cosmetics
    LOG("SetupWorld: GrantAllAbilitySets"); GrantAllAbilitySets();
    LOG("SetupWorld: SetupTacticalSprint"); SetupTacticalSprint();
    LOG("SetupWorld: EnableMovementFeatures"); EnableMovementFeatures();
    LOG("SetupWorld: ShowSkin"); ShowSkin();
    // [G19-d] DestroyLods DISABLED — FortHLODSMActor is the CLIENT's distant
    // POI stand-in mesh; deleting it (a server-side Erbium memory habit)
    // punched visible holes into cities whose real sublevels had not streamed
    // in. The HLODs stay; ForceSublevels now drives the real geometry.
    LOG("SetupWorld: ForceSublevels (DestroyLods disabled — G19-d)"); ForceSublevels();
    LOG("SetupWorld: InitializeInventory"); InitializeInventory();
    LOG("SetupWorld: ResetStaleContainers"); ResetStaleContainers();   // [F34]
    LOG("SetupWorld: SpawnFloorLoot"); SpawnFloorLoot();
    LOG("SetupWorld: SpawnVehicles"); SpawnVehicles();                 // [F28]
    LOG("SetupWorld: ApplySprintSpeed"); ApplySprintSpeed();
    LOG("SetupWorld: SpawnSafeZoneIndicator"); SpawnSafeZoneIndicator();
    // [F38] SpawnBuildPreviews removed — the hidden hand-rolled preview actors
    // collided with the native preview system when build mode ended (exit
    // crash) and were never used by the placement path.
    LOG("SetupWorld: DropLoadingScreen"); DropLoadingScreen();
    LOG("SetupWorld: SpawnBattleBus"); SpawnBattleBus();
    LOG("SetupWorld: RemoveNetDebugUI"); RemoveNetDebugUI();
    LOG("SetupWorld: FixMeshNetwork"); FixMeshNetwork();            // [G17-a/G19-d] Erbium NodeType=2 — missing cities
    LOG("SetupWorld: StreamPlaylistLevels"); StreamPlaylistLevels(); // [G17-a] Erbium additional playlist levels
    // [F23] Free building + direct readiness bit (standalone-safe fallbacks).
    SetBool(session.controller, "/Script/FortniteGame.FortPlayerController.bBuildFree", true);
    { uint8_t* ready = (uint8_t*)session.controller + profile::FortPlayerController_ReadyByte;
      *ready |= (uint8_t)(1u << profile::FortPlayerController_ReadyBit); }
    RefreshHUD();
    session.finalize_at = Monotonic() + 2.0;
    session.attempted   = true;
    session.initialized = true;
    LOG("SetupWorld: initial pass done");
}

void Athena::FinalizePlayer() {
    if (session.finalized) return;
    session.finalized = true;
    LOG("=== FinalizePlayer ===");
    void* pc = GetPC(session.world, 0);
    if (pc != session.controller) { LOG("Finalize: controller changed"); return; }
    void* pawnCls = FindObj("/Script/FortniteGame.FortPlayerPawnAthena");
    void* pawn = GetPawn(pc);
    if (!pawn || !pawnCls || !IsA(pawn, pawnCls)) { LOG("Finalize: pawn is not FortPlayerPawnAthena"); return; }
    session.pawn = pawn;
    if (!session.player_state || IsBadPtr(session.player_state)) EnsurePlayerState();   // [F43]
    // [F82] anim restoration: at SetupWorld time the PlayerState did not exist
    // yet, so LoadDefaultCosmetics early-returned (log proof: cosmetics line
    // printed BEFORE 'SetupWorld: EnsurePlayerState'). Re-run the full flow
    // now that the state is guaranteed.
    if (!session.cosmetics_with_state) { LOG("Finalize: cosmetics with state"); LoadDefaultCosmetics(); }
    // [G12] The MCP profile lands asynchronously — the locker is often still
    // empty at SetupWorld time (device log: "character= pickaxe="). One
    // re-sync here catches the loaded profile so the pickaxe resolves.
    if (!session.locker_pickaxe && !session.locker_resynced) {
        session.locker_resynced = true;
        LOG("Finalize: locker re-sync (MCP profile may have landed)");
        SyncLockerLoadout();
    }
    // Readiness can change possession — refresh cached pawn-dependent state.
    Call ps("/Script/Engine.Pawn.OnRep_PlayerState", pawn); if (ps.ok()) ps.run(pawn);
    if (!session.abilities_granted) GrantAllAbilitySets();
    if (!session.tactical_sprint_setup) SetupTacticalSprint();
    if (!session.movement_enabled) EnableMovementFeatures();
    FixBodyAndAnimations();        // [F41/F83-b] restore the locomotion AnimInstance
    GrantWorldAbilities();         // [F52/F83-b] slide + vehicle abilities as real specs
    if (!session.quickbars_initialized) {
        InitializeQuickBarsNative();
        if (!session.quickbars_initialized) BuildQuickBarsManually();
        SetQuickBarReady();
        ForceUpdateQuickBars();
        ActivateQuickBarSlot(0, 0);
    }
    VerifyHUDReadPath();   // [F92] once per minute max — names the exact broken HUD link if any
    if (!session.ready_called) {
        // [F89] commit only when the readiness RPC actually dispatched —
        // an unconditional commit here froze failed readiness forever
        // (same one-shot-commit family as F85/F88); the loading-screen
        // drop path retries until it lands.
        Call srt("/Script/FortniteGame.FortPlayerController.ServerReadyToStartMatch");
        bool srtOk = srt.ok() && srt.run(pc);
        Call cps("/Script/FortniteGame.FortPlayerControllerZone.ClientOnPawnSpawned"); if (cps.ok()) cps.run(pc);
        if (srtOk) session.ready_called = true;
    }
    Call restart("/Script/Engine.PlayerController.ClientRestart");
    if (restart.ok() && restart.set("NewPawn", pawn)) restart.run(pc);
    Call cam("/Script/Engine.PlayerController.SetViewTargetWithBlend");
    if (cam.ok() && cam.set("NewViewTarget", pawn)) cam.run(pc);
    session.visibility_forced = false;
    ForcePlayerVisible();
    SetPawnHealth(100.0f);
    SetupMatchState();   // [F40] AlivePlayers + PlayersLeft HUD state
    RefreshHUD();
    LOG("Finalize: complete");
}

void Athena::Tick() {
    if (!session.world) return;
    double now = Monotonic();
    if (!session.attempted) {
        if (now - session.retry_at < 1.0) return;
        session.retry_at = now;
        SetupWorld(session.world);
        return;
    }
    if (session.finalize_at > 0 && !session.finalized && now >= session.finalize_at) { FinalizePlayer(); return; }
    static double lastVisibility = 0;
    static double lastSpeedReapply = 0;
    static double lastAbilityCheck = 0;
    static double lastSprintCheck = 0;
    static double lastQBCheck = 0;
    static double lastMovementCheck = 0;
    static double lastAnimCheck = 0;      // [F83-b] body/anim + world ability retries
    static double lastCosmeticsCheck = 0;  // [F90-c/AUD-1] cosmetics with-state retry
    if (session.finalized && now - lastVisibility > 8.0) { lastVisibility = now; if (!session.visibility_forced) ForcePlayerVisible(); }
    if (session.finalized && session.pawn && now - lastSpeedReapply > 2.0) { lastSpeedReapply = now; ApplySprintSpeed(); }
    // [F92] 30s heartbeat — the whole state machine in one line so ANY
    // partial device-log fragment always carries the state picture.
    static double lastHB = 0;
    if (session.finalized && now - lastHB > 30.0) {
        lastHB = now;
        uint8_t hbGate = session.controller ? Read<uint8_t>(session.controller, profile::Controller_QuickBarReadyByte) : 0;
        void* qbLink = session.controller ? Read<void*>(session.controller, profile::Controller_ClientQuickBars) : nullptr;
        int hbP = -1, hbS = -1;
        if (session.quickbars && !IsBadPtr(session.quickbars)) {
            Array p = Read<Array>((char*)session.quickbars + profile::QuickBars_PrimaryOffset, profile::QuickBar_SlotsOffset);
            Array s = Read<Array>((char*)session.quickbars + profile::QuickBars_SecondaryOffset, profile::QuickBar_SlotsOffset);
            hbP = p.Count; hbS = s.Count;
        }
        int hbSpecs = gas::CountActivatableSpecs();
        float hbHp = session.pawn ? PawnHealth(session.pawn) : -1.0f;
        LOG("[HB] qb_init=%d qb_link=%s gate=%u bars=%d/%d loot_tables=%d veh=%d match=%d cosmetics=%d body=%d wabilities=%d specs=%d gas(init=%d gaa=%d ita=%d acc=%d mcp=%d) hp=%.0f alive=%d ever=%d qbok=%d death=%d won=%d meshnet=%d plvl=%d voids=%d src=%d wpp=%d",
            (int)session.quickbars_initialized,
            (qbLink == session.quickbars ? (session.quickbars ? "ok" : "n/a") : "BAD"),
            (unsigned)hbGate, hbP, hbS,
            local_loot::TableCount(),
            (int)session.vehicles_spawned,
            (int)session.match_state_ready,
            (int)session.cosmetics_with_state,
            (int)session.body_fixed,
            (int)session.world_abilities_granted,
            hbSpecs,
            gas::InitializePlayerGameplayAbilities ? 1 : 0,
            gas::GiveAbilityAndActivateOnce ? 1 : 0,
            gas::InternalTryActivateAbility ? 1 : 0,
            gas::ApplyCharacterCustomization ? 1 : 0,
            (gas::SendRequestNow && orig_SendRequestNow) ? 1 : 0,   // [G10]
            hbHp,
            session.real_alive,
            session.players_ever,
            (int)session.qb_slots_verified,
            (int)session.death_handled,
            (int)session.win_handled,
            (int)session.mesh_network_fixed,
            (int)session.playlist_levels_done,
            session.void_rescues,          // [G24] void-guard rescues
            session.src_force_passes,      // [G23-e] streaming-source passes
            session.wp_diag_passes);       // [G22] WP diagnostic passes
    }
    // [F94-a] reviewer note (1): state can change after FinalizePlayer — the
    // internal 60s rate-limit makes this call free next to the heartbeat.
    VerifyHUDReadPath();
    if (session.finalized && session.pawn && now - lastAbilityCheck > 10.0) {
        lastAbilityCheck = now;
        if (!session.abilities_granted) { GrantAllAbilitySets(); RefreshHUD(); }
    }
    if (session.finalized && session.controller && now - lastSprintCheck > 5.0) {
        lastSprintCheck = now;
        if (!session.tactical_sprint_setup || !session.tactical_sprint) { session.tactical_sprint_setup = false; SetupTacticalSprint(); }
    }
    if (session.finalized && session.controller && now - lastQBCheck > 6.0) {
        lastQBCheck = now;
        if (!session.quickbars_initialized && session.qb_retries++ < 5) {
            LOG("Tick: QuickBars retry #%d", session.qb_retries);
            InitializeQuickBarsNative();
            if (!session.quickbars_initialized) BuildQuickBarsManually();
            if (session.quickbars_initialized) SetQuickBarReady();
        }
    }
    // [F88] match-state retry — mode/state may replicate late; re-invoke
    // until SetupMatchState fully covers AlivePlayers + PlayersLeft.
    static double lastMatchState = 0;
    if (session.finalized && session.controller && now - lastMatchState > 5.0) {
        lastMatchState = now;
        if (!session.match_state_ready) SetupMatchState();
    }
    if (session.finalized && session.pawn && now - lastMovementCheck > 8.0) {
        lastMovementCheck = now;
        if (!session.movement_enabled) EnableMovementFeatures();
    }
    if (session.finalized && session.pawn && now - lastAnimCheck > 8.0) {
        lastAnimCheck = now;
        FixBodyAndAnimations();       // [F83-b] retries until the AnimBP streams in
        GrantWorldAbilities();        // [F52] retries until the core ability pair streams in
        PumpPendingAbilitySets();     // [G14-e] AS_HillScramble/AS_SlideImpulse late-stream retry
    }
    if (session.finalized && now - lastCosmeticsCheck > 8.0) {   // [F90-c/AUD-1]
        lastCosmeticsCheck = now;
        if (!session.cosmetics_with_state) LoadDefaultCosmetics();   // re-runs the full with-state flow once PlayerState replicates
    }
    if (session.finalized) {
        PollTouchBuild();
        TickVehicleSeats();
        ProcessPendingPickups();   // [F35] fly-time → inventory transfer
        TickMantleAssist();        // [F45] 21.30 wall climb assist
        TickRealityProximity();    // [F44] reality tree proximity effect
        TickSprintDoorAssist();    // [G16-a] safe bash-through (open-only)
        TickBuildingLoads();       // [F-cities] drive LoadBuildings over a streaming window
        TickStreamingSources();    // [G23-e] pawn/controller as WP streaming sources (own ticker)
        TickVoidGuard();           // [G24] fall-through rescue (below-map teleport)
        // [G22] World Partition diagnostics — 3 passes (first ~5 s after
        // finalize, then 45 s apart): what loaded, whether a WorldPartition
        // object exists, data-layer states + activation, streaming-source fix.
        {
            static double lastWpDiag = 0.0;
            if (session.wp_diag_passes < 3 && now - lastWpDiag > (session.wp_diag_passes == 0 ? 5.0 : 45.0)) {
                lastWpDiag = now;
                DiagnoseWorldPartition();
            }
        }
        TickFloorLootStreaming();  // [F-loot-auto] roll floor loot as spawners stream in
        TickMeshNetwork();         // [G17-a] NodeType retry window (subsystem streams in late)
        TickDeathWatch();          // [G17-c] health → 0 edge runs the server death flow
        TickRealAlive();             // [G18-a] REAL alive recount + Erbium win rules
        TickQuickBarWatch();         // [G18-b] self-healing quickbar slot verification
        // [G19-a] post-death staleness: once the engine unpossesses/destroys
        // the pawn, every cached session.pawn user becomes a use-after-free
        // risk. Clear it once, loudly, so the later ticks null-check out.
        if (session.death_handled && !session.pawn_cleared && session.pawn) {
            if (!PawnStillValid(session.pawn)) {
                session.pawn_cleared = true;
                LOG("[Death] pawn unpossessed/destroyed — clearing cached pawn (use-after-free guard)");
                session.pawn = nullptr;
            }
        }
    }
    // [F28/F41] one-shot vehicle spawning with a retry window (vehicle spawners
    // stream in after the world is ready — 12 attempts × 15 s ≈ 3 min max).
    static double lastVehicleTry = 0;
    if (session.finalized && session.world && now - lastVehicleTry > 15.0) {
        lastVehicleTry = now;
        if (!session.vehicles_spawned && session.vehicle_attempts < 12) SpawnVehicles();
    }
    // [G19-d] sublevel force retry — streaming levels appear over the first
    // minutes of play as the world settles; re-force until the set stabilises
    // (6 passes × 30 s). Empty (World Partition) worlds stop after pass 1.
    static double lastSublevelTry = 0;
    if (session.finalized && session.world && now - lastSublevelTry > 30.0) {
        lastSublevelTry = now;
        if (session.sublevel_passes < 6) { session.sublevel_passes++; ForceSublevels(); }
    }
}

void Athena::DestroyLods() {
    void* cm = GetObjField(session.controller, "/Script/Engine.PlayerController.CheatManager");
    if (!cm) { SetupCheats(); cm = GetObjField(session.controller, "/Script/Engine.PlayerController.CheatManager"); }
    if (!cm) return;
    void* cls = FindObj("/Script/FortniteGame.FortHLODSMActor");
    if (!cls) return;
    Call c("/Script/Engine.CheatManager.DestroyAll");
    if (c.ok() && c.set("aClass", cls)) c.run(cm);
    LOG("DestroyLods: done");
}

// ============================================================================
// [G19-d] SUBLEVEL GEOMETRY — "some cities/buildings never spawn, I fell
// through a hole where the building should be". Streaming sublevels normally
// load from the SERVER's streaming decisions; standalone they never do, so
// every POI that lives in its own sublevel is a hole in the collision. Force
// each ULevelStreaming in the world to loaded+visible — the engine's own
// UpdateLevelStreaming pass applies the flags asynchronously (memory-safe
// pacing) on its next ticks.
// ============================================================================
void Athena::ForceSublevels() {
    if (!session.world || IsBadPtr(session.world)) return;
    void* levelsProp = GetProp("/Script/Engine.World.StreamingLevels");
    if (!levelsProp) {
        LOG("[Levels] ⚠️ World.StreamingLevels not reflected on this build");
        return;
    }
    Array levels = Read<Array>(GetField(session.world, levelsProp));
    if (!levels.Data || levels.Count <= 0 || levels.Count > 4096 || IsBadPtr(levels.Data)) {
        // [G23-d] NOT an error and NOT permanent: on this build the array is
        // empty at SetupWorld time (~15 s) and fills to ~993 WP cell streamers
        // by ~30 s — the old "sublevel_passes = 100" early-quit was a bug
        // (round-10 log: "StreamingLevels empty" then "[WP] StreamingLevels:
        // 993"). Keep retrying inside the caller's pass budget.
        LOG("[Levels] StreamingLevels empty on pass %d — WP cells may still be registering, will re-check",
            session.sublevel_passes);
        return;
    }
    if (levels.Count > 128) {
        // [G23-d] WORLD PARTITION MAP — NEVER force bShouldBeLoaded on the
        // whole cell set (the engine would load the entire map → OOM kill).
        // Force VISIBILITY ONLY on streamers that already hold a loaded
        // level: WP can leave cells loaded-but-invisible, which is exactly
        // the "holes where buildings should be" symptom. The WP runtime
        // keeps managing loading; newly loaded cells get their visibility
        // forced on the next pass (30 s apart).
        int visForced = 0, alreadyVis = 0, loaded = 0;
        for (int i = 0; i < levels.Count; ++i) {
            void* ls = Read<void*>(levels.Data, (size_t)i * 8);
            if (!ls || IsBadPtr(ls) || !IsReadable(ls, 0x10)) continue;
            void* lv = GetObjField(ls, "/Script/Engine.LevelStreaming.LoadedLevel");
            if (!lv || IsBadPtr(lv)) continue;                    // not loaded — leave alone
            ++loaded;
            if (GetBool(ls, "/Script/Engine.LevelStreaming.bShouldBeVisible")) { ++alreadyVis; continue; }
            SetBool(ls, "/Script/Engine.LevelStreaming.bShouldBeVisible", true);
            ++visForced;
        }
        LOG("[WP] partition map: %d streamers, %d hold loaded levels — visibility forced on %d (%d already visible, pass %d) [G23-d]",
            (int)levels.Count, loaded, visForced, alreadyVis, session.sublevel_passes);
        return;
    }
    int changed = 0, already = 0;
    for (int i = 0; i < levels.Count; ++i) {
        void* ls = Read<void*>(levels.Data, (size_t)i * 8);
        if (!ls || IsBadPtr(ls)) continue;
        bool wasLoaded  = GetBool(ls, "/Script/Engine.LevelStreaming.bShouldBeLoaded");
        bool wasVisible = GetBool(ls, "/Script/Engine.LevelStreaming.bShouldBeVisible");
        if (wasLoaded && wasVisible) { ++already; continue; }
        SetBool(ls, "/Script/Engine.LevelStreaming.bShouldBeLoaded", true);
        SetBool(ls, "/Script/Engine.LevelStreaming.bShouldBeVisible", true);
        ++changed;
    }
    LOG("[Levels] sublevels: %d total, %d forced loaded+visible, %d already on (pass %d)",
        (int)levels.Count, changed, already, session.sublevel_passes);
}

void Athena::DropLoadingScreen() {
    if (!session.state || !session.controller) return;
    void* gp = GetProp("/Script/FortniteGame.FortGameStateAthena.GamePhase");
    if (gp && Read<int>(gp, profile::PropElementSize) == 1) {
        uint8_t old = Read<uint8_t>(GetField(session.state, gp));
        if (old < 2) {
            uint8_t phase = 2;
            Write<uint8_t>(GetField(session.state, gp), 0, phase);
            Call n("/Script/FortniteGame.FortGameStateAthena.OnRep_GamePhase");
            if (n.ok() && n.set("OldGamePhase", old)) n.run(session.state);
        }
    }
    SetBool(session.controller, "/Script/FortniteGame.FortPlayerController.bHasServerFinishedLoading", true);
    SetBool(session.controller, "/Script/FortniteGame.FortPlayerController.bHasClientFinishedLoading", true);
    void* playlist = FindObj("/Game/Athena/Playlists/Playlist_DefaultSolo.Playlist_DefaultSolo");
    if (!playlist) playlist = FindObj("/Game/Athena/Playlists/Playground/Playlist_Playground.Playlist_Playground");
    if (playlist) {
        void* infoProp = GetProp("/Script/FortniteGame.FortGameStateAthena.CurrentPlaylistInfo");
        void* bp = GetProp("/Script/FortniteGame.PlaylistPropertyArray.BasePlaylist");
        void* op = GetProp("/Script/FortniteGame.PlaylistPropertyArray.OverridePlaylist");
        if (infoProp && bp) Write<void*>(GetField(GetField(session.state, infoProp), bp), 0, playlist);
        if (infoProp && op) Write<void*>(GetField(GetField(session.state, infoProp), op), 0, playlist);
        Call a("/Script/FortniteGame.FortGameStateAthena.OnRep_CurrentPlaylistInfo"); if (a.ok()) a.run(session.state);
    }
    if (!session.ready_called) {
        // [F89] same conditional commit — see FinalizePlayer site
        Call srt("/Script/FortniteGame.FortPlayerController.ServerReadyToStartMatch");
        bool srtOk = srt.ok() && srt.run(session.controller);
        Call cps("/Script/FortniteGame.FortPlayerControllerZone.ClientOnPawnSpawned"); if (cps.ok()) cps.run(session.controller);
        if (srtOk) session.ready_called = true;
    }
    LOG("Loading screen dropped");
}

void Athena::InventoryUpdate() {
    if (session.inventory) { Call a("/Script/FortniteGame.FortInventory.HandleInventoryLocalUpdate"); if (a.ok()) a.run(session.inventory); }
    { Call b("/Script/FortniteGame.FortPlayerController.HandleWorldInventoryLocalUpdate"); if (b.ok()) b.run(session.controller); }
    { Call f("/Script/FortniteGame.FortPlayerController.ClientForceWorldInventoryUpdate"); if (f.ok()) f.run(session.controller); }
    // [F12] FortQuickBars OnRep_* and PC.OnRep_QuickBar do not exist in 21.30;
    // the ClientForceUpdateQuickbar calls below are the supported refresh path.
    for (uint8_t bar : {uint8_t(0), uint8_t(1)}) {
        Call c("/Script/FortniteGame.FortPlayerController.ClientForceUpdateQuickbar");
        if (c.ok() && c.set("QuickbarToRefresh", bar)) c.run(session.controller);
    }
}

// ============================================================================
// INVENTORY — [F17/F18/F19] native GUID + reconciliation + stacking + 2 bars
// ============================================================================
static void* ItemDefinitionOf(void* item) {
    Call c("/Script/FortniteGame.FortItem.GetItemDefinitionBP");
    return c.ok() && item && c.run(item) ? c.result<void*>() : nullptr;
}
static Guid ItemGuidOf(void* item) {
    Call c("/Script/FortniteGame.FortItem.GetItemGuid");
    return c.ok() && item && c.run(item) ? c.result<Guid>() : Guid{};
}

// [F17] Era-core give(): create the temporary instance, assign a native GUID
// BEFORE the replicated-entry copy, insert both arrays, then verify the
// surviving instance by GUID after a local inventory refresh.
bool Athena::GiveItem(void* def, int count, int slot, uint8_t bar, bool require_new, int loadedAmmo, int level) {
    if (!def || !session.inventory || count < 1 || count > 9999) return false;
    if (!IsA(def, FindObj("/Script/FortniteGame.FortItemDefinition"))) return false;
    void* invProp   = GetProp("/Script/FortniteGame.FortInventory.Inventory");
    void* instProp  = GetProp("/Script/FortniteGame.FortItemList.ItemInstances");
    void* entryProp = GetProp("/Script/FortniteGame.FortItemList.ReplicatedEntries");
    void* itemEntryProp = GetProp("/Script/FortniteGame.FortWorldItem.ItemEntry");
    if (!invProp || !instProp || !entryProp || !itemEntryProp) return false;
    void* list = GetField(session.inventory, invProp);
    void* instArr  = GetField(list, instProp);
    void* entryArr = GetField(list, entryProp);
    Array current = Read<Array>(instArr);
    Array replicated = Read<Array>(entryArr);
    if (current.Count < 0 || current.Count > 512 || current.Count > current.Max ||
        replicated.Count < 0 || replicated.Count > 512 || replicated.Count > replicated.Max) return false;

    // Duplicate protection — callers can give a different item instead.
    for (int i = 0; i < current.Count; ++i) {
        void* existing = Read<void*>(current.Data, (size_t)i * 8);
        if (existing && ItemDefinitionOf(existing) == def) {
            if (require_new) return false;
            if (slot >= 0 && slot < 6) (bar == 0 ? session.slots : session.secondary_slots)[(size_t)slot] = existing;
            LOG("Already present: %s", ObjName(def).c_str());
            return true;
        }
    }

    // CreateTemporaryItemInstanceBP(Count, Level) — single call, capture result.
    void* inst = nullptr;
    {
        Call c("/Script/FortniteGame.FortItemDefinition.CreateTemporaryItemInstanceBP");
        int lvl = level > 0 ? level : 1;   // [F31] loot level carry-over
        if (c.ok() && c.set("Count", count) && c.set("Level", lvl) && c.run(def)) inst = c.result<void*>();
    }
    if (!inst || !IsA(inst, FindObj("/Script/FortniteGame.FortWorldItem"))) return false;
    { Call c("/Script/FortniteGame.FortItem.SetOwningControllerForTemporaryItem");
      if (!c.ok() || !c.set("InController", session.controller) || !c.run(inst)) return false; }

    void* entrySrc = GetField(inst, itemEntryProp);
    if (!entrySrc) return false;
    void* guidProp = GetProp("/Script/FortniteGame.FortItemEntry.ItemGuid");
    if (!guidProp || Read<int>(guidProp, profile::PropElementSize) != sizeof(Guid)) return false;

    // [F36] Erbium GiveItem 1:1: OwnerInventory + ParentInventory bind the
    // instance to this inventory — without them the inventory UI shows the
    // item as orphaned and stacking/search miss it.
    {
        void* oiProp = GetProp("/Script/FortniteGame.FortWorldItem.OwnerInventory");
        if (oiProp) Write<void*>(GetField(inst, oiProp), 0, session.inventory);
        void* piProp = GetProp("/Script/FortniteGame.FortItemEntry.ParentInventory");
        if (piProp) Write<void*>(GetField(entrySrc, piProp), 0, session.inventory);
    }

    // Assign the native GUID before the deep copy so the replicated entry
    // and the instance share identity (Era core ordering). TrackerGuid is
    // zero-filled too (Erbium parity) — some equips read it.
    Guid guid = MakeGuid();
    if (!(guid.A | guid.B | guid.C | guid.D)) return false;
    Write<Guid>(GetField(entrySrc, guidProp), 0, guid);
    {
        void* trackerProp = GetProp("/Script/FortniteGame.FortItemEntry.TrackerGuid");
        if (trackerProp) {
            Guid t = Read<Guid>(GetField(entrySrc, trackerProp));
            if (!(t.A | t.B | t.C | t.D)) Write<Guid>(GetField(entrySrc, trackerProp), 0, MakeGuid());
        }
    }
    Guid reported = ItemGuidOf(inst);
    if (memcmp(&reported, &guid, sizeof(guid)) != 0) {
        LOG("Temporary item GUID accessor mismatch; insertion refused");
        return false;
    }

    Write<float>(entrySrc, profile::ItemEntry_Durability, 1.0f);
    if (IsA(def, FindObj("/Script/FortniteGame.FortWeaponItemDefinition"))) {
        // [F31] real clip size from the WeaponStatHandle row (Era GetStats
        // parity), or the pickup entry's own LoadedAmmo when carried over.
        int ammo = loadedAmmo >= 0 ? loadedAmmo : ClipSizeOf(def);
        Write<int32>(entrySrc, profile::ItemEntry_LoadedAmmo, ammo);
        if (GetBool(def, "/Script/FortniteGame.FortWeaponItemDefinition.bUsesPhantomReserveAmmo"))
            Write<int32>(entrySrc, profile::ItemEntry_PhantomAmmo, 300);
    }
    if (level > 1) {
        void* lvlProp = GetProp("/Script/FortniteGame.FortItemEntry.Level");
        if (lvlProp && Read<int>(lvlProp, profile::PropElementSize) == 4)
            Write<int32>(GetField(entrySrc, lvlProp), 0, level);
    }

    auto add = Fn<int(*)(void*,void*,const void*)>(profile::GenericArrayAdd);
    int idx = add(instArr, instProp, &inst);
    if (idx < 0) return false;
    int entryIdx = add(entryArr, entryProp, entrySrc);
    if (entryIdx < 0) { LOG("Entry copy failed after instance insertion; stop granting items"); return false; }
    // [F36] Erbium parity: flag the replicated copy + mark dirty so the
    // inventory UI and the net layer treat the new entry as authoritative.
    {
        void* brcProp = GetProp("/Script/FortniteGame.FortItemEntry.bIsReplicatedCopy");
        if (brcProp && Read<int>(brcProp, profile::PropElementSize) == 1) {
            void* list2 = GetField(session.inventory, invProp);
            Array reps = Read<Array>(GetField(list2, entryProp));
            if (entryIdx < reps.Count && reps.Data) {
                int stride = Read<int>(entryProp, profile::PropElementSize);
                int off = Read<int>(brcProp, profile::PropOffset);
                if (stride > 0 && off >= 0 && off < stride)
                    Write<uint8_t>((char*)reps.Data + (size_t)entryIdx * stride, off, 1);
            }
        }
    }

    InventoryUpdate();

    // Reconciliation: the refresh may rebuild cached state; resolve the
    // canonical instance by GUID and refuse silent loss (Era core).
    void* canonical = nullptr;
    Array after = Read<Array>(instArr);
    if (after.Count >= 0 && after.Count <= 512 && after.Data)
        for (int i = 0; i < after.Count; ++i) {
            void* candidate = Read<void*>(after.Data, (size_t)i * 8);
            if (candidate && !IsBadPtr(candidate)) {
                Guid id = ItemGuidOf(candidate);
                if (!memcmp(&id, &guid, sizeof(guid))) { canonical = candidate; break; }
            }
        }
    if (!canonical || ItemDefinitionOf(canonical) != def) {
        LOG("Inventory reconciliation lost %s GUID=%08x:%08x:%08x:%08x",
            ObjName(def).c_str(), guid.A, guid.B, guid.C, guid.D);
        return false;
    }
    inst = canonical;
    LOG("Inventory identity verified %s GUID=%08x:%08x:%08x:%08x",
        ObjName(def).c_str(), guid.A, guid.B, guid.C, guid.D);

    if (slot >= 0 && slot < 6) {
        (bar == 0 ? session.slots : session.secondary_slots)[(size_t)slot] = inst;
        // [G18-b] LAYERED SLOT BINDING — three independent paths, first win:
        //   1. AFortPlayerController.AddItemToQuickBars (SDK-verified client
        //      API, FortniteGame_functions.cpp:76307 — the exact function the
        //      real client uses to place an item definition in a quickbar slot)
        //   2. QuickBars.ServerAddItemInternal (Erbium GiveItem parity)
        //   3. EquipItemToQuickBarSlot + raw slot surgery (manual fallback)
        // Every path ends with a GetItemInQuickbarSlot read-back so the log
        // always states whether the HUD can actually SEE the item.
        bool bound = QBBindController(def, bar, slot);
        const char* via = bound ? "AddItemToQuickBars" : nullptr;
        if (!bound && QBAddItem(guid, bar == 1, slot)) { bound = true; via = "ServerAddItemInternal"; }
        if (!bound && BindSlot(inst, slot, bar))       { bound = true; via = "manual"; }
        if (bound) {
            void* visible = QBQuerySlot(bar, slot);
            LOG("[QB] bind bar=%u slot=%d via=%s HUD-read=%s", bar, slot, via,
                visible ? "✅" : "❌ (slot empty)");
        } else LOG("[QB] ⚠️ bind bar=%u slot=%d FAILED on all paths", bar, slot);
    }
    LOG("Inventory inserted %s count=%d slot=%d bar=%u indices=%d/%d",
        ObjName(def).c_str(), count, slot, bar, idx, entryIdx);
    return true;
}

// Legacy direct-add kept for ammo/resources (no quickbar binding).
bool Athena::AddToInventory(void* def, int count, int slot) {
    if (IsBadPtr(def) || !session.inventory) return false;
    return GiveItem(def, count, (slot >= 0 && slot < 6) ? slot : -1, 0, false);
}

// [F19] Two-bar starter loadout: pickaxe + weapons on Primary(0), building
// pieces on Secondary(1), ammo/resources unbound. Pickaxe comes from the
// locker when available (AthenaPickaxeItemDefinition.WeaponDefinition).
static void* ResolveWeaponDefForEquip(void* def) {
    if (!def) return nullptr;
    void* pickCls = FindObj("/Script/FortniteGame.AthenaPickaxeItemDefinition");
    if (pickCls && IsA(def, pickCls)) {
        void* weapon = GetObjField(def, "/Script/FortniteGame.AthenaPickaxeItemDefinition.WeaponDefinition");
        if (weapon) return weapon;
    }
    return def;
}

void Athena::InitializeInventory() {
    if (!session.controller) return;
    void* invProp = GetProp("/Script/FortniteGame.FortPlayerController.WorldInventory");
    if (!invProp) return;
    session.inventory = Read<void*>(GetField(session.controller, invProp));
    if (!session.inventory) {
        void* klass = FindObj("/Script/FortniteGame.FortInventory");
        Transform t{};
        session.inventory = SpawnActor(session.world, klass, session.controller, t);
        if (session.inventory) Write<void*>(GetField(session.controller, invProp), 0, session.inventory);
    }
    if (!session.inventory) { LOG("[Inv] No inventory"); return; }
    // [F43] SetOwner actually needs the owner param (the old call left it null).
    { Call so("/Script/FortniteGame.FortInventory.SetOwner");
      if (so.ok() && (so.set("NewOwner", session.controller) || so.set_at(0, session.controller))) so.run(session.inventory); }

    void* qbProp = GetProp("/Script/FortniteGame.FortPlayerController.ClientQuickBars");
    if (qbProp) {
        session.quickbars = Read<void*>(GetField(session.controller, qbProp));
        if (!session.quickbars) {
            LOG("[QB] Spawning FortQuickBarsAthena...");
            void* klass = FindObj("/Script/FortniteGame.FortQuickBarsAthena");
            if (!klass) { LOG("[QB] FortQuickBarsAthena not found, fallback"); klass = FindObj("/Script/FortniteGame.FortQuickBars"); }
            Transform t{};
            session.quickbars = SpawnActor(session.world, klass, session.controller, t);
            if (session.quickbars) {
                Write<void*>(GetField(session.controller, qbProp), 0, session.quickbars);
                LOG("[QB] Spawned: %p class=%s", session.quickbars, ObjName(Read<void*>(session.quickbars, profile::ObjectClass)).c_str());
            }
        }
        if (session.quickbars) {
            Write<void*>((char*)session.quickbars + profile::QuickBars_CtxOwner, 0, session.controller);
            Call soQB("/Script/FortniteGame.FortQuickBars.SetOwner");
            if (soQB.ok() && (soQB.set("NewOwner", session.controller) || soQB.set_at(0, session.controller))) soQB.run(session.quickbars);
            // [F43] Erbium binds the controller's QuickBars pointer (the
            // replicated prop) as well as ClientQuickBars — the hotbar HUD
            // reads one or the other depending on the build.
            void* qbp2 = GetProp("/Script/FortniteGame.FortPlayerController.QuickBars");
            if (qbp2 && qbp2 != qbProp) Write<void*>(GetField(session.controller, qbp2), 0, session.quickbars);
            LOG("[QB] Running native initialize...");
            InitializeQuickBarsNative();
        }
    }

    LOG("Inventory ready: inv=%p qb=%p", session.inventory, session.quickbars);

    // Pickaxe: locker first — Erbium parity: CosmeticLoadoutPC.Pickaxe is the
    // COSMETIC AthenaPickaxeItemDefinition; the carried weapon is its
    // WeaponDefinition. The old code handed the cosmetic def straight to
    // GiveItem, so a locker pickaxe never materialized as a real weapon.
    // Hardcoded default second.
    void* pickaxeDef = nullptr;
    if (session.locker_pickaxe) {
        pickaxeDef = ResolveWeaponDefForEquip(session.locker_pickaxe);
        if (pickaxeDef) LOG("[Pickaxe] ✅ locker pickaxe -> %s", ObjName(pickaxeDef).c_str());
        else LOG("[Pickaxe] locker item exposes no WeaponDefinition — default pickaxe");
    }
    if (!pickaxeDef) pickaxeDef = LoadAsset("/Game/Athena/Items/Weapons/WID_Harvest_Pickaxe_Athena_C_T01.WID_Harvest_Pickaxe_Athena_C_T01");
    if (!pickaxeDef) pickaxeDef = ByShortName("WID_Harvest_Pickaxe_Athena_C_T01");
    if (pickaxeDef) GiveItem(pickaxeDef, 1, 0, 0, false);

    // Starter weapon (slot 1 keeps 2..5 free for pickups).
    { void* def = ByShortName("WID_Assault_Auto_Athena_C_Ore_T03");
      if (!def) def = LoadAsset("/Game/Athena/Items/Weapons/WID_Assault_Auto_Athena_C_Ore_T03.WID_Assault_Auto_Athena_C_Ore_T03");
      if (def) GiveItem(def, 1, 1, 0, false); }

    // [F19] Build pieces live on the Secondary quickbar (bar 1).
    struct ToolSlot { const char* path; const char* shortName; int slot; };
    ToolSlot tools[] = {
        {"/Game/Items/Weapons/BuildingTools/BuildingItemData_Wall.BuildingItemData_Wall",        "BuildingItemData_Wall",     0},
        {"/Game/Items/Weapons/BuildingTools/BuildingItemData_Floor.BuildingItemData_Floor",      "BuildingItemData_Floor",    1},
        {"/Game/Items/Weapons/BuildingTools/BuildingItemData_Stair_W.BuildingItemData_Stair_W",  "BuildingItemData_Stair_W",  2},
        {"/Game/Items/Weapons/BuildingTools/BuildingItemData_RoofS.BuildingItemData_RoofS",      "BuildingItemData_RoofS",    3},
    };
    int buildsGiven = 0;
    for (auto& t : tools) {
        void* def = LoadAsset(t.path);
        if (!def) def = ByShortName(t.shortName);
        if (def) { if (GiveItem(def, 1, t.slot, 1, false)) ++buildsGiven; }
        else LOG("[Loadout] ⚠️ %s NOT FOUND (path + short-name both missed — build palette will miss this piece)", t.shortName);
    }
    session.edit_tool = LoadAsset("/Game/Items/Weapons/BuildingTools/EditTool.EditTool");
    bool editGiven = false;
    if (session.edit_tool) editGiven = GiveItem(session.edit_tool, 1, 4, 1, false);
    else LOG("[Loadout] ⚠️ EditTool NOT FOUND");

    const char* ammo[] = {
        "/Game/Athena/Items/Ammo/AthenaAmmoDataBulletsLight.AthenaAmmoDataBulletsLight",
        "/Game/Athena/Items/Ammo/AthenaAmmoDataBulletsMedium.AthenaAmmoDataBulletsMedium",
        "/Game/Athena/Items/Ammo/AthenaAmmoDataBulletsHeavy.AthenaAmmoDataBulletsHeavy",
        "/Game/Athena/Items/Ammo/AthenaAmmoDataShells.AthenaAmmoDataShells",
        "/Game/Athena/Items/Ammo/AmmoDataRockets.AmmoDataRockets", nullptr};
    int ammoGiven = 0, resGiven = 0;
    for (int i = 0; ammo[i]; ++i) { void* def = LoadAsset(ammo[i]); if (def && AddToInventory(def, 999, -1)) ++ammoGiven; }
    const char* resources[] = {
        "/Game/Items/ResourcePickups/WoodItemData.WoodItemData",
        "/Game/Items/ResourcePickups/StoneItemData.StoneItemData",
        "/Game/Items/ResourcePickups/MetalItemData.MetalItemData", nullptr};
    for (int i = 0; resources[i]; ++i) { void* def = LoadAsset(resources[i]); if (def && AddToInventory(def, 500, -1)) ++resGiven; }

    // [G18-b] one-line loadout verdict — the device log answers "is anything
    // actually in the inventory?" without scrolling 40 GiveItem lines.
    LOG("[Loadout] pickaxe=%d weapon1=%d builds=%d/4 edit=%d ammo=%d/5 mats=%d/3 (qb=%p)",
        (int)(session.slots[0] != nullptr), (int)(session.slots[1] != nullptr),
        buildsGiven, (int)editGiven, ammoGiven, resGiven, session.quickbars);

    InventoryUpdate();
    ForceUpdateQuickBars();
    ActivateQuickBarSlot(0, 0);
    session.inventory_ready = true;
    session.qb_watch_tries = 0;     // [G18-b] arm the self-healing watch
    session.qb_slots_verified = false;
    EquipSlot(0);
}

// [F19] EquipSlot resolves pickaxe WeaponDefinition and equips by GUID.
void Athena::EquipSlot(int slot) {
    if (slot < 0 || slot >= 6 || !session.slots[(size_t)slot]) return;
    void* item = session.slots[(size_t)slot];
    void* def = ItemDefinitionOf(item);
    if (!def) return;
    Guid guid = ItemGuidOf(item);
    if (!(guid.A | guid.B | guid.C | guid.D)) return;
    void* weaponDef = ResolveWeaponDefForEquip(def);
    if (!weaponDef) return;

    void* itemEntryProp = GetProp("/Script/FortniteGame.FortWorldItem.ItemEntry");
    Guid trackerGuid{};
    if (itemEntryProp) {
        void* entrySrc = GetField(item, itemEntryProp);
        void* trackerProp = GetProp("/Script/FortniteGame.FortItemEntry.TrackerGuid");
        if (entrySrc && trackerProp) trackerGuid = Read<Guid>(GetField(entrySrc, trackerProp));
    }
    Call eq("/Script/FortniteGame.FortPawn.EquipWeaponDefinition");
    if (!eq.ok()) return;
    bool bDisableAnim = false;
    eq.set("WeaponData", weaponDef); eq.set("ItemEntryGuid", guid);
    eq.set("TrackerGuid", trackerGuid); eq.set("bDisableEquipAnimation", bDisableAnim);
    if (eq.run(session.pawn)) {
        void* weapon = eq.result<void*>();
        LOG("Equip %s -> %p", ObjName(weaponDef).c_str(), weapon);
    }
}

void Athena::OnServerExecuteInventoryItem(Guid guid) {
    if (!session.inventory || !session.pawn) return;
    void* invProp = GetProp("/Script/FortniteGame.FortInventory.Inventory");
    void* instProp = GetProp("/Script/FortniteGame.FortItemList.ItemInstances");
    if (!instProp || !invProp) return;
    void* list = GetField(session.inventory, invProp);
    Array arr = Read<Array>(GetField(list, instProp));
    for (int i = 0; i < arr.Count; ++i) {
        void* inst = Read<void*>(arr.Data, (size_t)i * 8);
        if (!inst) continue;
        Guid cand = ItemGuidOf(inst);
        if (!memcmp(&cand, &guid, sizeof(Guid))) {
            for (int s = 0; s < 6; ++s) if (session.slots[(size_t)s] == inst) { EquipSlot(s); return; }
            // Not bound to a slot — still allow equipping weapons.
            void* def = ItemDefinitionOf(inst);
            void* weaponDef = ResolveWeaponDefForEquip(def);
            if (weaponDef) {
                Guid tracker{};
                Call eq("/Script/FortniteGame.FortPawn.EquipWeaponDefinition");
                bool bDisableAnim = false;
                if (eq.ok() && eq.set("WeaponData", weaponDef) && eq.set("ItemEntryGuid", cand) &&
                    eq.set("TrackerGuid", tracker) && eq.set("bDisableEquipAnimation", bDisableAnim))
                    eq.run(session.pawn);
            }
            return;
        }
    }
}
void Athena::OnServerExecuteInventoryWeapon(void* weapon) {
    if (!weapon || !session.pawn) return;
    void* p = GetProp("/Script/FortniteGame.FortWeapon.ItemEntryGuid");
    if (!p) return;
    Guid guid = Read<Guid>(GetField(weapon, p));
    OnServerExecuteInventoryItem(guid);
}

// ============================================================================
// PICKUPS — [F06/F18] entry-based access + stacking (Era core local_pickup)
// ============================================================================
static void* PickupEntry(void* pickup) {
    void* entryProp = GetProp("/Script/FortniteGame.FortPickup.PrimaryPickupItemEntry");
    return pickup && entryProp ? GetField(pickup, entryProp) : nullptr;
}
static void* EntryDefinition(void* entry) {
    void* p = entry ? GetProp("/Script/FortniteGame.FortItemEntry.ItemDefinition") : nullptr;
    return p ? Read<void*>(GetField(entry, p)) : nullptr;
}
static int EntryCount(void* entry) {
    void* p = entry ? GetProp("/Script/FortniteGame.FortItemEntry.Count") : nullptr;
    return p ? Read<int>(GetField(entry, p)) : 0;
}

// [F31] Resolve the GUID of an item already present in the world inventory
// (vehicle-seat weapon re-grants reuse the existing instance).
static Guid GuidOfDefInInventory(void* def) {
    void* invProp  = GetProp("/Script/FortniteGame.FortInventory.Inventory");
    void* instProp = GetProp("/Script/FortniteGame.FortItemList.ItemInstances");
    if (!def || !session.inventory || !invProp || !instProp) return Guid{};
    void* list = GetField(session.inventory, invProp);
    Array arr = Read<Array>(GetField(list, instProp));
    for (int i = 0; i < arr.Count; ++i) {
        void* inst = Read<void*>(arr.Data, (size_t)i * 8);
        if (inst && !IsBadPtr(inst) && ItemDefinitionOf(inst) == def) return ItemGuidOf(inst);
    }
    return Guid{};
}

// [F28/F31] Shared inventory removal: optionally spawns the world pickup
// (inventory-drop path) or silently removes (vehicle-seat weapon path —
// Erbium ServerOnExitVehicle removes the granted weapon without dropping).
static bool RemoveItemByGuid(Guid guid, bool drop, int count = -1) {
    if (!session.inventory) return false;
    void* invProp   = GetProp("/Script/FortniteGame.FortInventory.Inventory");
    void* instProp  = GetProp("/Script/FortniteGame.FortItemList.ItemInstances");
    void* entryProp = GetProp("/Script/FortniteGame.FortItemList.ReplicatedEntries");
    if (!invProp || !instProp || !entryProp) return false;
    void* list = GetField(session.inventory, invProp);
    void* instArr = GetField(list, instProp);
    void* entryArr = GetField(list, entryProp);
    Array arr = Read<Array>(instArr);
    for (int i = 0; i < arr.Count; ++i) {
        void* inst = Read<void*>(arr.Data, (size_t)i * 8);
        if (!inst) continue;
        Guid cand = ItemGuidOf(inst);
        if (memcmp(&cand, &guid, sizeof(Guid)) != 0) continue;
        void* def = ItemDefinitionOf(inst);
        int have = 1;
        if (drop) {
            void* itemEntryProp = GetProp("/Script/FortniteGame.FortWorldItem.ItemEntry");
            if (itemEntryProp) {
                void* e = GetField(inst, itemEntryProp);
                void* cp = GetProp("/Script/FortniteGame.FortItemEntry.Count");
                if (cp) have = Read<int>(GetField(e, cp));
            }
        }
        if (drop && def && session.pawn) {
            Vector loc{};
            ActorLocation(session.pawn, loc);
            loc.Z += 60.0;
            int dropped = count > 0 ? std::min(count, have > 0 ? have : count) : std::max(1, have);
            Athena::SpawnPickup(def, dropped, loc, 16 /*Tossed*/, 10 /*TossedByPlayer*/, true);
        }
        if (profile::GenericArrayRemove) Fn<void(*)(void*,void*,int)>(profile::GenericArrayRemove)(instArr, instProp, i);   // [G16-f] guarded
        Array entries = Read<Array>(entryArr);
        if (i < entries.Count)
            if (profile::GenericArrayRemove) Fn<void(*)(void*,void*,int)>(profile::GenericArrayRemove)(entryArr, entryProp, i);   // [G16-f] guarded
        for (int s = 0; s < 6; ++s) if (session.slots[(size_t)s] == inst) session.slots[(size_t)s] = nullptr;
        for (int s = 0; s < 6; ++s) if (session.secondary_slots[(size_t)s] == inst) session.secondary_slots[(size_t)s] = nullptr;
        // [F36] Erbium Remove(): empty the quickbar slot + native removal.
        Athena::QBRemoveItem(guid, i);
        Athena::InventoryUpdate();
        return true;
    }
    return false;
}

// ============================================================================
// PICKUPS — [F35] Erbium 1:1: AFortPlayerControllerAthena::InternalPickup
// (stacking / GiveOrSwapStack / GiveOrSwap) + the SetPickupTarget fly-to-
// player animation. In the standalone client the native chain
// (capsule overlap → ServerHandlePickup → FinishedTargetSpline) never
// completes because its inventory half lives on the dedicated-server build —
// loot either floated in place or flew to the player and vanished. This port
// performs the whole flow locally: native animation state + a scheduled,
// guaranteed inventory transfer.
// ============================================================================

// Erbium InternalPickup FinalLoc: pawn + forward*450 + z+50 + random 100u disc.
static Vector FinalLocForToss() {
    Vector loc{};
    if (!session.pawn || !ActorLocation(session.pawn, loc)) return loc;
    Vector fwd{1.0, 0.0, 0.0};
    { Call c("/Script/Engine.Actor.GetActorForwardVector");
      if (c.ok() && c.run(session.pawn)) fwd = c.result<Vector>(); }
    fwd.Z = 0.0;
    double len = std::sqrt(fwd.X * fwd.X + fwd.Y * fwd.Y);
    if (len > 0.0001) { fwd.X /= len; fwd.Y /= len; } else { fwd.X = 1.0; fwd.Y = 0.0; }
    loc.X += fwd.X * 450.0;
    loc.Y += fwd.Y * 450.0;
    loc.Z += 50.0;
    float angleVar = ((float)rand() * 0.00109866634f) - 18.0f;      // Erbium scaling
    double rad = (double)angleVar * 0.017453292519943295;
    loc.X += std::cos(rad) * 100.0;
    loc.Y += std::sin(rad) * 100.0;
    return loc;
}

// Erbium InternalPickup's ItemCount: occupied primary quickbar slots
// (pickaxe slot 0 excluded — IsPrimaryQuickbar(melee) is false in Erbium).
static int PrimaryStackCount() {
    int n = 0;
    for (int i = 1; i < 6; ++i)
        if (session.slots[(size_t)i] && !IsBadPtr(session.slots[(size_t)i])) ++n;
    return n;
}

static void EntryAmmoLevel(void* entry, int& loadedAmmo, int& level) {
    loadedAmmo = -1; level = 1;
    if (!entry) return;
    void* ap = GetProp("/Script/FortniteGame.FortItemEntry.LoadedAmmo");
    if (ap && Read<int>(ap, profile::PropElementSize) == 4) {
        int v = Read<int>(GetField(entry, ap));
        if (v > 0 && v <= 1000) loadedAmmo = v;
    }
    void* lp = GetProp("/Script/FortniteGame.FortItemEntry.Level");
    if (lp && Read<int>(lp, profile::PropElementSize) == 4) {
        int v = Read<int>(GetField(entry, lp));
        if (v > 1 && v <= 100) level = v;
    }
}

// [F35] SetPickupTarget emulation (Erbium's native-call fallback, 1:1):
//   PickupLocationData.PickupGuid / PickupTarget / FlyTime(+StartDirection)
//   → OnRep_PickupLocationData → bPickedUp → OnRep_bPickedUp
//   → Pawn.IncomingPickups.Add — this is what makes the pickup home to the
//   player exactly like a live-server pickup.
void Athena::SetPickupTargetAnim(void* pickup, float flyTime) {
    if (!pickup || IsBadPtr(pickup) || !session.pawn) return;
    void* entry = PickupEntry(pickup);
    void* data = GetObjField(pickup, "/Script/FortniteGame.FortPickup.PickupLocationData");
    if (entry && data && !IsBadPtr(data)) {
        void* guidProp = GetProp("/Script/FortniteGame.FortPickupLocationData.PickupGuid");
        if (guidProp && Read<int>(guidProp, profile::PropElementSize) == sizeof(Guid))
            Write<Guid>(GetField(data, guidProp), 0, Read<Guid>(GetField(entry, GetProp("/Script/FortniteGame.FortItemEntry.ItemGuid"))));
        // PickupTarget — a TWeakObjectPtr; build it from GObjects (cached).
        void* tgtProp = GetProp("/Script/FortniteGame.FortPickupLocationData.PickupTarget");
        if (tgtProp) {
            int sz = Read<int>(tgtProp, profile::PropElementSize);
            if (sz >= 8) {
                void* tgtField = GetField(data, tgtProp);
                MakeWeakRef(session.pawn, tgtField, session.pawn_weak_ready, session.pawn_weak_idx, session.pawn_weak_serial);
            }
        }
        // FlyTime — clamp: a zero/garbage FlyTime skips the whole animation.
        void* ftProp = GetProp("/Script/FortniteGame.FortPickupLocationData.FlyTime");
        if (ftProp && Read<int>(ftProp, profile::PropElementSize) == 4) {
            float ft = flyTime;
            if (!(ft > 0.05f) || !std::isfinite(ft)) ft = 0.45f;
            if (ft > 1.5f) ft = 1.5f;
            Write<float>(GetField(data, ftProp), 0, ft);
        }
        // StartDirection — small upward nudge (native sends the throw vector).
        void* sdProp = GetProp("/Script/FortniteGame.FortPickupLocationData.StartDirection");
        if (sdProp && Read<int>(sdProp, profile::PropElementSize) == 24)
            Write<Vector>(GetField(data, sdProp), 0, Vector{0.0, 0.0, 120.0});
        // bPlayPickupSound bit — best effort (struct-relative, on the data).
        SetBool(data, "/Script/FortniteGame.FortPickupLocationData.bPlayPickupSound", true);
        Call rep("/Script/FortniteGame.FortPickup.OnRep_PickupLocationData");
        if (rep.ok()) rep.run(pickup);
    }
    // bPickedUp + OnRep — hides the pickup from the ground-loot HUD and plays
    // the fly animation.
    SetBool(pickup, "/Script/FortniteGame.FortPickup.bPickedUp", true);
    { Call r("/Script/FortniteGame.FortPickup.OnRep_bPickedUp"); if (r.ok()) r.run(pickup); }
    // Pawn.IncomingPickups.Add(pickup) — the pawn keeps flying pickups here.
    {
        void* ipProp = GetProp("/Script/FortniteGame.FortPlayerPawn.IncomingPickups");
        if (!ipProp) ipProp = GetProp("/Script/FortniteGame.FortPawn.IncomingPickups");
        if (ipProp) {
            void* arrField = GetField(session.pawn, ipProp);
            if (arrField) {
                Array cur = Read<Array>(arrField);
                bool known = false;
                for (int i = 0; i < cur.Count && cur.Data; ++i)
                    if (Read<void*>(cur.Data, (size_t)i * 8) == pickup) { known = true; break; }
                if (!known && cur.Count >= 0 && cur.Count < 64)
                    Fn<int(*)(void*, void*, const void*)>(profile::GenericArrayAdd)(arrField, ipProp, &pickup);
            }
        }
    }
    LOG("[Pickup] SetPickupTarget anim fly=%.2f", flyTime > 0.05f && std::isfinite(flyTime) ? std::min(flyTime, 1.5f) : 0.45f);
}

// [F35] Entry point for every pickup request (ServerHandlePickup* RPCs and
// capsule overlaps). Deduplicates through bPickedUp + the pending list, sets
// the native animation, and schedules the inventory transfer for Tick.
void Athena::HandlePickupRPC(void* pickup, float flyTime) {
    if (!pickup || IsBadPtr(pickup) || !session.pawn || !session.inventory) return;
    if (!IsReadable(pickup, 0x40)) return;
    void* pickupCls = FindObj("/Script/FortniteGame.FortPickup");
    if (!pickupCls || !IsA(pickup, pickupCls)) return;
    if (GetBool(pickup, "/Script/FortniteGame.FortPickup.bPickedUp")) return;   // already in flight
    Vector location{};
    if (!ActorLocation(pickup, location)) return;
    if (!NearPlayer(location, 600.0f)) return;
    void* entry = PickupEntry(pickup);
    if (!entry || !EntryDefinition(entry)) return;   // nothing to give — let native handle
    for (auto& p : session.pending_pickups)
        if (p.pickup == pickup) return;              // already scheduled
    float ft = flyTime;
    if (!(ft > 0.05f) || !std::isfinite(ft)) ft = 0.45f;
    if (ft > 1.5f) ft = 1.5f;
    SetPickupTargetAnim(pickup, ft);
    session.pending_pickups.push_back({pickup, Monotonic() + (double)ft});
}

// [F35] Tick-side completion: after the fly time elapses, run the Erbium
// InternalPickup port and destroy the pickup (FinishedTargetSpline parity).
void Athena::ProcessPendingPickups() {
    if (session.pending_pickups.empty()) return;
    double now = Monotonic();
    std::vector<Session::PendingPickup> remaining;
    for (auto& p : session.pending_pickups) {
        if (now < p.at) { remaining.push_back(p); continue; }
        void* pickup = p.pickup;
        bool ok = false;
        // The tick runs on the game thread — GC cannot interleave here.
        if (pickup && !IsBadPtr(pickup) && IsReadable(pickup, 0x40)) {
            void* entry = PickupEntry(pickup);
            if (entry && EntryDefinition(entry)) ok = InternalPickup(entry, pickup);
        }
        if (ok) {
            Call d("/Script/Engine.Actor.K2_DestroyActor");
            if (d.ok() && pickup) d.run(pickup);
            LOG("[Pickup] ✅ transfer complete");
        } else {
            LOG("[Pickup] transfer skipped (dead pickup or full inventory) — dropped from queue");
        }
    }
    session.pending_pickups.swap(remaining);
}

// [F35] Erbium AFortPlayerControllerAthena::InternalPickup 1:1.
bool Athena::InternalPickup(void* entry, void* pickupActor) {
    (void)pickupActor;   // [G13] reserved (Erbium passes it; the entry carries all state here)
    if (!entry || !session.inventory) return false;
    void* def = EntryDefinition(entry);
    int count = EntryCount(entry);
    if (!def || count < 1 || count > 9999) return false;

    int loadedAmmo = -1, level = 1;
    EntryAmmoLevel(entry, loadedAmmo, level);

    int maxStack = MaxStackSizeOf(def);
    if (maxStack <= 0) maxStack = 1;
    int itemCount = PrimaryStackCount();

    bool bAllowMultipleStacks = false;
    { void* p = GetProp("/Script/FortniteGame.FortItemDefinition.bAllowMultipleStacks");
      if (p && Read<int>(p, profile::PropElementSize) == 1)
          bAllowMultipleStacks = Read<uint8_t>(GetField(def, p)) != 0; }

    void* invProp   = GetProp("/Script/FortniteGame.FortInventory.Inventory");
    void* instProp  = GetProp("/Script/FortniteGame.FortItemList.ItemInstances");
    void* entryProp = GetProp("/Script/FortniteGame.FortItemList.ReplicatedEntries");
    void* itemEntryProp = GetProp("/Script/FortniteGame.FortWorldItem.ItemEntry");
    void* countProp = GetProp("/Script/FortniteGame.FortItemEntry.Count");
    void* guidProp  = GetProp("/Script/FortniteGame.FortItemEntry.ItemGuid");
    void* defProp   = GetProp("/Script/FortniteGame.FortItemEntry.ItemDefinition");
    if (!invProp || !instProp || !entryProp || !itemEntryProp || !countProp || !guidProp || !defProp) return false;
    void* list = GetField(session.inventory, invProp);
    void* instArr = GetField(list, instProp);
    void* entryArr = GetField(list, entryProp);
    int entryStride = Read<int>(entryProp, profile::PropElementSize);
    if (entryStride <= 0 || entryStride > 1024) return false;

    // GiveItem wrapper that reports the GUID of the granted stack.
    // require_new is relaxed for multi-stack definitions (Erbium allows a
    // second stack); unique items stay duplicate-protected.
    auto giveNew = [&](int giveCount, int slot, uint8_t bar) -> Guid {
        if (!GiveItem(def, giveCount, slot, bar, !bAllowMultipleStacks, loadedAmmo, level)) return Guid{};
        return GuidOfDefInInventory(def);
    };
    // Re-toss (Erbium GiveOrSwapStack overflow / hotbar-full reject).
    auto retoss = [&](int outCount) {
        if (outCount < 1) return;
        Vector loc = FinalLocForToss();
        SpawnPickup(def, outCount, loc, 1 /*Player*/, 0, true, level, loadedAmmo);
    };
    // Equip + activate the slot the new item occupies (Erbium end of swap).
    auto equipNew = [&](Guid g) {
        if (!(g.A | g.B | g.C | g.D)) return;
        OnServerExecuteInventoryItem(g);
        for (int i = 0; i < 6; ++i)
            if (session.slots[(size_t)i] && !IsBadPtr(session.slots[(size_t)i])) {
                Guid ig = ItemGuidOf(session.slots[(size_t)i]);
                if (!memcmp(&ig, &g, sizeof g)) { QBActivateSlot(false, i); return; }
            }
    };

    if (maxStack > 1) {
        // ---- Erbium MaxStack > 1 branch (stack into an existing partial) ----
        void* item = nullptr;
        Array instances = Read<Array>(instArr);
        for (int i = 0; i < instances.Count; ++i) {
            void* cand = Read<void*>(instances.Data, (size_t)i * 8);
            if (!cand || IsBadPtr(cand) || ItemDefinitionOf(cand) != def) continue;
            void* ie = GetField(cand, itemEntryProp);
            if (Read<int>(GetField(ie, countProp)) < maxStack) { item = cand; break; }
        }
        void* matchingEntry = nullptr;
        Array replicated = Read<Array>(entryArr);
        for (int j = 0; j < replicated.Count && replicated.Data; ++j) {
            void* cand = (char*)replicated.Data + (size_t)j * entryStride;
            if (Read<void*>(GetField(cand, defProp)) != def) continue;
            if (Read<int>(GetField(cand, countProp)) < maxStack) { matchingEntry = cand; break; }
        }
        if (item && matchingEntry) {
            int original = Read<int>(GetField(matchingEntry, countProp));
            int updated = original + count;
            int overflow = 0;
            if (updated > maxStack) { overflow = updated - maxStack; updated = maxStack; }
            Write<int>(GetField(matchingEntry, countProp), 0, updated);
            // keep the instance entry in sync (Erbium: (*item)->ItemEntry.Count = ...)
            void* itemEntry = GetField(item, itemEntryProp);
            Write<int>(GetField(itemEntry, countProp), 0, updated);
            InventoryUpdate();
            LOG("[Pickup] stacked +%d -> %d/%d (%s)", count - overflow, updated, maxStack, ObjName(def).c_str());
            if (overflow > 0) {
                // Erbium GiveOrSwapStack: extra stacks when allowed, else re-toss
                if (bAllowMultipleStacks && itemCount < 5) {
                    GiveItem(def, overflow, -1, IsPrimaryQuickbarItem(def) ? (uint8_t)0 : (uint8_t)-1, true, loadedAmmo, level);
                } else {
                    retoss(overflow);
                }
            }
            return true;
        }
        // No partial stack — Erbium: full-stack guard for single-stack items.
        void* fullEntry = nullptr;
        for (int j = 0; j < replicated.Count && replicated.Data; ++j) {
            void* cand = (char*)replicated.Data + (size_t)j * entryStride;
            if (Read<void*>(GetField(cand, defProp)) == def) { fullEntry = cand; break; }
        }
        if (!matchingEntry && fullEntry && !bAllowMultipleStacks) { retoss(count); return true; }
        int giveCount = count;
        if (count > maxStack) { giveCount = maxStack; retoss(count - maxStack); }
        // ---- Erbium GiveOrSwap ----
        if (itemCount >= 5 && IsPrimaryQuickbarItem(def)) {
            void* cur = session.pawn ? Read<void*>(session.pawn, profile::FortPawn_CurrentWeapon) : nullptr;
            void* curDef = (cur && !IsBadPtr(cur)) ? GetObjField(cur, "/Script/FortniteGame.FortWeapon.WeaponData") : nullptr;
            if (cur && curDef && !IsBadPtr(curDef) && IsPrimaryQuickbarItem(curDef)) {
                // Toss the held weapon out in front of the player, remove, give.
                void* gp = GetProp("/Script/FortniteGame.FortWeapon.ItemEntryGuid");
                Guid curGuid = gp ? Read<Guid>(GetField(cur, gp)) : Guid{};
                if (curGuid.A | curGuid.B | curGuid.C | curGuid.D) {
                    int dropCount = 1;
                    Array insts = Read<Array>(instArr);
                    for (int i = 0; i < insts.Count; ++i) {
                        void* cand = Read<void*>(insts.Data, (size_t)i * 8);
                        if (cand && !IsBadPtr(cand)) {
                            Guid g = ItemGuidOf(cand);
                            if (!memcmp(&g, &curGuid, sizeof g)) {
                                void* ie = GetField(cand, itemEntryProp);
                                dropCount = std::max(1, Read<int>(GetField(ie, countProp)));
                                break;
                            }
                        }
                    }
                    Vector tossLoc{};
                    if (session.pawn) { ActorLocation(session.pawn, tossLoc); tossLoc.Z += 50.0; }
                    SpawnPickup(curDef, dropCount, tossLoc, 1 /*Player*/, 0, true, level, loadedAmmo);
                    RemoveItemByGuid(curGuid, false);
                    LOG("[Pickup] swap: tossed %s", ObjName(curDef).c_str());
                }
                int slot = -1;
                for (int i = 1; i < 6; ++i) if (!session.slots[(size_t)i]) { slot = i; break; }
                Guid g = giveNew(giveCount, slot, 0);
                equipNew(g);
                return true;
            }
            // Holding a non-primary (pickaxe): re-toss in front (Erbium parity).
            retoss(count);
            return true;
        }
        // Regular give — primary items take the first free slot.
        bool primary = IsPrimaryQuickbarItem(def);
        int slot = -1;
        if (primary) {
            for (int i = 1; i < 6; ++i) if (!session.slots[(size_t)i]) { slot = i; break; }
            if (slot < 0) { retoss(count); return true; }
        }
        Guid g = giveNew(giveCount, slot, primary ? (uint8_t)0 : (uint8_t)-1);
        return (g.A | g.B | g.C | g.D) != 0;
    }

    // ---- Erbium MaxStack <= 1 branch (unique items: weapons, gadgets) ----
    if (itemCount >= 5 && IsPrimaryQuickbarItem(def)) {
        void* cur = session.pawn ? Read<void*>(session.pawn, profile::FortPawn_CurrentWeapon) : nullptr;
        void* curDef = (cur && !IsBadPtr(cur)) ? GetObjField(cur, "/Script/FortniteGame.FortWeapon.WeaponData") : nullptr;
        if (cur && curDef && !IsBadPtr(curDef) && IsPrimaryQuickbarItem(curDef)) {
            void* gp = GetProp("/Script/FortniteGame.FortWeapon.ItemEntryGuid");
            Guid curGuid = gp ? Read<Guid>(GetField(cur, gp)) : Guid{};
            if (curGuid.A | curGuid.B | curGuid.C | curGuid.D) {
                int dropCount = 1;
                Array insts = Read<Array>(instArr);
                for (int i = 0; i < insts.Count; ++i) {
                    void* cand = Read<void*>(insts.Data, (size_t)i * 8);
                    if (cand && !IsBadPtr(cand)) {
                        Guid g = ItemGuidOf(cand);
                        if (!memcmp(&g, &curGuid, sizeof g)) {
                            void* ie = GetField(cand, itemEntryProp);
                            dropCount = std::max(1, Read<int>(GetField(ie, countProp)));
                            break;
                        }
                    }
                }
                Vector tossLoc{};
                if (session.pawn) { ActorLocation(session.pawn, tossLoc); tossLoc.Z += 50.0; }
                SpawnPickup(curDef, dropCount, tossLoc, 1 /*Player*/, 0, true, level, loadedAmmo);
                RemoveItemByGuid(curGuid, false);
                LOG("[Pickup] swap: tossed %s", ObjName(curDef).c_str());
            }
            int slot = -1;
            for (int i = 1; i < 6; ++i) if (!session.slots[(size_t)i]) { slot = i; break; }
            Guid g = giveNew(count, slot, 0);
            equipNew(g);
            return true;
        }
        retoss(count);
        return true;
    }
    bool primary = IsPrimaryQuickbarItem(def);
    int slot = -1;
    if (primary) {
        for (int i = 1; i < 6; ++i) if (!session.slots[(size_t)i]) { slot = i; break; }
        if (slot < 0) { retoss(count); return true; }
    }
    Guid g = giveNew(count, slot, primary ? (uint8_t)0 : (uint8_t)-1);
    return (g.A | g.B | g.C | g.D) != 0;
}

// [F35] Immediate pickup (compat path — era commands and direct requests).
bool Athena::LocalPickup(void* pickup) {
    if (!pickup || IsBadPtr(pickup)) return false;
    void* pickupCls = FindObj("/Script/FortniteGame.FortPickup");
    if (!pickupCls || !IsA(pickup, pickupCls)) return false;
    Vector location{};
    if (!ActorLocation(pickup, location) || !NearPlayer(location, 600.0f)) return false;
    void* entry = PickupEntry(pickup);
    if (!entry || !EntryDefinition(entry)) return true;
    if (!InternalPickup(entry, pickup)) return true;
    Call d("/Script/Engine.Actor.K2_DestroyActor");
    if (d.ok()) d.run(pickup);
    InventoryUpdate();
    return true;
}

void Athena::ServerHandlePickup(void* pickup) { HandlePickupRPC(pickup, 0.45f); }

// [F06/F33/F35] SpawnPickup — Erbium AFortInventory::SpawnPickup 1:1:
// entry (ItemDefinition/Count/Level/LoadedAmmo), PawnWhoDroppedPickup,
// source flags, bRandomRotation → OnRep_PrimaryPickupItemEntry → TossPickup
// (param names probed with fallbacks — Erbium's signature is
// (FinalLocation, Pawn, MaxStackCount, bToss, bCombine, SourceFlags, Source)).
void Athena::SpawnPickup(void* def, int count, Vector loc, uint8_t sourceFlags, uint8_t spawnSource, bool toss, int level, int loadedAmmo) {
    if (!def || !session.world) return;
    void* klass = FindObj("/Script/FortniteGame.FortPickupAthena");
    if (!klass) return;
    Transform t{};
    t.Location[0] = loc.X; t.Location[1] = loc.Y; t.Location[2] = loc.Z;
    void* pickup = SpawnActor(session.world, klass, session.pawn, t);
    if (!pickup) return;
    void* entry = PickupEntry(pickup);
    void* defProp = GetProp("/Script/FortniteGame.FortItemEntry.ItemDefinition");
    void* cntProp = GetProp("/Script/FortniteGame.FortItemEntry.Count");
    void* lvlProp = GetProp("/Script/FortniteGame.FortItemEntry.Level");
    void* ammProp = GetProp("/Script/FortniteGame.FortItemEntry.LoadedAmmo");
    if (entry && defProp) Write<void*>(GetField(entry, defProp), 0, def);
    if (entry && cntProp) Write<int>(GetField(entry, cntProp), 0, count);
    if (entry && lvlProp && level > 0 && Read<int>(lvlProp, profile::PropElementSize) == 4)
        Write<int>(GetField(entry, lvlProp), 0, level);
    if (entry && ammProp && loadedAmmo > 0 && Read<int>(ammProp, profile::PropElementSize) == 4)
        Write<int>(GetField(entry, ammProp), 0, loadedAmmo);
    // [F35] Erbium parity — the overlap pickup gate checks this pawn pointer.
    {
        void* pwdProp = GetProp("/Script/FortniteGame.FortPickup.PawnWhoDroppedPickup");
        if (pwdProp && session.pawn) Write<void*>(GetField(pickup, pwdProp), 0, session.pawn);
    }
    Write<uint8_t>(pickup, profile::AFortPickup_SourceFlags, sourceFlags);
    Write<uint8_t>(pickup, profile::AFortPickup_SpawnSource, spawnSource);
    SetBool(pickup, "/Script/FortniteGame.FortPickup.bRandomRotation", true);
    Call rep("/Script/FortniteGame.FortPickup.OnRep_PrimaryPickupItemEntry"); if (rep.ok()) rep.run(pickup);
    if (spawnSource == 2 || spawnSource == 4) {   // [F33] container-tossed FX
        SetBool(pickup, "/Script/FortniteGame.FortPickup.bTossedFromContainer", true);
        Call r3("/Script/FortniteGame.FortPickup.OnRep_TossedFromContainer");
        if (r3.ok()) r3.run(pickup);
    }
    if (toss) {
        Call c("/Script/FortniteGame.FortPickup.TossPickup");
        if (c.ok()) {
            bool locBound = c.set("FinalLocation", loc) || c.set("TargetLoc", loc) || c.set("InTargetLoc", loc);
            // [F35] Erbium param names — Pawn + MaxStackCount(-1).
            c.set("Pawn", session.pawn) || c.set("ItemOwner", session.pawn) || c.set("InItemOwner", session.pawn);
            int maxStack = -1;
            c.set("MaxStackCount", maxStack) || c.set("OverrideMaxStackCount", maxStack);
            bool bToss = true; c.set("bToss", bToss);
            bool combine = true; c.set("bShouldCombinePickupsWhenTossCompletes", combine);
            // EEFortPickupSourceTypeFlag (uint8) / EEFortPickupSpawnSource (uint8)
            c.set_any((uint8_t)sourceFlags, "InPickupSourceTypeFlags", "PickupSourceTypeFlags");
            c.set_any((uint8_t)spawnSource, "InPickupSpawnSource", "PickupSpawnSource");
            if (locBound) c.run(pickup);
            else LOG("[Pickup] TossPickup location unbound — pickup stays in place");
        }
    }
}

void Athena::DropInventoryItem(Guid guid, int count) {
    // [F31] shared removal path (visual drop + slot cleanup + inventory refresh)
    RemoveItemByGuid(guid, true, count);
}

// ============================================================================
// [G20] Loot pipeline fault-stage tracker — updated at every phase of the
// roll/spawn pipeline; included in the fault log when a guarded spawner roll
// aborts, so the device log names the exact phase that faulted.
// ============================================================================
static const char* g_lootStage = "-";

// ============================================================================
// LOOT — [F20/F32] Erbium pipeline 1:1: DataTable-driven roll
// (ChooseLootForContainer + SetupLDSForPackage over the real cooked tables)
// with the curated table kept as a last-resort fallback.
// ============================================================================
namespace local_loot {
    struct Row  { const char* name; float weight; int count; };
    struct Tier { const char* group; int weaponsMin, weaponsMax, ammoMin, ammoMax, resources; float rarityBoost; };

    // Weapon short names — every row is resolved through ByShortName and
    // silently skipped when the asset is absent, so the table is safe to
    // extend for other seasons. [F32] 21.30 (Chapter 3) pool first.
    static const Row weapons[] = {
        {"WID_Assault_Hammer_Athena_SR",             20, 1},
        {"WID_Assault_Ranger_Athena_SR",             18, 1},
        {"WID_Assault_Auto_Athena_VR_SR",            14, 1},
        {"WID_SMG_Stinger_Athena_SR",                18, 1},
        {"WID_Shotgun_StrikerPump_Athena_SR",        16, 1},
        {"WID_Shotgun_Auto_Athena_SR",               10, 1},
        {"WID_Shotgun_LeverAction_Athena_SR",         8, 1},
        {"WID_Sniper_DMR_Athena_SR",                 10, 1},
        {"WID_Sniper_HunterBoltAction_Athena_SR",     8, 1},
        {"WID_Assault_Auto_Athena_C_Ore_T03",        12, 1},
        {"WID_Shotgun_Standard_Athena_C_Ore_T03",    10, 1},
        {"WID_Shotgun_SemiAuto_Athena_C_Ore_T03",     8, 1},
        {"WID_Sniper_Standard_Athena_C_Ore_T03",      6, 1},
        {"WID_Assault_Semi_Athena_C_Ore_T03",         6, 1},
        {"WID_SMG_Tactical_Athena_C_Ore_T03",         6, 1},
        {"WID_Launcher_Rocket_Athena_C_Ore_T03",      4, 1},
        {"WID_Minigun_Athena_C_Ore_T03",              3, 1},
    };
    static const Row ammo[] = {
        {"AthenaAmmoDataBulletsLight",   30, 60},
        {"AthenaAmmoDataBulletsMedium",  34, 60},
        {"AthenaAmmoDataBulletsHeavy",   14, 30},
        {"AthenaAmmoDataShells",         18, 24},
        {"AmmoDataRockets",               4, 4},
    };
    static const Row resources[] = {
        {"WoodItemData",  40, 50},
        {"StoneItemData", 34, 50},
        {"MetalItemData", 26, 50},
    };
    // Loot-tier-group → package mapping (21.30 SearchLootTierGroup names).
    static const Tier tiers[] = {
        {"Loot_AthenaTreasure",       2, 3, 2, 3, 1, 1.6f},
        {"Loot_Treasure",             2, 3, 2, 3, 1, 1.6f},   // legacy alias
        {"Loot_AthenaAmmoLarge",      0, 0, 3, 4, 0, 0.0f},
        {"Loot_AthenaAmmoSmall",      0, 0, 2, 3, 0, 0.0f},
        {"Loot_AthenaAmmo",           0, 0, 2, 3, 0, 0.0f},
        {"Loot_AthenaFloorLoot",      1, 1, 1, 2, 1, 0.0f},
        {"Loot_AthenaFloorLoot_Warmup", 1, 1, 1, 2, 1, 0.0f},
    };

    static float random01() {
        static uint32_t state = 0;
        if (!state) state = (uint32_t)(Monotonic() * 1000000.0) | 1;
        state ^= state << 13; state ^= state >> 17; state ^= state << 5;
        return float(state >> 8) / 16777216.0f;
    }
    static void* resolve(const Row& row) {
        std::string shortName(row.name);
        void* def = ByShortName(shortName);
        if (def) return def;
        // Try the classic content paths before giving up.
        const char* prefixes[] = {
            "/Game/Athena/Items/Weapons/",
            "/Game/Athena/Items/Ammo/",
            "/Game/Items/ResourcePickups/",
            nullptr
        };
        for (int i = 0; prefixes[i]; ++i) {
            std::string path = std::string(prefixes[i]) + shortName + "." + shortName;
            def = LoadAsset(path);
            if (def) return def;
        }
        return nullptr;
    }
    static void* pickWeapon(float boost) {
        float total = 0;
        for (auto& w : weapons) total += w.weight * (1.0f + boost * (w.weight < 10.0f ? 2.0f : 0.0f));
        float pick = random01() * total;
        for (auto& w : weapons) {
            float weight = w.weight * (1.0f + boost * (w.weight < 10.0f ? 2.0f : 0.0f));
            pick -= weight;
            if (pick < 0) return resolve(w);
        }
        return nullptr;
    }
    static void* pickRow(const Row* table, size_t n) {
        float total = 0;
        for (size_t i = 0; i < n; ++i) total += table[i].weight;
        float pick = random01() * total;
        for (size_t i = 0; i < n; ++i) {
            pick -= table[i].weight;
            if (pick < 0) return resolve(table[i]);
        }
        return nullptr;
    }
    static int rollBetween(int lo, int hi) { return lo + (int)(random01() * float(hi - lo + 1)); }

    // ------------------------------------------------------------------
    // [F32] DataTable-driven roll — Erbium ChooseLootForContainer 1:1.
    // Sources: /Game/Items/Datatables/AthenaLootTierData_Client and
    // AthenaLootPackages_Client (playlist overrides are skipped: the
    // standalone session pins Playlist_DefaultSolo anyway).
    // ------------------------------------------------------------------
    struct Drop { void* def; int count; int level; };

    struct LPkgRow {
        std::string id;                 // LootPackageID (string form)
        float weight = 0;
        std::string call;               // LootPackageCall (recursion target)
        uint8_t soft[64]{};             // TSoftObjectPtr bytes (lazy resolve)
        int softStride = 0;
        std::string path;               // [G20] parsed soft path (resident-first resolve)
        int32 count = 0, category = -1, minW = -1, maxW = -1;
        void* def = nullptr;
    };
    struct LTierRow {
        std::string group;              // TierGroup
        float numDrops = 0, weight = 0;
        int32 lootTier = -1;
        std::string package;            // LootPackage
        std::vector<int32> wArr, minArr, maxArr;
    };
    struct Cache {
        std::vector<LTierRow> tiers;
        std::vector<LPkgRow> packages;
        std::map<std::string, std::vector<LTierRow*>> tierIndex;
        std::map<std::string, std::vector<LPkgRow*>> pkgIndex;
        bool ready = false;
        double failAt = -1000.0;
        double holdUntil = -1000.0;   // [G20] fault backoff — BuildCache skipped while now < holdUntil
    };
    static Cache g_cache;

    // [F92] heartbeat accessor — table count for the [HB] state line
    int TableCount() {
        return g_cache.ready ? (int)(g_cache.tiers.size() + g_cache.packages.size()) : 0;
    }

    static int WorldLevelNow() {
        int lvl = 0;
        if (session.state) GetFieldValue<int32>(session.state, "/Script/FortniteGame.FortGameStateAthena.WorldLevel", lvl);
        if (lvl < 0 || lvl > 64) lvl = 0;
        return lvl;
    }
    static void* ResolveSoft(LPkgRow& r, bool allowSyncLoad) {
        if (r.def) return r.def;
        if (!r.softStride) return nullptr;
        // [G20] RESIDENT-FIRST: parse the soft path to a string once and
        // StaticFindObject it — zero native calls. The batch drain must never
        // pay a synchronous LoadAsset_Blocking (multi-second stall → ANR/OOM
        // kill, the round-17 crash pattern); a resident miss in batch mode
        // falls to the next package row / the curated net instead.
        if (r.path.empty()) r.path = SoftObjectPathString(r.soft, r.softStride);
        if (!r.path.empty()) {
            void* res = FindObj(r.path.c_str());
            if (res && !IsBadPtr(res)) { r.def = res; return r.def; }
        }
        if (!allowSyncLoad) return nullptr;
        r.def = LoadSoftPtr(r.soft, r.softStride);   // interactive opens only (guarded)
        return r.def;
    }
    template<class T> static T* PickWeighted(std::vector<T*>& rows) {
        float total = 0;
        for (auto* r : rows) total += r->weight;
        if (total <= 0) return rows.empty() ? nullptr : rows.front();
        float pick = random01() * total;
        for (auto* r : rows) {
            if (r->weight <= 0) continue;
            pick -= r->weight;
            if (pick < 0) return r;
        }
        return rows.back();
    }

    static void BuildCache() {
        if (g_cache.ready) return;
        if (Monotonic() < g_cache.holdUntil) return;       // [G20] fault backoff
        if (Monotonic() - g_cache.failAt < 30.0) return;   // retry window
        g_lootStage = "cache-open";                        // [G20]
        auto pickTable = [&](const char* athena, const char* base) -> void* {
            void* t = FindObj(athena);
            if (!t) t = LoadAsset(athena, "/Script/Engine.DataTable");
            if (!t) { t = FindObj(base); if (!t) t = LoadAsset(base, "/Script/Engine.DataTable"); }
            return t;
        };
        void* tierTable = pickTable("/Game/Items/Datatables/AthenaLootTierData_Client.AthenaLootTierData_Client",
                                    "/Game/Items/Datatables/LootTierData_Client.LootTierData_Client");
        void* pkgTable  = pickTable("/Game/Items/Datatables/AthenaLootPackages_Client.AthenaLootPackages_Client",
                                    "/Game/Items/Datatables/LootPackages_Client.LootPackages_Client");
        if (!tierTable || !pkgTable) { LOG("[Loot] ⚠️ datatables unavailable (%p/%p) — curated fallback only", tierTable, pkgTable); g_cache.failAt = Monotonic(); return; }
        void* tierStruct = Read<void*>(tierTable, 0x28);   // UDataTable::RowStruct
        void* pkgStruct  = Read<void*>(pkgTable, 0x28);
        if (!tierStruct || !pkgStruct || IsBadPtr(tierStruct) || IsBadPtr(pkgStruct)) { LOG("[Loot] ⚠️ row structs missing"); g_cache.failAt = Monotonic(); return; }
        // [F86-a/LOOT-1] PropertiesSize@0xA8 reads 0/0 on this build (the
        // reported bug): raw read kept as primary when sane, FField-walk as
        // fallback + RowStruct validation via required members. Both values
        // logged — the first device run settles the true constant.
        static const char* const tierMust[] = { "LootPackage", "Weight", "TierGroup", "NumLootPackageDrops", "LootTier" };
        static const char* const pkgMust[]  = { "LootPackageID", "Weight", "ItemDefinition", "Count" };
        int tierRaw = Read<int>(tierStruct, profile::StructPropertiesSize);
        int pkgRaw  = Read<int>(pkgStruct, profile::StructPropertiesSize);
        // [F94-b] FField walk PRIMARY (0x54 read is wrong on this build — see
        // TableRowSizeOf). Walk validates the must-have members too.
        int tierRowSize = TableRowSizeOf(tierStruct, tierMust, 5);
        int pkgRowSize  = TableRowSizeOf(pkgStruct,  pkgMust,  4);
        LOG("[Loot] row sizes: tier=%d (walk, raw@0x54=%d) pkg=%d (walk, raw@0x54=%d) [F94-b]",
            tierRowSize, tierRaw, pkgRowSize, pkgRaw);
        if (tierRowSize <= 0 || tierRowSize > 512 || pkgRowSize <= 0 || pkgRowSize > 512) { LOG("[Loot] ⚠️ bad row sizes %d/%d", tierRowSize, pkgRowSize); g_cache.failAt = Monotonic(); return; }
        // [G19-c] own-map size snapshot — with the parent discovery active,
        // the split "own rows vs parent rows" is now visible in one line
        // (previous device run: tier own=1 of 252, pkg own=1717 of 2149).
        {
            static bool s_ownLogged = false;
            if (!s_ownLogged) {
                s_ownLogged = true;
                int tierOwn = Read<int32>((char*)tierTable + 0x38);   // RowMap@0x30 → ArrayNum@+8
                int pkgOwn  = Read<int32>((char*)pkgTable  + 0x38);
                LOG("[Loot][G19] own RowMap rows: tier=%d pkg=%d (rest live in parent tables)",
                    tierOwn, pkgOwn);
            }
        }

        int toNum  = PropOffsetOn(tierStruct, "NumLootPackageDrops");
        int toGrp  = PropOffsetOn(tierStruct, "TierGroup");
        int toTier = PropOffsetOn(tierStruct, "LootTier");
        int toWgt  = PropOffsetOn(tierStruct, "Weight");
        int toPkg  = PropOffsetOn(tierStruct, "LootPackage");
        int toWA   = PropOffsetOn(tierStruct, "LootPackageCategoryWeightArray");
        int toMin  = PropOffsetOn(tierStruct, "LootPackageCategoryMinArray");
        int toMax  = PropOffsetOn(tierStruct, "LootPackageCategoryMaxArray");
        if (toGrp < 0 || toPkg < 0 || toWgt < 0 || toNum < 0) { LOG("[Loot] ⚠️ LootTierData offsets unresolved"); g_cache.failAt = Monotonic(); return; }
        int poId  = PropOffsetOn(pkgStruct, "LootPackageID");
        int poWgt = PropOffsetOn(pkgStruct, "Weight");
        int poCal = PropOffsetOn(pkgStruct, "LootPackageCall");
        int poDef = PropOffsetOn(pkgStruct, "ItemDefinition");
        int poCnt = PropOffsetOn(pkgStruct, "Count");
        int poCat = PropOffsetOn(pkgStruct, "LootPackageCategory");
        int poMin = PropOffsetOn(pkgStruct, "MinWorldLevel");
        int poMax = PropOffsetOn(pkgStruct, "MaxWorldLevel");
        int defStride = 24;
        { int s = 0; if (FindChildProp(pkgStruct, "ItemDefinition", &s) && s >= 8 && s <= 64) defStride = s; }
        if (poId < 0 || poWgt < 0 || poDef < 0) { LOG("[Loot] ⚠️ LootPackageData offsets unresolved"); g_cache.failAt = Monotonic(); return; }

        std::vector<FName> tierNames, pkgNames;
        if (!DataTableRowNames(tierTable, tierNames) || !DataTableRowNames(pkgTable, pkgNames)) { LOG("[Loot] ⚠️ row enumeration failed"); g_cache.failAt = Monotonic(); return; }
        LOG("[Loot] tables: tiers=%d rows, packages=%d rows", (int)tierNames.size(), (int)pkgNames.size());
        // Erbium dedupe: tiers by (TierGroup, LootPackage) — later rows
        // override; packages by row key — later rows override.
        std::map<uint64_t, LTierRow> tierDedupe;
        std::map<uint32_t, LPkgRow> pkgDedupe;
        uint8_t buf[512];
        auto readArr = [&](const uint8_t* base, int off) {
            std::vector<int32> v;
            if (off < 0) return v;
            Array a = Read<Array>(base, off);
            if (a.Count > 0 && a.Count <= 64 && a.Data && !IsBadPtr(a.Data))
                for (int i = 0; i < a.Count; ++i) v.push_back(Read<int32>(a.Data, (size_t)i * 4));
            return v;
        };
        int tierScanned = 0, tierJunk = 0;   // [G20] progress + junk counters
        for (auto& n : tierNames) {
            g_lootStage = "cache-tier-rows";               // [G20] fault stage
            if (++tierScanned % 250 == 0)
                LOG("[Loot] tier row scan %d/%d [G20]", tierScanned, (int)tierNames.size());
            memset(buf, 0, sizeof(buf));
            if (!DataTableRow(tierTable, n, buf, tierRowSize)) continue;
            LTierRow r;
            r.numDrops = Read<float>(buf, toNum);
            r.weight   = Read<float>(buf, toWgt);
            r.lootTier = toTier >= 0 ? Read<int32>(buf, toTier) : -1;
            r.group    = FNameToString(buf + toGrp);
            r.package  = FNameToString(buf + toPkg);
            r.wArr  = readArr(buf, toWA);
            r.minArr = readArr(buf, toMin);
            r.maxArr = readArr(buf, toMax);
            if (r.group.empty()) { ++tierJunk; continue; }   // [F94] junk-row guard — self-filtering
            uint32_t gIdx = Read<uint32>(buf, toGrp);
            uint32_t pIdx = Read<uint32>(buf, toPkg);
            tierDedupe[(uint64_t(gIdx) << 32) | pIdx] = r;
        }
        int pkgScanned = 0, pkgJunk = 0;   // [G20] progress + junk counters
        for (auto& n : pkgNames) {
            g_lootStage = "cache-pkg-rows";                // [G20] fault stage
            if (++pkgScanned % 250 == 0)
                LOG("[Loot] pkg row scan %d/%d [G20]", pkgScanned, (int)pkgNames.size());
            memset(buf, 0, sizeof(buf));
            if (!DataTableRow(pkgTable, n, buf, pkgRowSize)) continue;
            LPkgRow r;
            r.weight = Read<float>(buf, poWgt);
            r.id     = FNameToString(buf + poId);
            if (r.id.empty()) { ++pkgJunk; continue; }   // [F94] junk-row guard — self-filtering
            if (poCal >= 0) {
                // [F94] The FString bytes are ENGINE-OWNED row memory (the
                // buffer is a shallow copy) — freeing Data here would corrupt
                // the table heap the moment direct row reads went live.
                // Narrow only, never Free. [G20] the count is clamped to 64
                // (LootPackageCall names are short) and the buffer is
                // kernel-validated readable before Narrow touches it.
                FStringNative fs = Read<FStringNative>(buf, poCal);
                int callLen = fs.Count;
                if (callLen < 0) callLen = 0;
                if (callLen > 64) callLen = 64;
                if (fs.Data && !IsBadPtr(fs.Data) && callLen > 0 && IsReadable(fs.Data, (size_t)callLen * 2))
                    r.call = Narrow(fs.Data, callLen);
            }
            if (poDef >= 0) {
                r.softStride = defStride;
                memcpy(r.soft, buf + poDef, (size_t)std::min(defStride, 64));
            }
            r.count    = poCnt >= 0 ? Read<int32>(buf, poCnt) : 1;
            r.category = poCat >= 0 ? Read<int32>(buf, poCat) : -1;
            r.minW     = poMin >= 0 ? Read<int32>(buf, poMin) : -1;
            r.maxW     = poMax >= 0 ? Read<int32>(buf, poMax) : -1;
            pkgDedupe[n.CompIdx] = r;
        }
        for (auto& kv : tierDedupe) g_cache.tiers.push_back(kv.second);
        for (auto& kv : pkgDedupe) g_cache.packages.push_back(kv.second);
        for (auto& r : g_cache.tiers) g_cache.tierIndex[r.group].push_back(&r);
        for (auto& r : g_cache.packages) g_cache.pkgIndex[r.id].push_back(&r);
        g_cache.ready = !g_cache.tiers.empty() && !g_cache.packages.empty();
        if (!g_cache.ready) g_cache.failAt = Monotonic();
        LOG("[Loot] cache ready=%d tiers=%zu packages=%zu (junk tier=%d pkg=%d)",
            (int)g_cache.ready, g_cache.tiers.size(), g_cache.packages.size(), tierJunk, pkgJunk);
    }

    // [F33] Ammo companion entry (Erbium SetupLDSForPackage step 5): match the
    // ammo definition's owned gameplay tags against FortWeaponPickupSpawnAmmo-
    // Data rows. Degrades to 0 (no companion ammo) on any resolution failure.
    static int AmmoCountFor(void* ammoDef, int worldLevel) {
        (void)worldLevel;
        void* dataObj = FindObj("/Game/Athena/Balance/Pickups/FortWeaponPickupSpawnAmmoData.FortWeaponPickupSpawnAmmoData");
        if (!dataObj || IsBadPtr(dataObj)) return 0;
        void* arrProp = GetProp("/Script/FortniteGame.FortWeaponPickupSpawnAmmoData.WeaponPickupAmmoCountArray");
        if (!arrProp) return 0;
        int stride = Read<int>(arrProp, profile::PropElementSize);
        if (stride < 12 || stride > 64) return 0;
        Array rows = Read<Array>(GetField(dataObj, arrProp));
        if (!rows.Data || IsBadPtr(rows.Data) || rows.Count <= 0 || rows.Count > 32) return 0;
        void* inner = Read<void*>(arrProp, 0x70);            // FArrayProperty.Inner
        void* rs = inner ? Read<void*>(inner, 0x70) : nullptr; // FStructProperty.Struct
        if (!rs || IsBadPtr(rs)) return 0;
        int tagOff = PropOffsetOn(rs, "AmmoItemDefinitionTag");
        int cntOff = PropOffsetOn(rs, "SpawnCount");
        if (tagOff < 0 || cntOff < 0) return 0;
        float mult = 1.0f;
        void* multProp = GetProp("/Script/FortniteGame.FortWeaponPickupSpawnAmmoData.DefaultWeaponAmmoMultiplier");
        if (multProp && Read<int>(multProp, profile::PropElementSize) >= 4) {
            float m = Read<float>(GetField(dataObj, multProp));
            if (m > 0) mult = m;
        }
        // owned tags of the ammo definition (IGameplayTagAssetInterface vft[2])
        uint8_t tags[64]{};
        bool haveTags = false;
        void* ifaceType = FindObj("/Script/GameplayAbilities.GameplayTagAssetInterface");
        if (!ifaceType) ifaceType = FindObj("/Script/GameplayAbilities.IGameplayTagAssetInterface");
        if (ifaceType) {
            void* iface = ResolveInterface(ammoDef, ifaceType);
            // [G20] both the interface proxy AND the code pointer are
            // kernel-validated before the raw vtable call (the one unguarded
            // native call in the roll pipeline; it also runs under the
            // drain/interact fault guard — this removes the common case).
            if (iface && !IsBadPtr(iface) && IsReadable(iface, 0x18)) {
                auto getTags = (void(*)(void*, void*))Read<uintptr_t>(iface, 0x10);
                if (getTags && !IsBadPtr((void*)getTags) && IsReadable((void*)getTags, 4)) {
                    getTags(iface, tags);
                    haveTags = true;
                }
            }
        }
        bool found = false;
        int out = 0;
        for (int i = 0; i < rows.Count && !found; ++i) {
            void* row = (char*)rows.Data + (size_t)i * stride;
            uint32_t rowTag = Read<uint32>(row, tagOff);
            bool match = true;
            if (haveTags) {
                match = false;
                Array owned = Read<Array>(tags, 0);
                if (owned.Data && !IsBadPtr(owned.Data) && owned.Count > 0 && owned.Count <= 64)
                    for (int t = 0; t < owned.Count; ++t)
                        if (Read<uint32>(owned.Data, (size_t)t * 8) == rowTag) { match = true; break; }
            }
            if (!match) continue;
            float count = Read<float>(row, cntOff);
            out = count > 0 ? (int)(count * mult) : 0;
            found = true;
        }
        if (!found && !haveTags) {
            float count = Read<float>(rows.Data, cntOff);   // tags unavailable — first row
            out = count > 0 ? (int)(count * mult) : 0;
        }
        return out;
    }

    // [F32] Item level from the def's LootLevelData handle (Era GetLevel 1:1).
    static int ItemLevelOf(void* def, int worldLevel) {
        if (!def || worldLevel <= 0) return 0;
        void* p = GetProp("/Script/FortniteGame.FortItemDefinition.LootLevelData");
        if (!p) return 0;
        void* handle = GetField(def, p);
        void* table = Read<void*>(handle, 0);
        uint32_t rowContents = Read<uint32>(handle, 16);   // FDataTableCategoryHandle.RowContents
        if (!table || IsBadPtr(table) || !rowContents) return 0;
        void* rs = Read<void*>(table, 0x28);
        if (!rs || IsBadPtr(rs)) return 0;
        int rowSize = TableRowSizeOf(rs, nullptr, 0);   // [F94-b] walk-first — Category/LootLevel validated below
        if (rowSize <= 0 || rowSize > 512) return 0;
        int catOff = PropOffsetOn(rs, "Category");
        if (catOff < 0) catOff = PropOffsetOn(rs, "category");
        int lvlOff = PropOffsetOn(rs, "LootLevel");
        int minOff = PropOffsetOn(rs, "MinItemLevel");
        int maxOff = PropOffsetOn(rs, "MaxItemLevel");
        if (catOff < 0 || lvlOff < 0) return 0;
        std::vector<FName> names;
        if (!DataTableRowNames(table, names)) return 0;
        int best = 0, bestLevel = 0;
        uint8_t buf[512];
        for (auto& n : names) {
            memset(buf, 0, sizeof(buf));
            if (!DataTableRow(table, n, buf, rowSize)) continue;
            if (Read<uint32>(buf, catOff) != rowContents) continue;
            int32_t ll = Read<int32>(buf, lvlOff);
            if (ll > worldLevel || ll <= bestLevel) continue;
            bestLevel = ll;
            int32_t mn = minOff >= 0 ? Read<int32>(buf, minOff) : 0;
            int32_t mx = maxOff >= 0 ? Read<int32>(buf, maxOff) : 0;
            int32_t span = mx - mn;
            int level = mn;
            if (span > 0) level = mn + (int)(random01() * float(span + 1));   // Erbium uniform pick
            if (level < 0) level = 0;
            best = level;
        }
        if (best > 0) {
            int32_t minL = 0, maxL = 0;
            if (GetFieldValue<int32>(def, "/Script/FortniteGame.FortItemDefinition.MinLevel", minL) &&
                GetFieldValue<int32>(def, "/Script/FortniteGame.FortItemDefinition.MaxLevel", maxL) && maxL > 0)
                best = std::clamp(best, minL, maxL);
        }
        return best;
    }

    static void SetupLDSForPackage(std::vector<Drop>& out, const std::string& package, int category, int worldLevel, bool deep) {
        auto it = g_cache.pkgIndex.find(package);
        if (it == g_cache.pkgIndex.end() || it->second.empty()) return;
        std::vector<LPkgRow*> rows;
        for (LPkgRow* r : it->second) {
            if (category != -1 && r->category != category) continue;
            if (worldLevel >= 0) {
                if (r->minW >= 0 && worldLevel < r->minW) continue;
                if (r->maxW >= 0 && worldLevel > r->maxW) continue;
            }
            rows.push_back(r);
        }
        if (rows.empty()) return;
        LPkgRow* pkg = PickWeighted(rows);
        if (!pkg) return;
        if (pkg->call.size() > 1) {
            int times = pkg->count > 0 ? pkg->count : 1;
            for (int i = 0; i < times; ++i) SetupLDSForPackage(out, pkg->call, 0, worldLevel, deep);   // Erbium recursion
            return;
        }
        g_lootStage = "resolve-def";   // [G20]
        void* def = ResolveSoft(*pkg, deep);
        if (!def) return;
        // [F33] ammo companion entry
        Drop ammoDrop{nullptr, 0, 0};
        {
            Call g("/Script/FortniteGame.FortWorldItemDefinition.GetAmmoWorldItemDefinition_BP");
            if (g.ok() && g.run(def)) {
                void* ammoDef = g.result<void*>();
                if (ammoDef && !IsBadPtr(ammoDef)) {
                    int finalCount = AmmoCountFor(ammoDef, worldLevel);
                    if (finalCount > 0) ammoDrop = {ammoDef, finalCount, 0};
                }
            }
        }
        // Stack merging with MaxStackSize clamping (Erbium 1:1)
        int maxStack = MaxStackSizeOf(def);
        if (maxStack <= 0) maxStack = 1;
        for (auto& d : out) {
            if (d.def != def || d.count >= maxStack) continue;
            d.count += pkg->count;
            if (maxStack > 1 && d.count > maxStack) {
                out.push_back({def, d.count - maxStack, d.level});
                d.count = maxStack;
            }
            if (ammoDrop.def) {
                int ammoMax = MaxStackSizeOf(ammoDrop.def);
                for (auto& a : out) {
                    if (a.def != ammoDrop.def || ammoMax <= 1 || a.count >= ammoMax) continue;
                    a.count += ammoDrop.count;
                    if (a.count > ammoMax) { out.push_back({ammoDrop.def, a.count - ammoMax, 0}); a.count = ammoMax; }
                    ammoDrop.def = nullptr;
                    break;
                }
            }
            return;
        }
        if (pkg->count > 0) out.push_back({def, pkg->count, ItemLevelOf(def, worldLevel)});
        if (ammoDrop.def) out.push_back(ammoDrop);
    }

    // Erbium ChooseLootForContainer 1:1 (weighted tier row → DropCount with
    // probabilistic fraction → category min/max bumping → package execution).
    static void ChooseLootForContainer(std::vector<Drop>& out, const std::string& tierGroup, int lootTier, int worldLevel, bool deep) {
        auto it = g_cache.tierIndex.find(tierGroup);
        if (it == g_cache.tierIndex.end() || it->second.empty()) return;
        std::vector<LTierRow*> rows;
        for (LTierRow* r : it->second)
            if (lootTier < 0 || r->lootTier == lootTier) rows.push_back(r);
        if (rows.empty()) return;
        LTierRow* tier = PickWeighted(rows);
        if (!tier || tier->numDrops <= 0) return;
        int dropCount;
        if (tier->numDrops < 1.f) dropCount = 1;
        else {
            dropCount = (int)((tier->numDrops * 2.f) - .5f) >> 1;
            float rem = tier->numDrops - (float)dropCount;
            if (rem > 0.0000099999997f && rem >= random01()) dropCount += 1;
        }
        std::map<int32_t, int32_t> numMap, weightMap;
        int amountOfDrops = 0, sumWeights = 0;
        for (size_t i = 0; i < tier->minArr.size(); ++i) { numMap[(int32_t)i] = tier->minArr[i]; amountOfDrops += tier->minArr[i]; }
        for (size_t i = 0; i < tier->wArr.size(); ++i) {
            int32_t cat = (int32_t)i, w = tier->wArr[i];
            int32_t mx = i < tier->maxArr.size() ? tier->maxArr[i] : -1;
            if (w > 0 && (mx < 0 || numMap[cat] < mx)) { weightMap[cat] = w; sumWeights += w; }
        }
        while (sumWeights > 0 && amountOfDrops < dropCount) {
            int randomWeight = (int)std::floor(random01() * (float)sumWeights);
            int category = -1;
            for (auto& kv : weightMap) {
                if (randomWeight <= kv.second && randomWeight <= (int)tier->numDrops) { category = kv.first; break; }
                randomWeight -= kv.second;
            }
            if (category == -1) break;
            amountOfDrops++;
            numMap[category]++;
            int32_t mx = category < (int)tier->maxArr.size() ? tier->maxArr[category] : -1;
            if (mx >= 0 && numMap[category] >= mx) { sumWeights -= tier->wArr[category]; weightMap.erase(category); }
            if (amountOfDrops >= dropCount) break;
        }
        for (auto& kv : numMap)                                    // ascending categories
            for (int i = 0; i < kv.second; ++i)
                SetupLDSForPackage(out, tier->package, kv.first, worldLevel, deep);
    }

    // Entry point for SearchContainer — true when the DataTable path exists.
    // deep: interactive opens may sync-load missing item defs (one at a time,
    // under the caller's fault guard); the batch drain is resident-only.
    static bool RollGroup(const std::string& group, std::vector<Drop>& out, bool deep) {
        g_lootStage = "cache";   // [G20]
        BuildCache();
        if (!g_cache.ready) return false;
        g_lootStage = "roll";   // [G20]
        ChooseLootForContainer(out, group, -1, WorldLevelNow(), deep);
        return true;
    }
    // [G20] fault backoff control — called by the drain circuit breaker when
    // spawner rolls keep faulting with the cache unbuilt (BuildCache retrying
    // into the same fault forever would spin the guard).
    static void HoldCache(double seconds) { g_cache.holdUntil = Monotonic() + seconds; }
}

// [F32] GetAllActorsOfClass with a CORRECT out-param read. The old code used
// Call::result<Array>() which only reads ReturnParm params — GetAllActors-
// OfClass returns void with an out TArray, so the old floor-loot pass always
// saw 0 actors (one of the reasons the map had no loot at all).
static bool GetAllActorsOfClassRaw(void* klass, std::vector<void*>& out) {
    out.clear();
    void* wb = FindObj("/Script/Engine.Default__GameplayStatics");
    if (!wb || !klass || IsBadPtr(klass) || !session.world) return false;
    Call gac("/Script/Engine.GameplayStatics.GetAllActorsOfClass");
    if (!gac.ok()) return false;
    Array empty{};
    if (!gac.set("WorldContextObject", session.world) ||
        !gac.set("ActorClass", klass) ||
        !gac.set_bytes("OutActors", &empty, sizeof(Array))) return false;
    if (!gac.run(wb)) return false;
    void* slot = gac.param_slot("OutActors");
    if (!slot) return false;
    Array res = Read<Array>(slot);
    if (res.Count < 0 || res.Count > 65536) { if (res.Data) Fn<void(*)(void*)>(profile::Free)(res.Data); return false; }
    out.reserve((size_t)res.Count);
    for (int i = 0; i < res.Count; ++i) {
        void* a = Read<void*>(res.Data, (size_t)i * 8);
        if (a && !IsBadPtr(a)) out.push_back(a);
    }
    if (res.Data) Fn<void(*)(void*)>(profile::Free)(res.Data);
    return true;
}

// [F34] Standalone worlds roll containers with EMPTY native caches (the
// server-side FinishWorldInitialization never runs here), so any bAlready-
// Searched flag observed before the first interaction is a dead roll. Reset
// it (honoring bStartAlreadySearched_Athena) so chests/ammo actually open.
void Athena::ResetStaleContainers() {
    void* cls = FindObj("/Script/FortniteGame.BuildingContainer");
    if (!cls) return;
    std::vector<void*> actors;
    if (!GetAllActorsOfClassRaw(cls, actors)) return;
    int reset = 0;
    for (void* c : actors) {
        if (!c || IsBadPtr(c)) continue;
        if (!GetBool(c, "/Script/FortniteGame.BuildingContainer.bAlreadySearched")) continue;
        if (GetBool(c, "/Script/FortniteGame.BuildingContainer.bStartAlreadySearched_Athena")) continue;
        SetBool(c, "/Script/FortniteGame.BuildingContainer.bAlreadySearched", false);
        ++reset;
    }
    LOG("[Loot] stale-search reset on %d/%zu containers", reset, actors.size());
}

// [F06/F20/F32/F33] Container search: Erbium DataTable roll first, curated
// table fallback, container-axis placement + guarded toss.
bool Athena::SearchContainer(void* actor, const Vector& location, bool force, bool floorLoot) {
    void* searchedProp = GetProp("/Script/FortniteGame.BuildingContainer.bAlreadySearched");
    if (!actor || !searchedProp) return false;
    if (!force && GetBool(actor, "/Script/FortniteGame.BuildingContainer.bAlreadySearched")) return true;

    // Read the container's loot tier group (FName).
    g_lootStage = "group-read";   // [G20]
    std::string group;
    void* groupProp = GetProp("/Script/FortniteGame.BuildingContainer.SearchLootTierGroup");
    if (groupProp) {
        FStringNative text{};
        Fn<void(*)(const void*, FStringNative*)>(profile::NameAppendString)(GetField(actor, groupProp), &text);
        group = Narrow(text.Data, 128);
        if (text.Data) Fn<void(*)(void*)>(profile::Free)(text.Data);
    }
    if (group == "Loot_Treasure") group = "Loot_AthenaTreasure";
    if (group == "Loot_Ammo") group = "Loot_AthenaAmmoLarge";

    std::vector<local_loot::Drop> drops;
    // [G20] deep = interactive opens (chest/ammo box): the roll may sync-load
    // a missing item def, one at a time, under the caller's fault guard. The
    // batch drain is RESIDENT-ONLY — no LoadAsset_Blocking, ever.
    bool usedTables = local_loot::RollGroup(group, drops, /*deep=*/!floorLoot);
    // [G19-c] the curated table is now a SAFETY NET, not just a missing-table
    // fallback: a DataTable roll that finds no tier row for this group (or
    // rolls zero categories) must not leave the container EMPTY — that is
    // the "no loot anywhere on the ground" symptom in one line.
    if (!usedTables || drops.empty()) {
        g_lootStage = "curated";   // [G20]
        // Curated fallback (DataTable roll unavailable).
        const local_loot::Tier* tier = nullptr;
        for (auto& t : local_loot::tiers)
            if (group == t.group) { tier = &t; break; }
        if (!tier) { LOG("[Loot] group not ported: %s", group.c_str()); return false; }
        int weapons = tier->weaponsMax > tier->weaponsMin ? local_loot::rollBetween(tier->weaponsMin, tier->weaponsMax) : tier->weaponsMin;
        int ammo = tier->ammoMax > tier->ammoMin ? local_loot::rollBetween(tier->ammoMin, tier->ammoMax) : tier->ammoMin;
        for (int i = 0; i < weapons; ++i)
            if (void* def = local_loot::pickWeapon(tier->rarityBoost)) drops.push_back({def, 1, 0});
        for (int i = 0; i < ammo; ++i)
            if (void* def = local_loot::pickRow(local_loot::ammo, sizeof(local_loot::ammo) / sizeof(local_loot::Row))) {
                int cnt = 30 + (int)(local_loot::random01() * 60.0f);
                drops.push_back({def, cnt, 0});
            }
        for (int i = 0; i < tier->resources; ++i)
            if (void* def = local_loot::pickRow(local_loot::resources, sizeof(local_loot::resources) / sizeof(local_loot::Row)))
                drops.push_back({def, 50, 0});
        // [G20] curated minimum — a matched tier that rolled 0 of everything
        // (all minimums zero, e.g. the Warmup group) still yields one weapon
        // + one ammo: "roll produced no drops" must never fire while the
        // tables are alive.
        if (drops.empty()) {
            if (void* def = local_loot::pickWeapon(tier->rarityBoost)) drops.push_back({def, 1, 0});
            if (void* def = local_loot::pickRow(local_loot::ammo, sizeof(local_loot::ammo) / sizeof(local_loot::Row)))
                drops.push_back({def, 30 + (int)(local_loot::random01() * 60.0f), 0});
            if (!drops.empty()) LOG("[Loot] curated minimum applied for %s [G20]", group.c_str());
        }
    }
    if (drops.empty()) { LOG("[Loot] roll produced no drops for %s", group.c_str()); return false; }

    // [G20] one-shot first-roll summary — proves the pipeline end-to-end on
    // the device log (or names the stage where it stopped producing).
    static bool s_firstRollLogged = false;
    if (!s_firstRollLogged) {
        s_firstRollLogged = true;
        std::string defs;
        int shown = 0;
        for (auto& d : drops) {
            if (shown++) defs += ",";
            defs += ObjName(d.def);
            if (shown >= 3) break;
        }
        LOG("[Loot] first roll: group=%s tables=%d drops=%zu defs=[%s] — pipeline live [G20]",
            group.c_str(), (int)usedTables, drops.size(), defs.c_str());
    }

    // [F33] Container-relative placement (Erbium SpawnPickup(Container) 1:1):
    // container origin offset by LootSpawnLocation_Athena along F/R/U axes.
    // [F-loot-place] floor loot skips ALL of this — the spawner point IS the
    // loot point. The old path also burned 3 axis UFunction calls × 3173
    // spawners (≈9500 native calls of pure waste in the batch stall).
    Vector fwd{1.0, 0.0, 0.0}, right{0.0, 1.0, 0.0}, up{0.0, 0.0, 1.0};
    Vector off{60.0, 0.0, 40.0};
    g_lootStage = "place";   // [G20]
    if (!floorLoot) {
        {
            Call c("/Script/Engine.Actor.GetActorForwardVector");
            if (c.ok() && c.run(actor)) fwd = c.result<Vector>();
            c = Call("/Script/Engine.Actor.GetActorRightVector");
            if (c.ok() && c.run(actor)) right = c.result<Vector>();
            c = Call("/Script/Engine.Actor.GetActorUpVector");
            if (c.ok() && c.run(actor)) up = c.result<Vector>();
        }
        {
            void* p = GetProp("/Script/FortniteGame.BuildingContainer.LootSpawnLocation_Athena");
            if (p && Read<int>(p, profile::PropElementSize) == 24) off = Read<Vector>(GetField(actor, p));
            else {
                p = GetProp("/Script/FortniteGame.BuildingContainer.LootSpawnLocation");
                if (p && Read<int>(p, profile::PropElementSize) == 24) off = Read<Vector>(GetField(actor, p));
            }
        }
    }

    // Resolve/spawn every pickup first; roll back partial spawns so a retry
    // cannot duplicate loot (Era core guarantee).
    std::vector<void*> spawned;
    size_t dropCount = floorLoot ? std::min<size_t>(drops.size(), 3) : drops.size();   // [F-loot-place] hard safety cap
    for (size_t i = 0; i < dropCount; ++i) {
        Vector loc;
        if (floorLoot) {
            // [F-loot-place] EXACT spawner location — extras (companion ammo
            // etc.) on a tight 35 cm ring, exactly like real floor loot sits.
            double ang = double(i) * 2.09439510239;   // 120° steps
            double rad = i ? 35.0 : 0.0;
            loc = Vector{ location.X + std::cos(ang) * rad,
                          location.Y + std::sin(ang) * rad,
                          location.Z };
        } else {
            double spreadX = (double)(i % 3) * 30.0;
            double spreadY = (double)(i / 3) * 30.0;
            loc = Vector{
                location.X + fwd.X * off.X + right.X * off.Y + up.X * off.Z + spreadX,
                location.Y + fwd.Y * off.X + right.Y * off.Y + up.Y * off.Z + spreadY,
                location.Z + fwd.Z * off.X + right.Z * off.Y + up.Z * off.Z + 40.0
            };
        }
        uint8_t spawnSource = 9; // LootDrop
        if (group == "Loot_AthenaTreasure") spawnSource = 2; // Chest
        if (group.find("Ammo") != std::string::npos) spawnSource = 4; // AmmoBox
        void* pickup = nullptr;
        {
            g_lootStage = "spawn";   // [G20]
            void* klass = FindObj("/Script/FortniteGame.FortPickupAthena");
            Transform t{};
            t.Location[0] = loc.X; t.Location[1] = loc.Y; t.Location[2] = loc.Z;
            pickup = klass ? SpawnActor(session.world, klass, nullptr, t) : nullptr;
            if (pickup) {
                void* entry = PickupEntry(pickup);
                void* defProp = GetProp("/Script/FortniteGame.FortItemEntry.ItemDefinition");
                void* cntProp = GetProp("/Script/FortniteGame.FortItemEntry.Count");
                void* lvlProp = GetProp("/Script/FortniteGame.FortItemEntry.Level");
                if (entry && defProp) Write<void*>(GetField(entry, defProp), 0, drops[i].def);
                if (entry && cntProp) Write<int>(GetField(entry, cntProp), 0, drops[i].count);
                if (entry && lvlProp && drops[i].level > 0 && Read<int>(lvlProp, profile::PropElementSize) == 4)
                    Write<int>(GetField(entry, lvlProp), 0, drops[i].level);
                Write<uint8_t>(pickup, profile::AFortPickup_SourceFlags, 4);
                Write<uint8_t>(pickup, profile::AFortPickup_SpawnSource, spawnSource);
                SetBool(pickup, "/Script/FortniteGame.FortPickup.bRandomRotation", true);   // [F33]
                Call rep("/Script/FortniteGame.FortPickup.OnRep_PrimaryPickupItemEntry");
                if (rep.ok()) rep.run(pickup);
                // [F35] Erbium SpawnPickup(Container) parity: without TossPickup
                // the loot hovers where it spawned (above the chest) and the
                // pickup overlap never sees it properly — toss it out like the
                // native chest does (fallback branch of the Erbium container
                // spawner) and mark the container-toss FX.
                // [F-loot-place] FLOOR LOOT NEVER TOSSES: real floor loot sits
                // ON the spawner point (the pickup's own idle bob handles the
                // look). The toss was scattering ~6300 pickups into random
                // spots AND kept every one an active physics body — both the
                // "weird random placement" and a large part of the phone
                // choke. Chests/ammo boxes keep the toss (authentic).
                if (!floorLoot) {
                    g_lootStage = "toss";   // [G20]
                    Call tp("/Script/FortniteGame.FortPickup.TossPickup");
                    if (tp.ok()) {
                        bool bound = tp.set("FinalLocation", loc) || tp.set("InTargetLoc", loc) || tp.set("TargetLoc", loc);
                        tp.set("Pawn", session.pawn) || tp.set("ItemOwner", session.pawn);
                        int maxStack = -1;
                        tp.set("MaxStackCount", maxStack) || tp.set("OverrideMaxStackCount", maxStack);
                        bool bToss = true; tp.set("bToss", bToss);
                        bool combine = true; tp.set("bShouldCombinePickupsWhenTossCompletes", combine);
                        uint8_t sf = 4; tp.set_any(sf, "InPickupSourceTypeFlags", "PickupSourceTypeFlags");
                        tp.set_any(spawnSource, "InPickupSpawnSource", "PickupSpawnSource");
                        if (bound) tp.run(pickup);
                    }
                    SetBool(pickup, "/Script/FortniteGame.FortPickup.bTossedFromContainer", true);
                    Call r3("/Script/FortniteGame.FortPickup.OnRep_TossedFromContainer");
                    if (r3.ok()) r3.run(pickup);
                }
            }
        }
        if (!pickup) {
            for (void* previous : spawned) { Call d("/Script/Engine.Actor.K2_DestroyActor"); if (d.ok()) d.run(previous); }
            LOG("Container pickup spawn failed; search not consumed");
            return true;
        }
        spawned.push_back(pickup);
    }
    g_lootStage = "consume";   // [G20]
    SetBool(actor, "/Script/FortniteGame.BuildingContainer.bAlreadySearched", true);
    Call r1("/Script/FortniteGame.BuildingContainer.OnRep_bAlreadySearched"); if (r1.ok()) r1.run(actor);
    if (!floorLoot) {
        Call r2("/Script/FortniteGame.BuildingContainer.BounceContainer"); if (r2.ok()) r2.run(actor);
        LOG("[Loot] ✅ Container search %s spawned=%zu tables=%d", group.c_str(), spawned.size(), (int)usedTables);
    }
    // [F-loot-batch] floor-loot rolls log NOTHING per spawner — 3173 lines
    // was the log spam; the queue (flo::Drain) logs aggregates only.
    return true;
}

void Athena::Loot(void* actor) {
    if (!actor || !session.pawn) return;
    LOG("[Loot] Opening %s", ObjName(Read<void*>(actor, profile::ObjectClass)).c_str());
    Vector location{};
    bool haveLoc = ActorLocation(actor, location);

    // Local roll first (offline-deterministic); fall back to the native
    // BP_SpawnLoot when the group is not ported.
    // [G20] the whole interactive roll is fault-guarded: the first chest of
    // the session can be the one that builds the DataTable cache, and a
    // fault there must degrade to the native BP path, not kill the process.
    bool opened = false;
    if (haveLoc) {
        bool clean = gas::RunGuarded("[Interact] loot", [&] {
            opened = SearchContainer(actor, location, /*force=*/false, /*floorLoot=*/false);
        });
        if (!clean)
            LOG("[Loot] ⚠️ interactive roll faulted at stage '%s' — native fallback [G20]", g_lootStage);
    }
    if (opened) return;
    Call spawn("/Script/FortniteGame.BuildingContainer.BP_SpawnLoot");
    if (spawn.ok()) {
        if (spawn.set("PlayerPawn", session.pawn)) {
            if (spawn.run(actor)) {
                SetBool(actor, "/Script/FortniteGame.BuildingContainer.bAlreadySearched", true);
                Call r1("/Script/FortniteGame.BuildingContainer.OnRep_bAlreadySearched"); if (r1.ok()) r1.run(actor);
                Call r2("/Script/FortniteGame.BuildingContainer.BounceContainer"); if (r2.ok()) r2.run(actor);
                LOG("[Loot] ✅ native BP_SpawnLoot path");
                return;
            }
        }
    }
    LOG("[Loot] ❌ container search failed (local+native)");
}

// ============================================================================
// FLOOR LOOT — [F-loot-batch r2] queued streaming rollout (THE CRASH FIX)
// ============================================================================
// Device evidence (the ~4000-line log + crash): the old path rolled EVERY
// spawner synchronously inside a single WorldTick — 1052 in the SetupWorld
// pass, then 2958 more in the streaming pass — each spawning 2-3 pickups
// with 4-6 native UFunction calls plus an active physics toss. That is a
// ~6300-actor storm in one frame → multi-second stall → phone choke →
// crash, and SearchContainer logged one line per spawner.
// Erbium parity kept (SpawnFloorLootForContainer ≥11: roll + destroy each
// spawner exactly once), but spread over frames:
//   • SpawnFloorLoot() only ENQUEUES spawners (deduped, never rolled twice).
//   • TickFloorLootStreaming() drains a few per tick — zero frame stalls.
//   • Rediscovery every 15 s (10 passes ≈ 2.5 min) picks up streamed-in POI
//     sublevel spawners, exactly like the old streaming window.
//   • Aggregated logging: one queue line, progress every 300, one summary.
//   • Hard cap 6000 pickups (authentic map ≈ 3200 spawners) memory guard.
namespace flo {
    static std::vector<void*> pending;   // spawners waiting for a roll
    static std::set<void*>    rolled;    // dedupe across rediscovery passes
    static int    spawned = 0, failed = 0, destroyed = 0, rediscoveries = 0;
    static int    lastProgress = 0;
    static double lastRediscover = -1;
    static bool   capLogged = false, doneLogged = false;
    constexpr int MAX_PICKUPS = 6000;

    // Discover floor-loot spawners not yet rolled. Cheap by design: two
    // GetAllActorsOfClass scans and NO per-actor work (the old pass read
    // 3173 actor locations up front — another mini-stall).
    static int Enqueue() {
        if (spawned >= MAX_PICKUPS) return 0;
        static const char* const kClasses[] = {
            "/Game/Athena/Environments/Blueprints/Tiered_Athena_FloorLoot_01.Tiered_Athena_FloorLoot_01_C",
            "/Game/Athena/Environments/Blueprints/Tiered_Athena_FloorLoot_Warmup.Tiered_Athena_FloorLoot_Warmup_C",
            nullptr
        };
        int added = 0;
        for (int i = 0; kClasses[i]; ++i) {
            void* cls = FindObj(kClasses[i]);
            if (!cls) cls = LoadClass(kClasses[i], "/Script/Engine.Actor");
            if (!cls) continue;
            std::vector<void*> actors;
            if (!GetAllActorsOfClassRaw(cls, actors)) continue;
            for (void* c : actors) {
                if (!c || IsBadPtr(c) || rolled.count(c)) continue;
                pending.push_back(c);
                ++added;
            }
        }
        return added;
    }

    // [G20] Roll up to `budget` spawners this tick, EVERY spawner inside one
    // fault-guard scope (DataTable pipeline + BP fallback + destroy): a fault
    // skips that spawner, names the stage, and the flow continues. Circuit
    // breaker: 8 consecutive faults suspend the drain for 120 s (and hold an
    // unbuilt cache for 600 s) so a persistent fault cannot spin the guard.
    // Wall-clock cap 8 ms per tick — the roll is resident-only ([G20] no sync
    // loads) but the pickup spawn still costs native calls.
    static void Drain(int budget) {
        static double holdUntil = 0.0;
        static int  consecutiveFaults = 0;
        double now = Monotonic();
        if (now < holdUntil) return;
        const double deadline = now + 0.008;   // 8 ms per tick
        while (budget > 0 && !pending.empty()) {
            void* spawner = pending.back();
            pending.pop_back();
            if (rolled.count(spawner)) continue;
            rolled.insert(spawner);
            if (spawned >= MAX_PICKUPS) {
                if (!capLogged) {
                    capLogged = true;
                    LOG("[Loot] ⚠️ pickup cap %d reached — rollout stopped, %zu spawners dropped", MAX_PICKUPS, pending.size());
                }
                pending.clear();
                return;
            }
            bool ok = false, destroyedThis = false;
            {
                Vector loc{};
                bool clean = gas::RunGuarded("[Loot] spawner", [&] {
                    if (ActorLocation(spawner, loc))
                        ok = Athena::SearchContainer(spawner, loc, /*force=*/true, /*floorLoot=*/true);
                    if (!ok) {
                        // Native BP path second (it rolls the real cooked tables).
                        Call bp("/Script/FortniteGame.BuildingContainer.BP_SpawnLoot");
                        if (bp.ok() && bp.set("PlayerPawn", session.pawn)) ok = bp.run(spawner);
                    }
                    // [G23-f] destroy INSIDE the same guard — the teardown is
                    // a UFunction dispatch too and must never be an unguarded
                    // crash window in the batch drain (Call::run is guarded
                    // since G23-a; same-scope keeps one sigsetjmp per spawner).
                    Call d("/Script/Engine.Actor.K2_DestroyActor");   // [F20 1:1] spawner removed after rolling
                    if (d.ok() && d.run(spawner)) destroyedThis = true;
                });
                if (!clean) {
                    if (++consecutiveFaults >= 8) {
                        LOG("[Loot] ⚠️ %d consecutive spawner faults (last stage '%s') — drain suspended 120 s [G20]",
                            consecutiveFaults, g_lootStage);
                        if (!local_loot::TableCount()) local_loot::HoldCache(600.0);
                        consecutiveFaults = 0;
                        holdUntil = Monotonic() + 120.0;
                        return;
                    }
                } else {
                    consecutiveFaults = 0;
                }
            }
            if (ok) ++spawned; else ++failed;
            if (destroyedThis) ++destroyed;
            --budget;
            if (Monotonic() > deadline) break;
        }
        if (spawned + failed - lastProgress >= 300) {
            lastProgress = spawned + failed;
            LOG("[Loot] floor-loot progress: %d rolled (%d spawners removed)", spawned + failed, destroyed);
        }
    }
}

// [F20/F32/F34/F-loot-batch] Floor loot: enqueue every un-rolled spawner.
// The actual rolling happens in TickFloorLootStreaming — a few per tick.
void Athena::SpawnFloorLoot() {
    int added = flo::Enqueue();
    LOG("[Loot] floor-loot queued: +%d spawners (%zu pending) — rolled in batches of 12/tick [F-loot-batch]",
        added, flo::pending.size());
}

// ============================================================================
// BUILDING — [F07/F21] local preview build (Era core) + fixed RPC parse
// ============================================================================
// Unreal FRotator (Pitch/Yaw/Roll degrees) → FQuat for the 21.30 scalar
// double-precision FTransform (Era core conversion, in doubles).
static void RotatorToQuat(const Rotator& r, double out[4]) {
    const double h = 0.00872664626; // pi / 360
    double sp = std::sin(r.Pitch * h), cp = std::cos(r.Pitch * h);
    double sy = std::sin(r.Yaw * h),   cy = std::cos(r.Yaw * h);
    double sr = std::sin(r.Roll * h),  cr = std::cos(r.Roll * h);
    out[0] = cr * sp * sy - sr * cp * cy;
    out[1] = -cr * sp * cy - sr * cp * sy;
    out[2] = cr * cp * sy - sr * sp * cy;
    out[3] = cr * cp * cy + sr * sp * sy;
}

// [F30] Erbium spawn order 1:1: SpawnActorUnfinished →
// InitializeKismetSpawnedBuildingActor → FinishSpawnActor → level/OnRep →
// SetMirrored → bPlayerPlaced → Team/TeamIndex bookkeeping.
bool Athena::SpawnBuildingActor(void* klass, const Vector& loc, const Rotator& rot, bool mirrored, int level) {
    if (!session.world || !klass || IsBadPtr(klass)) return false;
    void* smCls = FindObj("/Script/FortniteGame.BuildingSMActor");
    if (!smCls || !SubclassOfFast(klass, smCls)) return false;
    Transform t{};
    t.Location[0] = loc.X; t.Location[1] = loc.Y; t.Location[2] = loc.Z;
    RotatorToQuat(rot, t.Rotation);
    void* actor = Fn<void*(*)(void*, void*, const Transform*, uint8_t, void*)>(profile::BeginSpawn)(session.world, klass, &t, 1, session.controller);
    if (!actor) { LOG("[Build] BeginSpawn failed"); return false; }
    bool initOk = false;
    {
        Call init("/Script/FortniteGame.BuildingActor.InitializeKismetSpawnedBuildingActor");
        if (init.ok()) {
            init.set("BuildingOwner", actor);
            if (!init.set("SpawningController", session.controller))
                init.set("CreatingController", session.controller);
            bool animations = true;
            init.set("bUsePlayerBuildAnimations", animations);
            initOk = init.run(actor);
        }
    }
    void* finished = Fn<void*(*)(void*, const Transform*)>(profile::FinishSpawn)(actor, &t);
    if (!finished) { LOG("[Build] FinishSpawn failed"); return false; }
    if (!initOk) {
        // [F30] Erbium parity: abort the placement when init fails (the old
        // hook destroyed AFTER FinishSpawn, leaving partial state behind).
        LOG("[Build] init failed — piece destroyed (Erbium parity)");
        Call d("/Script/Engine.Actor.K2_DestroyActor");
        if (d.ok()) d.run(actor);
        return false;
    }
    SetBool(actor, "/Script/FortniteGame.BuildingActor.bPlayerPlaced", true);
    // [F03] SetMirrored lives on BuildingSMActor in 21.30.
    Call mirror("/Script/FortniteGame.BuildingSMActor.SetMirrored");
    if (mirror.ok()) { bool m = mirrored; if (mirror.set("bIsMirrored", m)) mirror.run(actor); }
    if (level > 0) {
        void* lvlProp = GetProp("/Script/FortniteGame.BuildingSMActor.CurrentBuildingLevel");
        if (lvlProp && Read<int>(lvlProp, profile::PropElementSize) == 4)
            Write<int32>(GetField(actor, lvlProp), 0, level);
        Call lr("/Script/FortniteGame.BuildingSMActor.OnRep_CurrentBuildingLevel");
        if (lr.ok()) lr.run(actor);
    }
    // [F30] Team bookkeeping (Erbium: PlayerState.TeamIndex → Team/TeamIndex).
    void* ps = GetObjField(session.controller, "/Script/Engine.Controller.PlayerState");
    if (ps && !IsBadPtr(ps)) {
        void* tiProp = GetProp("/Script/FortniteGame.FortPlayerStateAthena.TeamIndex");
        if (!tiProp) tiProp = GetProp("/Script/FortniteGame.FortPlayerState.TeamIndex");
        void* teamProp = GetProp("/Script/FortniteGame.BuildingSMActor.Team");
        void* ti2Prop = GetProp("/Script/FortniteGame.BuildingSMActor.TeamIndex");
        if (tiProp && teamProp && Read<int>(tiProp, profile::PropElementSize) == 1) {
            uint8_t team = Read<uint8_t>(GetField(ps, tiProp));
            Write<uint8_t>(GetField(actor, teamProp), 0, team);
            if (ti2Prop && Read<int>(ti2Prop, profile::PropElementSize) == 1)
                Write<uint8_t>(GetField(actor, ti2Prop), 0, team);
        }
    }
    Call net("/Script/Engine.Actor.ForceNetUpdate");
    if (net.ok()) net.run(actor);
    return true;
}

// [F21/F29/F30] Build from the controller's local preview state. The verified
// controller offsets (0x19B0/0x19C8/0x1898) are the primary source — the old
// reflection-only read was why building never fired on this build.
bool Athena::LocalBuild() {
    if (!session.controller || !session.world) return false;
    Vector loc{};
    Rotator rot{};
    loc = Read<Vector>(session.controller, profile::Controller_PreviewSnapLoc);
    rot = Read<Rotator>(session.controller, profile::Controller_PreviewSnapRot);
    if (!std::isfinite(loc.X) || !std::isfinite(loc.Y) || !std::isfinite(loc.Z))
        GetFieldValue<Vector>(session.controller, "/Script/FortniteGame.FortPlayerController.LastBuildPreviewGridSnapLoc", loc);
    if (!std::isfinite(rot.Pitch) || !std::isfinite(rot.Yaw) || !std::isfinite(rot.Roll))
        GetFieldValue<Rotator>(session.controller, "/Script/FortniteGame.FortPlayerController.LastBuildPreviewGridSnapRot", rot);
    void* klass = nullptr;
    {
        void* classProp = GetProp("/Script/FortniteGame.FortPlayerController.CurrentBuildableClass");
        if (classProp) klass = Read<void*>(GetField(session.controller, classProp));
        if (!klass || IsBadPtr(klass)) klass = Read<void*>(session.controller, profile::Controller_CurrentBuildableClass);
    }
    void* smCls = FindObj("/Script/FortniteGame.BuildingSMActor");
    if (!klass || IsBadPtr(klass) || !smCls || !SubclassOfFast(klass, smCls) || !NearPlayer(loc, 4000.0f) ||
        !std::isfinite(rot.Pitch) || !std::isfinite(rot.Yaw) || !std::isfinite(rot.Roll)) {
        LOG("[Build] invalid selected class or preview transform");
        return true;
    }
    // Mirroring from the controller state (bBuildPreviewMirrored @0x1859).
    bool mirrored = Read<uint8_t>(session.controller, profile::Controller_BuildPreviewMirrored) != 0;

    // [F29] Dedup: identical class+pose within 0.25s (touch repeat) plus the
    // shared guard against the ServerCreateBuildingActor RPC path.
    double pose[6] = {loc.X, loc.Y, loc.Z, rot.Pitch, rot.Yaw, rot.Roll};
    double now = Monotonic();
    if (now - session.rpc_build_time < 0.6) return true;
    if (klass == session.last_build_class && now - session.last_build_time < 0.25) {
        bool same = true;
        for (int i = 0; i < 6; ++i) if (session.last_build_pose[(size_t)i] != pose[i]) { same = false; break; }
        if (same) return true;
    }
    LOG("[Build] preview %s at %.0f %.0f %.0f", ObjName(klass).c_str(), loc.X, loc.Y, loc.Z);
    if (SpawnBuildingActor(klass, loc, rot, mirrored, 0)) {
        session.last_build_class = klass;
        session.last_build_time = now;
        for (int i = 0; i < 6; ++i) session.last_build_pose[(size_t)i] = pose[i];
    }
    return true;
}

// [F21] Mobile touch placement: poll the touch-primary-fire hold while in
// build mode and place when the preview pose changes (Era core behaviour).
void Athena::PollTouchBuild() {
    if (!session.controller || !session.world) return;
    double now = Monotonic();
    if (now < session.next_build_poll) return;
    session.next_build_poll = now + 0.05;
    bool held = false;
    if (!GetFieldValue<bool>(session.controller, "/Script/FortniteGame.FortPlayerController.bHoldingPrimaryFireFromTouch", held)) return;
    if (!held) return;
    Call mode("/Script/FortniteGame.FortPlayerController.IsInBuildMode");
    if (!mode.ok() || !mode.run(session.controller) || !mode.result<bool>()) return;
    Vector loc{};
    Rotator rot{};
    if (!GetFieldValue<Vector>(session.controller, "/Script/FortniteGame.FortPlayerController.LastBuildPreviewGridSnapLoc", loc) ||
        !GetFieldValue<Rotator>(session.controller, "/Script/FortniteGame.FortPlayerController.LastBuildPreviewGridSnapRot", rot)) return;
    void* classProp = GetProp("/Script/FortniteGame.FortPlayerController.CurrentBuildableClass");
    void* klass = classProp ? Read<void*>(GetField(session.controller, classProp)) : nullptr;
    double pose[6] = {loc.X, loc.Y, loc.Z, rot.Pitch, rot.Yaw, rot.Roll};
    if (session.held_build_class == klass && now - session.held_build_time < 0.25) {
        bool same = true;
        for (int i = 0; i < 6; ++i) if (session.held_build_pose[(size_t)i] != pose[i]) { same = false; break; }
        if (same) return;
    }
    session.held_build_class = klass;
    session.held_build_time = now;
    for (int i = 0; i < 6; ++i) session.held_build_pose[(size_t)i] = pose[i];
    LOG("[Build] touch placement confirm observed");
    LocalBuild();
}

// [F07/F30] ServerCreateBuildingActor parse — FCreateBuildingActorData (0x50):
//   BuildingClassHandle u32 @0x00, BuildLoc FVector @0x08, BuildRot FRotator
//   @0x20, bMirrored @0x38, SyncKey f32 @0x3C, FBuildingClassData @0x40
//   (BuildingClass UClass* @0x40, PreviousBuildingLevel i32 @0x48,
//   UpgradeLevel i32 @0x4C).
// The v20+ wire format carries ONLY BuildingClassHandle from the client —
// the class is resolved locally from CurrentBuildableClass (exactly the piece
// the preview is showing; Erbium BuildingClassMap parity).
void Athena::OnServerCreateBuildingActor(void* params) {
    if (!params) return;
    void* fn = FindObj("/Script/FortniteGame.FortPlayerController.ServerCreateBuildingActor");
    uint16_t total = fn ? Read<uint16_t>(fn, profile::FunctionParamSize) : 0;
    if (total == 0x50) {
        Vector loc = Read<Vector>(params, 0x08);
        Rotator rot = Read<Rotator>(params, 0x20);
        bool mirrored = Read<uint8_t>(params, 0x38) != 0;
        int32_t upgradeLevel = Read<int32>(params, 0x4C);
        void* cls = Read<void*>(params, 0x40);
        if (!cls || IsBadPtr(cls)) {
            void* cp = GetProp("/Script/FortniteGame.FortPlayerController.CurrentBuildableClass");
            if (cp) cls = Read<void*>(GetField(session.controller, cp));
            if (!cls || IsBadPtr(cls)) cls = Read<void*>(session.controller, profile::Controller_CurrentBuildableClass);
        }
        if (!cls || IsBadPtr(cls)) { LOG("[Build] RPC class unresolved — preview fallback"); LocalBuild(); return; }
        if (!session.world || !NearPlayer(loc, 4000.0f)) { LOG("[Build] RPC rejected (far/no world)"); return; }
        if (SpawnBuildingActor(cls, loc, rot, mirrored, upgradeLevel)) {
            double now = Monotonic();
            session.last_build_class = cls;
            session.last_build_time = now;
            session.rpc_build_time = now;   // [F29] guard the touch-poll path
            LOG("[Build] ✅ RPC placement %s lvl=%d mirrored=%d", ObjName(cls).c_str(), upgradeLevel, (int)mirrored);
        }
    } else {
        LOG("[Build] unexpected ServerCreateBuildingActor size %u — using local preview", total);
        LocalBuild();
    }
}

// [G08] Erbium SetEditingPlayer (AFortPlayerControllerAthena.cpp): authority
// bookkeeping while a piece is being edited — EditingPlayer + OnRep + the
// SetNetDormancy(Initial while editing / DormantAll when released) +
// ForceNetUpdate pair. Standalone IS the authority, so the guard passes.
static void SetEditingPlayer(void* building, void* newEditor) {
    if (!building || IsBadPtr(building)) return;
    void* ep = GetProp("/Script/FortniteGame.BuildingSMActor.EditingPlayer");
    if (ep && Read<int>(ep, profile::PropElementSize) == 8) {
        Write<void*>(GetField(building, ep), 0, newEditor);
        Call r("/Script/FortniteGame.BuildingSMActor.OnRep_EditingPlayer");
        if (r.ok()) r.run(building);
    }
    Call d("/Script/Engine.Actor.SetNetDormancy");
    if (d.ok()) {
        uint8_t dorm = newEditor ? 1 : 2;   // DORM_Initial while edited, DORM_DormantAll when released
        if (d.set("Dormancy", dorm)) d.run(building);
    }
    Call n("/Script/Engine.Actor.ForceNetUpdate");
    if (n.ok()) n.run(building);
}

// [F10] Edit tool — EditActor and OnRep_EditActor live on FortWeap_EditingTool.
void Athena::OnBeginEditActor(void* piece) {
    if (!session.edit_tool || !session.pawn || !piece) return;
    // [G08] claim the piece as the editing player first (Erbium order).
    SetEditingPlayer(piece, session.player_state);
    void* inst = nullptr;
    {
        Call c("/Script/FortniteGame.FortItemDefinition.CreateTemporaryItemInstanceBP");
        int cnt = 1, lvl = 1;
        if (c.ok() && c.set("Count", cnt) && c.set("Level", lvl) && c.run(session.edit_tool)) inst = c.result<void*>();
    }
    if (!inst) return;
    Guid guid{};
    { Call g("/Script/FortniteGame.FortItem.GetItemGuid"); if (g.ok() && g.run(inst)) guid = g.result<Guid>(); }
    Call eq("/Script/FortniteGame.FortPawn.EquipWeaponDefinition");
    if (!eq.ok()) return;
    bool bd = false; Guid tg{};
    eq.set("WeaponData", session.edit_tool); eq.set("ItemEntryGuid", guid);
    eq.set("TrackerGuid", tg); eq.set("bDisableEquipAnimation", bd);
    if (!eq.run(session.pawn)) return;
    void* editActor = eq.result<void*>();
    if (!editActor) return;
    void* p = GetProp("/Script/FortniteGame.FortWeap_EditingTool.EditActor");   // [F10]
    if (p) Write<void*>(GetField(editActor, p), 0, piece);
    else Write<void*>(editActor, profile::EditingTool_EditActor, piece);
    Call r("/Script/FortniteGame.FortWeap_EditingTool.OnRep_EditActor");        // [F10]
    if (r.ok()) r.run(editActor);
    LOG("[Edit] editing tool bound to %s", ObjName(piece).c_str());
}

// [F08/F10/F30] Edit replacement at the OLD actor transform (RotationIterations
// × 90° yaw), keeping the building level — the old code spawned the new piece
// at the world origin.
void Athena::OnFinishEditActor(void* actor, void* newClass, int rotIter, bool mirrored) {
    if (!actor) return;
    Vector loc{};
    Rotator rot{};
    if (!ActorLocation(actor, loc)) { LOG("[Edit] no location on edited piece"); return; }
    {
        Call c("/Script/Engine.Actor.K2_GetActorRotation");
        if (c.ok() && c.run(actor)) rot = c.result<Rotator>();
    }
    int oldLevel = 0;
    {
        void* lvlProp = GetProp("/Script/FortniteGame.BuildingSMActor.CurrentBuildingLevel");
        if (lvlProp && Read<int>(lvlProp, profile::PropElementSize) == 4)
            oldLevel = Read<int>(GetField(actor, lvlProp));
    }
    if (!std::isfinite(rot.Yaw)) rot.Yaw = 0.0;
    rot.Yaw += double(rotIter) * 90.0;
    SetBool(actor, "/Script/FortniteGame.BuildingActor.bPlayDestructionEffects", false);
    if (!newClass || IsBadPtr(newClass)) return;
    // [G08] release the editor claim before the swap (Erbium SetEditingPlayer
    // → ReplaceBuildingActor order).
    SetEditingPlayer(actor, nullptr);
    if (!SpawnBuildingActor(newClass, loc, rot, mirrored != 0, oldLevel)) return;
    // Erbium parity: the replaced piece is removed via SilentDie (no FX).
    Call sd("/Script/FortniteGame.BuildingSMActor.SilentDie");
    if (sd.ok() && sd.run(actor)) { /* replaced quietly */ }
    else { Call d("/Script/Engine.Actor.K2_DestroyActor"); if (d.ok()) d.run(actor); }
    LOG("[Edit] replaced with %s (level=%d iter=%d mirrored=%d)", ObjName(newClass).c_str(), oldLevel, rotIter, (int)mirrored);
}

void Athena::SpawnBuildPreviews() {
    void* previewCls = FindObj("BuildingPlayerPrimitivePreview");
    if (!previewCls || !session.controller) return;
    auto build = [&](const char* meshPath) -> void* {
        Transform t{};
        void* actor = SpawnActor(session.world, previewCls, session.controller, t);
        if (!actor) return nullptr;
        void* mesh = LoadAsset(meshPath, nullptr);
        void* mc = nullptr;
        Call g("/Script/FortniteGame.BuildingActor.GetBuildingMeshComponent");
        if (g.ok() && g.run(actor)) mc = g.result<void*>();
        if (mc && mesh) { Call s("/Script/Engine.StaticMeshComponent.SetStaticMesh"); if (s.ok() && s.set("NewMesh", mesh)) s.run(mc); }
        Call hide("/Script/Engine.Actor.SetActorHiddenInGame");
        bool h = true;
        if (hide.ok() && hide.set("bNewHidden", h)) hide.run(actor);
        return actor;
    };
    session.wall   = build("/Game/Building/ActorBlueprints/Player/Wood/L1/PBWA_W1_Solid.PBWA_W1_Solid");
    session.floor  = build("/Game/Building/ActorBlueprints/Player/Wood/L1/PBWA_W1_Floor.PBWA_W1_Floor");
    session.stairs = build("/Game/Building/ActorBlueprints/Player/Wood/L1/PBWA_W1_StairW.PBWA_W1_StairW");
    session.roof   = build("/Game/Building/ActorBlueprints/Player/Wood/L1/PBWA_W1_RoofC.PBWA_W1_RoofC");
}

// ============================================================================
// [F39] HARVEST — Erbium ABuildingSMActor::OnDamageServer 1:1. The pickaxe
// damage on trees/rocks/builds grants the mapped resource directly into the
// inventory (stack-clamped, overflow re-tossed) and reports it to the HUD.
// The original damage still applies: pe_hook falls through to the native.
// ============================================================================
void Athena::HandleDamageServer(void* actor, void* func, void* params) {
    if (!actor || !func || !params || !session.controller || !session.inventory) return;
    if (!IsReadable(actor, 0x60) || !IsReadable(params, 0x20)) return;
    static void* smCls = nullptr;
    if (!smCls) smCls = FindObj("/Script/FortniteGame.BuildingSMActor");
    if (!smCls) return;
    void* cls = Read<void*>(actor, profile::ObjectClass);
    if (!cls || !SubclassOfFast(cls, smCls)) return;

    void* instSlot = incoming(func, params, "InstigatedBy", 8);
    void* causeSlot = incoming(func, params, "DamageCauser", 8);
    void* dmgSlot  = incoming(func, params, "Damage", 4);
    if (!instSlot || !causeSlot || !dmgSlot) return;
    void* inst = Read<void*>(instSlot);
    void* causer = Read<void*>(causeSlot);
    float damage = Read<float>(dmgSlot);
    if (!inst || !causer) return;

    // Erbium gates: bPlayerPlaced pieces and dropped-resource gating.
    if (GetBool(actor, "/Script/FortniteGame.BuildingActor.bPlayerPlaced")) return;
    {
        // Erbium: HasbAllowResourceDrop() && !bAllowResourceDrop → no harvest.
        void* ardProp = GetProp("/Script/FortniteGame.BuildingSMActor.bAllowResourceDrop");
        if (ardProp && Read<int>(ardProp, profile::PropElementSize) == 1) {
            void* f = GetField(actor, ardProp);
            if (f && !Read<uint8_t>(f)) return;
        }
    }

    // Erbium: only the pickaxe (melee WeaponData) harvests.
    static void* weaponCls = nullptr, *meleeCls = nullptr;
    if (!weaponCls) weaponCls = FindObj("/Script/FortniteGame.FortWeapon");
    if (!meleeCls) meleeCls = FindObj("/Script/FortniteGame.FortWeaponMeleeItemDefinition");
    if (!weaponCls || !meleeCls) return;
    if (!IsA(causer, weaponCls)) return;
    void* weaponDef = GetObjField(causer, "/Script/FortniteGame.FortWeapon.WeaponData");
    if (!weaponDef || IsBadPtr(weaponDef) || !IsA(weaponDef, meleeCls)) return;

    // Resource definition for the actor's ResourceType (Kismet helper).
    uint8_t resType = 0;
    {
        void* rtProp = GetProp("/Script/FortniteGame.BuildingSMActor.ResourceType");
        if (rtProp && Read<int>(rtProp, profile::PropElementSize) == 1)
            resType = Read<uint8_t>(GetField(actor, rtProp));
    }
    void* resource = nullptr;
    {
        Call c("/Script/FortniteGame.FortKismetLibrary.K2_GetResourceItemDefinition");
        if (c.ok()) {
            uint8_t t = resType;
            if (c.set("Type", t) || c.set_at(0, t)) {
                if (c.run(FindObj("/Script/FortniteGame.Default__FortKismetLibrary")))
                    resource = c.result<void*>();
            }
        }
    }
    if (!resource || IsBadPtr(resource)) return;
    int maxMat = MaxStackSizeOf(resource);
    if (maxMat <= 0) maxMat = 999;

    // Erbium curve: BuildingResourceAmountOverride row → Out / (MaxHealth / Damage).
    int resCount = 0;
    float maxHealth = 0.0f;
    {
        void* mhProp = GetProp("/Script/FortniteGame.BuildingActor.MaxHealth");
        if (!mhProp) mhProp = GetProp("/Script/FortniteGame.BuildingSMActor.MaxHealth");
        if (mhProp && Read<int>(mhProp, profile::PropElementSize) == 4) {
            float v = Read<float>(GetField(actor, mhProp));
            if (v > 1.0f && v < 100000.0f) maxHealth = v;
        }
    }
    {
        void* broProp = GetProp("/Script/FortniteGame.BuildingSMActor.BuildingResourceAmountOverride");
        if (broProp) {
            void* handle = GetField(actor, broProp);
            void* curveTable = handle ? Read<void*>(handle, 0) : nullptr;
            FName row = handle ? Read<FName>(handle, 8) : FName{};
            if (!curveTable || IsBadPtr(curveTable) || !row.CompIdx) {
                // Class-level override lives on the row table default — try the
                // shared AthenaResourceRates table with the class's row.
                curveTable = FindObj("/Game/Athena/Balance/DataTables/AthenaResourceRates.AthenaResourceRates");
                if (!curveTable) curveTable = LoadAsset("/Game/Athena/Balance/DataTables/AthenaResourceRates.AthenaResourceRates", "/Script/Engine.DataTable");
            }
            if (curveTable && !IsBadPtr(curveTable) && row.CompIdx) {
                Call ev("/Script/Engine.DataTableFunctionLibrary.EvaluateCurveTableRow");
                if (ev.ok()) {
                    float t0 = 0.0f;
                    uint8_t zeroRow[64]{};
                    bool bound = ev.set("CurveTable", curveTable) && ev.set("RowName", row) && ev.set("InTime", t0);
                    bound |= ev.set_bytes("OutRow", zeroRow, 8);
                    if (bound && ev.run(FindObj("/Script/Engine.Default__DataTableFunctionLibrary"))) {
                        void* slot = ev.param_slot("OutValue");
                        float out = slot ? Read<float>(slot) : 0.0f;
                        if (out > 0.0f && damage > 0.0f) {
                            float denom = maxHealth > 1.0f ? maxHealth : 100.0f;
                            float rc = out / (denom / damage);
                            resCount = (int)std::lround(rc);
                        }
                    }
                }
            }
        }
        if (resCount <= 0 && damage > 0.0f) {
            // No curve row resolved — proportional fallback: 50 resources per
            // full break of the target (AthenaResourceRates ballpark).
            float denom = maxHealth > 1.0f ? maxHealth : 100.0f;
            resCount = (int)std::lround(50.0f * damage / denom);
        }
        if (resCount < 0) resCount = 0;
        if (resCount > 100) resCount = 100;   // single-hit sanity cap
    }
    if (resCount <= 0) return;

    // Erbium give: merge into the existing stack, overflow re-tossed.
    void* invProp   = GetProp("/Script/FortniteGame.FortInventory.Inventory");
    void* instProp  = GetProp("/Script/FortniteGame.FortItemList.ItemInstances");
    void* entryProp = GetProp("/Script/FortniteGame.FortItemList.ReplicatedEntries");
    void* itemEntryProp = GetProp("/Script/FortniteGame.FortWorldItem.ItemEntry");
    void* countProp = GetProp("/Script/FortniteGame.FortItemEntry.Count");
    void* defProp   = GetProp("/Script/FortniteGame.FortItemEntry.ItemDefinition");
    if (invProp && instProp && entryProp && itemEntryProp && countProp && defProp) {
        void* list = GetField(session.inventory, invProp);
        Array instances = Read<Array>(GetField(list, instProp));
        Array replicated = Read<Array>(GetField(list, entryProp));
        void* foundInst = nullptr;
        for (int i = 0; i < instances.Count; ++i) {
            void* cand = Read<void*>(instances.Data, (size_t)i * 8);
            if (cand && !IsBadPtr(cand) && ItemDefinitionOf(cand) == resource) { foundInst = cand; break; }
        }
        if (foundInst) {
            int stride = Read<int>(entryProp, profile::PropElementSize);
            for (int j = 0; j < replicated.Count && replicated.Data && stride > 0; ++j) {
                void* ent = (char*)replicated.Data + (size_t)j * stride;
                if (Read<void*>(GetField(ent, defProp)) != resource) continue;
                int updated = Read<int>(GetField(ent, countProp)) + resCount;
                if (updated > maxMat) {
                    Vector loc{};
                    if (session.pawn) ActorLocation(session.pawn, loc);
                    SpawnPickup(resource, updated - maxMat, loc, 16 /*Tossed*/, 0, true);
                    updated = maxMat;
                }
                Write<int>(GetField(ent, countProp), 0, updated);
                Write<int>(GetField(GetField(foundInst, itemEntryProp), countProp), 0, updated);
                InventoryUpdate();
                LOG("[Harvest] ✅ +%d %s (total %d)", resCount, ObjName(resource).c_str(), updated);
                break;
            }
        } else {
            int grant = resCount;
            if (grant > maxMat) {
                Vector loc{};
                if (session.pawn) ActorLocation(session.pawn, loc);
                SpawnPickup(resource, grant - maxMat, loc, 16 /*Tossed*/, 0, true);
                grant = maxMat;
            }
            if (GiveItem(resource, grant, -1, (uint8_t)-1, false))
                LOG("[Harvest] ✅ new stack +%d %s", grant, ObjName(resource).c_str());
        }
    }

    // HUD feedback (Erbium ClientReportDamagedResourceBuilding).
    {
        Call rep("/Script/FortniteGame.FortPlayerController.ClientReportDamagedResourceBuilding");
        if (rep.ok() && session.controller) {
            bool bound = rep.set_any((uint8_t)resType, "ResourceType", "Type");
            int rc = resCount; bound |= rep.set("ResCount", rc) || rep.set("Count", rc) || rep.set("ResourceCount", rc);
            bool destroyed = false; bound |= rep.set("bDestroyed", destroyed) || rep.set("Destroyed", destroyed);
            bool weak = damage >= 100.0f; bound |= rep.set("bWeakspot", weak) || rep.set("bWeakSpot", weak) || rep.set("bWeakSpotAttack", weak);
            if (bound) rep.run(session.controller);
        }
    }
    { Call n("/Script/Engine.Actor.ForceNetUpdate"); if (n.ok()) n.run(actor); }
}

// ============================================================================
// [F40] MATCH STATE — Erbium GameMode/GameState parity for the HUD:
// AlivePlayers.Add(PC) + PlayersLeft=1 + OnRep_PlayersLeft so the alive-
// players counter actually shows a number instead of staying empty.
// ============================================================================
void Athena::SetupMatchState() {
    if (session.match_state_ready || !session.world || !session.controller) return;
    // [F88] re-resolve mode/state at call time + commit ready ONLY when both
    // blocks actually ran. The old code set match_state_ready=true BEFORE the
    // blocks: if GameMode/GameState were not replicated yet on the first call
    // (end of FinalizePlayer), the flag committed and the alive-players HUD
    // stayed dead forever (same one-shot-commit family as F85).
    void* msModeProp  = GetProp("/Script/Engine.World.AuthorityGameMode");
    void* msStateProp = GetProp("/Script/Engine.World.GameState");
    void* msMode  = msModeProp  ? Read<void*>(GetField(session.world, msModeProp))  : nullptr;
    void* msState = msStateProp ? Read<void*>(GetField(session.world, msStateProp)) : nullptr;
    if (msMode && !IsBadPtr(msMode))   session.mode  = msMode;
    if (msState && !IsBadPtr(msState)) session.state = msState;
    bool aliveDone = false, leftDone = false;
    if (session.mode && !IsBadPtr(session.mode)) {
        void* apProp = GetProp("/Script/FortniteGame.FortGameMode.AlivePlayers");
        if (!apProp) apProp = GetProp("/Script/FortniteGame.FortGameModeAthena.AlivePlayers");
        if (apProp) {
            void* arrField = GetField(session.mode, apProp);
            Array cur = Read<Array>(arrField);
            bool known = false;
            for (int i = 0; i < cur.Count && cur.Data; ++i)
                if (Read<void*>(cur.Data, (size_t)i * 8) == session.controller) known = true;
            if (!known && cur.Count >= 0 && cur.Count < 256)
                Fn<int(*)(void*, void*, const void*)>(profile::GenericArrayAdd)(arrField, apProp, &session.controller);
            LOG("[MatchState] AlivePlayers count=%d", Read<Array>(arrField).Count);
        }
        aliveDone = true;
    }
    if (session.state && !IsBadPtr(session.state)) {
        void* plProp = GetProp("/Script/FortniteGame.FortGameStateAthena.PlayersLeft");
        if (plProp && Read<int>(plProp, profile::PropElementSize) == 4) {
            // [G18-a] REAL counting (Erbium parity: PlayersLeft++ per joining
            // player). No virtual lobby, no fake curve — the counter shows the
            // number of REAL alive player states (1 when alone). The win rules
            // follow the game: alone you never win while alive; dying alone
            // places you #1 and shows the win window; a real 2nd player that
            // ever joined makes last-standing trigger the victory banner.
            int total = 0;
            int alive = CountRealAlivePlayers(&total);
            if (session.players_override > 0 && alive < session.players_override)
                alive = session.players_override;   // era.players N testing floor
            if (alive < 1) alive = 1;
            Write<int>(GetField(session.state, plProp), 0, alive);
            Call n("/Script/FortniteGame.FortGameStateAthena.OnRep_PlayersLeft");
            if (n.ok()) n.run(session.state);
            session.real_alive = alive;
            if (total > session.players_ever) session.players_ever = total;
            LOG("[MatchState] PlayersLeft=%d (REAL — %d player state(s), source=%s)",
                alive, total, session.alive_source == 1 ? "PlayerArray" : "local-fallback");
        }
        leftDone = true;
    }
    // [F88] commit only on full coverage; the Tick retry (5s) re-invokes us
    // until mode/state exist, then the flag freezes the pass.
    session.match_state_ready = aliveDone && leftDone;
    if (session.match_state_ready) LOG("[MatchState] ✅ ready (alive + playersleft)");
}

// ============================================================================
// [G17-a] MISSING CITIES — Erbium FortGameMode.cpp parity (UE4.27+):
//
//   if (VersionInfo.EngineVersion >= 4.27 && floor(FortniteVersion) != 20) {
//       auto MeshNetworkSubsystem = TUObjectArray::FindFirstObject("MeshNetworkSubsystem");
//       if (MeshNetworkSubsystem)
//           *(uint8_t*)(MeshNetworkSubsystem + GetOffset("NodeType")) = 2;
//   }
//
// Chapter 3+ geometry (whole POIs, city buildings) streams through the mesh
// network. NodeType selects the streaming mode; a standalone client world
// sits in a client node type that waits for server-driven mesh data that
// never arrives — distant POIs never materialise ("some cities are missing").
// NodeType 2 (Erbium's standalone value) makes the subsystem self-sufficient:
// every mesh loads locally exactly like the Erbium game server does it.
// v20 is skipped by Erbium ("does some weird stuff") — 21.30 takes the fix.
// ============================================================================
void Athena::FixMeshNetwork() {
    if (session.mesh_network_fixed || !session.world) return;
    // [G19-d] REWRITTEN — the old GObjects name scan never produced a single
    // log line on device (meshnet=0 forever): it bailed silently before the
    // scan on every attempt. Resolve the subsystem the canonical way instead
    // — UGameplayStatics::GetGameInstanceSubsystem(WorldContextObject, Class)
    // runs entirely through StaticFindObject + ProcessEvent, both proven on
    // this build — with the CDO (Default__X) as the class discovery fallback.
    static void* s_class = nullptr;
    static bool s_classTried = false;
    if (!s_classTried) {
        s_classTried = true;
        const char* candidates[] = {
            "/Script/FortniteGame.MeshNetworkSubsystem",
            "/Script/FortniteGame.FortMeshNetworkSubsystem",
            "/Script/FortniteGame.MeshNetworkSubsystemBase",
        };
        for (const char* path : candidates) {
            s_class = FindObj(path);
            if (s_class) break;
        }
        if (!s_class) {
            // CDO fallback: Default__X is an instance OF the class — its
            // ClassPrivate pointer IS the class we need.
            void* cdo = FindObj("/Script/FortniteGame.Default__MeshNetworkSubsystem");
            if (!cdo) cdo = FindObj("/Script/FortniteGame.Default__FortMeshNetworkSubsystem");
            if (cdo && !IsBadPtr(cdo)) s_class = Read<void*>(cdo, profile::ObjectClass);
        }
        LOG("[MeshNet] class lookup: %p%s", s_class, s_class ? "" : " (unresolved)");
    }
    void* subsystem = nullptr;
    if (s_class) {
        Call g("/Script/Engine.GameplayStatics.GetGameInstanceSubsystem");
        if (g.ok() && g.set("WorldContextObject", session.world) && g.set("Class", s_class)) {
            if (g.run(FindObj("/Script/Engine.Default__GameplayStatics")))
                subsystem = g.result<void*>();
        }
    }
    if (!subsystem || IsBadPtr(subsystem)) {
        LOG("[MeshNet] subsystem not resolved yet (attempt %d)", session.mesh_network_attempts);
        return;
    }
    void* klass = Read<void*>(subsystem, profile::ObjectClass);
    int sz = 0;
    void* nt = klass ? FindChildProp(klass, "NodeType", &sz) : nullptr;
    if (!nt || sz != 1) {
        LOG("[MeshNet] NodeType property not found on %s — stopping (feature off)",
            ObjName(klass).c_str());
        session.mesh_network_fixed = true;   // cannot act; do not retry forever
        return;
    }
    int off = Read<int>(nt, profile::PropOffset);
    if (off < 0 || off > 0x8000) {
        LOG("[MeshNet] NodeType offset insane (%d) — stopping", off);
        session.mesh_network_fixed = true;
        return;
    }
    uint8_t old = Read<uint8_t>(subsystem, (size_t)off);
    if (old == 2) {
        LOG("[MeshNet] NodeType already 2 (self-sufficient)");
        session.mesh_network_fixed = true;
        return;
    }
    Write<uint8_t>(subsystem, (size_t)off, 2);
    session.mesh_network_fixed = true;
    LOG("[MeshNet] ✅ NodeType %u → 2 (Erbium 4.27+ parity — POI mesh streaming self-sufficient)",
        (unsigned)old);
}

void Athena::TickMeshNetwork() {
    if (session.mesh_network_fixed) return;
    static double last = 0;
    double now = Monotonic();
    if (now - last < 15.0) return;
    last = now;
    if (++session.mesh_network_attempts > 10) {
        session.mesh_network_fixed = true;   // stop the window
        LOG("[MeshNet] window closed after 10 attempts");
        return;
    }
    FixMeshNetwork();
}

// ============================================================================
// [G17-a] Erbium StreamAdditionalPlaylistLevels parity. The playlist carries
// extra levels (AdditionalLevels / server-only / shared-asset groups) that a
// real server streams in at match start — standalone nobody does, so any POI
// content shipped in those sublevels never appears. Erbium prefers the native
// StartStreamingAdditionalPlaylistLevel (string-scanned); we use the SAME
// public fallback Erbium ships for builds without it:
// ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr, driven entirely
// through reflection (element stride from the array property, soft-pointer
// bytes handed to the UFunction untouched).
// ============================================================================
void Athena::StreamPlaylistLevels() {
    if (session.playlist_levels_done || !session.state || !session.world) return;
    session.playlist_levels_done = true;   // one-shot regardless of outcome
    // Resolve the playlist (Erbium: OverridePlaylist ?: BasePlaylist).
    // CurrentPlaylistInfo is an EMBEDDED struct — data pointer first, then
    // the playlist object pointers live inside it.
    void* playlist = nullptr;
    {
        void* infoProp = GetProp("/Script/FortniteGame.FortGameStateAthena.CurrentPlaylistInfo");
        void* info = infoProp ? GetField(session.state, infoProp) : nullptr;
        if (info && !IsBadPtr(info)) {
            playlist = GetObjField(info, "/Script/FortniteGame.PlaylistPropertyArray.OverridePlaylist");
            if (!playlist || IsBadPtr(playlist))
                playlist = GetObjField(info, "/Script/FortniteGame.PlaylistPropertyArray.BasePlaylist");
        }
        if (!playlist || IsBadPtr(playlist))
            playlist = FindObj("/Game/Athena/Playlists/Playlist_DefaultSolo.Playlist_DefaultSolo");
    }
    if (!playlist || IsBadPtr(playlist)) { LOG("[PlaylistLevels] no playlist resolved"); return; }
    // [G22] per-source visibility — WHICH playlist object resolved and the
    // size of every levels array we inspect (the round-17 "0 levels streamed"
    // was silent about whether the playlist was real and the arrays empty).
    LOG("[PlaylistLevels] playlist resolved: %s (class %s)",
        ObjName(playlist).c_str(), ObjName(Read<void*>(playlist, profile::ObjectClass)).c_str());
    {
        auto logArrSize = [&](const char* propPath) {
            void* prop = GetProp(propPath);
            if (!prop) { LOG("[PlaylistLevels] %s: not reflected", propPath); return; }
            Array arr = Read<Array>(GetField(playlist, prop));
            LOG("[PlaylistLevels] %s: %d entries", propPath, arr.Count);
        };
        logArrSize("/Script/FortniteGame.FortPlaylistAthena.AdditionalLevels");
        logArrSize("/Script/FortniteGame.FortPlaylistAthena.AdditionalLevelsServerOnly");
    }
    int streamed = 0;
    // Streams a TSoftObjectPtr<UWorld> array off an owner object.
    auto streamArray = [&](void* owner, const char* propPath) {
        if (!owner || IsBadPtr(owner)) return;
        void* prop = GetProp(propPath);
        if (!prop) return;
        int stride = Read<int>(prop, profile::PropElementSize);
        Array arr = Read<Array>(GetField(owner, prop));
        if (!arr.Data || arr.Count <= 0 || arr.Count > 64) return;
        if (stride < 0x20 || stride > 0x40) return;   // TSoftObjectPtr sanity
        for (int i = 0; i < arr.Count; ++i) {
            uint8_t* elem = (uint8_t*)arr.Data + (size_t)i * stride;
            if (!IsReadable(elem, (size_t)stride)) continue;
            bool any = false;
            for (int b = 0; b < stride && !any; ++b) any = elem[b] != 0;
            if (!any) continue;                        // empty slot
            Call load("/Script/Engine.LevelStreamingDynamic.LoadLevelInstanceBySoftObjectPtr");
            if (!load.ok()) return;
            Vector loc{0.0, 0.0, 0.0};
            Rotator rot{0.0, 0.0, 0.0};
            bool success = false;
            FStringNative emptyName{nullptr, 0, 0};
            bool bound = load.set("WorldContextObject", session.world);
            bound = load.set_bytes("Level", elem, stride) && bound;
            bound = load.set("Location", loc) && bound;
            bound = load.set("Rotation", rot) && bound;
            // bOutSuccess is a bool* OUT param — the parmbuffer slot holds the
            // pointer the native dereferences. Bind by reflected size: 8 =
            // pointer slot (hand it our bool's address), 1 = by-value variant.
            bool outIsPointer = load.param_size("bOutSuccess") == 8;
            if (outIsPointer)
                bound = load.set("bOutSuccess", (void*)&success) && bound;
            else {
                bool bv = false;
                bound = load.set("bOutSuccess", bv) && bound;
            }
            bound = load.set("OptionalLevelNameOverride", emptyName) && bound;
            if (bound && load.run(FindObj("/Script/Engine.Default__LevelStreamingDynamic"))) {
                bool okLevel = outIsPointer ? success : false;
                if (!outIsPointer) {
                    void* slot = load.param_slot("bOutSuccess");
                    if (slot) okLevel = Read<uint8_t>(slot) != 0;
                }
                if (okLevel) ++streamed;
            }
        }
    };
    streamArray(playlist, "/Script/FortniteGame.FortPlaylistAthena.AdditionalLevels");
    streamArray(playlist, "/Script/FortniteGame.FortPlaylistAthena.AdditionalLevelsServerOnly");
    // Shared asset group: SharedAssetsToLoad[] objects each carry SharedAdditionalLevels.
    {
        void* group = GetObjField(playlist, "/Script/FortniteGame.FortPlaylistAthena.SharedAssetGroup");
        if (group && !IsBadPtr(group)) {
            void* listProp = GetProp("/Script/FortniteGame.FortSharedAssetGroup.SharedAssetsToLoad");
            if (listProp) {
                Array assets = Read<Array>(GetField(group, listProp));
                if (assets.Data && assets.Count > 0 && assets.Count <= 64) {
                    for (int i = 0; i < assets.Count; ++i) {
                        void* asset = Read<void*>(assets.Data, (size_t)i * 8);
                        if (!asset || IsBadPtr(asset)) continue;
                        streamArray(asset, "/Script/FortniteGame.FortSharedAssetDatum.SharedAdditionalLevels");
                    }
                }
            }
        }
    }
    LOG("[PlaylistLevels] %d additional playlist levels streamed (Erbium parity)", streamed);
}

// ============================================================================
// [G17-c] DEATH WATCH
// ============================================================================
// [G17-c] 0.5s health poll — the client predicts damage into its local
// attribute set, but death itself is a server decision. With no server the
// pawn sits at 0 HP forever ("player stays standing after death"). The
// watcher is that missing server: on the health → 0 EDGE we run the full
// Erbium server death flow.
void Athena::TickDeathWatch() {
    if (!session.finalized || session.death_handled || session.win_handled) return;
    if (!session.pawn || !PawnStillValid(session.pawn)) return;   // [G19-a] freed pawn = no polls
    double now = Monotonic();
    if (now - session.last_health_poll < 0.5) return;
    session.last_health_poll = now;
    float hp = PawnHealth(session.pawn);
    if (hp < 0.0f) return;                    // accessor not ready — no opinion
    if (hp > 0.01f) {
        session.last_known_health = hp;       // observed ALIVE — arm the edge detector
        session.death_zero_streak = 0;
        return;
    }
    // hp == 0. Only a REAL alive→dead transition fires the flow:
    //  • we must have observed a positive health at least once (a pawn whose
    //    attributes never initialised reads 0 forever — that is not a death);
    //  • two consecutive zero polls (1 s) debounce transient reads.
    if (session.last_known_health <= 0.01f) return;
    if (++session.death_zero_streak < 2) return;
    HandlePlayerDeath(false);
}

// ============================================================================
// [G17-c] HandlePlayerDeath — Erbium AFortPlayerControllerAthena::ClientOn-
// PawnDied server-side parity. Erbium HOOKS the native client RPC to inject
// its logic around the original; our standalone world never dispatches the
// RPC at all, so we ARE the server: same order, same writes, and finally the
// client RPC body itself runs locally through ProcessEvent (client RPCs fall
// through to local execution on a non-authority actor — the same mechanism
// every ClientXxx call in this file already relies on).
// ============================================================================
void Athena::HandlePlayerDeath(bool forced) {
    if (session.death_handled || !session.finalized || !session.controller) return;
    session.death_handled = true;   // one-shot: re-entry is impossible
    LOG("[Death] %s — running Erbium server death flow (G19-a guarded)",
        forced ? "forced (era.die)" : "health reached 0");
    Vector loc{};
    // [G19-a] the engine may have already destroyed the pawn (KillZ / fast
    // eliminate) while our 1s debounce ran — gate every pawn-touching step.
    bool pawnOk = PawnStillValid(session.pawn);
    if (pawnOk && session.pawn && !ActorLocation(session.pawn, loc)) loc = Vector{};
    if (!pawnOk) LOG("[Death] ⚠️ pawn stale/destroyed (KillZ?) — pawn steps skipped");

    // ---- 1) Drop every droppable item as a pickup (Erbium inventory drop) --
    int dropped = 0;
    if (pawnOk) gas::RunGuarded("[Death] drops", [&] {
        void* invProp       = GetProp("/Script/FortniteGame.FortInventory.Inventory");
        void* instProp      = GetProp("/Script/FortniteGame.FortItemList.ItemInstances");
        void* entryProp     = GetProp("/Script/FortniteGame.FortItemList.ReplicatedEntries");
        void* itemEntryProp = GetProp("/Script/FortniteGame.FortWorldItem.ItemEntry");
        void* countProp     = GetProp("/Script/FortniteGame.FortItemEntry.Count");
        void* levelProp     = GetProp("/Script/FortniteGame.FortItemEntry.Level");
        void* ammoProp      = GetProp("/Script/FortniteGame.FortItemEntry.LoadedAmmo");
        if (session.inventory && invProp && instProp && entryProp && itemEntryProp && countProp) {
            void* list = GetField(session.inventory, invProp);
            if (list && !IsBadPtr(list)) {
                struct DropRef { void* def; int count; int level; int ammo; };
                std::vector<DropRef> drops;
                Array instances = Read<Array>(GetField(list, instProp));
                for (int i = 0; instances.Data && i < instances.Count && i < 128; ++i) {
                    void* inst = Read<void*>(instances.Data, (size_t)i * 8);
                    if (!inst || IsBadPtr(inst)) continue;
                    // Erbium gate: item must be droppable.
                    Call cbd("/Script/FortniteGame.FortItem.CanBeDropped");
                    if (cbd.ok() && cbd.run(inst) && !cbd.result<bool>()) continue;
                    void* def = ItemDefinitionOf(inst);
                    if (!def || IsBadPtr(def)) continue;
                    void* entry = GetField(inst, itemEntryProp);
                    if (!entry || IsBadPtr(entry)) continue;
                    int count = Read<int>(GetField(entry, countProp));
                    if (count <= 0) continue;
                    int lvl = levelProp ? Read<int>(GetField(entry, levelProp)) : -1;
                    int ammo = ammoProp ? Read<int>(GetField(entry, ammoProp)) : -1;
                    if (lvl < 0) lvl = -1;
                    if (ammo < 0) ammo = -1;
                    drops.push_back({def, count, lvl, ammo});
                }
                for (auto& d : drops) {
                    Vector tossLoc = loc;
                    tossLoc.Z += 60.0;
                    SpawnPickup(d.def, d.count, tossLoc, 1 /*Player*/, 10 /*TossedByPlayer*/, true, d.level, d.ammo);
                    ++dropped;
                }
                // The real server removes the dead player's inventory — mirror
                // it (Count=0 hides the entries from GC marking and the HUD).
                if (!drops.empty()) {
                    Array instances2 = Read<Array>(GetField(list, instProp));
                    Array replicated = Read<Array>(GetField(list, entryProp));
                    Array empty{};
                    if (instances2.Count > 0 && instances2.Count < 4096) Write<Array>(GetField(list, instProp), 0, empty);
                    if (replicated.Count > 0 && replicated.Count < 4096) Write<Array>(GetField(list, entryProp), 0, empty);
                    InventoryUpdate();
                }
            }
        }
    });
    LOG("[Death] step1 drops done (%d)", dropped);

    // ---- 2) PlayerState DeathInfo + PawnDeathLocation (Erbium 1:1) --------
    if (session.player_state && !IsBadPtr(session.player_state)) {
        {
            void* pdlProp = GetProp("/Script/FortniteGame.FortPlayerStateAthena.PawnDeathLocation");
            if (pdlProp && Read<int>(pdlProp, profile::PropElementSize) == 24)
                Write<Vector>(GetField(session.player_state, pdlProp), 0, loc);
        }
        void* diProp = GetProp("/Script/FortniteGame.FortPlayerStateAthena.DeathInfo");
        if (diProp) {
            void* di = GetField(session.player_state, diProp);
            void* diStruct = FindScriptStructNamed("DeathInfo", "Killer", "bInitialized");
            if (di && !IsBadPtr(di) && diStruct && !IsBadPtr(diStruct)) {
                auto sub = [&](const char* name) -> int {
                    void* p = FindChildProp(diStruct, name);
                    return p ? Read<int>(p, profile::PropOffset) : -1;
                };
                int diSize = Read<int>(diProp, profile::PropElementSize);
                if (diSize > 0 && diSize <= 256) memset(di, 0, (size_t)diSize);
                int off;
                if ((off = sub("bDBNO")) >= 0)          Write<uint8_t>(di, (size_t)off, 0);
                if ((off = sub("DeathLocation")) >= 0)  Write<Vector>(di, (size_t)off, loc);
                if ((off = sub("DeathCause")) >= 0)     Write<uint8_t>(di, (size_t)off, 0);
                if ((off = sub("Distance")) >= 0)       Write<float>(di, (size_t)off, 0.0f);
                if ((off = sub("bInitialized")) >= 0)   Write<uint8_t>(di, (size_t)off, 1);
                // Killer / Downer / FinisherOrDowner / tags stay zeroed — the
                // world killed us (storm/fall), not another player.
            }
            gas::RunGuarded("[Death] OnRep_DeathInfo", [&] {
                Call r("/Script/FortniteGame.FortPlayerStateAthena.OnRep_DeathInfo");
                if (r.ok()) r.run(session.player_state);
            });
        }
    }
    LOG("[Death] step2 death-info done");

    // ---- 3) Place = PlayersLeft BEFORE the decrement (Erbium order) -------
    int place = 1;
    int playersLeft = 1;
    void* plProp = GetProp("/Script/FortniteGame.FortGameStateAthena.PlayersLeft");
    bool haveLeft = session.state && plProp && Read<int>(plProp, profile::PropElementSize) == 4;
    if (haveLeft) {
        playersLeft = Read<int>(GetField(session.state, plProp));
        if (playersLeft < 1) playersLeft = 1;
        place = playersLeft;
    }
    if (session.player_state && !IsBadPtr(session.player_state)) {
        void* placeProp = GetProp("/Script/FortniteGame.FortPlayerStateAthena.Place");
        if (placeProp && Read<int>(placeProp, profile::PropElementSize) == 4) {
            Write<int>(GetField(session.player_state, placeProp), 0, place);
            gas::RunGuarded("[Death] OnRep_Place", [&] {
                Call r("/Script/FortniteGame.FortPlayerStateAthena.OnRep_Place");
                if (r.ok()) r.run(session.player_state);
            });
        }
    }
    LOG("[Death] step3 place=%d done", place);

    // ---- 4) Freeze movement (Erbium v15+: DisableMovement) ----------------
    if (pawnOk && session.pawn && !IsBadPtr(session.pawn)) {
        void* movement = GetObjField(session.pawn, "/Script/Engine.Character.CharacterMovement");
        if (movement && !IsBadPtr(movement)) {
            gas::RunGuarded("[Death] DisableMovement", [&] {
                Call d("/Script/Engine.CharacterMovementComponent.DisableMovement");
                if (d.ok()) d.run(movement);
            });
        }
    }
    LOG("[Death] step4 movement frozen=%d", (int)pawnOk);

    // ---- 5) The client RPC body — death UI / camera / spectate switch -----
    gas::RunGuarded("[Death] ClientOnPawnDied", [&] {
        Call c("/Script/FortniteGame.FortPlayerControllerZone.ClientOnPawnDied");
        if (c.ok()) {
            int psize = c.param_size("DeathReport");
            if (psize > 0 && psize <= 512) {
                std::vector<uint8_t> rep((size_t)psize, 0);
                // Zeroed report = "killed by the world" (no killer player
                // state, no causer) — the native body handles storm deaths
                // with exactly this shape.
                if (!c.set_bytes("DeathReport", rep.data(), psize))
                    c.bind_at(0, rep.data(), psize);
            }
            c.run(session.controller);
        }
    });
    LOG("[Death] step5 ClientOnPawnDied dispatched");

    // ---- 6) Pawn dying state (ragdoll/death anim attempt) -----------------
    if (pawnOk && session.pawn && !IsBadPtr(session.pawn)) {
        gas::RunGuarded("[Death] HandleIsDying", [&] {
            Call h("/Script/FortniteGame.FortPawn.HandleIsDying");
            if (h.ok()) h.run(session.pawn);
        });
    }
    LOG("[Death] step6 is-dying done");

    // ---- 7) PlayersLeft-- + OnRep (native RemoveFromAlivePlayers parity) --
    if (haveLeft) {
        int left = playersLeft - 1;
        if (left < 0) left = 0;
        Write<int>(GetField(session.state, plProp), 0, left);
        gas::RunGuarded("[Death] OnRep_PlayersLeft", [&] {
            Call n("/Script/FortniteGame.FortGameStateAthena.OnRep_PlayersLeft");
            if (n.ok()) n.run(session.state);
        });
        session.real_alive = left;
    }
    LOG("[Death] ✅ complete: place=%d playersLeft=%d itemsDropped=%d", place, playersLeft - 1, dropped);
    // [G18-a] THE GAME'S RULE (user-specified Erbium behaviour): if you were
    // the ONLY player, you never win while alive — the win window appears
    // exactly when you die (Place == 1). Fire the victory flow post-death.
    if (place == 1 && !session.win_handled) {
        LOG("[Death] solo death — Place #1 → win window (game rules)");
        HandleVictory(true /*onDeath*/);
    }
}

// ============================================================================
// [G18-a] REAL ALIVE PLAYERS — Erbium parity (FortPlayerControllerAthena.cpp
// 2094: PlayersLeft++ per join; ClientOnPawnDied: Place = PlayersLeft pre-
// decrement, then RemoveFromAlivePlayers decrements). No fake curve, no
// virtual lobby: the HUD counter is fed by the number of REAL alive player
// states in the world. When alone it reads 1 and stays 1 — the victory flow
// is reachable ONLY through death (Place=1 → win window) or a real second
// player joining and being eliminated (players_ever >= 2 → last-standing win).
// ============================================================================
int Athena::CountRealAlivePlayers(int* totalOut) {
    if (totalOut) *totalOut = 0;
    int alive = 0, total = 0;
    // Primary source: GameState->PlayerArray (TArray<APlayerState*>).
    if (session.state && !IsBadPtr(session.state)) {
        void* paProp = GetProp("/Script/Engine.GameStateBase.PlayerArray");
        if (!paProp) paProp = GetProp("/Script/Engine.GameState.PlayerArray");
        if (paProp) {
            Array arr = Read<Array>(GetField(session.state, paProp));
            if (arr.Count > 0 && arr.Count < 256 && arr.Data && !IsBadPtr(arr.Data)) {
                session.alive_source = 1;
                for (int i = 0; i < arr.Count; ++i) {
                    void* ps = Read<void*>(arr.Data, (size_t)i * 8);
                    if (!ps || IsBadPtr(ps)) continue;
                    ++total;
                    // Dead = a placement was recorded (Erbium sets Place only
                    // on elimination). Anything else counts as alive.
                    bool dead = false;
                    void* placeProp = GetProp("/Script/FortniteGame.FortPlayerStateAthena.Place");
                    if (placeProp && Read<int>(placeProp, profile::PropElementSize) == 4)
                        dead = Read<int>(GetField(ps, placeProp)) > 0;
                    if (!dead) ++alive;
                }
            }
        }
    }
    // Fallback: the local player ALWAYS exists even when PlayerArray has not
    // been populated (standalone never runs the server-side AddPlayerState).
    if (total == 0) {
        session.alive_source = 2;
        bool dead = false;
        if (session.player_state && !IsBadPtr(session.player_state)) {
            void* placeProp = GetProp("/Script/FortniteGame.FortPlayerStateAthena.Place");
            if (placeProp && Read<int>(placeProp, profile::PropElementSize) == 4)
                dead = Read<int>(GetField(session.player_state, placeProp)) > 0;
        }
        total = 1;
        alive = dead ? 0 : 1;
    }
    if (totalOut) *totalOut = total;
    return alive;
}

void Athena::TickRealAlive() {
    if (!session.finalized || !session.state) return;
    void* plProp = GetProp("/Script/FortniteGame.FortGameStateAthena.PlayersLeft");
    if (!plProp || Read<int>(plProp, profile::PropElementSize) != 4) return;
    double now = Monotonic();
    if (now - session.last_alive_poll < 1.0) return;
    session.last_alive_poll = now;
    if (!session.match_state_ready) return;   // waits for the seeding pass
    int total = 0;
    int alive = CountRealAlivePlayers(&total);
    if (total > session.players_ever) session.players_ever = total;
    // era.players N — explicit FAKE floor for device testing (default: real only).
    if (session.players_override > 0 && alive < session.players_override)
        alive = session.players_override;
    session.real_alive = alive;
    if (session.death_handled || session.win_handled) return;   // game over — freeze
    int cur = Read<int>(GetField(session.state, plProp));
    if (cur != alive) {
        Write<int>(GetField(session.state, plProp), 0, alive);
        Call n("/Script/FortniteGame.FortGameStateAthena.OnRep_PlayersLeft");
        if (n.ok()) n.run(session.state);
        LOG("[Alive] REAL count %d → %d (ever=%d, source=%s)",
            cur, alive, session.players_ever,
            session.alive_source == 1 ? "PlayerArray" : "local-fallback");
    }
    // Win-while-alive ONLY when a real second player joined this match and
    // every other player is gone (the game's last-standing rule).
    if (alive == 1 && session.players_ever >= 2) {
        LOG("[Alive] last player standing (players_ever=%d) — victory", session.players_ever);
        HandleVictory();
    }
}

// ============================================================================
// [G17-e/G18-a] HandleVictory — Erbium win parity (ClientOnPawnDied lines
// 1262-1301): when the last opponent dies the server notifies the winner —
//   PlayWinEffects (v16+) → ClientNotifyWon → ClientNotifyTeamWon,
//   GameState->WinningTeam = TeamIndex + OnRep,
//   GameState->WinningPlayerState + OnRep.
// Triggers: (a) [G18-a] death with Place == 1 — the game shows the win window
// exactly when the last (only) player dies; (b) real last-standing (a second
// real player joined and was eliminated); (c) era.win for device testing.
// ============================================================================
void Athena::HandleVictory(bool onDeath) {
    if (session.win_handled) return;
    if (session.death_handled && !onDeath) return;   // alive-win only before death
    if (!session.finalized || !session.controller) return;
    session.win_handled = true;
    LOG("[Victory] 🏆 %s — running Erbium win flow",
        onDeath ? "died as the last player (Place #1 — game rules)" : "last player standing");
    int teamIndex = 3;
    // Place 1 + bIsAWinner (when the build still reflects it).
    if (session.player_state && !IsBadPtr(session.player_state)) {
        {
            void* placeProp = GetProp("/Script/FortniteGame.FortPlayerStateAthena.Place");
            if (placeProp && Read<int>(placeProp, profile::PropElementSize) == 4) {
                Write<int>(GetField(session.player_state, placeProp), 0, 1);
                Call r("/Script/FortniteGame.FortPlayerStateAthena.OnRep_Place");
                if (r.ok()) r.run(session.player_state);
            }
        }
        {
            void* wProp = GetProp("/Script/FortniteGame.FortPlayerStateAthena.bIsAWinner");
            if (wProp) {
                SetBool(session.player_state, "/Script/FortniteGame.FortPlayerStateAthena.bIsAWinner", true);
                Call r("/Script/FortniteGame.FortPlayerStateAthena.OnRep_IsAWinner");
                if (r.ok()) r.run(session.player_state);
            }
        }
        {
            void* tiProp = GetProp("/Script/FortniteGame.FortPlayerStateAthena.TeamIndex");
            if (tiProp) {
                int sz = Read<int>(tiProp, profile::PropElementSize);
                if (sz == 1) teamIndex = (int)Read<uint8_t>(GetField(session.player_state, tiProp));
                else if (sz == 4) teamIndex = Read<int>(GetField(session.player_state, tiProp));
            }
        }
    }
    if (session.state && !IsBadPtr(session.state)) {
        void* wtProp = GetProp("/Script/FortniteGame.FortGameStateAthena.WinningTeam");
        if (wtProp && Read<int>(wtProp, profile::PropElementSize) == 4) {
            Write<int>(GetField(session.state, wtProp), 0, teamIndex);
            Call r("/Script/FortniteGame.FortGameStateAthena.OnRep_WinningTeam");
            if (r.ok()) r.run(session.state);
        }
        void* wpsProp = GetProp("/Script/FortniteGame.FortGameStateAthena.WinningPlayerState");
        if (wpsProp && Read<int>(wpsProp, profile::PropElementSize) == 8 && session.player_state) {
            Write<void*>(GetField(session.state, wpsProp), 0, session.player_state);
            Call r("/Script/FortniteGame.FortGameStateAthena.OnRep_WinningPlayerState");
            if (r.ok()) r.run(session.state);
        }
    }
    // Erbium order: PlayWinEffects → ClientNotifyWon → ClientNotifyTeamWon.
    // [G19-a] each banner call is fault-guarded — a faulting win effect must
    // not take the placement window down with it.
    void* pawn = (session.pawn && PawnStillValid(session.pawn)) ? session.pawn : nullptr;
    void* noWeapon = nullptr;
    uint8_t cause = 0;
    {
        Call e("/Script/FortniteGame.FortPlayerControllerAthena.PlayWinEffects");
        if (e.ok() && pawn) {
            bool bound = e.set_any(pawn, "FinisherPawn", "Pawn", "Finisher");
            bound = e.set_any(noWeapon, "FinishingWeapon", "Weapon") && bound;
            bound = e.set_any(cause, "DeathCause", "Cause") && bound;
            bool audioOnly = false;
            bound = e.set_any(audioOnly, "bAudioOnly", "AudioOnly") && bound;
            if (bound) gas::RunGuarded("[Victory] PlayWinEffects", [&] { e.run(session.controller); });
        }
    }
    {
        Call w("/Script/FortniteGame.FortPlayerControllerAthena.ClientNotifyWon");
        if (w.ok()) {
            bool bound = w.set_any(pawn, "FinisherPawn", "Pawn", "Finisher");
            bound = w.set_any(noWeapon, "FinishingWeapon", "Weapon") && bound;
            bound = w.set_any(cause, "DeathCause", "Cause") && bound;
            if (bound) gas::RunGuarded("[Victory] ClientNotifyWon", [&] { w.run(session.controller); });
        }
    }
    {
        Call t("/Script/FortniteGame.FortPlayerControllerAthena.ClientNotifyTeamWon");
        if (t.ok()) {
            bool bound = t.set_any(pawn, "FinisherPawn", "Pawn", "Finisher");
            bound = t.set_any(noWeapon, "FinishingWeapon", "Weapon") && bound;
            bound = t.set_any(cause, "DeathCause", "Cause") && bound;
            if (bound) gas::RunGuarded("[Victory] ClientNotifyTeamWon", [&] { t.run(session.controller); });
        }
    }
    // [G26] Erbium 1:1 (FortPlayerControllerAthena.cpp:1277-1292): the winner
    // receives the Victory Crown item (v19+) — AGID_VictoryCrown.
    {
        void* crown = FindObj("/VictoryCrownsGameplay/Items/AGID_VictoryCrown.AGID_VictoryCrown");
        if (crown && !IsBadPtr(crown) && session.inventory) {
            if (GiveItem(crown, 1, -1, 0, false))
                LOG("[Victory] 👑 Victory Crown granted (Erbium parity)");
            else
                LOG("[Victory] ⚠️ crown grant failed");
        } else LOG("[Victory] crown asset not resident — skipped");
    }
    LOG("[Victory] ✅ banner dispatched (team=%d place=1)", teamIndex);
}


// [F42] Diagnostics dump — quickbar slots, inventory entries, match state.
void Athena::DebugDump() {
    LOG("=== era.debug ===");
    LOG("world=%p pc=%p pawn=%p inv=%p qb=%p pendingPickups=%zu",
        session.world, session.controller, session.pawn, session.inventory,
        session.quickbars, session.pending_pickups.size());
    for (int i = 0; i < 6; ++i)
        LOG("SLOT %d: %s", i, ObjName(ItemDefinitionOf(session.slots[(size_t)i])).c_str());
    for (int i = 0; i < 6; ++i)
        LOG("BUILD %d: %s", i, ObjName(ItemDefinitionOf(session.secondary_slots[(size_t)i])).c_str());
    if (session.quickbars && !IsBadPtr(session.quickbars)) {
        void* barProp = GetProp("/Script/FortniteGame.FortQuickBars.PrimaryQuickBar");
        void* slotsProp = GetProp("/Script/FortniteGame.QuickBar.Slots");
        if (barProp && slotsProp) {
            void* barField = GetField(session.quickbars, barProp);
            Array slots = Read<Array>(GetField(barField, slotsProp));
            LOG("PrimaryQuickBar slots=%d", slots.Count);
            for (int i = 0; i < slots.Count && slots.Data && i < 6; ++i) {
                void* slot = (char*)slots.Data + (size_t)i * profile::QuickBarSlot_SlotSize;
                Array items = Read<Array>(slot);
                uint8_t flags = Read<uint8_t>(slot, profile::QuickBarSlot_EnabledOffset);
                LOG("  slot %d items=%d flags=0x%02X", i, items.Count, flags);
            }
        }
    }
    if (session.state && !IsBadPtr(session.state)) {
        void* plProp = GetProp("/Script/FortniteGame.FortGameStateAthena.PlayersLeft");
        if (plProp) LOG("PlayersLeft=%d", Read<int>(GetField(session.state, plProp)));
    }
    LOG("death=%d win=%d alive=%d ever=%d qbw=%d hp=%.0f meshnet=%d(attempts=%d) plvl=%d",
        (int)session.death_handled, (int)session.win_handled, session.real_alive,
        session.players_ever, session.qb_watch_tries,
        (session.pawn ? PawnHealth(session.pawn) : -1.0f),
        (int)session.mesh_network_fixed, session.mesh_network_attempts,
        (int)session.playlist_levels_done);
    LOG("=== end dump ===");
}

// ============================================================================
// VEHICLES — [F28] Erbium AFortAthenaVehicleSpawner path (1:1) + seat weapons
// ============================================================================
// [F43] Erbium's LivingWorld vehicle tag map (1:1): AFortAthenaLivingWorld-
// StaticPointProvider spawners carry GameplayTags that select the vehicle
// class. On real s20–s22 servers the AFortAthenaVehicleSpawner path is native-
// auto, so this provider pass is what actually fills the map with cars.
// [G16-c] ERBIUM s21 PROVIDER MAP 1:1 (FortGameMode.cpp ReadyToStartMatch_):
// on s20–s22 the spawner rollout is SKIPPED ("its auto") and ALL tagged
// vehicles come from the LivingWorld providers — with the UPGRADE variants
// for the Upgraded tags. Order matters: the most specific tag first (a plain
// "Valet.BasicCar" rule must not eat the ".Upgraded"/.Taxi/.Modded tags).
static const struct { const char* needle; const char* classPath; const char* shortName; } kVehicleTagRules[] = {
    {"Motorcycle.Dirtbike",      "/Dirtbike/Vehicle/Motorcycle_DirtBike_Vehicle.Motorcycle_DirtBike_Vehicle_C",                        "Motorcycle_DirtBike_Vehicle_C"},
    {"Motorcycle.Sportbike",     "/Sportbike/Vehicle/Motorcycle_Sport_Vehicle.Motorcycle_Sport_Vehicle_C",                              "Motorcycle_Sport_Vehicle_C"},
    {"Valet.BasicCar.Taxi",      "/Valet/TaxiCab/Valet_TaxiCab_Vehicle.Valet_TaxiCab_Vehicle_C",                                        "Valet_TaxiCab_Vehicle_C"},
    {"Valet.BasicCar.Modded",    "/ModdedBasicCar/Vehicle/Valet_BasicCar_Vehicle_SuperSedan.Valet_BasicCar_Vehicle_SuperSedan_C",      "Valet_BasicCar_Vehicle_SuperSedan_C"},
    {"Valet.BasicTruck.Upgraded", "/Valet/BasicTruck/Valet_BasicTruck_Vehicle_Upgrade.Valet_BasicTruck_Vehicle_Upgrade_C",            "Valet_BasicTruck_Vehicle_Upgrade_C"},
    {"Valet.BigRig.Upgraded",    "/Valet/BigRig/Valet_BigRig_Vehicle_Upgrade.Valet_BigRig_Vehicle_Upgrade_C",                            "Valet_BigRig_Vehicle_Upgrade_C"},
    {"Valet.SportsCar.Upgraded", "/Valet/SportsCar/Valet_SportsCar_Vehicle_Upgrade.Valet_SportsCar_Vehicle_Upgrade_C",                  "Valet_SportsCar_Vehicle_Upgrade_C"},
    {"Valet.BasicCar.Upgraded",  "/Valet/BasicCar/Vehicle/Valet_BasicCar_Vehicle_Upgrade.Valet_BasicCar_Vehicle_Upgrade_C",              "Valet_BasicCar_Vehicle_Upgrade_C"},
    {"Valet.BasicTruck",         "/Valet/BasicTruck/Valet_BasicTruck_Vehicle.Valet_BasicTruck_Vehicle_C",                              "Valet_BasicTruck_Vehicle_C"},
    {"Valet.BigRig",             "/Valet/BigRig/Valet_BigRig_Vehicle.Valet_BigRig_Vehicle_C",                                          "Valet_BigRig_Vehicle_C"},
    {"Valet.SportsCar",          "/Valet/SportsCar/Valet_SportsCar_Vehicle.Valet_SportsCar_Vehicle_C",                                  "Valet_SportsCar_Vehicle_C"},
    {"Valet.BasicCar",           "/Valet/BasicCar/Valet_BasicCar_Vehicle.Valet_BasicCar_Vehicle_C",                                    "Valet_BasicCar_Vehicle_C"},
    {"SpawnLocation.Boat",       "/Athena/Vehicles/Boat/Boat_Athena_Boat.Boat_Athena_Boat_C",                                          "Boat_Athena_Boat_C"},
    {nullptr, nullptr, nullptr}
};

static bool SpawnVehicleAt(void* cls, const Vector& loc, const Rotator& rot) {
    if (!cls || IsBadPtr(cls)) return false;
    Transform t{};
    t.Location[0] = loc.X; t.Location[1] = loc.Y; t.Location[2] = loc.Z + 50.0;
    RotatorToQuat(rot, t.Rotation);
    void* vehicle = SpawnActor(session.world, cls, nullptr, t);
    if (!vehicle) return false;
    // [F28] full fuel — SetFuel lives on the car subclasses; the property
    // write covers every vehicle shape.
    {
        std::string clsName = ObjName(Read<void*>(vehicle, profile::ObjectClass));
        Call f((std::string("/Script/FortniteGame.") + clsName + ".SetFuel").c_str());
        if (!f.ok()) f = Call("/Script/FortniteGame.FortDagwoodVehicle.SetFuel");
        if (!f.ok()) f = Call("/Script/FortniteGame.FortAthenaVehicle.SetFuel");
        if (f.ok() && f.set_at(0, 100.0f)) f.run(vehicle);
        void* fuelProp = GetProp("/Script/FortniteGame.FortAthenaVehicle.Fuel");
        if (fuelProp && Read<int>(fuelProp, profile::PropElementSize) == 4)
            Write<float>(GetField(vehicle, fuelProp), 0, 100.0f);
    }
    return true;
}

void Athena::SpawnVehicles() {
    if (!session.world || session.vehicles_spawned) return;
    if (!WorldIsStandalone(session.world)) return;
    // [F41] The flag is only committed when spawners are actually found —
    // the map streams its spawners in over time, and the old code marked the
    // pass done on the first (usually empty) attempt, so cars never came back.
    ++session.vehicle_attempts;
    static std::vector<Vector> spawnedAt;   // [F43] dedupe across passes
    int spawned = 0;
    int viable = 0;                         // [F85] resolvable-class targets this pass (diagnostics)

    // ---- Pass 1 (Erbium 1:1): LivingWorld providers matched by GameplayTag ----
    {
        void* providerCls = FindObj("/Script/FortniteGame.FortAthenaLivingWorldStaticPointProvider");
        if (providerCls) {
            std::vector<void*> providers;
            if (GetAllActorsOfClassRaw(providerCls, providers) && !providers.empty()) {
                LOG("[Vehicles] LivingWorld providers: %zu", providers.size());
                void* tagsProp    = GetProp("/Script/FortniteGame.FortAthenaLivingWorldStaticPointProvider.FiltersTags");
                void* tagsArrProp = tagsProp ? GetProp("/Script/GameplayTags.GameplayTagContainer.GameplayTags") : nullptr;
                if (!tagsArrProp) tagsArrProp = GetProp("/Script/FortniteGame.GameplayTagContainer.GameplayTags");
                for (void* sp : providers) {
                    if (!sp || IsBadPtr(sp)) continue;
                    std::vector<std::string> tags;
                    if (tagsProp && tagsArrProp) {
                        void* container = GetField(sp, tagsProp);
                        if (container) {
                            Array arr = Read<Array>(GetField(container, tagsArrProp));
                            for (int i = 0; i < arr.Count && i < 16 && arr.Data; ++i)
                                tags.push_back(FNameToString((char*)arr.Data + (size_t)i * 8));
                        }
                    }
                    if (tags.empty()) continue;
                    Vector loc{}; Rotator rot{};
                    if (!ActorLocation(sp, loc)) continue;
                    { Call c("/Script/Engine.Actor.K2_GetActorRotation"); if (c.ok() && c.run(sp)) rot = c.result<Rotator>(); }
                    bool dup = false;
                    for (auto& s : spawnedAt) {
                        double dx = s.X - loc.X, dy = s.Y - loc.Y;
                        if (dx*dx + dy*dy < 600.0 * 600.0) { dup = true; break; }
                    }
                    if (dup) continue;
                    for (int r = 0; kVehicleTagRules[r].needle; ++r) {
                        bool hit = false;
                        for (auto& tg : tags) if (tg.find(kVehicleTagRules[r].needle) != std::string::npos) { hit = true; break; }
                        if (!hit) continue;
                        void* cls = LoadClass(kVehicleTagRules[r].classPath, "/Script/Engine.Actor");
                        if (!cls) cls = ByShortName(kVehicleTagRules[r].shortName);
                        if (!cls || IsBadPtr(cls)) {
                            LOG("[Vehicles] ⚠️ class missing for '%s' (%s)", tags[0].c_str(), kVehicleTagRules[r].shortName);
                            break;
                        }
                        ++viable;   // [F85] class resolved — this provider counts
                        if (SpawnVehicleAt(cls, loc, rot)) {
                            ++spawned;
                            spawnedAt.push_back(loc);
                            LOG("[Vehicles] ✅ %s (tag %s) @ %.0f %.0f %.0f", kVehicleTagRules[r].shortName, tags[0].c_str(), loc.X, loc.Y, loc.Z);
                        }
                        break;
                    }
                }
            }
        }
    }

    // ---- Pass 2: AFortAthenaVehicleSpawner ----
    // [G16-c] ERBIUM s21 SEMANTICS: on s20–s22 Erbium SKIPS the spawner
    // rollout entirely ("its auto on s20, s21, and s22") — the native auto
    // system populates the map. Our standalone has no native auto system, so
    // WE are it: spawn at EVERY spawner whose vehicle class resolves. The
    // percent roll is REMOVED (the run-5 device result was 3/325 — the roll
    // read near-zero percents and starved the map; Erbium's s21 branch never
    // rolls it either). A spawner is only marked processed when it actually
    // spawned or was a duplicate — unresolved classes retry on later passes
    // as the streaming levels bring them in.
    static std::set<void*> s_processedSpawners;   // [G16-c] spawned or dup'd
    {
        void* spawnerCls = FindObj("/Script/FortniteGame.FortAthenaVehicleSpawner");
        if (!spawnerCls) spawnerCls = LoadClass("/Script/FortniteGame.FortAthenaVehicleSpawner", "/Script/Engine.Actor");
        std::vector<void*> spawners;
        if (spawnerCls && GetAllActorsOfClassRaw(spawnerCls, spawners) && !spawners.empty()) {
            int resolved = 0, pending = 0;
            const int kPerPassCap = 60;   // [G16-c] phone guard: full map over ~5 passes, no single-frame storm
            for (void* spawner : spawners) {
                if (spawned >= kPerPassCap) { ++pending; continue; }   // next pass — no frame storm
                if (!spawner || IsBadPtr(spawner)) continue;
                if (s_processedSpawners.count(spawner)) continue;
                void* vehicleClass = nullptr;
                {
                    Call c("/Script/FortniteGame.FortAthenaVehicleSpawner.GetVehicleClass");
                    if (c.ok() && c.run(spawner)) vehicleClass = c.result<void*>();
                }
                if (!vehicleClass || IsBadPtr(vehicleClass)) { ++pending; continue; }   // class not streamed yet
                ++resolved;
                // [G16-f] PRE-s20 (Erbium branch): the spawner IS the source on
                // old seasons — evaluate the CachedFortVehicleItemDef percent
                // exactly like Erbium's pre-20 path, once per spawner.
                if (profile::FortniteVersion < 20) {
                    void* cachedDef = GetObjField(spawner, "/Script/FortniteGame.FortAthenaVehicleSpawner.CachedFortVehicleItemDef");
                    bool forced = GetBool(spawner, "/Script/FortniteGame.FortAthenaVehicleSpawner.bForceSpawnAlways");
                    if (cachedDef && !IsBadPtr(cachedDef) && !forced) {
                        float mn = 100.0f, mx = 100.0f;
                        void* minProp = GetProp("/Script/FortniteGame.FortVehicleItemDefinition.VehicleMinSpawnPercent");
                        void* maxProp = GetProp("/Script/FortniteGame.FortVehicleItemDefinition.VehicleMaxSpawnPercent");
                        if (minProp && Read<int>(minProp, profile::PropElementSize) >= 4) mn = Read<float>(GetField(cachedDef, minProp));
                        if (maxProp && Read<int>(maxProp, profile::PropElementSize) >= 4) mx = Read<float>(GetField(cachedDef, maxProp));
                        double lo = std::clamp((double)mn * 0.01, 0.0, 1.0);
                        double hi = std::clamp((double)mx * 0.01, 0.0, 1.0);
                        if (hi < lo) hi = lo;
                        double spawnPercent = lo + (hi - lo) * (rand() / (float)RAND_MAX);
                        if ((rand() / (float)RAND_MAX) > spawnPercent) {
                            s_processedSpawners.insert(spawner);   // rolled once — outcome stands
                            continue;
                        }
                    }
                }
                Vector loc{};
                Rotator rot{};
                if (!ActorLocation(spawner, loc)) { ++pending; continue; }
                { Call c("/Script/Engine.Actor.K2_GetActorRotation"); if (c.ok() && c.run(spawner)) rot = c.result<Rotator>(); }
                bool dup = false;
                for (auto& s : spawnedAt) {
                    double dx = s.X - loc.X, dy = s.Y - loc.Y;
                    if (dx*dx + dy*dy < 600.0 * 600.0) { dup = true; break; }
                }
                if (dup) { s_processedSpawners.insert(spawner); continue; }
                if (SpawnVehicleAt(vehicleClass, loc, rot)) {
                    ++spawned;
                    spawnedAt.push_back(loc);
                    s_processedSpawners.insert(spawner);
                    if (spawned <= 12)
                        LOG("[Vehicles] ✅ %s @ %.0f %.0f %.0f", ObjName(vehicleClass).c_str(), loc.X, loc.Y, loc.Z);
                } else {
                    ++pending;   // spawn failed this pass (streaming) — retry
                }
            }
            if (s_processedSpawners.size() >= spawners.size()) {
                session.vehicles_spawned = true;
                LOG("[Vehicles] ✅ complete: %zu/%zu spawners → %d vehicles (attempts=%d)",
                    s_processedSpawners.size(), spawners.size(), (int)spawnedAt.size(), session.vehicle_attempts);
            } else if (session.vehicle_attempts >= 12) {
                session.vehicles_spawned = true;   // streaming window exhausted — stop cleanly
                LOG("[Vehicles] retry window exhausted: %zu/%zu spawners, %d vehicles",
                    s_processedSpawners.size(), spawners.size(), (int)spawnedAt.size());
            } else {
                LOG("[Vehicles] pass: +%d this pass, %d total, %d/%zu spawners done, %d pending — retrying (attempts=%d)",
                    spawned, (int)spawnedAt.size(), resolved == 0 ? 0 : (int)s_processedSpawners.size(),
                    spawners.size(), pending, session.vehicle_attempts);
            }
        }
    }
}

// [F45/G18-c] WALL CLIMB — v2. The old assist only fired while FALLING
// (vel.Z < 0), so jumping INTO a wall (vel.Z > 0 on the way up) never
// boosted: climbing felt dead. v2 fires in BOTH phases whenever the pawn is
// pressed against an obstacle (horizontal speed collapsed) with jump held:
//   • upward phase (vel.Z >= 0): hSpeed < 300 → blocked by the wall → boost
//   • falling phase (vel.Z < 0): hSpeed < 480 → falling into the face → boost
// Boosts chain every 0.45s while jump is held (hold-to-climb), capped at 4
// per air-time (reset the moment the pawn leaves MOVE_Falling) so open-air
// jumps can never turn into flight.
void Athena::TickMantleAssist() {
    if (!session.pawn) return;
    void* movement = GetObjField(session.pawn, "/Script/Engine.Character.CharacterMovement");
    if (!movement || IsBadPtr(movement)) return;
    uint8_t mode = Read<uint8_t>(movement, profile::CharacterMovement_MovementMode);
    if (mode != 3) { session.climb_boosts = 0; return; }   // MOVE_Falling only; reset chain on ground
    if (session.climb_boosts >= 4) return;                 // chain cap — no infinite flight
    if (!GetBool(session.pawn, "/Script/Engine.Character.bPressedJump")) return;
    static double next = 0;
    double now = Monotonic();
    if (now < next) return;
    Vector vel{};
    void* velProp = GetProp("/Script/Engine.MovementComponent.Velocity");
    if (velProp && Read<int>(velProp, profile::PropElementSize) == 24)
        vel = Read<Vector>(GetField(movement, velProp));
    if (!std::isfinite(vel.X) || !std::isfinite(vel.Y) || !std::isfinite(vel.Z)) return;
    double hSpeed = std::sqrt(vel.X * vel.X + vel.Y * vel.Y);
    // Wall evidence: while moving INTO an obstacle the engine zeroes the
    // horizontal velocity — falling with residual speed OR rising with a
    // collapsed speed both mean "pressed against a wall".
    bool againstWall = (vel.Z >= 0.0 && hSpeed < 300.0) || (vel.Z < 0.0 && hSpeed < 480.0);
    if (!againstWall) return;
    Vector fwd{};
    { Call f("/Script/Engine.Actor.GetActorForwardVector"); if (f.ok() && f.run(session.pawn)) fwd = f.result<Vector>(); }
    if (!std::isfinite(fwd.X) || !std::isfinite(fwd.Y)) return;
    Vector launch{ fwd.X * 160.0, fwd.Y * 160.0, 640.0 };
    Call l("/Script/Engine.Character.LaunchCharacter");
    // Override both components — a predictable boost regardless of the
    // current fall speed (additive mode was eaten by fast falls).
    bool xy = true, z = true;
    if (l.ok() && l.set("LaunchVelocity", launch)) {
        l.set("bXYOverride", xy);
        l.set("bZOverride", z);
        if (l.run(session.pawn)) {
            ++session.climb_boosts;
            next = now + 0.45;
            static bool logged = false;
            if (!logged) { logged = true; LOG("[Climb] ✅ wall-climb v2 live (hold jump against a wall to climb)"); }
        }
    }
}

// [F44] Reality Tree proximity (21.30) — discover the map's reality actors
// once through GObjects, then re-trigger their proximity blueprint events
// when the player walks close. Best-effort: the visual/audio systems are
// cooked inside the tree's BP; without a server they just need a nudge.
void Athena::TickRealityProximity() {
    static std::vector<void*> classes;
    static std::vector<std::string> classNames;
    static std::vector<void*> actors;
    static int failedSweeps = 0;
    static bool discovered = false;
    static double nextScan = 0;
    static double lastApproach = 0;
    double now = Monotonic();

    if (!discovered) {
        if (failedSweeps >= 2 || now < nextScan) return;
        nextScan = now + 8.0;
        GObjects_t* go = GObjectArray();
        void* classType = FindObj("/Script/CoreUObject.Class");
        if (!go || !classType) { ++failedSweeps; return; }
        classes.clear(); classNames.clear();
        for (int32 i = 0; i < go->NumElements && (int)classes.size() < 4; ++i) {
            void* object = go->GetById((uint32)i);
            if (!object || IsBadPtr(object)) continue;
            if (Read<void*>(object, profile::ObjectClass) != classType) continue;   // UClass objects only
            std::string n = ObjName(object);
            if (n.find("RealityTree") == std::string::npos &&
                n.find("RealitySapling") == std::string::npos) continue;
            classes.push_back(object);
            classNames.push_back(n);
        }
        if (!classes.empty()) {
            discovered = true;
            LOG("[Reality] ✅ discovered %zu reality classes (e.g. %s)", classes.size(), classNames[0].c_str());
        } else {
            ++failedSweeps;
        }
        return;
    }

    if (now - nextScan > 5.0) {
        nextScan = now;
        actors.clear();
        for (void* cls : classes) {
            std::vector<void*> found;
            if (GetAllActorsOfClassRaw(cls, found))
                for (void* a : found) if (a && !IsBadPtr(a)) actors.push_back(a);
        }
    }
    if (actors.empty() || !session.pawn) return;

    Vector pawnLoc{};
    if (!ActorLocation(session.pawn, pawnLoc)) return;
    for (void* a : actors) {
        Vector loc{};
        if (!ActorLocation(a, loc)) continue;
        double dx = loc.X - pawnLoc.X, dy = loc.Y - pawnLoc.Y, dz = loc.Z - pawnLoc.Z;
        if (dx*dx + dy*dy + dz*dz > 25000.0 * 25000.0) continue;
        if (now - lastApproach < 45.0) return;
        lastApproach = now;
        LOG("[Reality] ✅ proximity to %s — triggering effect", ObjName(Read<void*>(a, profile::ObjectClass)).c_str());
        static const char* events[] = {"OnPlayerNear", "BP_OnPlayerNear", "OnApproach", "PlayerNear", "BP_OnApproach"};
        std::string cn = ObjName(Read<void*>(a, profile::ObjectClass));
        for (const char* ev : events) {
            Call c(("/Script/FortniteGame." + cn + "." + ev).c_str());
            if (c.ok() && c.run(a)) { LOG("[Reality] fired %s", ev); break; }
        }
        break;
    }
}

// [F28] Seat weapons (Erbium EquipVehicleWeapon parity): while the pawn sits
// in a WeaponSeatDefinitions seat, grant + equip the seat weapon with a full
// clip; on seat change/exit remove it and re-equip the previous weapon or the
// pickaxe (ServerOnExitVehicle parity).
void Athena::GiveVehicleWeapon(void* weaponDef, int seatIdx) {
    if (!weaponDef || IsBadPtr(weaponDef) || !session.pawn || !session.inventory) return;
    if (session.vehicle_weapon_active && seatIdx == session.vehicle_weapon_seat) return;
    if (session.vehicle_weapon_active && seatIdx != session.vehicle_weapon_seat) RemoveVehicleWeapon(false);
    void* cur = Read<void*>(session.pawn, profile::FortPawn_CurrentWeapon);
    if (cur && !IsBadPtr(cur)) {
        void* p = GetProp("/Script/FortniteGame.FortWeapon.ItemEntryGuid");
        Guid cg = p ? Read<Guid>(GetField(cur, p)) : Guid{};
        if (!memcmp(&cg, &session.vehicle_weapon_guid, sizeof(Guid))) {
            session.vehicle_weapon_active = true;   // already holding it
            return;
        }
        if (!session.vehicle_weapon_active)
            session.previous_weapon_guid = cg;      // remembered for re-equip
    }
    Guid guid = GuidOfDefInInventory(weaponDef);
    if (!(guid.A | guid.B | guid.C | guid.D)) {
        int slot = -1;
        for (int i = 1; i < 6; ++i) if (!session.slots[(size_t)i]) { slot = i; break; }
        if (!GiveItem(weaponDef, 1, slot, slot >= 0 ? 0 : (uint8_t)-1, false, ClipSizeOf(weaponDef))) return;
        guid = GuidOfDefInInventory(weaponDef);
    }
    if (!(guid.A | guid.B | guid.C | guid.D)) return;
    session.vehicle_weapon_guid = guid;
    session.vehicle_weapon_seat = seatIdx;
    session.vehicle_weapon_active = true;
    OnServerExecuteInventoryItem(guid);
    LOG("[Vehicle] ✅ seat weapon %s seat=%d", ObjName(weaponDef).c_str(), seatIdx);
}

void Athena::RemoveVehicleWeapon(bool reequip) {
    if (!session.vehicle_weapon_active) return;
    session.vehicle_weapon_active = false;
    RemoveItemByGuid(session.vehicle_weapon_guid, false, -1);
    session.vehicle_weapon_guid = Guid{};
    session.vehicle_weapon_seat = -1;
    if (reequip) {
        if (session.previous_weapon_guid.A | session.previous_weapon_guid.B | session.previous_weapon_guid.C | session.previous_weapon_guid.D) {
            OnServerExecuteInventoryItem(session.previous_weapon_guid);
        } else if (session.slots[0]) {
            EquipSlot(0);   // pickaxe fallback (Erbium parity)
        }
        session.previous_weapon_guid = Guid{};
    }
}

void Athena::TickVehicleSeats(bool force) {
    if (!session.pawn) return;
    static double nextPoll = 0;
    double now = Monotonic();
    if (!force && now < nextPoll) return;   // [G09] event-driven calls are instant
    nextPoll = now + 0.5;
    void* vehicle = nullptr;
    for (const char* name : {"GetVehicleActor", "GetVehicle"}) {
        Call c((std::string("/Script/FortniteGame.FortPlayerPawnAthena.") + name).c_str());
        if (!c.ok()) c = Call((std::string("/Script/FortniteGame.FortPawn.") + name).c_str());
        if (c.ok() && c.run(session.pawn)) vehicle = c.result<void*>();
        if (vehicle && !IsBadPtr(vehicle)) break;
        vehicle = nullptr;
    }
    if (!vehicle) { RemoveVehicleWeapon(true); return; }
    void* seatComp = nullptr, *wpnComp = nullptr;
    {
        Call get("/Script/Engine.Actor.GetComponentByClass");
        if (get.ok()) {
            void* sc = FindObj("/Script/FortniteGame.FortVehicleSeatComponent");
            if (sc && get.set("ComponentClass", sc) && get.run(vehicle)) seatComp = get.result<void*>();
            void* wc = FindObj("/Script/FortniteGame.FortVehicleSeatWeaponComponent");
            if (wc) {
                get = Call("/Script/Engine.Actor.GetComponentByClass");
                if (get.ok() && get.set("ComponentClass", wc) && get.run(vehicle)) wpnComp = get.result<void*>();
            }
        }
    }
    // [F28] seat index via PlayerSlots (Erbium FindSeatIndex 1:1)
    int seatIdx = -1;
    if (seatComp && !IsBadPtr(seatComp)) {
        void* slotsProp = GetProp("/Script/FortniteGame.FortVehicleSeatComponent.PlayerSlots");
        void* playerProp = GetProp("/Script/FortniteGame.AthenaCarPlayerSlot.Player");
        if (slotsProp && playerProp) {
            int stride = Read<int>(slotsProp, profile::PropElementSize);
            Array slots = Read<Array>(GetField(seatComp, slotsProp));
            if (stride >= 8 && slots.Count >= 0 && slots.Count < 32 && slots.Data && !IsBadPtr(slots.Data)) {
                for (int i = 0; i < slots.Count; ++i) {
                    void* row = (char*)slots.Data + (size_t)i * stride;
                    void* player = ResolveWeak(GetField(row, playerProp));
                    if (player == session.pawn) { seatIdx = i; break; }
                }
            }
        }
    }
    if (seatIdx < 0) { RemoveVehicleWeapon(true); return; }
    if (!wpnComp || IsBadPtr(wpnComp)) { RemoveVehicleWeapon(true); return; }
    void* wsdProp = GetProp("/Script/FortniteGame.FortVehicleSeatWeaponComponent.WeaponSeatDefinitions");
    if (!wsdProp) { RemoveVehicleWeapon(true); return; }
    int stride = Read<int>(wsdProp, profile::PropElementSize);
    void* inner = Read<void*>(wsdProp, 0x70);                 // FArrayProperty.Inner
    void* sStruct = inner ? Read<void*>(inner, 0x70) : nullptr; // FStructProperty.Struct
    int seatOff = sStruct ? PropOffsetOn(sStruct, "SeatIndex") : -1;
    int wpnOff  = sStruct ? PropOffsetOn(sStruct, "VehicleWeapon") : -1;
    if (stride < 16 || seatOff < 0 || wpnOff < 0) { RemoveVehicleWeapon(true); return; }
    Array defs = Read<Array>(GetField(wpnComp, wsdProp));
    if (!defs.Data || IsBadPtr(defs.Data) || defs.Count < 0 || defs.Count > 16) { RemoveVehicleWeapon(true); return; }
    for (int i = 0; i < defs.Count; ++i) {
        void* row = (char*)defs.Data + (size_t)i * stride;
        if (Read<int32>(row, seatOff) != seatIdx) continue;
        void* wdef = Read<void*>(row, wpnOff);
        if (wdef && !IsBadPtr(wdef)) { GiveVehicleWeapon(wdef, seatIdx); return; }
        break;
    }
    RemoveVehicleWeapon(true);
}

// ============================================================================
// SAFE ZONE / MISC
// ============================================================================
void Athena::SpawnSafeZoneIndicator() {
    if (session.safezone_spawned || !session.state || !session.world) return;
    void* existing = GetObjField(session.state, "/Script/FortniteGame.FortGameStateAthena.SafeZoneIndicator");
    if (existing && !IsBadPtr(existing)) { session.safezone_spawned = true; return; }
    void* cls = FindObj("/Script/FortniteGame.FortSafeZoneIndicator");
    if (!cls) return;
    Transform t{};
    void* indicator = SpawnActor(session.world, cls, session.state, t);
    if (!indicator) return;
    void* prop = GetProp("/Script/FortniteGame.FortGameStateAthena.SafeZoneIndicator");
    if (prop) Write<void*>(GetField(session.state, prop), 0, indicator);
    Call n("/Script/FortniteGame.FortGameStateAthena.OnRep_SafeZoneIndicator");
    if (n.ok()) n.run(session.state);
    session.safezone_spawned = true;
}

void Athena::FixBuildingFoundations() {
    void* cls = FindObj("/Script/FortniteGame.BuildingFoundation");
    if (!cls || !session.world) return;
    GObjects_t* go = GObjectArray();
    if (!go) return;
    void* p = GetProp("/Script/FortniteGame.BuildingFoundation.DynamicFoundationType");
    if (!p) return;
    int propOff = Read<int>(p, profile::PropOffset);
    if (propOff < 0 || propOff > 0x20000) return;
    int fixed = 0;
    for (int32 i = 0; i < go->NumElements; ++i) {
        void* o = go->GetById((uint32)i);
        if (!o || IsBadPtr(o)) continue;
        if (Read<void*>(o, profile::ObjectClass) != cls) continue;
        Write<char>((char*)o + propOff, 0, 0);
        ++fixed;
    }
    LOG("[Foundations] Static type applied to %d foundations", fixed);
}

// ============================================================================
// MISSING CITIES / BUILDINGS — [F-cities r1]
// ============================================================================
// 21.30 preassembled structures (POIs, cities, buildings) stream through
// AFortSaveFileBuildingInstructionsHandler::LoadBuildings(FortPC) — the same
// native driver the live server runs. In standalone nobody drives it, so the
// handler ships inert (bLoadInvisible / no auto-spawn) and whole POIs never
// materialise. Fix: find the handler actors, flip the invisible flag off and
// run the load pass ourselves. The retry window (TickBuildingLoads) covers
// handlers that stream in late with the POI sublevels.
void Athena::FixMissingBuildings() {
    if (!session.world) return;
    void* cls = FindObj("/Script/FortniteGame.FortSaveFileBuildingInstructionsHandler");
    if (!cls) { LOG("[Cities] handler class not present"); return; }
    std::vector<void*> handlers;
    if (!GetAllActorsOfClassRaw(cls, handlers) || handlers.empty()) { LOG("[Cities] no handler actors (yet)"); return; }
    int driven = 0;
    for (void* h : handlers) {
        if (!h || IsBadPtr(h)) continue;
        SetBool(h, "/Script/FortniteGame.FortSaveFileBuildingInstructionsHandler.bLoadInvisible", false);
        Call load("/Script/FortniteGame.FortSaveFileBuildingInstructionsHandler.LoadBuildings");
        if (!load.ok()) continue;
        load.set("FortPC", session.controller);
        if (load.run(h)) ++driven;
    }
    LOG("[Cities] LoadBuildings driven on %d/%zu handlers", driven, handlers.size());
    if (driven > 0) {
        // Native driver loaded the pieces — level-set the foundations so the
        // static/ dynamic split matches the standalone expectations.
        FixBuildingFoundations();
    }
}

void Athena::TickBuildingLoads() {
    if (session.building_load_attempts >= 10) return;
    static double last = 0;
    double now = Monotonic();
    if (now - last < 15.0) return;
    last = now;
    void* cls = FindObj("/Script/FortniteGame.FortSaveFileBuildingInstructionsHandler");
    if (!cls) return;
    std::vector<void*> handlers;
    if (!GetAllActorsOfClassRaw(cls, handlers) || handlers.empty()) return;
    if (session.building_load_attempts == 0) LOG("[Cities] handler actors found: %zu", handlers.size());
    ++session.building_load_attempts;
    LOG("[Cities] loading attempt %d", session.building_load_attempts);
    FixMissingBuildings();
}

// ============================================================================
// [G22] WORLD PARTITION / MISSING POIs — diagnostics + activation attempts
// ============================================================================
// Symptom (round 17): terrain loads (persistent level) but NO POI buildings
// stream in, and the landscape has open holes where POI geometry plugs into
// the terrain (the player fell through one and died). Artemis_Terrain is a
// World Partition map: POIs live in runtime cells that stream around
// streaming sources (the local player). This pass logs the exact WP state
// and makes every SAFE activation attempt available to us without the engine
// binaries — [G23-c] each part fault-guarded, part B's scan class-verified:
//   A. World->Levels / StreamingLevels counts + the streaming-state split
//      (wantLoad / wantVisible / haveLoadedLevel — the holes evidence)
//   B. the UWorldPartition object itself (reflected property, then a
//      class-verified memory scan of the World — NO name calls on
//      candidates) + its reflected member dump
//   C. ADataLayerManager actors + per-data-layer effective state + an
//      activation attempt through any resolvable SetDataLayer* UFunction
// The streaming-source forcing + FlushLevelStreaming moved to their own
// ticker (TickStreamingSources [G23-e]) — the round-10 part-B crash used to
// kill them before they ever ran.
void Athena::DiagnoseWorldPartition() {
    if (!session.world || IsBadPtr(session.world)) return;
    LOG("[WP] === World Partition diagnostic pass %d === [G22/G23]", session.wp_diag_passes + 1);
    void* worldCls = Read<void*>(session.world, profile::ObjectClass);
    // ---- A. what is actually loaded ------------------------------------
    // [G23-c] part A is fault-guarded too — a bad Levels entry must not
    // kill the diagnostic (or the process: the whole tick follows it).
    gas::RunGuarded("[WP] partA", [&] {
        if (worldCls && !IsBadPtr(worldCls)) {
            int levelsOff = PropOffsetOn(worldCls, "Levels");
            if (levelsOff >= 0) {
                Array levels = Read<Array>((char*)session.world + levelsOff);
                int named = 0;
                for (int i = 0; levels.Data && !IsBadPtr(levels.Data) && i < levels.Count && i < 6; ++i) {
                    void* lvl = Read<void*>(levels.Data, (size_t)i * 8);
                    if (!lvl || IsBadPtr(lvl) || !IsReadable(lvl, 0x20)) continue;
                    LOG("[WP]   loaded level[%d]: %s", i, ObjName(lvl).c_str());
                    ++named;
                }
                LOG("[WP] Levels array: %d loaded (%d named above)", levels.Count, named);
            } else LOG("[WP] UWorld.Levels not reflected");
            int ssOff = PropOffsetOn(worldCls, "StreamingLevels");
            if (ssOff >= 0) {
                Array ss = Read<Array>((char*)session.world + ssOff);
                // [G23-c] streaming-state split — the "holes where buildings
                // should be" diagnosis: how many streamers are flagged
                // loaded/visible vs actually holding a loaded level.
                int wantLoad = 0, wantVis = 0, haveLevel = 0, checked = 0;
                for (int i = 0; ss.Data && !IsBadPtr(ss.Data) && i < ss.Count && checked < 256; ++i) {
                    void* ls = Read<void*>(ss.Data, (size_t)i * 8);
                    if (!ls || IsBadPtr(ls) || !IsReadable(ls, 0x10)) continue;
                    ++checked;
                    if (GetBool(ls, "/Script/Engine.LevelStreaming.bShouldBeLoaded")) ++wantLoad;
                    if (GetBool(ls, "/Script/Engine.LevelStreaming.bShouldBeVisible")) ++wantVis;
                    void* lv = GetObjField(ls, "/Script/Engine.LevelStreaming.LoadedLevel");
                    if (lv && !IsBadPtr(lv)) ++haveLevel;
                }
                LOG("[WP] StreamingLevels: %d (checked %d: wantLoad=%d wantVisible=%d haveLoadedLevel=%d)",
                    ss.Count, checked, wantLoad, wantVis, haveLevel);
            } else LOG("[WP] UWorld.StreamingLevels not reflected");
        }
    });
    // ---- B. the WorldPartition object ----------------------------------
    // [G23-c] REWRITTEN — the round-10 crash lived here: the scan called
    // ObjName() (native FName::ToString) on any readable candidate, and a
    // garbage-but-readable "class" with an in-range FName index walked
    // GNames out of bounds → unguarded SIGSEGV → process death (the log
    // died between "StreamingLevels: 993" and part B's first line). The
    // scan now COMPARES the candidate's class pointer against the real
    // UWorldPartition class object — zero native name calls on candidates.
    // Every part is fault-guarded so even a surprise fault is named + skipped.
    void* partition = nullptr;
    gas::RunGuarded("[WP] partB", [&] {
        static const char* const kPartProps[] = {
            "/Script/Engine.World.WorldPartition", "/Script/Engine.World.Partition"
        };
        for (const char* pp : kPartProps) {
            void* prop = GetProp(pp);
            if (prop) {
                void* p = Read<void*>(GetField(session.world, prop));
                if (p && !IsBadPtr(p)) partition = p;
                break;
            }
        }
        if (partition) LOG("[WP] Partition object via reflection: %p", partition);
        if (!partition) {
            void* wpClass = FindObj("/Script/Engine.WorldPartition");
            if (!wpClass) wpClass = FindObj("/Script/Engine.WorldPartitionRuntimeHash");
            for (size_t off = 0x40; off <= 0x800 && !partition; off += 8) {
                void* p = Read<void*>((char*)session.world + off, 0);
                if (!p || IsBadPtr(p) || ((uintptr_t)p & 7) || !IsReadable(p, 0x20)) continue;
                void* cls = Read<void*>(p, profile::ObjectClass);
                if (!cls || IsBadPtr(cls) || !IsReadable(cls, 0x20)) continue;
                // [G23-c] exact class-object comparison — NO ObjName on the
                // candidate (the round-10 crash class).
                if (wpClass && cls == wpClass) {
                    partition = p;
                    LOG("[WP] Partition object found via scan @ World+0x%zX (class-verified, no name calls)", off);
                }
            }
        }
        if (partition && !IsBadPtr(partition) && IsReadable(partition, 0x40)) {
            int dumped = 0;
            void* u = partition;
            for (int hop = 0; u && !IsBadPtr(u) && hop < 4 && dumped < 24; ++hop) {
                void* f = (profile::HasFField ? Read<void*>(u, profile::StructChildProps) : nullptr);
                for (int i = 0; f && !IsBadPtr(f) && i < 64 && dumped < 24; ++i, f = Read<void*>(f, profile::FFieldNext)) {
                    std::string n;
                    if (!SafeFieldName(f, profile::FFieldName, n)) continue;
                    LOG("[WP]   member '%s' off=%d size=%d", n.c_str(),
                        Read<int>(f, profile::PropOffset), Read<int>(f, profile::PropElementSize));
                    ++dumped;
                }
                void* c = Read<void*>(u, profile::StructChildren);
                for (int i = 0; c && !IsBadPtr(c) && i < 64 && dumped < 24; ++i, c = Read<void*>(c, profile::FieldNext)) {
                    std::string n;
                    if (!SafeFieldName(c, profile::ObjectName, n)) continue;
                    LOG("[WP]   member '%s' off=%d size=%d", n.c_str(),
                        Read<int>(c, profile::PropOffset), Read<int>(c, profile::PropElementSize));
                    ++dumped;
                }
                void* next = Read<void*>(u, profile::StructSuper);
                if (next == u) break;
                u = next;
            }
            LOG("[WP] partition reflected member dump: %d properties", dumped);
        } else {
            LOG("[WP] ⚠️ NO WorldPartition object found on this world — WP streaming never initialized");
        }
    });
    // ---- C. data layers ---------------------------------------------------
    // [G23-c] guarded + class-verified lookups only.
    int layersActivated = 0;
    gas::RunGuarded("[WP] partC", [&] {
        void* dlmCls = FindObj("/Script/Engine.DataLayerManager");
        if (!dlmCls) LOG("[WP] DataLayerManager class not present");
        else {
            std::vector<void*> managers;
            if (GetAllActorsOfClassRaw(dlmCls, managers) && !managers.empty()) {
                for (void* m : managers) {
                    if (!m || IsBadPtr(m)) continue;
                    void* dlProp = GetProp("/Script/Engine.DataLayerManager.DataLayers");
                    if (!dlProp) { LOG("[WP] DataLayers property not reflected"); continue; }
                    Array layers = Read<Array>(GetField(m, dlProp));
                    LOG("[WP] DataLayerManager %p: %d data layers", m, layers.Count);
                    for (int i = 0; layers.Data && !IsBadPtr(layers.Data) && i < layers.Count && i < 64; ++i) {
                        void* layer = Read<void*>(layers.Data, (size_t)i * 8);
                        if (!layer || IsBadPtr(layer) || !IsReadable(layer, 0x20)) continue;
                        bool effective = GetBool(layer, "/Script/Engine.DataLayer.bIsEffective");
                        LOG("[WP]   layer[%d] '%s' effective=%d", i, ObjName(layer).c_str(), (int)effective);
                    }
                    // activation attempt — first resolvable SetDataLayer* UFunction
                    static const char* const kAct[] = {
                        "SetDataLayerRuntimeState", "SetDataLayerState", "SetDataLayerIsEffective",
                        "ActivateDataLayer", "SetDataLayerActive"
                    };
                    for (const char* act : kAct) {
                        Call c((std::string("/Script/Engine.DataLayerManager.") + act).c_str());
                        if (!c.ok()) continue;
                        LOG("[WP] activation attempt via %s", act);
                        for (int i = 0; layers.Data && !IsBadPtr(layers.Data) && i < layers.Count && i < 64; ++i) {
                            void* layer = Read<void*>(layers.Data, (size_t)i * 8);
                            if (!layer || IsBadPtr(layer) || !IsReadable(layer, 0x20)) continue;
                            bool bound = c.set("DataLayer", layer) || c.set("InDataLayer", layer) || c.set("Layer", layer);
                            if (!bound) continue;
                            bool on = true;
                            c.set("bIsEffective", on); c.set("RuntimeState", on); c.set("bActive", on); c.set("NewState", on);
                            if (c.run(m)) ++layersActivated;
                        }
                        break;   // first resolvable function only
                    }
                }
            } else LOG("[WP] no DataLayerManager actors");
        }
    });
    if (layersActivated > 0) LOG("[WP] ✅ %d data layers activated [G22]", layersActivated);
    ++session.wp_diag_passes;
}

// ============================================================================
// [G23-e] STREAMING SOURCES — extracted from the WP diagnostic into its own
// ticker. Round-10 proof it was needed: the diagnostic crashed in part B
// BEFORE the old "attempts 1 & 2" block ever ran, so on that entire session
// the pawn never became a World Partition streaming source — WP streamed
// around nothing useful and the player fell through unloaded cells. The
// reflected bIsStreamingSource flag is what the WP streaming-source iterator
// keys on; forcing it on pawn + controller (4 passes × 30 s, idempotent) plus
// a FlushLevelStreaming console pass gives the runtime a source at the player
// for the whole match.
// ============================================================================
void Athena::TickStreamingSources() {
    if (session.src_force_passes >= 4) return;
    static double lastTry = 0;
    double now = Monotonic();
    if (session.src_force_passes > 0 && now - lastTry < 30.0) return;
    lastTry = now;
    ++session.src_force_passes;
    int forced = 0;
    void* srcProp = GetProp("/Script/Engine.Actor.bIsStreamingSource");
    if (srcProp) {
        if (session.pawn && PawnStillValid(session.pawn)) {
            SetBool(session.pawn, "/Script/Engine.Actor.bIsStreamingSource", true);
            ++forced;
        }
        if (session.controller) {
            SetBool(session.controller, "/Script/Engine.Actor.bIsStreamingSource", true);
            ++forced;
        }
    }
    LOG("[WP] streaming-source pass %d/4: %d forced%s",
        session.src_force_passes, forced,
        srcProp ? " (pawn+controller)" : " — bIsStreamingSource not reflected");
    Call f("/Script/Engine.KismetSystemLibrary.ExecuteConsoleCommand");
    if (f.ok()) {
        auto cmd = ToWide("FlushLevelStreaming");
        FStringNative command{(TCHAR*)cmd.data(), (int32)cmd.size() + 1, (int32)cmd.size() + 1};
        if (f.set("WorldContextObject", session.world) && f.set("Command", command) && f.set("SpecificPlayer", session.controller))
            f.run(FindObj("/Script/Engine.Default__KismetSystemLibrary"));
        LOG("[WP] FlushLevelStreaming console pass dispatched");
    }
}

// ============================================================================
// [G24] VOID GUARD — "fell through a hole where the building should be and
// died at the bottom of the map". The root causes (unstreamed WP cell,
// loaded-but-invisible geometry) are being fixed at the streaming layer, but
// the player must never lose their run to a streaming hiccup: this watch
// records the last grounded position (sane terrain band) and teleports the
// pawn back above it when it falls far below the map floor — loud log, 6 s
// cooldown, and the third rescue re-arms the streaming-source passes.
// ============================================================================
void Athena::TickVoidGuard() {
    if (!session.finalized || !session.pawn || session.death_handled || session.win_handled) return;
    static double lastCheck = 0;
    double now = Monotonic();
    if (now - lastCheck < 0.4) return;
    lastCheck = now;
    if (!PawnStillValid(session.pawn)) return;
    Vector loc{};
    if (!ActorLocation(session.pawn, loc)) return;
    if (loc.Z > -1000.0 && loc.Z < 3000.0) {          // grounded band (excludes skydive altitudes)
        session.last_safe_loc = loc;
        session.has_safe_loc = true;
        return;
    }
    if (loc.Z < -2500.0 && now - session.last_void_rescue > 6.0) {
        session.last_void_rescue = now;
        Vector target = session.has_safe_loc ? session.last_safe_loc
            : (session.has_spawn_loc ? session.spawn_loc : Vector{loc.X, loc.Y, 1000.0});
        target.Z += 400.0;
        bool done = false;
        Call sl("/Script/Engine.Actor.K2_SetActorLocationAndRotation");
        if (sl.ok()) {
            bool bSweep = false, bTeleport = true;
            Rotator rot{};
            sl.set("NewLocation", target);
            sl.set("NewRotation", rot);
            sl.set("bSweep", bSweep);
            sl.set("bTeleport", bTeleport);
            done = sl.run(session.pawn);
        }
        ++session.void_rescues;
        LOG("[VoidGuard] 🔁 pawn below the map floor (Z=%.0f) — %s (%.0f, %.0f, %.0f) rescue #%d [G24]",
            loc.Z, done ? "teleported to last grounded spot" : "teleport dispatch failed (will retry)",
            target.X, target.Y, target.Z, session.void_rescues);
        if (session.void_rescues == 3) {
            // repeated falls — the cell under the player is not streaming in:
            // re-arm the streaming-source pass budget and flush streaming.
            session.src_force_passes = 0;
            LOG("[VoidGuard] repeated falls — streaming-source passes re-armed + FlushLevelStreaming");
            Call f("/Script/Engine.KismetSystemLibrary.ExecuteConsoleCommand");
            if (f.ok()) {
                auto cmd = ToWide("FlushLevelStreaming");
                FStringNative command{(TCHAR*)cmd.data(), (int32)cmd.size() + 1, (int32)cmd.size() + 1};
                if (f.set("WorldContextObject", session.world) && f.set("Command", command) && f.set("SpecificPlayer", session.controller))
                    f.run(FindObj("/Script/Engine.Default__KismetSystemLibrary"));
            }
        }
    }
}

// ============================================================================
// FLOOR LOOT WHILE STREAMING — [F-loot-batch r2]
// ============================================================================
// Drains the spawner queue a few entries per tick (no frame stalls), keeps
// rediscovering streamed-in POI sublevel spawners for ~2.5 minutes, then
// logs one final summary. The old version re-ran the FULL synchronous
// rollout here — the second 2958-actor storm that finished the phone off.
void Athena::TickFloorLootStreaming() {
    if (!session.finalized) return;
    flo::Drain(8);   // [G20] 8/tick + 8 ms wall-clock cap (was 12, unbounded)
    double now = Monotonic();
    if (flo::lastRediscover < 0) flo::lastRediscover = now;   // arm on first tick
    if (flo::rediscoveries < 10 && now - flo::lastRediscover >= 15.0) {
        flo::lastRediscover = now;
        ++flo::rediscoveries;
        int added = flo::Enqueue();
        if (added) LOG("[Loot] streaming check %d/10: +%d spawners queued (%d rolled so far)",
                       flo::rediscoveries, added, flo::spawned + flo::failed);
    }
    if (flo::rediscoveries >= 10 && flo::pending.empty() && !flo::doneLogged) {
        flo::doneLogged = true;
        LOG("[Loot] ✅ floor loot complete: %d spawners rolled (%d failed, %d removed) over %d streaming passes",
            flo::spawned + flo::failed, flo::failed, flo::destroyed, flo::rediscoveries);
    }
}

void Athena::TeleportToSpawnIsland() {
    if (!session.pawn) return;
    void* cls = FindObj("/Script/FortniteGame.FortPlayerStartWarmup");
    if (!cls) return;
    GObjects_t* go = GObjectArray();
    if (!go) return;
    std::vector<void*> starts;
    for (int32 i = 0; i < go->NumElements; ++i) {
        void* o = go->GetById((uint32)i);
        if (o && !IsBadPtr(o)) {
            void* k = Read<void*>(o, profile::ObjectClass);
            if (k == cls) starts.push_back(o);
        }
    }
    if (starts.empty()) return;
    void* start = starts[rand() % starts.size()];
    Call c("/Script/Engine.Actor.K2_GetActorLocation");
    Vector loc{0.0, 0.0, 1000.0};
    if (c.ok() && c.run(start)) loc = c.result<Vector>();
    Rotator rot{0.0, 0.0, 0.0};
    Call sl("/Script/Engine.Actor.K2_SetActorLocationAndRotation");
    if (sl.ok()) { bool bSweep = false, bTeleport = true; sl.set("NewLocation", loc); sl.set("NewRotation", rot); sl.set("bSweep", bSweep); sl.set("bTeleport", bTeleport); sl.run(session.pawn); }
}

void Athena::RemoveNetDebugUI() {
    void* wb = FindObj("/Script/UMG.Default__WidgetBlueprintLibrary");
    if (!wb) return;
    void* cls = FindObj("/Game/Athena/HUD/NetDebugUI.NetDebugUI_C");
    if (!cls) return;
    Call c("/Script/UMG.WidgetBlueprintLibrary.GetAllWidgetsOfClass");
    if (!c.ok() || !c.set("WorldContextObject", session.world) || !c.set("WidgetClass", cls)) return;
    if (!c.run(wb)) return;
    auto arr = c.result<Array>();
    for (int i = 0; i < arr.Count; ++i) {
        void* w = Read<void*>(arr.Data, (size_t)i * 8);
        if (w) { Call r("/Script/UMG.UserWidget.RemoveFromViewport"); if (r.ok()) r.run(w); }
    }
}

// ============================================================================
// EMOTES — [G07] Erbium ServerPlayEmoteItem parity + [F09] montage fallback
// ============================================================================
void Athena::PlayEmoteItem(void* item) {
    if (!item || !session.pawn) return;

    // [G07] NATIVE PATH (Erbium ServerPlayEmoteItem 1:1): build the ability
    // spec, hand it to GiveAbilityAndActivateOnce, and replicate the emote —
    // this plays through GAS (montage + replication + moving-emote support)
    // instead of a bare local Montage_Play.
    if (gas::GiveAbilityAndActivateOnce) {
        // [G13] Erbium ServerPlayEmoteItem parity: the PlayerState's ASC owns
        // emotes (fallback to the pawn's ASC when the prop read fails).
        void* asc = session.player_state
            ? GetObjField(session.player_state, "/Script/FortniteGame.FortPlayerState.AbilitySystemComponent")
            : nullptr;
        if (!asc || IsBadPtr(asc)) asc = GetASC(session.pawn);
        // Vehicle override (Erbium): while driving a FortCharacterVehicle the
        // vehicle's OverrideAbilitySystemComponent owns emotes.
        void* vehicle = nullptr;
        for (const char* name : {"GetVehicleActor", "GetVehicle"}) {
            Call c((std::string("/Script/FortniteGame.FortPlayerPawnAthena.") + name).c_str());
            if (!c.ok()) c = Call((std::string("/Script/FortniteGame.FortPawn.") + name).c_str());
            if (c.ok() && c.run(session.pawn)) vehicle = c.result<void*>();
            if (vehicle && !IsBadPtr(vehicle)) break;
            vehicle = nullptr;
        }
        if (vehicle && !IsBadPtr(vehicle)) {
            void* oasc = GetObjField(vehicle, "/Script/FortniteGame.FortCharacterVehicle.OverrideAbilitySystemComponent");
            if (oasc && !IsBadPtr(oasc)) asc = oasc;
        }
        if (asc) {
            // Resolve the ability class: CustomDanceAbility when the item
            // carries one, otherwise the generic emote ability.
            void* abilityClass = GetObjField(item, "/Script/FortniteGame.AthenaDanceItemDefinition.CustomDanceAbility");
            if (!abilityClass || IsBadPtr(abilityClass))
                abilityClass = LoadClass("/Game/Abilities/Emotes/GAB_Emote_Generic.GAB_Emote_Generic_C",
                                         "/Script/GameplayAbilities.GameplayAbility");
            if (!abilityClass) abilityClass = FindAbilityClassNamed("GAB_Emote_Generic_C");
            void* cdo = abilityClass ? gas::ClassDefaultOf(abilityClass) : nullptr;
            void* spec = cdo ? gas::BuildSpec(cdo, item) : nullptr;
            if (spec) {
                int32 outHandle = 0;
                // [G13] Fault-guarded: a scanner-resolved native must never
                // kill the process — on fault the native emote path is
                // disabled for the session and the montage fallback runs.
                bool gaaOk = gas::RunGuarded("Emote", gas::NativeGaaCall, asc, &outHandle, spec);
                free(spec);
                if (gaaOk) {
                    // Replicate the emote for every client (Erbium):
                    // LastReplicatedEmoteExecuted + OnRep_LastReplicatedEmoteExecuted.
                    void* prop = GetProp("/Script/FortniteGame.FortPlayerPawn.LastReplicatedEmoteExecuted");
                    if (prop && Read<int>(prop, profile::PropElementSize) == 8) {
                        void* old = Read<void*>(GetField(session.pawn, prop));
                        Write<void*>(GetField(session.pawn, prop), 0, item);
                        Call r("/Script/FortniteGame.FortPlayerPawn.OnRep_LastReplicatedEmoteExecuted");
                        if (r.ok()) {
                            if (r.set("LastEmotePlayed", old)) r.run(session.pawn);
                            else r.run(session.pawn);
                        }
                    }
                    LOG("[Emote] ✅ native GiveAbilityAndActivateOnce (handle=0x%X)", (unsigned)outHandle);
                    return;
                }
                LOG("[Emote] native GiveAbilityAndActivateOnce faulted — disabled for the session, montage fallback");
                gas::GiveAbilityAndActivateOnce = 0;
            }
            LOG("[Emote] native path incomplete — montage fallback");
        }
    }

    // [F09] Montage fallback: 21.30 signature BodyType/Gender/PawnContext.
    void* mesh = GetObjField(session.pawn, "/Script/Engine.Character.Mesh");
    if (!mesh) return;
    Call anim("/Script/Engine.SkeletalMeshComponent.GetAnimInstance");
    if (!anim.ok() || !anim.run(mesh)) return;
    void* ai = anim.result<void*>();
    if (!ai) return;
    Call ref("/Script/FortniteGame.FortMontageItemDefinitionBase.GetAnimationHardReference");
    void* montage = nullptr;
    if (ref.ok()) {
        // EEFortCustomBodyType / EEFortCustomGender are uint8 enums; try the
        // female/male combinations and fall back gracefully.
        struct Combo { uint8_t body; uint8_t gender; };
        const Combo combos[] = {{1, 1}, {0, 0}, {1, 0}, {0, 1}};
        for (auto& cb : combos) {
            if (ref.set("BodyType", cb.body) && ref.set("Gender", cb.gender) &&
                ref.set("PawnContext", session.pawn) && ref.run(item)) {
                montage = ref.result<void*>();
                if (montage) break;
            }
        }
    }
    if (!montage) { LOG("[Emote] no montage hard reference resolved"); return; }
    Call play("/Script/Engine.AnimInstance.Montage_Play");
    if (play.ok()) {
        play.set("MontageToPlay", montage);
        float rate = 1.0f; play.set("InPlayRate", rate);
        float start = 0.0f; play.set("InTimeToStartMontageAt", start);
        bool bStop = true; play.set("bStopAllMontages", bStop);
        play.run(ai);
        LOG("[Emote] playing %s", ObjName(montage).c_str());
    }
}

// [F11] ExitForceBehavior is EETryExitVehicleBehavior (uint8).
void Athena::OnExitVehicle() {
    if (!session.pawn) return;
    Call v("/Script/FortniteGame.FortPlayerPawn.ServerOnExitVehicle");
    if (v.ok()) {
        uint8_t ExitForce = 0; bool bDestroy = false;
        if (v.set("ExitForceBehavior", ExitForce) && v.set("bDestroyVehicleWhenForced", bDestroy)) v.run(session.pawn);
    }
}

// ============================================================================
// [G19-b] THE VEHICLE ENTER LADDER — shared by the enter RPC catch and the
// interact path. ServerAttemptEnterVehicle had NO handler before: the ability
// dispatched the Server RPC, the hook ignored it, the net layer dropped it —
// "the enter button does nothing". Each step is verified through
// FortPlayerPawnAthena.GetVehicleActor before the next runs, exactly like the
// G18-d interact ladder.
// ============================================================================
bool Athena::EnterVehicleDirect(void* vehicle, int seatIdx) {
    if (!vehicle || IsBadPtr(vehicle) || !IsReadable(vehicle, 0x60)) return false;
    if (!session.pawn || !PawnStillValid(session.pawn)) return false;
    void* targetCls = Read<void*>(vehicle, profile::ObjectClass);
    if (!IsVehicleTarget(targetCls)) {
        LOG("[Vehicle] ⚠️ enter target is not a vehicle: %s", ObjName(targetCls).c_str());
        return false;
    }
    auto verifySeated = []() -> bool {
        Call v("/Script/FortniteGame.FortPlayerPawnAthena.GetVehicleActor");
        if (!v.ok()) v = Call("/Script/FortniteGame.FortPawn.GetVehicleActor");
        if (v.ok() && session.pawn && v.run(session.pawn)) {
            void* cur = v.result<void*>();
            return cur && !IsBadPtr(cur);
        }
        return false;
    };
    if (verifySeated()) {   // already in a seat (double tap / re-entry)
        Athena::TickVehicleSeats(true);
        return true;
    }
    // 1) ForceSeatSinglePawn(PawnToSeat, SeatIndex) — SDK-verified
    {
        Call f("/Script/FortniteGame.FortAthenaVehicle.ForceSeatSinglePawn");
        if (!f.ok()) f = Call("/Script/FortniteGame.FortCharacterVehicle.ForceSeatSinglePawn");
        if (f.ok() && f.set("PawnToSeat", session.pawn) && f.set("SeatIndex", seatIdx) && f.run(vehicle)) {
            if (f.result<bool>() || verifySeated()) {
                LOG("[Vehicle] ✅ seated into %s seat %d (ForceSeatSinglePawn)",
                    ObjName(targetCls).c_str(), seatIdx);
                Athena::TickVehicleSeats(true);
                return true;
            }
        }
    }
    // 2) ForceSeatPawns(TArray<AFortPlayerPawn*>) — SDK-verified sibling
    {
        Call fp("/Script/FortniteGame.FortAthenaVehicle.ForceSeatPawns");
        if (!fp.ok()) fp = Call("/Script/FortniteGame.FortCharacterVehicle.ForceSeatPawns");
        if (fp.ok()) {
            static void* cell = nullptr;      // persistent backing store
            cell = session.pawn;
            struct TArr { void* data; int32_t n; int32_t max; } ta{ &cell, 1, 1 };
            if (fp.set_bytes("PawnsToSeat", &ta, sizeof(ta)) || fp.bind_at(0, &ta, sizeof(ta))) {
                fp.run(vehicle);
                if (verifySeated()) {
                    LOG("[Vehicle] ✅ seated into %s (ForceSeatPawns)", ObjName(targetCls).c_str());
                    Athena::TickVehicleSeats(true);
                    return true;
                }
            }
        }
    }
    // 3) OnPawnEnterVehicle(pawn, seat) — the server→client seat bookkeeping
    //    notification as the last resort.
    {
        Call enter("/Script/FortniteGame.FortCharacterVehicle.OnPawnEnterVehicle");
        if (enter.ok() && enter.set("PlayerPawn", session.pawn)) {
            enter.set("SeatIdx", seatIdx);
            if (enter.run(vehicle)) {
                LOG("[Vehicle] OnPawnEnterVehicle dispatched on %s (verify next tick)",
                    ObjName(targetCls).c_str());
                Athena::TickVehicleSeats(true);
                return true;
            }
        }
    }
    LOG("[Vehicle] ⚠️ enter ladder exhausted for %s", ObjName(targetCls).c_str());
    return false;
}

// [G19-b] Nearest vehicle to the pawn (mobile HUD-button fallback when the
// button fires a function that carries no vehicle parameter).
void* Athena::NearestVehicle(float radius) {
    if (!session.pawn) return nullptr;
    void* cls = FindObj("/Script/FortniteGame.FortAthenaVehicle");
    if (!cls) return nullptr;
    std::vector<void*> actors;
    if (!GetAllActorsOfClassRaw(cls, actors)) return nullptr;
    void* best = nullptr;
    double bestD2 = double(radius) * double(radius);
    Vector origin{};
    if (!ActorLocation(session.pawn, origin)) return nullptr;
    for (void* a : actors) {
        Vector loc{};
        if (!a || IsBadPtr(a) || !ActorLocation(a, loc)) continue;
        double dx = loc.X - origin.X, dy = loc.Y - origin.Y, dz = loc.Z - origin.Z;
        double d2 = dx*dx + dy*dy + dz*dz;
        if (d2 <= bestD2) { bestD2 = d2; best = a; }
    }
    return best;
}

// [G21] Vehicle exit ladder — mirrors the enter ladder's shape: find the
// current vehicle through the pawn's own accessor, dispatch the server→
// client bookkeeping event (OnPawnExitVehicle) on it, restore the held
// weapon (ServerOnExitVehicle parity), and the seat event above handles the
// instant weapon removal when the engine dispatches it itself.
bool Athena::ExitVehicleDirect() {
    static bool s_inExit = false;   // [G21] reentrancy latch — our own OnPawnExitVehicle dispatch re-enters pe_hook
    if (s_inExit) return true;
    if (!session.pawn || !PawnStillValid(session.pawn)) return false;
    void* vehicle = nullptr;
    for (const char* name : {"GetVehicleActor", "GetVehicle"}) {
        Call c((std::string("/Script/FortniteGame.FortPlayerPawnAthena.") + name).c_str());
        if (!c.ok()) c = Call((std::string("/Script/FortniteGame.FortPawn.") + name).c_str());
        if (c.ok() && c.run(session.pawn)) { vehicle = c.result<void*>(); if (vehicle && !IsBadPtr(vehicle)) break; vehicle = nullptr; }
    }
    if (!vehicle) { LOG("[Vehicle] exit: not currently in a vehicle"); return false; }
    void* vehicleCls = Read<void*>(vehicle, profile::ObjectClass);
    LOG("[Vehicle] exiting %s", ObjName(vehicleCls).c_str());
    {
        Call x("/Script/FortniteGame.FortCharacterVehicle.OnPawnExitVehicle");
        if (!x.ok()) x = Call("/Script/FortniteGame.FortAthenaVehicle.OnPawnExitVehicle");
        if (x.ok()) {
            bool bound = x.set("PlayerPawn", session.pawn) || x.set("Pawn", session.pawn);
            int seat = session.vehicle_weapon_seat >= 0 ? session.vehicle_weapon_seat : 0;
            x.set("SeatIdx", seat);
            x.set("SeatIndex", seat);
            s_inExit = true;
            bool ran = bound && x.run(vehicle);
            s_inExit = false;
            if (ran) LOG("[Vehicle] ✅ OnPawnExitVehicle dispatched");
        }
    }
    Athena::RemoveVehicleWeapon(true);   // event-driven removal parity
    return true;
}

// [G21] HUD interact dispatch — the touch button gives us no target, so we
// resolve it exactly like the client would: nearest interactable in range
// (an un-searched container 350uu beats a vehicle 900uu — chest prompts
// always win in-game).
void Athena::DispatchHudInteract() {
    if (!session.pawn || !PawnStillValid(session.pawn)) return;
    Vector pawnLoc{};
    if (!ActorLocation(session.pawn, pawnLoc)) return;
    void* container = nullptr;
    double bestC = 350.0 * 350.0;
    {
        void* cls = FindObj("/Script/FortniteGame.BuildingContainer");
        std::vector<void*> actors;
        if (cls && GetAllActorsOfClassRaw(cls, actors)) {
            for (void* a : actors) {
                if (!a || IsBadPtr(a)) continue;
                if (GetBool(a, "/Script/FortniteGame.BuildingContainer.bAlreadySearched")) continue;
                Vector l{};
                if (!ActorLocation(a, l)) continue;
                double dx = l.X - pawnLoc.X, dy = l.Y - pawnLoc.Y, dz = l.Z - pawnLoc.Z;
                double d2 = dx*dx + dy*dy + dz*dz;
                if (d2 < bestC) { bestC = d2; container = a; }
            }
        }
    }
    if (container) {
        LOG("[HUD] interact → container %p [G21]", container);
        Athena::Loot(container);
        return;
    }
    if (void* vehicle = Athena::NearestVehicle(900.0f)) {
        LOG("[HUD] interact → vehicle [G21]");
        Athena::EnterVehicleDirect(vehicle, 0);
        return;
    }
    LOG("[HUD] interact pressed — nothing in range [G21]");
}

void Athena::CheatScript(const char* script) {
    if (!script || !session.pawn) return;
    std::string s = script;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    LOG("CheatScript: %s", s.c_str());
}

// ============================================================================
// LOBBY / CHEAT COMMANDS — [F24]
// ============================================================================
static void* g_depart_world = nullptr;

void Athena::ReturnToLobby(void* world) {
    // Travel after WorldTick, never from inside a controller return-menu RPC
    // (Era core return_to_lobby ported).
    void* controller = session.controller;
    g_depart_world = world;
    session = Session{};
    auto text = ToWide("open Frontend");
    FStringNative command{(TCHAR*)text.data(), (int32)text.size() + 1, (int32)text.size() + 1};
    Call travel("/Script/Engine.KismetSystemLibrary.ExecuteConsoleCommand");
    bool dispatched = travel.ok() && travel.set("WorldContextObject", world) &&
                      travel.set("Command", command) && travel.set("SpecificPlayer", controller) &&
                      travel.run(FindObj("/Script/Engine.Default__KismetSystemLibrary"));
    LOG("Lobby travel dispatched=%d; old session cleared", (int)dispatched);
    if (!dispatched) { g_depart_world = nullptr; }
}

void Athena::TeleportToReticle() {
    if (!session.controller) return;
    Vector target{};
    if (!GetFieldValue<Vector>(session.controller, "/Script/FortniteGame.FortPlayerController.LocationUnderReticle", target)) {
        LOG("Teleport: no reticle location"); return;
    }
    Call teleport("/Script/FortniteGame.FortPlayerController.ServerTeleportToReticle");
    if (teleport.ok() && teleport.set("TeleportLocation", target) && teleport.run(session.controller))
        LOG("Teleport-to-reticle dispatched to %.0f %.0f %.0f", target.X, target.Y, target.Z);
}

// fly/walk/ghost/god — Era core exec handlers ported.
void Athena::ApplyCheats(const std::string& cmd) {
    if (!session.pawn || !session.controller) return;
    if (cmd == "god") {
        bool current = GetBool(session.pawn, "/Script/Engine.Actor.bCanBeDamaged");
        SetBool(session.pawn, "/Script/Engine.Actor.bCanBeDamaged", !current);
        LOG("God mode=%d", (int)current);
        return;
    }
    void* movement = GetObjField(session.pawn, "/Script/Engine.Character.CharacterMovement");
    Call mode("/Script/Engine.CharacterMovementComponent.SetMovementMode");
    uint8_t movement_mode = cmd == "walk" ? 1 : 5, custom = 0;
    if (movement && mode.ok() && mode.set("NewMovementMode", movement_mode) && mode.set("NewCustomMode", custom))
        mode.run(movement);
    Call collision("/Script/Engine.Actor.SetActorEnableCollision");
    bool enabled = cmd != "ghost";
    if (collision.ok() && collision.set("bNewActorEnableCollision", enabled)) collision.run(session.pawn);
    LOG("Local movement cheat %s", cmd.c_str());
}

// ============================================================================
// HOOKS
// ============================================================================
static void* g_interactionClass = nullptr;
static void* g_hudContextClass = nullptr;
static void* g_aircraftClass = nullptr;

// [F25] Prologue verification before hooking (Era core pattern) — refuses to
// patch when the bytes at an RVA do not match the profiled build (protects
// against wrong offsets or another mod's detour).
static bool PrologueMatch(const char* label, uintptr_t rva, const uint32_t* words, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        uint32_t actual = Read<uint32_t>((const void*)(g_base + rva), 4 * i);
        if (actual != words[i]) {
            LOG("PROLOGUE MISMATCH %s RVA=%llx word=%zu expected=%08x actual=%08x",
                label, (unsigned long long)rva, i, words[i], actual);
            return false;
        }
    }
    LOG("Prologue verified %s RVA=%llx", label, (unsigned long long)rva);
    return true;
}

// [F26] ProcessEvent prologue varies across builds in its first instruction:
//   libUE4.so  (CL-8936006, reference): STR X28,[SP,#-0x60]!  = 0xf81a0ffc
//   libUnreal.so (CL-21088273 / 21.30): STP X28,X27,[SP,#-0x60]! = 0xa9ba6ffc
// Words 1..3 are identical in both builds (STP X26,X25 / X24,X23 / X22,X21).
// Accept either save-X28 form, then verify the shared tail.
static bool PrologueMatchPE(const char* label, uintptr_t rva) {
    static const uint32_t word0_variants[] = {0xa9ba6ffc, 0xf81a0ffc};
    static const uint32_t tail[]           = {0xa90167fa, 0xa9025ff8, 0xa90357f6};
    uint32_t actual0 = Read<uint32_t>((const void*)(g_base + rva), 0);
    bool ok0 = false;
    for (size_t i = 0; i < sizeof(word0_variants) / sizeof(word0_variants[0]); ++i)
        if (actual0 == word0_variants[i]) { ok0 = true; break; }
    if (!ok0) {
        LOG("PROLOGUE MISMATCH %s RVA=%llx word=0 expected=a9ba6ffc|f81a0ffc actual=%08x",
            label, (unsigned long long)rva, actual0);
        return false;
    }
    for (size_t i = 0; i < sizeof(tail) / sizeof(tail[0]); ++i) {
        uint32_t actual = Read<uint32_t>((const void*)(g_base + rva), 4 * (i + 1));
        if (actual != tail[i]) {
            LOG("PROLOGUE MISMATCH %s RVA=%llx word=%zu expected=%08x actual=%08x",
                label, (unsigned long long)rva, i + 1, tail[i], actual);
            return false;
        }
    }
    LOG("Prologue verified %s RVA=%llx word0=%08x", label, (unsigned long long)rva, actual0);
    return true;
}

// [G19-b] One-shot RPC param dump — ServerAttemptEnterVehicle's cooked
// signature is build-specific; the first device call logs every parm
// (name/offset/size + raw qwords) so the exact layout is on record.
static void DumpRpcParams(void* func, void* params, const char* tag) {
    if (!func || IsBadPtr(func)) return;
    int total = Read<uint16_t>(func, profile::FunctionParamSize);
    LOG("[%s] param dump: size=%d", tag, total);
    auto walk = [&](void* p, uint32 nameOff, bool ffield) {
        for (int i = 0; p && !IsBadPtr(p) && i < 24; ++i, p = Read<void*>(p, ffield ? profile::FFieldNext : profile::FieldNext)) {
            if (!(Read<uint64_t>(p, profile::PropFlags) & profile::CPF_Parm)) continue;
            std::string n;
            if (!SafeFieldName(p, nameOff, n)) continue;
            int offset = Read<int>(p, profile::PropOffset);
            int size   = Read<int>(p, profile::PropElementSize);
            uint64_t v = 0;
            if (params && offset >= 0 && offset + 8 <= total) v = Read<uint64_t>(params, (size_t)offset);
            LOG("[%s]   parm '%s' off=%d size=%d qword=0x%llx", tag, n.c_str(), offset, size, (unsigned long long)v);
        }
    };
    walk(Read<void*>(func, profile::StructChildren), profile::ObjectName, false);
    walk(profile::HasFField ? Read<void*>(func, profile::StructChildProps) : nullptr, profile::FFieldName, true);
}

static void pe_hook(void* obj, void* func, void* params) {
    if (!orig_PE) return;
    if (!g_internal && func && g_gameTid.load() == (int)gettid()) {
        if (session.lobby_requested) { orig_PE(obj, func, params); return; }

        std::string fn = ObjName(func);
        bool isPC   = (obj == session.controller);
        bool isPawn = (obj == session.pawn);
        void* currentWeapon = nullptr;
        if (session.pawn) currentWeapon = Read<void*>(session.pawn, profile::FortPawn_CurrentWeapon);
        bool isWeapon = (obj == currentWeapon && obj);

        // [F05] JumpFromHudStart comes through UAthenaHUDContext (touch HUD).
        if (!g_hudContextClass) g_hudContextClass = FindObj("/Script/FortniteGame.AthenaHUDContext");
        if (g_hudContextClass && obj && fn == "JumpFromHudStart") {
            void* cls = Read<void*>(obj, profile::ObjectClass);
            if (SubclassOfFast(cls, g_hudContextClass)) { Athena::HandleJumpFromHudStart(); return orig_PE(obj, func, params); }
        }
        // Direct receiver match (original behaviour retained for safety).
        if (isPC && fn == "JumpFromHudStart") { Athena::HandleJumpFromHudStart(); return orig_PE(obj, func, params); }

        // [G21] MOBILE HUD VOCABULARY + INTERACT DISPATCH. JumpFromHudStart
        // proved the touch buttons live on AthenaHUDContext — the interact /
        // vehicle / edit button names are siblings we have not met yet (the
        // round-17 log shows the enter attempt reached NO hooked function).
        // (a) every unique HUD-context function name is logged once;
        // (b) interact-ish names dispatch a full interact: nearest container
        //     (350uu) first, else nearest vehicle (900uu) → EnterVehicleDirect.
        //     Additive — the OG body still runs below.
        if (g_hudContextClass && obj && fn != "JumpFromHudStart" && fn.size() < 64) {
            void* hudCls = Read<void*>(obj, profile::ObjectClass);
            if (hudCls && SubclassOfFast(hudCls, g_hudContextClass)) {
                static std::set<std::string> s_hudFns;
                if (s_hudFns.size() < 80 && s_hudFns.insert(fn).second)
                    LOG("[HUD] context function: '%s' [G21]", fn.c_str());
                bool interactish = fn.find("nteract") != std::string::npos ||
                                   fn.find("UseFromHud") != std::string::npos ||
                                   fn == "Use" || fn == "Interact" || fn == "UseHud";
                if (interactish && session.finalized && session.pawn && !session.death_handled) {
                    LOG("[HUD] interact button '%s' — dispatching interact [G21]", fn.c_str());
                    Athena::DispatchHudInteract();
                }
            }
        }

        // [G21] vehicle vocabulary + EXIT. Every 'ehicle' function name seen
        // anywhere is logged once (the enter RPC still has not shown up in a
        // device log — this reveals the real names the client dispatches).
        // 'ehicle'+'xit' names run the exit ladder additively (OG still runs
        // below — the client body may animate the dismount locally).
        if (fn.find("ehicle") != std::string::npos) {
            static std::set<std::string> s_vehFns;
            if (s_vehFns.size() < 60 && s_vehFns.insert(fn).second)
                LOG("[Vehicle] fn seen: '%s' on %s [G21]", fn.c_str(),
                    ObjName(Read<void*>(obj, profile::ObjectClass)).c_str());
            // The engine's own seat bookkeeping events are consumed by the
            // [G09] branch below — never re-drive the ladder from inside them
            // (OnPawnExitVehicle dispatched by ExitVehicleDirect itself would
            // re-enter this catch recursively otherwise).
            if (session.finalized && session.pawn &&
                fn != "OnPawnEnterVehicle" && fn != "OnPawnExitVehicle" &&
                fn.find("xit") != std::string::npos && fn.find("nter") == std::string::npos) {
                static bool s_exitDumped = false;
                if (!s_exitDumped) { s_exitDumped = true; DumpRpcParams(func, params, "Vehicle-EXIT"); }
                LOG("[Vehicle] exit request '%s' on %s [G21]", fn.c_str(),
                    ObjName(Read<void*>(obj, profile::ObjectClass)).c_str());
                Athena::ExitVehicleDirect();
            }
        }

        // [G09] Vehicle seat events are dispatched ON the vehicle actor —
        // outside the PC/pawn receiver gate below. Enter: instant seat-weapon
        // grant (no 0.5s poll wait). Exit: instant removal + re-equip.
        if ((fn == "OnPawnEnterVehicle" || fn == "OnPawnExitVehicle") && params && session.pawn) {
            void* who = Read<void*>(params, 0);   // AFortPlayerPawn* Pawn
            if (who == session.pawn) {
                if (fn == "OnPawnEnterVehicle") {
                    LOG("[Vehicle] enter-seat event seat=%d", Read<int32>(params, 8));
                    Athena::TickVehicleSeats(/*force=*/true);   // [G09] instant grant
                } else {
                    LOG("[Vehicle] exit-seat event");
                    Athena::RemoveVehicleWeapon(true);
                }
            }
            return orig_PE(obj, func, params);
        }

        // [F24] Lobby return interception (standalone: travel locally).
        if (isPC && (fn == "ReturnToMainMenu" || fn == "ServerReturnToMainMenu" ||
                     fn == "ClientReturnToMainMenu" || fn == "ClientReturnToMainMenuWithTextReason")) {
            session.lobby_requested = true;
            LOG("Lobby return queued: %s", fn.c_str());
            return orig_PE(obj, func, params);
        }

        // [F04] Aircraft jump RPC arrives on the aircraft component.
        if (!g_aircraftClass) g_aircraftClass = FindObj("/Script/FortniteGame.FortControllerComponent_Aircraft");
        if (g_aircraftClass && obj && fn == "ServerAttemptAircraftJump") {
            void* cls = Read<void*>(obj, profile::ObjectClass);
            if (SubclassOfFast(cls, g_aircraftClass)) { Athena::HandleJumpFromHudStart(); return orig_PE(obj, func, params); }
        }

        // [G19-b] VEHICLE ENTER RPC — caught by NAME on ANY receiver (PC,
        // pawn, vehicle component): the ability dispatches it, the net layer
        // would drop it, and before G19 the hook had NO handler at all — the
        // whole "enter button does nothing" bug in one missing branch.
        if (fn == "ServerAttemptEnterVehicle" && params && session.finalized) {
            static bool s_dumpedEnter = false;
            if (!s_dumpedEnter) { s_dumpedEnter = true; DumpRpcParams(func, params, "Vehicle-RPC"); }
            void* slot = incoming(func, params, "Vehicle", 8);
            if (!slot) slot = incoming(func, params, "VehicleActor", 8);
            if (!slot) slot = incoming(func, params, "TargetVehicle", 8);
            void* vehicle = slot ? Read<void*>(slot) : nullptr;
            int seatIdx = 0;
            if (void* s2 = incoming(func, params, "SeatIdx", 4)) seatIdx = Read<int32>(s2);
            else if (void* s2 = incoming(func, params, "SeatIndex", 4)) seatIdx = Read<int32>(s2);
            else if (void* s2 = incoming(func, params, "Seat", 4)) seatIdx = Read<int32>(s2);
            LOG("[Vehicle] enter RPC: receiver=%s vehicle=%p seat=%d",
                ObjName(Read<void*>(obj, profile::ObjectClass)).c_str(), vehicle, seatIdx);
            if (!vehicle || IsBadPtr(vehicle) || !IsReadable(vehicle, 0x60)) {
                // No sane vehicle in the params — nearest vehicle fallback.
                vehicle = Athena::NearestVehicle(900.0f);
                LOG("[Vehicle] param vehicle unusable — nearest fallback %p", vehicle);
            }
            if (vehicle) Athena::EnterVehicleDirect(vehicle, seatIdx);
            return;   // server RPC — the native client body is a net no-op; ladder replaces it
        }

        // [G19-b] HUD/controller vehicle-enter BUTTON whose function name we
        // do not know yet (mobile builds route buttons through the HUD context
        // like JumpFromHudStart): ADDITIVELY run the seat ladder (OG still
        // dispatches — this is a client function, not a dead RPC) and log the
        // exact name + params once so the next iteration pins the real path.
        if (session.finalized && params != nullptr && fn != "OnPawnEnterVehicle" && fn != "OnPawnExitVehicle" &&
            fn.find("ehicle") != std::string::npos && fn.find("xit") == std::string::npos &&
            (fn.find("nter") != std::string::npos || fn.rfind("Attempt", 0) == 0)) {
            static bool s_btnLogged = false;
            LOG("[Vehicle] enter-ish function '%s' on %s",
                fn.c_str(), ObjName(Read<void*>(obj, profile::ObjectClass)).c_str());
            if (!s_btnLogged) { s_btnLogged = true; DumpRpcParams(func, params, "Vehicle-BTN"); }
            void* cls = Read<void*>(obj, profile::ObjectClass);
            void* vehicle = IsVehicleTarget(cls) ? obj : Athena::NearestVehicle(900.0f);
            if (vehicle) Athena::EnterVehicleDirect(vehicle, 0);   // additive — OG runs below
        }

        if (!g_interactionClass) g_interactionClass = FindObj("/Script/FortniteGame.FortControllerComponent_Interaction");
        bool isInteraction = false;
        if (g_interactionClass && obj) {
            void* cls = Read<void*>(obj, profile::ObjectClass);
            isInteraction = SubclassOfFast(cls, g_interactionClass);
        }

        // [F37] Container/BuildingSMActor receivers: interaction + harvest
        // functions are dispatched ON the world actor, not on the controller
        // (ABuildingContainer::ServerOnAttemptInteract in Erbium v3,
        // ABuildingSMActor::OnDamageServer) — the old filter dropped them.
        bool isWorldTarget = false;
        static void* s_bcCls = nullptr, *s_smCls = nullptr;
        if (obj && !isPC && !isPawn && !isWeapon && !isInteraction &&
            (fn == "ServerOnAttemptInteract" || fn == "OnDamageServer")) {
            void* cls = Read<void*>(obj, profile::ObjectClass);
            if (!s_bcCls) s_bcCls = FindObj("/Script/FortniteGame.BuildingContainer");
            if (!s_smCls) s_smCls = FindObj("/Script/FortniteGame.BuildingSMActor");
            if ((s_bcCls && SubclassOfFast(cls, s_bcCls)) || (s_smCls && SubclassOfFast(cls, s_smCls)))
                isWorldTarget = true;
        }

        if (isPC || isPawn || isWeapon || isInteraction || isWorldTarget) {
            Guard g;
            if (fn == "ServerExecuteInventoryItem") {
                if (Read<uint16_t>(func, profile::FunctionParamSize) >= (int)sizeof(Guid))
                    Athena::OnServerExecuteInventoryItem(Read<Guid>(params));
            }
            else if (fn == "ServerExecuteInventoryWeapon") Athena::OnServerExecuteInventoryWeapon(Read<void*>(params));
            // [F35] ServerHandlePickup(PickUp, InFlyTime, InStartDirection,
            // bPlayPickupSound) / ServerHandlePickupInfo(Pickup, Info) /
            // ServerHandlePickupWithRequestedSwap — the fly-to-player flow is
            // FULLY replaced (Erbium SetPickupTarget + scheduled InternalPickup);
            // the native server-side half does not exist in this build, which
            // is why loot vanished instead of being picked up. The OG call is
            // therefore SKIPPED for pickup RPCs.
            else if ((fn == "ServerHandlePickupInfo" || fn == "ServerHandlePickup" ||
                      fn == "ServerHandlePickupWithRequestedSwap") && isPawn && params) {
                void* arg = incoming(func, params, "PickUp", 8);
                if (!arg) arg = incoming(func, params, "Pickup", 8);
                if (!arg) arg = params;   // PickUp is the first parameter
                float flyTime = 0.45f;
                { void* ft = incoming(func, params, "InFlyTime", 4);
                  if (ft) flyTime = Read<float>(ft);
                  else if ((ft = incoming(func, params, "FlyTime", 4))) flyTime = Read<float>(ft); }
                Athena::HandlePickupRPC(Read<void*>(arg), flyTime);
                return;   // native flow replaced — no OG
            }
            // [F35] Erbium OnCapsuleBeginOverlap parity: the pawn's capsule is
            // what triggers pickup on a live server; in standalone the native
            // client pawn never requests the pickup, so detect it here.
            else if (fn == "OnCapsuleBeginOverlap" && isPawn && params) {
                void* otherSlot = incoming(func, params, "OtherActor", 8);
                if (!otherSlot) otherSlot = incoming(func, params, "OtherActor", 0);
                if (otherSlot) {
                    void* other = Read<void*>(otherSlot);
                    if (other && !IsBadPtr(other) && IsReadable(other, 0x40)) {
                        static void* pickCls = nullptr;
                        if (!pickCls) pickCls = FindObj("/Script/FortniteGame.FortPickupAthena");
                        if (!pickCls) pickCls = FindObj("/Script/FortniteGame.FortPickup");
                        if (pickCls && IsA(other, pickCls))
                            Athena::HandlePickupRPC(other, 0.45f);
                    }
                }
            }
            // [F37] Interaction — receivers: interaction component, PC, and
            // (v3 Erbium parity) the container itself via ServerOnAttemptInteract.
            else if ((fn == "ServerAttemptInteract" || fn == "ServerOnAttemptInteract") && params && (isInteraction || isPC || isWorldTarget)) {
                // [F84] Named-param target resolution (Erbium local_interact
                // parity): ServerAttemptInteract carries ReceivingActor as a
                // NAMED 8-byte property; a raw offset-0 read returns garbage
                // whenever the cooked param layout shifts — the silent
                // dead-interact symptom. Named lookup first, raw fallback
                // second, container-self third.
                void* slot = incoming(func, params, "ReceivingActor", 8);
                void* target = slot ? Read<void*>(slot) : nullptr;
                if (!target || IsBadPtr(target)) target = Read<void*>(params, 0);
                // ServerOnAttemptInteract dispatched on the container: obj IS the target.
                if ((!target || IsBadPtr(target)) && isWorldTarget) target = obj;
                if (target && !IsBadPtr(target) && IsReadable(target, 0x60)) {
                    // [F84] Erbium gates (local_interact): the target must be
                    // a real Actor within 600 uu of the pawn — wild pointers
                    // from a stale param slot are rejected instead of being
                    // dispatched into (crash/poison source). Un-locatable
                    // targets still dispatch to preserve the old paths.
                    static void* s_engineActorCls = nullptr;
                    if (!s_engineActorCls) s_engineActorCls = FindObj("/Script/Engine.Actor");
                    Vector tloc{};
                    if (s_engineActorCls && !IsA(target, s_engineActorCls)) {
                        LOG("[Interact] F84 non-actor target rejected");
                    } else if (ActorLocation(target, tloc) && !NearPlayer(tloc, 600.0f)) {
                        LOG("[Interact] F84 far target rejected (%.0f,%.0f,%.0f)", tloc.X, tloc.Y, tloc.Z);
                    } else {
                    void* bcCls   = FindObj("/Script/FortniteGame.BuildingContainer");
                    void* targetCls = Read<void*>(target, profile::ObjectClass);
                    LOG("[Interact] target=%p class=%s", target, ObjName(targetCls).c_str());
                    if (bcCls && SubclassOfFast(targetCls, bcCls)) Athena::Loot(target);
                    else if (IsVehicleTarget(targetCls)) {
                        // [G15-c/G18-d] VEHICLE ENTER — Erbium parity: the NATIVE
                        // ServerAttemptInteract seats the pawn (Erbium's own
                        // vehicle branch in ServerAttemptInteract_ is
                        // commented out — they rely on the OG call). Run the
                        // native FIRST, verify the seat took through
                        // FortPlayerPawnAthena.GetVehicleActor, then fall back
                        // through the full ladder:
                        //   1. ForceSeatSinglePawn (SDK-verified BlueprintCallable)
                        //   2. ForceSeatPawns({pawn}) (SDK-verified, TArray form)
                        //   3. OnPawnEnterVehicle(pawn, 0) dispatch on the vehicle
                        //      (the server notification the native path would send)
                        orig_PE(obj, func, params);
                        auto verifySeated = []() -> bool {
                            Call v("/Script/FortniteGame.FortPlayerPawnAthena.GetVehicleActor");
                            if (!v.ok()) v = Call("/Script/FortniteGame.FortPawn.GetVehicleActor");
                            if (v.ok() && session.pawn && v.run(session.pawn)) {
                                void* cur = v.result<void*>();
                                return cur && !IsBadPtr(cur);
                            }
                            return false;
                        };
                        bool seated = verifySeated();
                        if (!seated && session.pawn) {
                            Call f("/Script/FortniteGame.FortAthenaVehicle.ForceSeatSinglePawn");
                            if (!f.ok()) f = Call("/Script/FortniteGame.FortCharacterVehicle.ForceSeatSinglePawn");
                            int seatIdx = 0;
                            if (f.ok() && f.set("PawnToSeat", session.pawn) && f.set("SeatIndex", seatIdx) && f.run(target)) {
                                if (f.result<bool>()) {
                                    LOG("[Vehicle] ✅ force-seated into %s (native interact declined)",
                                        ObjName(targetCls).c_str());
                                    Athena::TickVehicleSeats(true);   // seat weapon path, instantly
                                    seated = true;
                                }
                            }
                        }
                        if (!seated && session.pawn) {
                            // [G18-d] ForceSeatPawns(TArray<AFortPlayerPawn*>) —
                            // SDK-verified sibling; bind the array by reflection
                            // (16-byte TArray = {Data, Count, Max}).
                            Call fp("/Script/FortniteGame.FortAthenaVehicle.ForceSeatPawns");
                            if (!fp.ok()) fp = Call("/Script/FortniteGame.FortCharacterVehicle.ForceSeatPawns");
                            if (fp.ok()) {
                                static void* cell = nullptr;      // persistent backing store
                                cell = session.pawn;
                                struct TArr { void* data; int32_t n; int32_t max; } ta{ &cell, 1, 1 };
                                if (fp.set_bytes("PawnsToSeat", &ta, sizeof(ta)) || fp.bind_at(0, &ta, sizeof(ta))) {
                                    fp.run(target);
                                    seated = verifySeated();
                                    if (seated) {
                                        LOG("[Vehicle] ✅ ForceSeatPawns seated into %s", ObjName(targetCls).c_str());
                                        Athena::TickVehicleSeats(true);
                                    }
                                }
                            }
                        }
                        if (!seated && session.pawn) {
                            // [G18-d] last resort: dispatch the enter notification
                            // on the vehicle ourselves — OnPawnEnterVehicle(pawn,
                            // seat) is the server→client seat bookkeeping event
                            // (params: AFortPlayerPawn* @0, int32 SeatIdx @8).
                            Call enter("/Script/FortniteGame.FortCharacterVehicle.OnPawnEnterVehicle");
                            if (enter.ok() && enter.set("PlayerPawn", session.pawn)) {
                                int seatIdx = 0;
                                enter.set("SeatIdx", seatIdx);
                                if (enter.run(target)) {
                                    LOG("[Vehicle] OnPawnEnterVehicle dispatched on %s (verify next tick)",
                                        ObjName(targetCls).c_str());
                                    Athena::TickVehicleSeats(true);
                                }
                            } else {
                                LOG("[Vehicle] enter fallback unavailable for %s", ObjName(targetCls).c_str());
                            }
                        }
                        return;   // native already dispatched above — skip the trailing orig_PE
                    }
                    else {
                        // [G16-a] EVERYTHING ELSE — BuildingActor family AND plain
                        // BP actors (GasStation_Door_C_C implements IOpenableInterface
                        // without deriving BuildingActor). Openable toggle first,
                        // BP interact fallback second.
                        Athena::HandleOpenable(target);
                    }
                    }
                }
            }
            // [F39] Harvest resources on pickaxe damage (Erbium OnDamageServer).
            else if (fn == "OnDamageServer" && isWorldTarget) {
                Athena::HandleDamageServer(obj, func, params);
            }
            else if (fn == "ServerAttemptAircraftJump" && !session.aircraft_jumped) Athena::HandleJumpFromHudStart();
            // [F38] Building RPCs are FULLY replaced (Erbium ServerCreate-
            // BuildingActor path). The native implementation is skipped: in
            // standalone its BuildingClassMap is empty and the exit-build-mode
            // payload crashes it — the crash the user saw when leaving build mode.
            else if (fn == "ServerCreateBuildingActor" && params && isPC) {
                Athena::OnServerCreateBuildingActor(params);
                return;   // native path replaced — no OG
            }
            else if (fn.rfind("ServerCreateBuilding", 0) == 0 && params && isPC) {
                Athena::LocalBuild();
                return;   // native path replaced — no OG
            }
            else if (fn == "ServerBeginEditingBuildingActor" && params) {
                void* piece = Read<void*>(params, 0);
                if (piece && !IsBadPtr(piece) && IsReadable(piece, 0x40)) Athena::OnBeginEditActor(piece);
            }
            else if (fn == "ServerEditBuildingActor" && params) {
                // [F08] Named extraction: BuildingActorToEdit(ptr),
                // NewBuildingClass(ptr), RotationIterations(u8), bMirrored(bool).
                void* ba = incoming(func, params, "BuildingActorToEdit", 8);
                void* nc = incoming(func, params, "NewBuildingClass", 8);
                void* ri = incoming(func, params, "RotationIterations", 1);
                void* mi = incoming(func, params, "bMirrored", 1);
                if (ba && nc && ri && mi)
                    Athena::OnFinishEditActor(Read<void*>(ba), Read<void*>(nc), Read<uint8_t>(ri), Read<uint8_t>(mi) != 0);
                else {
                    struct P { void* BA; void* NC; uint8_t RI; uint8_t M; } *p = (P*)params;
                    Athena::OnFinishEditActor(p->BA, p->NC, p->RI, p->M != 0);
                }
            }
            else if (fn == "ServerPlayEmoteItem" && isPC) Athena::PlayEmoteItem(Read<void*>(params));
            else if (fn == "ServerAttemptExitVehicle") Athena::OnExitVehicle();
            else if (fn == "ServerOnExitVehicle" && isPawn) Athena::RemoveVehicleWeapon(true);   // [F28] seat-weapon cleanup
            // [F24] ServerAttemptInventoryDrop(ItemGuid, Count, bTrash).
            else if (fn == "ServerAttemptInventoryDrop" && params) {
                Guid gg = Read<Guid>(params);
                int cnt = Read<int>(params, sizeof(Guid));
                Athena::DropInventoryItem(gg, cnt);
            }
        }
    }
    orig_PE(obj, func, params);
}

static const char* PhaseName(int phase) {
    switch (phase) {
        case 0: return "None"; case 1: return "Setup"; case 2: return "Warmup";
        case 3: return "Aircraft"; case 4: return "SafeZones"; case 5: return "EndGame";
        case 6: return "Count"; default: return "Unknown";
    }
}

static void tick_hook(void* world, int type, float delta) {
    if (orig_Tick) orig_Tick(world, type, delta);
    if (g_internal || !world) return;
    int expected = 0;
    if (g_gameTid.compare_exchange_strong(expected, (int)gettid())) LOG("WorldTick first: tid=%d", (int)gettid());
    if (g_gameTid.load() != (int)gettid()) return;
    // [F27] The old [F14] guard read libUnreal.so CODE bytes at
    // profile::IsSavingPackage (0x0760A59C is an EXEC RVA in 21.30 — it was
    // defined-but-never-used in the original hook.cpp). First byte there is
    // 0xFD, so the check was always true and tick_hook returned before ANY
    // world detection ran (no controller binding, no spawn). game.cpp's
    // reference used a real .bss bool there, but this build has no known data
    // address for it — both probes are dropped (the original hook.cpp never
    // used them either). Re-entrancy is still guarded below.
    Guard g;

    // [F27] One-shot diagnostics: proves world pointer + name resolution work
    // on-device (appears once, right after the first game-thread tick).
    static bool diag_logged = false;
    if (!diag_logged) {
        diag_logged = true;
        void* wcls = Read<void*>(world, profile::ObjectClass);
        LOG("DIAG world=%p class=%p ('%s') name='%s'",
            world, wcls, ObjName(wcls).c_str(), ObjName(world).c_str());
    }

    // [F24] Lobby travel was dispatched inside ReturnToLobby; while it is in
    // flight, ignore the departing world so the session does not resurrect.
    if (g_depart_world) {
        if (world == g_depart_world) return;
        g_depart_world = nullptr;
    }
    if (session.lobby_requested && session.world == world) { Athena::ReturnToLobby(world); return; }

    std::string wn = ObjName(world);
    static std::string lastWorld;
    if (wn != lastWorld) { LOG(">>> WorldTick world: '%s'", wn.c_str()); lastWorld = wn; }
    if (wn.find("Frontend") != std::string::npos) {
        if (session.attempted && !session.finalized && session.finalize_at > 0) {
            double now = Monotonic();
            if (now >= session.finalize_at) Athena::FinalizePlayer();
        }
        // [F91-b/BEX-1] transition-window scrub: we are in Frontend while the
        // session still points at the departed terrain world — every actor
        // pointer (pawn/controller/inventory/quickbars/state) is stale and
        // pe_hook class-based handlers (JumpFromHudStart family) would
        // dereference freed actors. Scrub the pointers (flags keep their
        // semantics for the pending-finalize check above); the terrain
        // detector issues a full fresh session on the next match, which also
        // re-arms session.qb_retries [F91-a].
        if (session.world && session.world != world) {
            session.pawn = nullptr; session.controller = nullptr;
            session.inventory = nullptr; session.quickbars = nullptr;
            session.player_state = nullptr; session.tactical_sprint = nullptr;
            session.world = nullptr; session.world_index = -1;
            LOG(">>> [F91] stale terrain session scrubbed (entered %s)", wn.c_str());
        }
        return;
    }
    if (wn.find("Artemis_Terrain") == std::string::npos && wn.find("Athena_Terrain") == std::string::npos) return;
    int index = Read<int>(world, 0xC);
    if (world != session.world || index != session.world_index) {
        session = Session{};
        session.world = world; session.world_index = index;
        session.observed_at = Monotonic();
        LOG(">>> Terrain detected: %s", wn.c_str());
        // [G12] Late native re-scan (Erbium lazy finders): anything the
        // startup scan missed — paged-out .rodata, relaxed-form refs — gets
        // one more pass now that the game world is resident, and a
        // newly-found SendRequestNow is hooked immediately. This is the MCP
        // profile/pickaxe enabler.
        static bool g_lateScanDone = false;
        if (!g_lateScanDone) {
            g_lateScanDone = true;
            gas::RediscoverMissing();
            TryInstallMcpHook();
        }
    }
    double now = Monotonic();
    // [F24] GamePhase tracking for diagnostics (reflection cached).
    if (session.state) {
        static void* phaseProp = nullptr;
        static bool phaseResolved = false;
        if (!phaseResolved) { phaseProp = GetProp("/Script/FortniteGame.FortGameStateAthena.GamePhase"); phaseResolved = true; }
        if (phaseProp && Read<int>(phaseProp, profile::PropElementSize) == 1) {
            uint8_t phase = Read<uint8_t>(GetField(session.state, phaseProp));
            if ((int)phase != session.last_phase) {
                LOG("GamePhase observed=%u (%s) elapsed=%.1f", phase, PhaseName(phase), now - session.observed_at);
                session.last_phase = (int)phase;
            }
        }
    }
    if (now - session.observed_at < 5.0) return;
    Athena::Tick();
}

static bool exec_hook(void* engine, void* world, const char16_t* command, void* output) {
    if (!orig_Exec) return false;
    std::string cmd = Narrow(command, 256);
    std::string normalized = cmd;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::tolower);
    while (!normalized.empty() && (normalized.back() == ' ' || normalized.back() == '\t')) normalized.pop_back();
    if (!cmd.empty()) LOG("Exec: %s", cmd.c_str());
    bool onGameThread = (g_gameTid.load() == (int)gettid());

    if (normalized == "open athena_terrain" || normalized.rfind("open athena_terrain", 0) == 0) {
        LOG("Intercepting Athena travel");
        session = Session{};
        session.attempted = false;
        return orig_Exec(engine, world, u"open Athena_Terrain", output);
    }
    if (onGameThread && session.world && (normalized == "era.lobby" || normalized == "athena.lobby")) {
        session.lobby_requested = true;
        return true;
    }
    if (onGameThread && session.world && normalized == "teleport") {
        if (session.finalized) Athena::TeleportToReticle();
        return true;
    }
    // [F24] Movement cheats (Era core parity).
    if (onGameThread && session.world && session.finalized &&
        (normalized == "fly" || normalized == "walk" || normalized == "ghost" || normalized == "god")) {
        Guard g;
        Athena::ApplyCheats(normalized);
        return true;
    }
    // [F24] Diagnostics/cheat console (era.* mirror of the Era core athena.*).
    if (onGameThread && normalized.rfind("era.", 0) == 0) {
        Guard g;
        if (normalized == "era.status") {
            LOG("STATUS world=%p controller=%p pawn=%p inv=%p qb=%p ready=%d phase=%d",
                session.world, session.controller, session.pawn, session.inventory,
                session.quickbars, (int)session.finalized, session.last_phase);
        } else if (normalized == "era.inventory" && session.inventory) {
            for (size_t i = 0; i < session.slots.size(); ++i)
                LOG("SLOT %zu %s", i, ObjName(ItemDefinitionOf(session.slots[i])).c_str());
            for (size_t i = 0; i < session.secondary_slots.size(); ++i)
                LOG("BUILD %zu %s", i, ObjName(ItemDefinitionOf(session.secondary_slots[i])).c_str());
        } else if (normalized.rfind("era.equip ", 0) == 0) {
            int slot = -1;
            if (sscanf(normalized.c_str(), "era.equip %d", &slot) == 1 && slot >= 0 && slot < 6)
                Athena::EquipSlot(slot);
        } else if (normalized.rfind("era.give ", 0) == 0 && session.inventory_ready) {
            char asset[512]{}; int count = 1, slot = -1;
            if (sscanf(cmd.c_str(), "%*s %511s %d %d", asset, &count, &slot) >= 1) {
                void* def = asset[0] == '/' ? LoadAsset(asset) : ByShortName(asset);
                if (!def) LOG("Give: asset not found: %s", asset);
                else LOG("Give result=%d", (int)Athena::GiveItem(def, count, slot, slot >= 0 && slot < 6 ? 0 : (uint8_t)-1, false));
            }
        } else if (normalized == "era.vehicles" && session.world) {
            // [F28] manual re-run of the vehicle spawner pass
            session.vehicles_spawned = false;
            session.vehicle_attempts = 0;
            Athena::SpawnVehicles();
        } else if (normalized == "era.debug") {
            Athena::DebugDump();   // [F42] quickbars/inventory/match-state dump
        } else if (normalized == "era.abilities" || normalized == "era.specs") {
            // [G10/G04] dump every granted spec with its InputID
            gas::DumpSpecs("era.abilities");
        } else if (normalized == "era.fix" && session.world && session.finalized) {
            // [F42/F43] re-run every world-side fix pass (state, QBs, loot, cars, HUD)
            Athena::EnsurePlayerState();
            if (!session.quickbars_initialized) {
                Athena::InitializeQuickBarsNative();
                if (!session.quickbars_initialized) Athena::BuildQuickBarsManually();
                if (session.quickbars_initialized) Athena::SetQuickBarReady();
            }
            Athena::ResetStaleContainers();
            Athena::SpawnFloorLoot();
            session.vehicles_spawned = false;
            session.vehicle_attempts = 0;
            Athena::SpawnVehicles();
            Athena::SetupMatchState();
            Athena::InventoryUpdate();
            Athena::ForceUpdateQuickBars();
            LOG("era.fix: all passes re-run");
        } else if (normalized == "era.loot" && session.world && session.pawn) {
            // [F32] roll every unsearched container within 30 m (diagnostic aid)
            void* cls = FindObj("/Script/FortniteGame.BuildingContainer");
            std::vector<void*> actors;
            if (cls && GetAllActorsOfClassRaw(cls, actors)) {
                int n = 0;
                for (void* c : actors) {
                    Vector loc{};
                    if (!c || IsBadPtr(c) || !ActorLocation(c, loc) || !NearPlayer(loc, 3000.0f)) continue;
                    // [G20] fault-guarded like every other roll entry point
                    bool rolled = false;
                    if (!gas::RunGuarded("era.loot", [&] { rolled = Athena::SearchContainer(c, loc, false, false); }))
                        LOG("[Loot] ⚠️ era.loot roll faulted at stage '%s' [G20]", g_lootStage);
                    if (rolled) ++n;
                }
                LOG("era.loot: %d containers rolled", n);
            }
        } else if (normalized == "era.die" && session.world && session.finalized) {
            // [G17-c] force the full death flow (placement, drops, death UI)
            Athena::HandlePlayerDeath(true);
        } else if (normalized == "era.win" && session.world && session.finalized) {
            // [G17-e] force the Victory Royale banner
            Athena::HandleVictory();
        } else if (normalized == "era.alive" && session.world) {
            // [G18-a] real alive-players diagnostic
            int total = 0;
            int alive = Athena::CountRealAlivePlayers(&total);
            int shown = -1;
            void* plProp = GetProp("/Script/FortniteGame.FortGameStateAthena.PlayersLeft");
            if (plProp && session.state) shown = Read<int>(GetField(session.state, plProp));
            LOG("[Alive] real=%d total_ever=%d PlayersLeft=%d override=%d source=%s death=%d win=%d",
                alive, total, shown, session.players_override,
                session.alive_source == 1 ? "PlayerArray" : "local-fallback",
                (int)session.death_handled, (int)session.win_handled);
        } else if (normalized == "era.qb" && session.controller) {
            // [G18-b] force a quickbar rebind + verify pass
            session.qb_slots_verified = false;
            session.qb_watch_tries = 0;
            session.last_qb_watch = 0;   // bypass the 10s rate limiter
            Athena::TickQuickBarWatch();
        } else if (normalized.rfind("era.players ", 0) == 0 && session.world) {
            // [G18-a] FAKE testing floor override — the REAL system counts
            // actual player states; this forces the counter to pretend a
            // bigger lobby (0 disables the override, back to real counting).
            int n = 0;
            if (sscanf(normalized.c_str(), "era.players %d", &n) == 1 && n >= 0 && n <= 200) {
                session.players_override = n;
                if (session.state && session.finalized && !session.death_handled && !session.win_handled) {
                    void* plProp = GetProp("/Script/FortniteGame.FortGameStateAthena.PlayersLeft");
                    if (plProp && Read<int>(plProp, profile::PropElementSize) == 4) {
                        int total = 0;
                        int alive = Athena::CountRealAlivePlayers(&total);
                        if (n > 0 && alive < n) alive = n;
                        if (alive < 1) alive = 1;
                        Write<int>(GetField(session.state, plProp), 0, alive);
                        Call r("/Script/FortniteGame.FortGameStateAthena.OnRep_PlayersLeft");
                        if (r.ok()) r.run(session.state);
                        session.real_alive = alive;
                    }
                }
                LOG("era.players: override = %d (0 = REAL counting only)", n);
            }
        } else if (normalized.rfind("era.spawn ", 0) == 0 && session.world && session.pawn) {
            char path[512]{};
            if (sscanf(cmd.c_str(), "%*s %511[^\r\n]", path) == 1) {
                void* asDef = LoadAsset(path, "/Script/FortniteGame.FortItemDefinition");
                if (asDef) {
                    Vector loc{}; ActorLocation(session.pawn, loc); loc.Z += 100.0;
                    Athena::SpawnPickup(asDef, 1, loc, 1, 0, true);
                    LOG("Spawned pickup %s", path);
                } else {
                    void* cls = LoadClass(path, "/Script/Engine.Actor");
                    if (cls) {
                        Vector loc{}; ActorLocation(session.pawn, loc); loc.Z += 200.0;
                        Transform t{}; t.Location[0] = loc.X; t.Location[1] = loc.Y; t.Location[2] = loc.Z;
                        void* a = SpawnActor(session.world, cls, session.controller, t);
                        LOG("Spawned %s -> %p", path, a);
                    } else LOG("era.spawn: nothing found at %s", path);
                }
            }
        } else LOG("Commands: era.status | era.inventory | era.debug | era.abilities | era.alive | era.qb | era.fix | era.equip 0..5 | era.give Name [count] [slot] | era.players N | era.vehicles | era.loot | era.die | era.win | era.spawn /Game/Path | fly | walk | ghost | god | teleport | era.lobby");
        return true;
    }
    return orig_Exec(engine, world, command, output);
}

// [G05/G12] MCP dispatch hook installer — shared by InstallHooks (startup)
// and the late terrain re-scan path so a lazily-discovered SendRequestNow
// still gets hooked without a restart. Erbium installs its finders lazily
// for exactly the same reason.
static bool TryInstallMcpHook() {
    if (orig_SendRequestNow) return true;   // already installed
    if (!gas::SendRequestNow || !IsReadable((void*)gas::SendRequestNow, 16)) {
        LOG("[MCP] SendRequestNow not found — profile dispatch unpatched");
        return false;
    }
    uint32_t w0 = Read<uint32_t>((void*)gas::SendRequestNow);
    // [G13] Accept every STP prologue shape this build actually emits
    // (x29/x30 pairs AND callee-saved pairs like ProcessEvent's
    // STP X28,X27,[SP,#-N]!) plus SUB SP openers. PAC hints excluded —
    // Dobby relocation of hint instructions is version-dependent.
    if (!(arm::IsStpX29X30Sp(w0) || arm::IsSubSpImm(w0) || arm::IsCalleeSavedStpSp(w0))) {
        LOG("[MCP] SendRequestNow prologue unexpected (0x%08X) — skipped", w0);
        return false;
    }
    if (DobbyHook((void*)gas::SendRequestNow, (void*)hook_SendRequestNow, (void**)&orig_SendRequestNow) == 0 && orig_SendRequestNow) {
        LOG("[MCP] ✅ SendRequestNow hooked (CXC_Public forced)");
        return true;
    }
    LOG("[MCP] SendRequestNow hook FAILED");
    return false;
}


static void InstallHooks() {
    LOG("=== InstallHooks ===");
    // [F25/F26/G16-f] Verify every hooked RVA belongs to this build before
    // patching — prologue words come from the ACTIVE profile (the 10.40
    // profile carries the user's own verified words). ProcessEvent is
    // dual-build (PrologueMatchPE accepts both families' word0 + tail).
    bool pe_valid   = PrologueMatchPE("ProcessEvent", profile::ProcessEvent);
    const BuildProfile& bp = *s_activeProfile;
    bool tick_valid = PrologueMatch("WorldTick",  profile::WorldTick,  bp.tickWords, 4);
    bool exec_valid = PrologueMatch("EngineExec", profile::EngineExec, bp.execWords, 4);
    bool qb_valid   = profile::InitializeAthenaQuickBars != 0
                      && PrologueMatch("AthenaQuickBars", profile::InitializeAthenaQuickBars, bp.qbWords, 4);

    int installed = 0, failed = 0;
    if (pe_valid) {
        if (DobbyHook((void*)(g_base + profile::ProcessEvent), (void*)pe_hook, (void**)&orig_PE) == 0 && orig_PE) { LOG("ProcessEvent hooked"); ++installed; }
        else { LOG("ProcessEvent FAILED"); ++failed; }
    } else { LOG("ProcessEvent skipped (prologue)"); ++failed; }
    if (tick_valid) {
        if (DobbyHook((void*)(g_base + profile::WorldTick), (void*)tick_hook, (void**)&orig_Tick) == 0 && orig_Tick) { LOG("WorldTick hooked"); ++installed; }
        else { LOG("WorldTick FAILED"); ++failed; }
    } else { LOG("WorldTick skipped (prologue)"); ++failed; }
    if (exec_valid) {
        if (DobbyHook((void*)(g_base + profile::EngineExec), (void*)exec_hook, (void**)&orig_Exec) == 0 && orig_Exec) { LOG("EngineExec hooked"); ++installed; }
        else { LOG("EngineExec FAILED"); ++failed; }
    } else { LOG("EngineExec skipped (prologue)"); ++failed; }
    if (!qb_valid) LOG("[QB] initializer prologue mismatch — native init disabled, manual path only");

    // [G05/G12] MCP dispatch hook — through the shared installer (also used
    // by the late terrain re-scan path).
    if (TryInstallMcpHook()) ++installed;

    LOG("InstallHooks done: installed=%d failed=%d", installed, failed);
}

static int InspectCb(dl_phdr_info* info, size_t, void*) {
    const char* name = strrchr(info->dlpi_name, '/');
    name = name ? name + 1 : info->dlpi_name;
    if (strcmp(name, "libUnreal.so")) return 0;
    g_base = info->dlpi_addr;
    g_status.store(1);
    LOG("libUnreal.so base=%p", (void*)g_base);
    return 1;
}

static void* Worker(void*) {
    LOG("=== EraCH3 starting — multi-version build (auto-detected profiles) ===");
    LOG("sizeof(Vector)=%zu sizeof(Rotator)=%zu sizeof(Transform)=%zu", sizeof(Vector), sizeof(Rotator), sizeof(Transform));
    for (int i = 0; i < 600 && g_status.load() == 0; ++i) {
        dl_iterate_phdr(InspectCb, nullptr);
        if (g_status.load() == 0) usleep(100000);
    }
    if (g_status.load() != 1) { LOG("Timeout"); return nullptr; }
    usleep(300000);
    // [G16-f] PROFILE SELECTION FIRST — the build ID decides every RVA and
    // structural offset before anything else runs.
    DetectAndApplyProfile();
    // [G02] Native discovery NEXT (Erbium Finders parity): the scanner walks
    // the loaded image once on this worker thread — the found addresses feed
    // InstallHooks ([G05] MCP) and the ability/animation/vehicle systems.
    // Version-independent: the log strings are stable across builds.
    gas::Discover();
    InstallHooks();
    LOG("=== Ready ===");
    return nullptr;
}

extern "C" __attribute__((constructor))
void LibMain() {
    InitLog();
    pthread_t t;
    if (pthread_create(&t, nullptr, Worker, nullptr) == 0) pthread_detach(t);
}

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM*, void*) { return JNI_VERSION_1_6; }
