# CLAUDE.md

Claude Code project adapter for Test. This file is a concise supplement; AGENTS.md is the durable collaboration and safety contract. When they conflict, follow AGENTS.md.

## Authority And Reading Order

- Respond in Chinese by default. Give direct conclusions and name concrete uncertainty rather than guessing.
- Read the real repository state before making behavioral or architectural claims. Preserve user work and keep changes within the approved scope.
- Before structural work, read ARCHITECTURE.md. Before continuing or starting a staged task, read the header and active content of plan.md.
- Evidence precedence is:
  source/assets/config > ARCHITECTURE.md > plan.md > ROADMAP.md > README.md > AGENTS.md > CLAUDE.md > GEMINI.md.
- ROADMAP.md records accepted future direction, not implementation permission. Promote only the selected work into a concrete plan.md stage.
- Do not retain or invent narrow implementation facts in this file. Verify current C++ and assets before relying on gameplay details, montage contracts, or asset paths.

## Project And Build

- UE 5.7, Windows only, Visual Studio 2022 required.
- Runtime module: Test. Editor plugin: SmartBPCreator; UnrealBridge also exists in the checkout but is disabled in Test.uproject. Editor target: TestEditor. Game target: SoulslikeCombat.
- The user owns TestEditor compilation, packaging, and PIE validation. Do not run UBT, Build.bat, packaging, or other build commands unless explicitly requested.
- Generate project files by right-clicking Test.uproject and selecting Generate Visual Studio project files. Build TestEditor (Development Editor) manually in Visual Studio 2022.
- Public module dependencies include Core, CoreUObject, Engine, InputCore, EnhancedInput, AnimGraphRuntime, Niagara, GeometryCollectionEngine, PCG, UMG, AIModule, SlateCore, and MotionWarping. Slate and NavigationSystem are private.

## Scope, Content, And Documentation

- C++ belongs under Source/Test with the existing Public/Private layout. Game-owned editable assets belong under Content/_GAME.
- Marketplace, sample, Paragon, Mixamo, and other reference content are read-only unless the user explicitly authorizes edits.
- Never manually patch binary .uasset or .umap files. Use the approved live Unreal Editor route for Blueprint, UMG, level, and other asset changes.
- Formal review and closeout ignore tracked Content/*.uasset changes by default unless the user explicitly asks to inspect or include them.
- README.md is the public overview. ARCHITECTURE.md records stable ownership and data/state contracts. ROADMAP.md owns future direction, ordering, recommendations, and validation debt. plan.md is short-lived stage coordination.
- Update only the documentation layer affected by the work. Update ARCHITECTURE.md during stage closeout only after implementation, relevant validation, and review are stable.
- plan.md rules above its separator are immutable. Keep the active plan concrete, do not turn it into history, and clear completed detail according to its header.

## Unreal Editor And MCP

- Asset mutation requires the Test project Editor to be running, the required 3000 and/or 8088 endpoint to be listening, and a successful initialize -> tools/list -> one read-only tools/call flow.
- UnrealClaude is the primary route for normal Editor operations. Confirm exposed tool names and parameters with tools/list before use.
- VibeUE is auxiliary for its distinct documented capabilities, such as Python discovery/execution, animation graph or montage services, terrain tools, and log reading. Do not repeat the same mutation through both routes.
- Keep every asset operation sequential: read, mutate, then read back and verify. Use one documented writer per asset operation.
- If the required Editor surface is unavailable, stop before changing binary assets and report the missing prerequisite. Continue only with source/config inspection or C++ work.
- For bulk or hard-to-reverse asset work, confirm a recovery point first. After visible asset changes, prefer a screenshot; otherwise state the limitation and use property/graph readback plus user PIE validation.
- After the user manually changes a Widget Blueprint, perform one live read-only verification of the relevant widget hierarchy, graph, binding, property, or compile state.

## Code Intelligence And Rider

- When .codegraph exists, use CodeGraph first for C++ symbols, callers/callees, call paths, and blast-radius checks. If it is unavailable or stale, say so and use rg plus direct source reads.
- Rider MCP supplements CodeGraph for live IDE state, Problems View, semantic lookup, refactor previews, debug sessions, and runtime inspection. Prove the requested capability with one targeted read-only call before relying on it.
- Do not use Rider to bypass the user's build ownership. Rider is not an Unreal asset-mutation route.

## Review, Validation, And Git

- Review findings lead with bugs, regressions, risks, and missing validation, ordered by severity with file and line references.
- Explain the player-facing impact of every blocker requiring a user decision: trigger, visible consequence, and whether save/level progress is affected.
- A repair reopens validation relevant to the changed behavior. State separately what tools proved, what the user manually verified, and what remains unverified.
- Before asking the user to compile a non-trivial C++ change, perform a lightweight static review and a targeted server-memory query using code-derived terms. During a compiler or runtime repair loop, query server-memory before selecting a fix.
- Do not commit immediately after implementation. Wait for known validation status, review, documentation check, and explicit user approval.
- Use focused staging. For LFS-routed assets, verify a relevant staged asset is an LFS pointer and run git diff --cached --check before committing.
- Default commit titles are bilingual and factual, for example: [Feature] 中文标题（English Title）. Non-trivial gameplay commits use a factual body beginning with ## 核心改动 and include ## 文档更新 when applicable. Do not claim unperformed build, PIE, push, or asset verification.

## Gameplay Safety

- This is a single-player, level-based action RPG. Do not introduce replication, authority, rollback, GAS, or a generic ability framework unless ROADMAP.md explicitly changes that boundary.
- Timer callbacks, AnimNotifies, montage delegates, collision delegates, and perception delegates must validate object/current state before changing gameplay. They may arrive after interruption, hit stun, death, or cleanup.
- Use hysteresis for distance, angle, and threshold-driven transitions where boundaries can flicker. Follow the existing combat-exit pattern based on CombatingRadius plus CombatExitBuffer.
- Converge natural montage end, interruption, Notify, and delegate recovery paths through a shared helper when they restore the same action state.
- Mandatory gameplay configuration must warn or fail at the source; do not silently report success through an accidental fallback.
- Debug UI initialization uses raw values such as GetPlayerEnabledRaw(); runtime debug output uses effective getters such as IsPlayerEnabled(). Keep debug guards at function or block entry, and keep FDebugDrawHelper independent of gameplay classes and combat-state enums.

## C++ Conventions

- Put engine headers before project headers. Forward declare where practical. The generated header is the final include.
- Components are normally VisibleAnywhere and BlueprintReadOnly with Components category and AllowPrivateAccess when private. Use EditAnywhere or EditDefaultsOnly for configuration and VisibleInstanceOnly for runtime state.
- Blueprint-callable functions use the appropriate Blueprint specifier. AddDynamic targets must be UFUNCTION functions. UMG child widgets use BindWidget; Blueprint presentation hooks use BlueprintImplementableEvent.
- Keep editable/runtime-visible members private behind narrow getters or controlled operations. Use protected only when subclasses require direct access.
- Follow local raw-pointer style; do not begin a repository-wide TObjectPtr migration.
- Use A/U-prefixed PascalCase classes, E-prefixed enum types and project value prefixes, PascalCase members/methods, and b-prefixed booleans.
- Call Super in lifecycle/interface overrides unless deliberately avoided. Use check or ensure for invariants and avoid try/catch except around third-party throwing code.
- Null-check every SpawnActor result, log the caller and GetNameSafe class on failure, and return before use.
- Write gameplay-intent comments in Chinese. Use block documentation comments for UFUNCTION and UPROPERTY API surfaces.
