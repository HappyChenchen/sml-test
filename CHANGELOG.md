# Changelog

## 2026-03-19

### 关键功能
- 回退 `smart_layout` 的多轮兜底求解策略，恢复为单轮求解流程。
- 将 `sizeScale` 变量域收紧为 `0.3 ~ 1`。

### 关键改动
- `smart_layout/smart_layout_algorithm.h`
  - 删除 `SolveMode` 及 `LayoutContext.solveMode`。
- `smart_layout/smart_layout_algorithm.cpp`
  - 删除 `SolveMode` 分支与多轮尝试逻辑。
  - `PerformSmartLayout(...)` 恢复为单轮：初始化 -> 建模 -> 求解 -> 回写。
  - `addColumnLayout/addRowLayout` 恢复为固定 `sizeScale == finalScale`（`min(mainAxisScale, crossAxisScale)`）。
  - 末尾约束恢复严格贴边窗口（`+1 >= end` 与 `<= end` 同时存在）。
- `smart_layout/smart_layout_node.h`
  - `sizeScale` 变量域从 `[0,1]` 改为 `[0.3,1]`。
- `SMART_LAYOUT_GUIDE.md`
  - 同步文档到当前实现（单轮求解 + `sizeScale` 新区间）。

## 2026-03-18 (Linear Row/Column Bugfix)

### Highlights
- Added cross-axis spacing solve for linear layout (`row/col`): side-axis overflow now follows "shrink spacing first, then shrink size".
- Moved cross-axis margin handling from write-back patching into solver constraints to keep behavior consistent under scaling.
- Short-circuit path now checks both main axis and cross axis capacity, avoiding false "fit" when side axis overflows.

### Key changes
- `smart_layout_algorithm.cpp`
  - Updated `AddMainAxisAlignmentConstraints(...)`:
    - `SPACE_BETWEEN + single child` now enforces `mainAxisOffset = 0` and `betweenGap = 0`.
  - Updated `AddCrossAxisAlignmentConstraints(...)`:
    - Introduced `crossSpaceScale` and side margins in constraints for `COLUMN/ROW` `START/CENTER/END`.
  - Updated optimization objectives:
    - `maximize(sizeScale) -> maximize(spaceScale) -> maximize(crossSpaceScale)`.
  - Removed write-back side-axis margin compensation:
    - side margins are now solved directly in constraints.
  - Clamped main-axis baseline gaps before solving:
    - `leadingGap = max(0, leadingGapRaw)`
    - `innerGap = max(0, innerGapRaw)`
  - Added side-axis quick checks:
    - `IsColumnCrossSpaceEnough(...)`
    - `IsRowCrossSpaceEnough(...)`
    - short-circuit requires both main-axis and cross-axis enough.
- `SMART_LAYOUT_GUIDE.md`
  - Added `crossSpaceScale` and side-axis constraint formulas with scaled margins.
  - Updated write-back stage note (no second margin compensation in write-back).

## 2026-03-18

### 关键功能
- 解耦 Column/Row 约束建模：提取统一的 `addLinearLayout(...)`，由方向参数驱动主轴建模。
- 新增 JSON 树状测试用例，覆盖 `COLUMN/ROW` 全部主轴和侧轴对齐组合，并在多父容器尺寸下输出智能布局结果。

### 关键改动
- `smart_layout_algorithm.cpp`
  - 新增 `addLinearLayout(...)`，合并 `addColumnLayout/addRowLayout` 的重复逻辑。
  - `addColumnLayout` 与 `addRowLayout` 改为薄封装，只负责传递 `LayoutType`。
- `smart_layout_algorithm.h`
  - 新增 `addLinearLayout(...)` 私有声明。
  - 补充并细化中文注释，明确成员职责、函数输入输出与阶段语义。
- `smart_layout_algorithm.cpp`
  - 补充非常详细的中文注释，覆盖默认约束、主/侧轴约束、建模目标、短路优化、回写流程。
- 新增测试资产
  - `tests/data/smart_layout_overflow_tree.json`
  - `tests/smart_layout_json_case_runner.py`
  - `tests/generate_smart_layout_html.py`
  - `tests/README.md`
  - `tests/output/smart_layout_overflow_result.json`（运行脚本后生成）
  - `tests/output/smart_layout_overflow_report.html`（运行脚本后生成）
  - `smart_layout_json_case_runner.py` 运行后自动联动生成 HTML 报告。

### 关键代码
- `smart_layout_algorithm.cpp`
- `smart_layout_algorithm.h`
- `tests/smart_layout_json_case_runner.py`
- `tests/data/smart_layout_overflow_tree.json`

## 2026-03-17

### Highlights
- Switched cross-axis alignment to direction-specific enums:
  - `COLUMN -> HorizontalAlign`
  - `ROW -> VerticalAlign`
- Added detailed inline comments in `smart_layout_algorithm.cpp` for constraints, solver flow, and write-back.

### Key changes
- In `PerformSmartLayout`, mapped `GetCrossAxisAlignValue(...)` to `horizontalAlign_` and `verticalAlign_`.
- In `AddCrossAxisAlignmentConstraints`, removed `crossAxisAlign_` dependency and used direction-specific enums.
- Updated short-circuit condition to: `mainAxis == FLEX_START` and current direction cross-axis start.
- Wrapped helper functions in an anonymous namespace.

### Files
- `smart_layout_algorithm.cpp`
- `SMART_LAYOUT_GUIDE.md`
- `CHANGELOG.md`

## 2026-03-18

### 关键功能
- 新增一阶段 Flex 独立引擎，支持 `wrap`、`wrap-reverse`、`baseline` 与 `ItemAlign::AUTO`。

### 关键改动
- 新增 `smart_flex_layout_algorithm.h/.cpp`：
  - 采用“分行 -> 行内主轴求解 -> 行间侧轴放置 -> 回写”的四段流程。
  - 主轴保持智能缩放策略：先压间距（`spacescale`），再压尺寸（`sizescale`）。
  - `ItemAlign::AUTO` 回退到 `containerItemAlign`，双 AUTO 兜底 `START`。
- 更新 `smart_layout_algorithm.h/.cpp`：
  - 新增 `SmartFlexLayout(LayoutWrapper*, const SmartFlexConfig&)` 独立入口。
  - `LayoutType` 补齐 `FLEX` 枚举项并完善字符串映射。

### 关键代码
- `smart_flex_layout_algorithm.h`
- `smart_flex_layout_algorithm.cpp`
- `smart_layout_algorithm.h`
- `smart_layout_algorithm.cpp`
