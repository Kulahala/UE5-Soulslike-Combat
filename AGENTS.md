# AGENTS.md

Guidance for coding agents working in this repository.

## Operating Style

- Respond in Chinese by default. Give direct conclusions; state the specific uncertainty when evidence is incomplete.
- Read the real repository state before changing behavior or making architectural claims. Prefer the smallest change that satisfies the approved scope.
- Before editing files, state the intended edit in one concise sentence. Preserve user and existing working-tree changes.
- Ask before destructive, high-risk, or irreversible work. Do not broaden a task into a refactor or a new system without approval.
- Delegate only user-authorized, low-risk, bounded work. The main agent owns architecture, behavior-sensitive implementation, and review conclusions.

## Project And Build

- UE 5.7, Windows only, Visual Studio 2022 required.
- Modules: `Test` (Runtime) and `SmartBPCreator` (Editor plugin). Targets: `TestEditor` (Editor) and `SoulslikeCombat` (Game).
- No CI pipeline, lint command, or automated tests are configured. The user owns compilation, packaging, and PIE validation; do not run UBT, `Build.bat`, packaging, or other compile commands unless explicitly asked.

```powershell
# Generate project files: right-click Test.uproject -> Generate Visual Studio project files
# Compile manually: build TestEditor (Development Editor) in Visual Studio 2022
# Launch editor: open Test.uproject
```

`Test.Build.cs` public dependencies:
`Core`, `CoreUObject`, `Engine`, `InputCore`, `EnhancedInput`, `AnimGraphRuntime`, `Niagara`, `GeometryCollectionEngine`, `PCG`, `UMG`, `AIModule`, `SlateCore`, `MotionWarping`.

Private dependency: `Slate`.

## Evidence, Scope, And Content Boundaries

- Source code, assets, `.uproject`, and `.Build.cs` files are the primary truth. When repository documents disagree, use: **source/assets/config > ARCHITECTURE.md > plan.md > ROADMAP.md > README.md > AGENTS.md > CLAUDE.md > GEMINI.md**.
- Read `ARCHITECTURE.md` before structural changes. Verify behavior-critical claims against current C++ before editing gameplay logic.
- When `.codegraph/` exists, use CodeGraph first for C++ symbols, call paths, and blast-radius checks. If it is unavailable or stale, say so and fall back to `rg` plus direct source reads.
- C++ belongs under `Source/Test/`; Public/Private follows the UE module split. Game-owned editable assets belong under `Content/_GAME/`.
- Marketplace, sample, Paragon, Mixamo, and other large reference folders are read-only unless the user explicitly authorizes edits.
- Never manually patch binary `.uasset` or `.umap` files. Use Unreal Editor and the approved live-editor MCP route for Blueprint, UMG, level, and other asset work.
- Formal review and closeout ignore tracked `Content/*.uasset` changes by default unless the user explicitly asks to inspect or include them.

## Planning, Roadmap, And Documentation

- `README.md` is the public overview and current feature/status summary.
- `ARCHITECTURE.md` records stable implemented ownership, state flow, data flow, source-of-truth rules, and durable asset topology. Do not put ordinary tuning values there unless a value is itself a system contract.
- `ROADMAP.md` owns future direction, ordering, deferrals, non-goals, and Recommendations with adoption conditions. It is not an implementation plan or a source of truth for current behavior.
- `plan.md` is the short-lived active-stage plan and agent handoff. Read its header before continuing prior work, keep it current, and clear completed working detail according to its header rather than turning it into history.
- `AGENTS.md` owns durable collaboration, safety, tool, and workflow rules. Add a rule only when the pattern repeats, a mistake would be expensive, or it will guide future agents long-term.
- Update `ARCHITECTURE.md` only as stage-closeout work when source or authored assets introduce stable ownership, class responsibilities, state/data flow, source-of-truth rules, or durable asset topology. Write it after the approved implementation has passed its relevant validation and review, when the behavior is stable, and before the stage commit. Keep provisional designs, in-progress wiring, and unresolved behavior in `plan.md` until then.

For complex multi-file C++, Blueprint/UMG, content, architecture, source-plus-asset, or systemic gameplay work, write a concrete `plan.md` before implementation. It must state scope, affected systems and public APIs/assets, execution order, validation, document impact, and commit boundary.

- A queued roadmap TODO is accepted future work, not permission to start. Promote only the selected item into a complete `plan.md` stage before implementation.
- When accepting a new TODO, requirement, or follow-up, place it in `ROADMAP.md` by prerequisite order, validation dependency, and player-loop value; do not append it to the queue by default. If it changes the intended order of existing work, state the dependency reason, reposition the affected TODOs, and update the active `plan.md` before implementation resumes.
- Split work that crosses two or more primary implementation or validation boundaries into ordered, independently verifiable stages such as `TODO-04A` and `TODO-04B`. Each stage needs one narrow result, explicit dependencies, completion evidence, and one commit boundary. Do not hide reflected C++ contracts, unverified asset wiring, persistence restoration, and end-to-end validation in one opaque task; do not artificially split a small local fix.
- Do not silently append broad work to an active stage. If new facts materially change an approved plan, pause, explain the changed assumption, update the relevant plan or roadmap decision, and obtain approval before continuing.
- If a promoted roadmap task is paused or rejected, return it to `TODO Queue` or a Recommendation with its current reason.
- After approved validation and review, move a completed roadmap stage from `TODO Queue` to `Done Milestones`, mark it `[x]`, and retain only its compact durable result and completion commit. Do not recreate its plan unless a new requirement explicitly reopens it.
- A Recommendation must name a concrete adoption condition. Do not promote optional ideas to TODOs or introduce a framework merely because it might be useful later.
- `ROADMAP.md` owns `Known Risks And Validation Debt`: record a non-blocking stage-end gap there only when it has a concrete affected boundary, current evidence, technical or player impact, resolution/adoption condition, and a next owning stage or release gate. Do not use it as an unbounded warning list or a substitute TODO queue. Blockers must be fixed in the current stage; transient coordination notes stay in `plan.md`; resolved entries are removed or summarized in the relevant Done Milestone.
- Before a commit, check whether `README.md`, `ARCHITECTURE.md`, `ROADMAP.md`, and `AGENTS.md` need updates; change only documents whose scope actually changed.

## Live Editor And MCP

### Routing

- `UnrealClaude` is the primary route for normal Unreal Editor work: actor and level queries/edits, asset and dependency checks, Blueprint work, Enhanced Input, materials, viewport capture, console commands, and general editor scripts. Its REST backend is `http://127.0.0.1:3000`; standard MCP clients use `Resources/mcp-bridge/index.js` over stdio with `UNREAL_MCP_URL`, not `http://127.0.0.1:3000/mcp` directly.
- With UnrealClaude 1.5.x, domain mutations go through the exposed `unreal_ue` router with the relevant domain (`blueprint`, `anim`, `character`, `enhanced_input`, `material`, or `asset`). Direct calls are reserved for simple read-only or explicitly simple tools. Confirm actual tool names and parameters with `tools/list` first.
- `VibeUE` is the auxiliary route for capabilities specific to its registry: Python discovery/execution, animation graph or montage services, terrain tools, log reading, and VibeUE skills. Its UE 5.7 server is independent at `http://127.0.0.1:8088`; do not repeat an operation already assigned to UnrealClaude.
- Before a VibeUE-specific operation, load the relevant Agent Skill through the project-exposed skill manager, then use targeted `discover_python_class` or `discover_python_function` calls before writing `execute_python_code`. Prefer `manage_asset` for asset search, open, save, move, duplicate, and delete operations when it is exposed; do not guess method names or asset paths.

### Preconditions And Mutation Protocol

- Live asset work requires all of: this task scoped to `E:\UnRealEngine\Test`, `UnrealEditor.exe` running the Test project, the required `3000` and/or `8088` endpoint listening, and the configured MCP connection successfully completing `initialize -> tools/list ->` one read-only `tools/call`.
- A status endpoint, enabled plugin, or `unreal_status` response alone is not proof of a callable editor surface. Conversely, a successful configured-MCP flow is sufficient even if the client does not separately expose every native tool at top level.
- If any required process, endpoint, or callable surface is missing, stop before changing `.uasset`, `.umap`, Blueprint, or UMG. Continue only with source/config inspection or C++ work and report the exact missing prerequisite.
- Keep editor mutations sequential: query, mutate, then read back and verify. Do not issue the same mutation through both servers.
- When an Editor mutation fails on one MCP, the other MCP may be tried when it exposes a distinct, documented capability for that exact operation; start that route with its own targeted read/query rather than blindly repeating the mutation. If both MCP routes fail their direct documented attempts, or the remaining path would require speculative graph reconstruction, repeated API workarounds, or guessing asset internals, stop and hand the concrete Editor steps to the user. After the user completes them, perform one targeted read-only query of the relevant property, graph, hierarchy, compile result, or level Actor state and report that evidence separately.
- Confirm a saved recovery point before bulk or hard-to-reverse asset, Blueprint, or level work. VibeUE Python has no automatic rollback: print full paths and `CREATED`, `MODIFIED`, or `DELETED` actions, save affected assets, and read back the result.
- After visible editor changes, prefer a viewport or widget screenshot and inspect it. If capture is unavailable or incompatible, state that limitation and use property/graph readback, compile status, and user visual PIE validation instead; never claim screenshot-based verification without a captured image.
- After a user manually changes a UMG Widget Blueprint, perform one live read-only verification of the relevant graph connections, widget hierarchy, bound properties, or Blueprint compile result. Report that evidence separately from the user's visual/PIE validation.

For this UE 5.7 project, use the standard MCP `initialize -> tools/list -> tools/call` flow. Do not require UE 5.8 native-MCP-only methods such as `list_toolsets` or `call_tool` when validating either UnrealClaude's bridge or VibeUE.

## Review, Validation, Memory, And Git

- Formal review prioritizes bugs, behavioral regressions, risks, and missing validation. Put findings first, ordered by severity, with file and line references.
- For every blocker or finding that requires a user decision, explain the concrete player-facing impact alongside the technical cause: what the player will see, lose, be unable to do, or risk corrupting; the normal trigger; and whether save data, level progress, or only logs are affected. A severity label, source location, or abstract term such as "lifecycle race" is not sufficient by itself.
- Strict or stage-end review has two passes: normal review, then adversarial review that defends only technically sound design choices and records weak points as risks. It is not a defense exercise.
- A review fix reopens the validation relevant to that changed behavior. State what was verified by tools, what the user manually verified, and what remains unverified.
- Before asking the user to compile a non-trivial C++ change, perform a lightweight static review and a targeted server-memory MCP query using code-derived terms from the touched Unreal types, reflection surface, subsystem, or likely error family. Do not scan the full memory graph without a concrete query.
- During a compile/runtime repair loop, query server-memory MCP before selecting a fix. Reusable compiler errors, runtime failures, repeated warnings, and tool failures belong there, not in `plan.md`; record the trigger, symptom, wrong approach, correct approach, and scope. If server-memory is unavailable, say so and provide the proposed memory entry in the closeout. Do not query memory during strict review unless a concrete recurring pattern appears.
- Do not commit immediately after implementation. Wait for known validation status, completed review, documentation check, and explicit user approval.
- Feature titles use `[Feature] 中文标题（English Title）`. Non-trivial gameplay commits need a factual body beginning with `## 核心改动`, followed by focused domain sections and `## 文档更新` where applicable. Put verified results in the relevant section; do not invent a standalone validation section or claim unperformed validation.
- Stage Unreal assets with normal Git commands so Git LFS filters apply. For a commit containing LFS-routed `.uasset` files, verify at least one relevant staged asset is an LFS pointer. Use focused staging unless the user has confirmed the entire working tree is intentional.

## Gameplay And Debug Guardrails

- The product is a single-player, level-based action RPG. Do not add multiplayer replication, client authority, listen-server validation, rollback, or GAS as speculative infrastructure unless `ROADMAP.md` explicitly changes that boundary.
- Timer callbacks, AnimNotifies, montage delegates, collision delegates, and perception delegates must check current state and object validity before mutating gameplay. They may fire after death, interruption, hit stun, or lock-on cleanup has already changed state.
- Distance, angle, and threshold-driven FSM transitions require hysteresis where boundary flicker is possible. Follow the existing combat-exit pattern: `CombatingRadius + CombatExitBuffer`.
- Converge natural montage end, interruption, Notify, and delegate paths that recover the same action state through a shared helper.
- When a new system replaces an early gameplay path, converge on one explicit path. Keep temporary fallbacks only for a documented migration window with a removal condition.
- Missing mandatory gameplay configuration must warn or fail at the source rather than silently report success. Keep a fallback only when it is intentional, documented, and still playable.
- UI debug checkboxes initialize from raw values such as `GetPlayerEnabledRaw()`; runtime debug output uses effective getters such as `IsPlayerEnabled()` so the global gate is respected.
- Put debug guards at function or block entry, not before every `Add()` call. `FDebugDrawHelper` remains independent of gameplay classes and combat state enums and does not own gameplay state.

## C++ Conventions

### Includes And Reflection

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Character/CharacterTypes.h"
#include "MyCharacter.generated.h" // Must be the final include.

class UAttributeComponent;
class AWeapon;
```

- Put engine headers before project headers. Use full relative project include paths from `Public/`; forward declare in headers when possible.
- Components use `VisibleAnywhere, BlueprintReadOnly, Category = "Components"` and `meta = (AllowPrivateAccess = "true")` when private. Editable configuration is `EditAnywhere` or `EditDefaultsOnly`; runtime-only state is `VisibleInstanceOnly`.
- Use `meta = (ToolTip = "...")` for Details/Blueprint help. Blueprint-callable functions use `BlueprintCallable` or `BlueprintNativeEvent`; interface events implement the `_Implementation` override, and every `AddDynamic(...)` target has `UFUNCTION()`.
- UMG child widgets use `UPROPERTY(meta = (BindWidget))`. Blueprint-owned presentation hooks use `BlueprintImplementableEvent`.
- Follow surrounding pointer style. This repository primarily uses reflected raw pointers; do not start a repo-wide `TObjectPtr` migration.

### Encapsulation And Naming

- Keep externally read editable configuration and runtime-visible state private with `meta = (AllowPrivateAccess = "true")` plus narrow `FORCEINLINE` getters. Use `protected` only when subclasses need direct access.
- Keep containers private behind narrow operations when callers do not need general mutation; for example, clear `AWeapon::IgnoreActors` through `ClearIgnoreActors()`.
- Classes use `A`/`U` plus PascalCase; enums use `E` plus project value prefixes such as `ECS_`, `EAS_`, and `EES_`; interfaces use `I`; members use PascalCase; booleans use `b`; methods use PascalCase.

```cpp
FORCEINLINE EWeaponState GetCharacterState() const { return CharacterState; }
FORCEINLINE void SetActionState(const EActionState NewState) { ActionState = NewState; }
```

### Runtime Safety And Comments

- Override lifecycle and interface functions in subclasses, and call `Super::` unless deliberately avoiding it.
- Use `check()` or `ensure()` for invariants. Avoid `try/catch` except around third-party code that throws.
- Null-check every `SpawnActor` result before use. On failure, log `GetNameSafe(ActorClass)` and return early.
- Write gameplay-intent comments in Chinese. Use `/** */` only for `UFUNCTION` and `UPROPERTY` API documentation.
