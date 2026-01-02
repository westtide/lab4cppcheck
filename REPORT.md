# UninitVar 作业报告

## 目标
- 用极简用例覆盖多类“未初始化变量”情形。
- 在 UninitVar 核心路径做结构化日志插桩（phase/event/state）。
- 日志输出到文件，stdout/ stderr 分离。
- 修改一处小逻辑，编译运行并对比前后输出。

## 用例说明
- `cases/case_alloc_uninit.c`：已分配但未初始化内存（*p）
- `cases/case_definite_uninit.c`：确定未初始化变量（x）
- `cases/case_init_ok.c`：已初始化（不报）
- `cases/case_maybe_uninit_branch.c`：分支导致“可能未初始化”（x）
- `cases/case_struct_member_uninit.cpp`：结构体成员未初始化（s.a）

## 代码修改概述
### 新增文件
- `lib/uninittrace.h`
- `lib/uninittrace.cpp`

作用：提供统一的 trace 输出入口，支持开关与变量过滤。

### 修改函数与插桩位置
- `CheckUninitVar::check`：模块入口，输出 phase=ENTRY
- `CheckUninitVar::checkScope`：进入函数/作用域，输出 phase=SCOPE
- `CheckUninitVar::checkScopeForVariable`：变量级分析入口，输出 phase=VAR；
  在 ASSIGN/READ/MERGE 关键点输出 phase=FLOW
- `CheckUninitVar::checkIfForWhileHead`：条件读取点，输出 READ
- `CheckUninitVar::checkLoopBody`：循环内读取点，输出 READ
- `CheckUninitVar::checkRhs`：右值读取点，输出 READ
- `CheckUninitVar::uninitvarError / uninitdataError / uninitStructMemberError`：最终报告点，输出 phase=REPORT

### 插桩输出格式
单行 key=value，便于 grep：

```
phase=... event=... var=... varid=... loc=... state_before=... state_after=... reason=... scope=...
```

reason 约定：
- `CheckUninitVar::<函数>::<PHASE>[:细节]`，用于定位日志来自哪个函数与阶段。

phase 约定：
- ENTRY：进入 UninitVar 模块
- SCOPE：进入函数/作用域
- VAR：开始分析变量
- FLOW：扫描/状态变化
- REPORT：产生告警

event 约定：
- DECL / ASSIGN / READ / MERGE / REPORT

state 约定：
- UNINIT / INIT / MAYBE / UNKNOWN

### 开关与过滤
- `CPPCHECK_UNINIT_TRACE=1`：开启 trace
- `CPPCHECK_UNINIT_TRACE_VAR=x`：只输出变量名匹配 x 或 x.member 的日志

## 逻辑改动
位置：`CheckUninitVar::uninitvarError(const ValueFlow::Value& v)`

改动：
- 如果 `v.isKnown()` 为 false，即 ValueFlow 的不确定/可能性路径，直接返回，不再上报该 uninitvar。

目的：
- 让“可能未初始化”warning被抑制，但不影响“确定未初始化”error与 uninitdata/structMember。

效果：
- `case_maybe_uninit_branch.c` 的 uninitvar 告警消失。
- `case_definite_uninit.c`、`case_alloc_uninit.c`、`case_struct_member_uninit.cpp` 仍保持原告警。

## 运行与日志输出
日志命名改为 `case_*.out.log` / `case_*.trace.log`，更直观。
旧的聚合日志已移动到 `logs/legacy/`，避免混淆。

### 基线
基线需要“未应用 0002”时生成，以确保包含 maybe-uninit 告警：

```sh
git apply -R patches/0002-behavior-change.patch
cmake -S . -B build
cmake --build build

for f in cases/case_*.c cases/case_*.cpp; do
  base=$(basename "$f")
  name=${base%.*}
  build/bin/cppcheck --quiet \
    --enable=warning,style,performance,portability --inconclusive \
    --template=gcc \
    --output-file=logs/baseline/${name}.out.log \
    "$f" \
    2> logs/baseline/${name}.trace.log
 done
```

说明：基线 trace 为空是正常的，因为未开启 `CPPCHECK_UNINIT_TRACE`。

### 修改后（开启 trace + 行为改动）
```sh
git apply patches/0002-behavior-change.patch
cmake --build build

for f in cases/case_*.c cases/case_*.cpp; do
  base=$(basename "$f")
  name=${base%.*}
  CPPCHECK_UNINIT_TRACE=1 build/bin/cppcheck --quiet \
    --enable=warning,style,performance,portability --inconclusive \
    --template=gcc \
    --output-file=logs/modified/${name}.out.log \
    "$f" \
    2> logs/modified/${name}.trace.log
 done
```

## 日志解读
### 示例 1：确定未初始化（case_definite_uninit）
```
phase=VAR event=DECL var=x varid=1 loc=[cases/case_definite_uninit.c:3] state_before=UNINIT state_after=UNINIT reason=CheckUninitVar::checkScopeForVariable::VAR
phase=FLOW event=READ var=x varid=1 loc=[cases/case_definite_uninit.c:4] state_before=UNINIT state_after=UNINIT reason=CheckUninitVar::checkScopeForVariable::READ:return-read
phase=REPORT event=REPORT var=- varid=- loc=[cases/case_definite_uninit.c:4] state_before=UNKNOWN state_after=UNKNOWN reason=CheckUninitVar::uninitvarError::REPORT:report-uninitvar-valueflow
```
解读：
- DECL：变量进入分析，初始 UNINIT
- READ：return 读取时仍为 UNINIT
- REPORT：生成 uninitvar 告警

### 示例 2：分支导致可能未初始化（case_maybe_uninit_branch）
```
phase=VAR event=DECL var=x varid=3 loc=[cases/case_maybe_uninit_branch.c:3] state_before=UNINIT state_after=UNINIT reason=CheckUninitVar::checkScopeForVariable::VAR
phase=FLOW event=ASSIGN var=x varid=3 loc=[cases/case_maybe_uninit_branch.c:5] state_before=UNINIT state_after=INIT reason=CheckUninitVar::checkScopeForVariable::ASSIGN:assume-assign
phase=FLOW event=MERGE var=x varid=3 loc=[cases/case_maybe_uninit_branch.c:6] state_before=UNINIT state_after=MAYBE reason=CheckUninitVar::checkScopeForVariable::MERGE:if-merge
phase=FLOW event=READ var=x varid=3 loc=[cases/case_maybe_uninit_branch.c:7] state_before=MAYBE state_after=MAYBE reason=CheckUninitVar::checkScopeForVariable::READ:return-read
```
解读：
- 分支内 ASSIGN 使状态变为 INIT
- if 结束时 MERGE 成 MAYBE
- READ 发生在 MAYBE 状态
- 因小逻辑改动，ValueFlow 的“未知/可能性”不再上报

注：ValueFlow 的报告可能出现在 ENTRY 之前，这是因为 `valueFlowUninit()` 在 `check()` 之前执行。

## 调试验证与效果复现（
### 1) 理论与函数映射
核心思路（状态机 + 读写判定）：
- 遍历可执行作用域，逐变量进行状态追踪（UNINIT/INIT/MAYBE）。
- 遇到赋值则转为 INIT；分支合并则为 MAYBE；读取仍是 UNINIT/MAYBE 则报错。
- 条件表达式、循环体、右值表达式是典型“读取点”。
- 触发错误后进入 REPORT 上报。

对应函数映射：
- 模块入口/调度：`CheckUninitVar::check`、`CheckUninitVar::checkScope`
- 变量级状态机：`CheckUninitVar::checkScopeForVariable`
- 条件/循环/右值读取：`CheckUninitVar::checkIfForWhileHead`、`CheckUninitVar::checkLoopBody`、`CheckUninitVar::checkRhs`
- 最终上报：`CheckUninitVar::uninitvarError` / `CheckUninitVar::uninitdataError` / `CheckUninitVar::uninitStructMemberError`
- ValueFlow 提前报告：`CheckUninitVar::uninitvarError(const ValueFlow::Value& v)`（可能早于 ENTRY）

### 2) 构建与运行（日志方式调试）
构建命令（来自 `readme.md` 的 CMake 章节）：
```
cmake -S . -B build
cmake --build build
```
构建观察：
- CMake 警告：Boost 未找到（可选依赖，不影响 CLI 构建）。
- 链接告警：duplicate libraries（未影响 `build/bin/cppcheck` 生成）。

极简案例（用于调试验证）：
- `cases/case_definite_uninit.c`
- trace + out 输出：`logs/analysis/case_definite_uninit.trace.log`、`logs/analysis/case_definite_uninit.out.log`

调试记录（步骤 + 文本截图 + 说明）：
- 详见 `logs/analysis/uninitvar_debug.log`（已按“步骤+日志片段+说明”记录）。 

### 3) 修改前后效果对比
对比对象：
- 修改前：`logs/baseline/case_maybe_uninit_branch.out.log`
- 修改后：`logs/analysis/case_maybe_uninit_branch.out.log`

变化说明：
- 修改点：`CheckUninitVar::uninitvarError(const ValueFlow::Value& v)` 中 `!v.isKnown()` 直接返回。
- 结果：分支导致 MAYBE 的不确定路径不再上报，warning 消失。

## 函数调用链（基于两份日志）
来源：
- `logs/legacy/cppcheck.trace.log`（摘取 case_alloc_uninit 片段）
- `logs/modified/case_struct_member_uninit.trace.log`

### 链路 A：case_alloc_uninit
```
[cppcheck]
  -> CheckUninitVar::check (phase=ENTRY)
     -> CheckUninitVar::checkScope (phase=SCOPE)
        -> CheckUninitVar::checkScopeForVariable (phase=VAR)
           -> CheckUninitVar::checkScopeForVariable::READ (phase=FLOW, read-before-write)
              -> CheckUninitVar::uninitdataError (phase=REPORT)
```

### 链路 B：case_struct_member_uninit
```
[cppcheck]
  -> CheckUninitVar::uninitvarError (phase=REPORT, valueflow 报告先于 ENTRY)
  -> CheckUninitVar::check (phase=ENTRY)
     -> CheckUninitVar::checkScope (phase=SCOPE)
        -> CheckUninitVar::checkScopeForVariable (phase=VAR)
           -> CheckUninitVar::checkScopeForVariable::READ (phase=FLOW, return-read)
              -> CheckUninitVar::uninitStructMemberError (phase=REPORT)
```


## 结论与差异
- 基线输出包含 `case_maybe_uninit_branch` 的 uninitvar warning。
- 修改后该 warning 消失，其它确定性告警保持。
- trace 日志可用 `CPPCHECK_UNINIT_TRACE_VAR=x` 精确过滤变量生命周期。
- 调试日志中的调用链与“状态机 + 读写判定”的理论拆解一致（ENTRY -> SCOPE -> VAR -> FLOW -> REPORT）。
