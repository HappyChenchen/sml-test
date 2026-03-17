# smart_layout_algorithm.cpp：当前版本 vs init 版本对比

## 1. 总体差异（init: `0f9f697`，当前: `HEAD`）

| 维度 | init (`0f9f697`) | 当前 (`HEAD`) | 影响 |
|---|---|---|---|
| 主轴对齐能力 | 基本按链式排布 + 末项贴尾；没有统一的 `AddMainAxisAlignmentConstraints` | 统一由 `AddMainAxisAlignmentConstraints` 处理 `FLEX_START/CENTER/FLEX_END/SPACE_*` | 对齐语义完整且一致 |
| Column 主轴（非 SPACE） | `y0 = parentY + top*spaceScale`，末项强制贴底 | 头尾间距按 `spaceScale` 同比缩放，可用 `mainAxisOffset` 处理 center/end | 间距比例更可控 |
| Row 主轴（非 SPACE） | `x0 = parentX`，后续 `+ prevW + spaceScale`（固定增量） | `x0 = parentX + left*spaceScale (+offset)`，后续 `+ prevW + right*spaceScale` | Row 间距从“常量”改为“历史间距同比” |
| 侧轴对齐实现 | 直接用 `crossAxisAlign_`（`FlexAlign`）在 Row/Col 内部分支 | 先映射：Column 用 `HorizontalAlign`，Row 用 `VerticalAlign` | Row/Col 侧轴语义更清晰 |
| Row 的 `spaceScale` 约束 | 未完整约束（无显式 `[0,1]`） | 明确 `0 <= spaceScale <= 1` | 求解更稳定 |
| `betweenGap/mainAxisOffset` | 基本未系统化 | 作为主轴对齐变量统一使用（`SPACE_*` 和 center/end） | 公式表达更完整 |
| 短路优化 | 无 | `start/start + 空间足够` 直接跳过求解器 | 性能更好 |
| 日志可观测性 | 以调试打印约束/变量为主 | 增加 trigger/align/solver/compact 路径日志 | 更易定位“为何没触发” |

## 2. 原始对齐方式（init）Row/Col 对比表

> 下面是你要的“原始（init）对齐方式”表，只描述 `0f9f697` 行为。

| 项目 | Column（init） | Row（init） |
|---|---|---|
| 主轴起点（第 1 个子项） | `y0 = parentY + top0 * spaceScale` | `x0 = parentX` |
| 主轴相邻项（第 i>0 个） | `yi = y(i-1) + h(i-1) + bottom(i-1) * spaceScale` | `xi = x(i-1) + w(i-1) + spaceScale` |
| 主轴末尾约束 | 最后一个子项强制贴底：`y_last + h_last = parentBottom` | 最后一个子项强制贴右：`x_last + w_last = parentRight` |
| 侧轴 `FLEX_START` | `x = parentX` | `y = parentY` |
| 侧轴 `CENTER` | `x - parentX = parentRight - (x+w)` | `y - parentY = parentBottom - (y+h)` |
| 侧轴 `FLEX_END` | 未单独分支（Column 只区分 start / 非start） | `y + h = parentBottom` |
| 主轴 `SPACE_BETWEEN/AROUND/EVENLY` | 不支持独立公式 | 不支持独立公式 |
| `betweenGap/mainAxisOffset` | 无系统变量 | 无系统变量 |

## 3. 当前版本与 init 在“原始对齐”上的关键变化（按 Row/Col）

| 方向 | init 的核心问题 | 当前修正 |
|---|---|---|
| Column | 末项强制贴底，头尾比例不可同时保持 | 头尾间距都参与 `spaceScale`，支持 start/center/end 统一表达 |
| Row | 主轴间距用常量 `spaceScale` 递增，未使用历史 right 间距 | 主轴改为按历史间距同比缩放，和 Column 对称 |

---

### 代码定位（当前文件）
- 主轴对齐入口：`AddMainAxisAlignmentConstraints`（`smart_layout_algorithm.cpp`）
- Column 约束组装：`addColumnLayout`
- Row 约束组装：`addRowLayout`
