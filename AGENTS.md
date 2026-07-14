# AGENTS.md

This file provides guidance to coding agents working in this repository.

## Project Overview

- **UE 5.7** project, **Windows only**, **Visual Studio 2022** required.
- Modules: `Test` (Runtime), `SmartBPCreator` (Editor plugin).
- Targets: `TestEditor` (Editor), `SoulslikeCombat` (Game).
- No automated tests exist in this project.
- `ROADMAP.md` records long-term product direction and architecture adoption gates; `plan.md` remains the short-lived stage handoff document.

## Build & Editor

```bash
# Generate VS project files
# Right-click Test.uproject → "Generate Visual Studio project files" → open Test.sln

# Compile via UBT (Development Editor)
UnrealBuildTool TestEditor Win64 Development Test.uproject

# Compile via IDE
# Open Test.sln in VS 2022, build TestEditor target (Development Editor configuration)

# Launch editor
# Open Test.uproject directly
```

No CI pipeline, no lint/test commands configured.

- The user compiles manually. Do not run local build/compile commands unless the user explicitly asks for it.

## Module Dependencies

`Test.Build.cs` — `PublicDependencyModuleNames`:
`Core`, `CoreUObject`, `Engine`, `InputCore`, `EnhancedInput`, `AnimGraphRuntime`, `Niagara`, `GeometryCollectionEngine`, `PCG`, `UMG`, `AIModule`, `SlateCore`

`PrivateDependencyModuleNames`:
`Slate`

## Truth Sources

- Source code, `.uproject`, and `.Build.cs` files are the primary truth source.
- When repository markdown documents disagree, use this precedence: **source/assets/config > ARCHITECTURE.md > plan.md > ROADMAP.md > README.md > AGENTS.md > CLAUDE.md > GEMINI.md**.
- **ARCHITECTURE.md** is the shared architectural knowledge base for all agents. Read it before making structural changes.
- `AGENTS.md` and `CLAUDE.md` contain agent-specific collaboration rules; verify behavior-critical claims against actual C++ before editing gameplay logic.

## Agent Coordination

- `plan.md` is the shared agent-to-agent communication and handoff file.
- Before continuing prior collaboration work, read the header rules in `plan.md`.
- Write agent feedback and implementation plans there as working context, not end-user documentation.
- Keep `plan.md` short and current. Completed items are cleared according to its header rules; do not turn it into a permanent project history.
- `ROADMAP.md` records durable product direction, milestone ordering, explicit non-goals, and recommendations with adoption conditions. It is not an implementation plan or a source-of-truth for current behavior.
- For code review and closeout passes, ignore `Content/*.uasset` by default unless the user explicitly asks to inspect or include asset changes.
- When `.codegraph/` exists, use CodeGraph first for C++ symbol navigation, call paths, and blast-radius checks. Fall back to `rg` and direct source reads when the index is unavailable or stale, and state that fallback.
- Use Unreal Editor / VibeUE for Blueprint, level, UMG, and other `.uasset` changes. Do not manually patch Unreal binary assets. Confirm a save/recovery point before bulk or hard-to-reverse asset operations.

## MCP Tool Routing

- `UnrealClaude` is the primary MCP route for normal Unreal Editor work: actor and level queries or edits, asset search and dependency checks, Blueprint query or modification, Enhanced Input, materials, viewport capture, console commands, and general editor scripts. Its UE REST backend uses port `3000` (`http://127.0.0.1:3000`); external standard MCP clients must start `Resources/mcp-bridge/index.js` over stdio with `UNREAL_MCP_URL` pointing to that backend, rather than registering `http://127.0.0.1:3000/mcp` directly.
- With the installed UnrealClaude 1.5.x bridge, domain-specific operations should go through the exposed `unreal_ue` router with the appropriate domain (`blueprint`, `anim`, `character`, `enhanced_input`, `material`, or `asset`) instead of calling the underlying domain mutation tool directly. Use direct calls only for simple tools such as actor queries, asset search, Blueprint query, viewport capture, and similar read-only or explicitly simple operations; confirm the actual names and parameters through `tools/list` first.
- `VibeUE` is the auxiliary MCP route for capabilities that are specific to its tool registry, especially Python discovery or execution, animation graph or montage services, terrain tools, log reading, and VibeUE skills. Its UE 5.7 server is independent and defaults to port `8088` (`http://127.0.0.1:8088`). Do not use it to repeat an operation already assigned to `UnrealClaude`.
- Before a VibeUE-specific operation, load the relevant VibeUE Agent Skill through `vibeue-skills-manager`, then use targeted `discover_python_class` or `discover_python_function` calls before writing `execute_python_code`. Prefer `manage_asset` for asset search, open, save, move, duplicate, and delete operations; do not guess VibeUE method names or raw asset paths.
- VibeUE Python execution has no automatic rollback. For every mutating batch, print the full paths and actions using `CREATED`, `MODIFIED`, `DELETED`, or equivalent markers, save the affected assets, and read back the result. After visible editor changes, capture a screenshot and review it before claiming the change is complete.
- When both servers expose similar asset, Blueprint, animation, or scripting operations, prefer `UnrealClaude` and use `VibeUE` only when it provides a materially more specific tool. Do not issue the same mutating operation through both servers.
- Keep MCP calls sequential for mutating editor work. Before bulk or hard-to-reverse asset, Blueprint, or level edits, confirm that the project has a saved recovery point. Query first, then mutate, then verify the result.
- After a user manually changes a UMG Widget Blueprint, use the live MCP surface for one read-only verification before treating the edit as complete. Inspect the relevant graph connections, widget hierarchy, bound properties, or Blueprint compile result as applicable; report the verification evidence and keep user visual/PIE validation as a separate step.
- Live Editor asset work requires all of the following: the current Codex task is scoped to `E:\UnRealEngine\Test`, `UnrealEditor.exe` is running the Test project, the expected `3000` and/or `8088` endpoint is listening, and the current Codex session exposes actual MCP tools. A reachable status endpoint, an enabled plugin in `Test.uproject`, or a status-only probe is not sufficient evidence that editor mutation tools are available.
- For this UE 5.7 project, validate the active MCP surface with the standard `initialize` -> `tools/list` -> `tools/call` flow. Do not require UE 5.8 native-MCP-only methods such as `list_toolsets` or `call_tool` when validating UnrealClaude's bridge or VibeUE's independent server; conversely, do not treat a status probe as a substitute for `tools/list` and a successful read-only `tools/call`.
- If the Editor process, required port, or callable MCP tool surface is missing, stop before editing `.uasset`, `.umap`, Blueprint, or UMG assets. Continue with source/config inspection or C++ work only, and state the exact missing prerequisite.

## Content Boundaries

- C++ source lives under `Source/Test/`; Public/Private mirrors the UE module structure.
- Game-owned assets live under `Content/_GAME/`; modify this directory by default for project gameplay/HUD/content work.
- Marketplace, sample, Paragon, Mixamo, and other large reference asset folders are read-only context unless the user explicitly asks to edit them.
- For review and closeout passes, ignore tracked `.uasset` changes under `Content/` by default unless the user explicitly asks to inspect or include asset changes.

## Planning And Documentation

- For complex multi-file C++, Content, Blueprint architecture, source plus `.uasset` commits, long-term extension points, or systemic gameplay work, write a concrete `plan.md` plan before implementation. Include scope, affected systems, execution order, validation, document impact, and commit boundary.
- When an approved task crosses two or more implementation or validation boundaries (for example C++, Blueprint/UMG, map assets, project configuration, user compilation, or PIE), split `plan.md` into independently verifiable sub-stages. Each sub-stage must name its narrow goal, affected files/assets, completion evidence, and the input it hands to the next stage. Do not bundle new reflected C++ APIs, unverified asset wiring, and end-to-end gameplay validation into one opaque change; small local fixes do not need artificial stage splitting.
- If implementation materially changes an approved plan, pause, explain the changed assumption, update the relevant plan or roadmap decision, then continue only after the user approves the new direction.
- `README.md` is the public project overview and feature/status summary. `ARCHITECTURE.md` contains stable implemented facts, ownership boundaries, state flow, and data source-of-truth rules. `ROADMAP.md` owns future direction, milestone priorities, deferrals, and adoption triggers. `plan.md` owns the active stage only.
- `AGENTS.md` is not a stage board. Add rules here only when a behavior pattern repeats, one mistake would be expensive, or the rule will guide future agents long-term.
- Update `ARCHITECTURE.md` when code or authored assets alter stable ownership, state flow, data flow, class responsibilities, or durable asset topology. Do not store ordinary tunable numbers there unless the value is itself a system contract.
- `ROADMAP.md` TODOs are accepted future work, ordered but not active implementation work. When one becomes the next stage, remove it from the roadmap queue and write a complete `plan.md` plan before implementation: goal, affected systems, public APIs/assets, execution order, validation, documentation impact, and commit boundary. If a promoted task is paused or rejected, return it to `ROADMAP.md` as a TODO or Recommendation with its current reason.
- A roadmap Recommendation must name a concrete adoption condition. Do not turn optional ideas into TODOs, and do not implement a new framework merely because it might be useful later.
- Before committing, check whether `README.md`, `ARCHITECTURE.md`, `ROADMAP.md`, and `AGENTS.md` need updates. Update only documents whose scope actually changed.

## Commit Message Policy

- Feature commits use `[Feature] 中文标题（English Title）`; keep the English title concise and descriptive.
- Non-trivial gameplay changes should include a factual multi-section body, following prior detailed commits such as `60e33a5ee14320561f1e3aad7016f5c671d98316`:
  - `## 核心改动`
  - domain sections as needed, for example `## 输入与战斗流程`, `## 动画与资产`, `## 蒙太奇约束`
  - `## 文档更新`
- Do not require a standalone `## 验证` section. Include verified results in the most relevant section when useful.
- The body must describe actual changed files, asset/editor work, and documentation updates. Do not claim compile, PIE, or asset validation unless the user or tool output confirmed it.
- Do not commit immediately after implementation. Confirm the validation status is known, review is complete, relevant documentation has been checked, and the user has approved the commit.
- Stage Unreal assets with normal Git commands so Git LFS filters apply. For a commit containing `.uasset` files, verify at least one staged relevant asset is an LFS pointer when the repository routes it through LFS; do not disable filters for staging or committing.

## Working Rules

### Communication
- 默认中文回答，除非用户明确要求英文。
- 直接回答问题，不用"好问题"、"很高兴为您服务"等客套开场。

### Code Investigation
- 代码库任务先读真实文件，再下结论。不要凭记忆改关键逻辑。
- 用户已有改动默认保留，除非用户明确要求回滚。

### Change Authorization
- 未明确授权时不做大范围重构、删除、回滚。
- 文件修改前先简短说明要改什么（一句话即可）。
- 停下来比猜错强。不确定就问，有多种解读就都摆出来。

### Code Review Protocol
- **两轮 review 必须**：正常 review + 对抗性 review
- **第一轮（正常 review）**：
  - 优先找 bug、行为回归、风险和缺测试
  - 按严重程度排序（P0/P1/P2）
  - 给文件/行号依据
- **第二轮（对抗性 review）**：
  - 假设代码是自己写的，先为关键设计辩护
  - 辩护中发现站不住脚的地方就诚实标成风险
  - 给改进建议
  - **不是洗地**：能辩护才辩护，不能就直说

### Sub-Agent Strategy
- **轻量子代理（mini 模型）适用场景**：低风险只读任务、机械文档任务、批量搜索
- **不要交给 mini**：架构决策、关键 review、行为敏感逻辑、设计方案制定

### Error Memory
- 编译错误、运行错误、重复 warning 或工具故障出现时，先判断是否属于可复用模式。
- 可复用模式应记录到 server-memory MCP：触发场景、错误做法、正确做法和适用范围。MCP 不可用时，在收尾中给出建议记录的内容，不要只口头承诺。

## Project Gameplay Rules

- 当前产品方向是**单机、关卡制动作 RPG**。不要为多人复制、客户端权威、Listen Server 验证或联机回滚提前设计接口；只有 `ROADMAP.md` 明确变更该边界后才能引入。
- Async callback guards: timer callbacks, AnimNotifies, montage delegates, collision delegates, and perception delegates must check current state and object validity before mutating gameplay state. These callbacks may fire after a higher-priority path such as hit stun, death, interruption, or lock-on clearing has already changed state.
- FSM hysteresis: distance-, angle-, or threshold-based state transitions need separate enter/exit thresholds to avoid boundary flicker. Enemy combat exit currently uses `CombatingRadius + CombatExitBuffer`; use the same pattern for similar states.
- Shared recovery entry: if natural montage end, interruption, Notify, and delegate paths recover the same action state, converge them through a shared helper instead of maintaining separate recovery logic.
- When a new system replaces an early-project gameplay path, converge on one explicit path instead of retaining duplicate hidden fallbacks unless a real migration window and removal condition exist.
- Missing mandatory gameplay configuration must warn or fail at the source rather than silently pretending the feature succeeded. Keep an intentional minimal fallback only when it is documented and remains playable.
- Compile ownership: the user compiles and packages manually. Do not run UBT, `Build.bat`, packaging, or other long-running compile/build commands unless the user explicitly asks.

## Debug Output Rules

- UI checkbox initialization must read raw debug values such as `GetPlayerEnabledRaw()` so the UI shows the sub-toggle's own state.
- Runtime debug output must use effective getters such as `IsPlayerEnabled()` so the global debug gate is respected.
- Place debug guards at function or block entry. Do not repeat per-line checks before every debug `Add()`.
- `FDebugDrawHelper` must stay independent from gameplay classes such as `AEnemy`, `AMyCharacter`, and combat state enums, and must not own gameplay state.

## Architecture

**For all architectural knowledge (state machines, class hierarchy, combat pipeline, system boundaries), see [ARCHITECTURE.md](ARCHITECTURE.md).**

This is the shared source of truth for Codex, Claude, Gemini, and all other agents. Read it before making structural changes.

## Code Conventions

### Includes

```cpp
// System/engine first, then project — forward declare when possible
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Character/CharacterTypes.h"   // project includes use full relative path from Public/
#include "MyCharacter.generated.h"        // .generated.h must be last include

class UAttributeComponent;               // forward declare in headers, include in .cpp
class AWeapon;
```

### UPROPERTY / UFUNCTION

- Components: `VisibleAnywhere, BlueprintReadOnly, Category = "Components"` with `meta = (AllowPrivateAccess = "true")` when private.
- Editable config: `EditAnywhere` or `EditDefaultsOnly`.
- Runtime-only props: `VisibleInstanceOnly`.
- If the goal is a readable tooltip in Blueprint / Details panels, use `meta = (ToolTip = "...")`; line comments only help source readers.
- Functions exposed to BP: use `BlueprintCallable` or `BlueprintNativeEvent`.
- Interface functions: use `BlueprintNativeEvent` + `_Implementation` suffix.
- Any function bound through `AddDynamic(...)` must be marked with `UFUNCTION()`, even if it is not exposed to Blueprint. Current examples include `HandleExhausted`, `TargetPerceptionUpdated`, `SphereOverlap`, `SphereEndOverlap`, and HUD health/stamina update callbacks.
- UMG child widgets are bound with `UPROPERTY(meta = (BindWidget))`; Blueprint-owned presentation hooks use `BlueprintImplementableEvent` (for example `UBaseHealthBarWidget::PlayFadeOutAnim()`).
- Do not assume a repo-wide `TObjectPtr` migration. Follow the surrounding file style: this codebase mostly uses reflected raw pointers, with `TObjectPtr` appearing only in a few spots such as `AArenaGenerator`.

### Encapsulation

- Editable config that external code only reads should be `private` `UPROPERTY(EditAnywhere/EditDefaultsOnly, meta = (AllowPrivateAccess = "true"))` plus a `FORCEINLINE` getter.
- Runtime state that should be visible but not externally mutated should be `private` `UPROPERTY(VisibleInstanceOnly, meta = (AllowPrivateAccess = "true"))` plus a getter.
- Keep members `protected` only when subclasses genuinely need direct access; current examples include `PendingHitContext` and `ConsumePendingHitKnockback()`.
- Container members such as `TArray` / `TMap` should stay private behind controlled methods when callers only need a narrow operation. Current example: `AWeapon::IgnoreActors` is cleared through `ClearIgnoreActors()`.
- Recent examples to follow: `AEnemy` poise parameters, `AShield` block/parry/equip parameters, `AWeapon::IgnoreActors`, and `ABaseCharacter` internal knockback state.

### Naming

| Type | Convention | Example |
|------|-----------|---------|
| Classes | `A`/`U` prefix, PascalCase | `AMyCharacter`, `UAttributeComponent` |
| Enums | `E` prefix, `ECS_`/`EAS_`/`EES_` prefixes for values | `EActionState::EAS_Attacking` |
| Interfaces | `I` prefix, PascalCase | `IHitInterface`, `IPickupInterface` |
| Members | PascalCase, no `m_` or `_` prefix | `OverLapItem`, `ActionState` |
| Booleans | `b` prefix | `bIsSprinting` |
| Methods | PascalCase | `GetCharacterState()`, `PlayAttackMontage()` |

### Accessors

Use `FORCEINLINE` inline in headers:

```cpp
FORCEINLINE EWeaponState GetCharacterState() const { return CharacterState; }
FORCEINLINE void SetActionState(const EActionState NewState) { ActionState = NewState; }
```

### Virtual Functions

Override lifecycle functions (`BeginPlay`, `Tick`) and interface implementations in subclasses. Always call `Super::` in overrides unless explicitly avoiding it.

### Error Handling

Use `check()` / `ensure()` for invariants. No `try/catch` unless interacting with third-party code that throws.

### Spawn Actor Safety

All `SpawnActor` calls must null-check the returned actor before use. On failure, log with `GetNameSafe(ActorClass)` and return early instead of dereferencing a null actor.

### Comments

- Comments in **Chinese** for gameplay intent (current convention in codebase).
- English is acceptable for API docs and technical notes.
- Use `/** */` only when documenting UFUNCTION/UPROPERTY.

## User Profile

- UE 5.7 C++ developer, based in the United States.
- Prefers **concise, accurate** answers grounded in source code facts.
- Chinese is acceptable for general communication; code and technical terms remain in English.
