# Smart Layout Guide（按容器分类）

## 1. 文档范围
本文只描述当前代码中的容器布局规则：
- `Linear 容器`：`COLUMN / ROW`（约束求解版）
- `Flex 容器`：`ROW / COLUMN + wrap/wrap-reverse`（分行启发式版）

本文不包含：测试说明、历史变更记录、手算用例。

## 2. 通用符号
- 父容器：
  - 位置：`x_p, y_p`
  - 尺寸：`W_p, H_p`
- 子项 `i`：
  - 原始尺寸：`w_i0, h_i0`
  - 求解后位置：`x_i, y_i`
  - 求解后尺寸：`w_i, h_i`
- 缩放变量：
  - `sizeScale`：尺寸缩放
  - `spaceScale`：间距缩放

边界约束（两类容器通用）：
- `x_i >= x_p`
- `y_i >= y_p`
- `x_i + w_i <= x_p + W_p`
- `y_i + h_i <= y_p + H_p`

---

## 3. Linear 容器（COLUMN / ROW）
对应代码：`smart_layout_algorithm.cpp`（`PerformSmartLayout` + `addLinearLayout`）

### 3.1 支持的对齐方式
- 主轴（`FlexAlign`）：
  - `FLEX_START / CENTER / FLEX_END / SPACE_BETWEEN / SPACE_AROUND / SPACE_EVENLY`
- 侧轴：
  - `COLUMN` 使用 `HorizontalAlign`：`START / CENTER / END`
  - `ROW` 使用 `VerticalAlign`：`TOP / CENTER / BOTTOM`

### 3.2 支持的属性
- 布局方向：`COLUMN / ROW`
- 主轴对齐：`mainAxisAlign_`
- 侧轴对齐：方向映射后的 `horizontalAlign_ / verticalAlign_`
- 统一缩放：`sizeScale`、`spaceScale`

### 3.3 核心变量
- 尺寸：
  - `w_i = w_i0 * sizeScale`
  - `h_i = h_i0 * sizeScale`
- 主轴变量：
  - `mainAxisOffset >= 0`
  - `betweenGap >= 0`（仅 `SPACE_*` 模式主导）

### 3.4 主轴链式公式
`COLUMN`（主轴 = `Y`）：
- 首项：
  - 非 `SPACE_*`：`y_0 = y_p + top_0 * spaceScale + mainAxisOffset`
  - `SPACE_*`：`y_0 = y_p + mainAxisOffset`
- 后续：
  - 非 `SPACE_*`：`y_i = y_{i-1} + h_{i-1} + bottom_{i-1} * spaceScale`
  - `SPACE_*`：`y_i = y_{i-1} + h_{i-1} + betweenGap`

`ROW`（主轴 = `X`）：
- 首项：
  - 非 `SPACE_*`：`x_0 = x_p + left_0 * spaceScale + mainAxisOffset`
  - `SPACE_*`：`x_0 = x_p + mainAxisOffset`
- 后续：
  - 非 `SPACE_*`：`x_i = x_{i-1} + w_{i-1} + right_{i-1} * spaceScale`
  - `SPACE_*`：`x_i = x_{i-1} + w_{i-1} + betweenGap`

### 3.5 主轴对齐约束
设：
- `startPos`：内容块起点（首项主轴起点）
- `endPos`：内容块终点（末项主轴终点）
- `parentStart / parentEnd`：父容器主轴边界

公式：
- `FLEX_START`：`mainAxisOffset = 0`，`betweenGap = 0`
- `CENTER`：`startPos - parentStart = parentEnd - endPos`
- `FLEX_END`：`endPos = parentEnd`
- `SPACE_BETWEEN`：`startPos = parentStart`，`endPos = parentEnd`
  - 单子项特例：退化为 `FLEX_START`（`mainAxisOffset = 0`，`betweenGap = 0`）
- `SPACE_AROUND`：`startPos - parentStart = betweenGap / 2`，`parentEnd - endPos = betweenGap / 2`
- `SPACE_EVENLY`：`startPos - parentStart = betweenGap`，`parentEnd - endPos = betweenGap`

### 3.6 侧轴对齐公式
`COLUMN`（侧轴 = `X`）：
- `START`：`x_i = x_p`
- `CENTER`：`x_i - x_p = (x_p + W_p) - (x_i + w_i)`
- `END`：`x_i + w_i = x_p + W_p`

`ROW`（侧轴 = `Y`）：
- `TOP`：`y_i = y_p`
- `CENTER`：`y_i - y_p = (y_p + H_p) - (y_i + h_i)`
- `BOTTOM`：`y_i + h_i = y_p + H_p`

### 3.7 优化目标
- 一阶段目标优先级：
  - `maximize(sizeScale)`
  - `maximize(spaceScale)`
- 语义：先尽量保尺寸，再尽量保间距。

### 3.8 回写阶段侧轴 margin 补偿
- 线性布局回写时只补偿侧轴 margin（主轴间距已在主轴链式约束里建模）。
- `COLUMN`（侧轴为 `X`）：
  - `START`：`x += margin.left`
  - `CENTER`：`x += (margin.left - margin.right) / 2`
  - `END`：`x -= margin.right`
- `ROW`（侧轴为 `Y`）：
  - `TOP`：`y += margin.top`
  - `CENTER`：`y += (margin.top - margin.bottom) / 2`
  - `BOTTOM`：`y -= margin.bottom`

---

## 4. Flex 容器（ROW / COLUMN + WRAP）
对应代码：`smart_flex_layout_algorithm.cpp`（`SmartFlexLayoutAlgorithm::Layout`）

### 4.1 支持的对齐方式
- 主轴（`FlexAlign`）：
  - `FLEX_START / CENTER / FLEX_END / SPACE_BETWEEN / SPACE_AROUND / SPACE_EVENLY`
- 侧轴（`SmartItemAlign`）：
  - `AUTO / START / CENTER / END / STRETCH / BASELINE`

### 4.2 支持的属性
- 方向：`SmartFlexDirection::ROW / COLUMN`
- 换行：`SmartFlexWrapMode::NO_WRAP / WRAP / WRAP_REVERSE`
- 间距：`mainGap`、`crossGap`
- 侧轴默认对齐：`containerItemAlign`

`AUTO` 规则：
- `itemAlign != AUTO`：用子项指定值
- `itemAlign == AUTO`：用 `containerItemAlign`
- 两者都 `AUTO`：兜底 `START`

### 4.3 计算阶段
1. `BuildSnapshots`：采集子项几何并复位缩放。
2. `BuildLines`：按主轴累计尺寸 + `mainGap` 做分行。
3. `SolveMainAxisForLine`：每行做主轴缩放与对齐。
4. `PlaceLinesOnCrossAxis`：行在侧轴排布，支持 `wrap-reverse`。
5. `PlaceChildrenInLine`：行内侧轴对齐（含 `stretch`、`baseline`）。
6. `WriteBack`：回写坐标与缩放。

### 4.4 分行规则
设当前行主轴占用 `M`，新子项主轴尺寸 `m_i`：
- 预测值：
  - 首项：`pred = m_i`
  - 非首项：`pred = M + mainGap + m_i`
- 当 `wrap != NO_WRAP` 且 `pred > parentMain` 时换行。

### 4.5 行内主轴缩放与对齐
设一行：
- 子项主轴总和 `S = Σ itemMain`
- 原始总间距 `G = mainGap * (n - 1)`
- 可用主轴 `P = parentMain`

非 `SPACE_*`：
- 若 `S + G <= P`：`sizeScale=1, spaceScale=1`
- 否则先压间距：
  - `spaceScale = clamp((P - S) / G, 0, 1)`（仅 `S < P` 且 `G>0`）
- 若仍超限再压尺寸：
  - `sizeScale = clamp(P / S, 0, 1)`

`SPACE_*`：
- 主轴间距由自由空间二次分配，若 `S > P` 先按 `sizeScale = P / S` 缩尺寸。

自由空间：
- `free = max(0, P - usedMain)`

对应 `FlexAlign`：
- `FLEX_START`：`leading = 0`
- `CENTER`：`leading = free / 2`
- `FLEX_END`：`leading = free`
- `SPACE_BETWEEN`：`gap = free / (n - 1)`
- `SPACE_AROUND`：`gap = free / n`，`leading = gap / 2`
- `SPACE_EVENLY`：`gap = free / (n + 1)`，`leading = gap`

### 4.6 行在侧轴排布（含 wrap-reverse）
设行侧轴尺寸总和 `L = Σ lineCross + crossGap*(lineCount-1)`，父侧轴可用 `C`：
- 若 `L > C`：整体侧轴缩放 `crossScale = C / L`
- 行间距：`crossGapSolved = crossGap * crossScale`

行起点：
- `WRAP`：从 `0` 向正方向累计
- `WRAP_REVERSE`：从 `parentCross` 反向累计

### 4.7 子项侧轴对齐公式
设当前行侧轴起点 `lineStart`，行侧轴尺寸 `lineCross`，子项侧轴尺寸 `itemCross`。

- `START`：`itemCrossPos = lineStart`
- `CENTER`：`itemCrossPos = lineStart + (lineCross - itemCross)/2`
- `END`：`itemCrossPos = lineStart + (lineCross - itemCross)`
- `STRETCH`：通过非等比缩放使 `itemCross -> lineCross`
- `BASELINE`（一期仅 `ROW`）：
  - `ascent_i = baselineOffset_i`
  - `descent_i = itemCross_i - baselineOffset_i`
  - `lineAscent = max(ascent_i)`
  - `itemCrossPos = lineStart + (lineAscent - ascent_i)`

备注：一期基线兜底采用“子项底边基线”（`baselineOffset = itemHeight`）。

---

## 5. 日志字段（两类容器统一）
日志格式保持精简，仅输出：
- 布局方式（`layout`）
- 主轴对齐（`main`）
- 侧轴对齐（`cross`）
- `childId`、`parentId`
- `parentXYWH`、`childXYWH`
- `spacescale`、`sizescale`
