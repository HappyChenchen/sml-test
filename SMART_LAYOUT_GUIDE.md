# Smart Layout 代码与公式说明（更新版）

## 1. 目标

这套算法把 Flex 布局转成“约束优化问题”：
- 在父容器固定尺寸下，求每个子项的最终位置和尺寸；
- 当空间不足时，自动缩放（而不是直接溢出）；
- 优先保持组件尺寸尽量大（最大化 `sizeScale`）。

## 2. 输入与输出

### 输入（已知量）
- 父容器：`W_parent`, `H_parent`
- 子项原始尺寸：`w_i0`, `h_i0`
- 子项原始间距（从初始几何提取）：
  - 纵向：`top_i`, `bottom_i`
  - 横向：`left_i`, `right_i`
- 布局方向：`COLUMN` 或 `ROW`
- 对齐策略：
  - 主轴：`mainAxisAlign_`
  - 交叉轴：`crossAxisAlign_`

### 输出（求解结果）
- 全局尺寸缩放：`sizeScale`
- 全局间距缩放：`spaceScale`
- 主轴均分间距变量：`betweenGap`（仅 `SPACE_*` 使用）
- 主轴整体偏移：`mainAxisOffset`
- 每个子项的最终：`x_i`, `y_i`, `w_i`, `h_i`

## 3. 核心变量与约束

### 3.1 尺寸缩放
对每个子项 `i`：
- `w_i = w_i0 * sizeScale`
- `h_i = h_i0 * sizeScale`

边界：
- `0 < sizeScale <= 1`
- `0 <= spaceScale <= 1`
- `betweenGap >= 0`
- `mainAxisOffset >= 0`

### 3.2 几何边界约束（所有模式通用）
- `x_i >= x_parent`
- `y_i >= y_parent`
- `x_i + w_i <= x_parent + W_parent`
- `y_i + h_i <= y_parent + H_parent`

### 3.3 交叉轴对齐约束

#### Column（交叉轴是 X）
- `FLEX_START`：`x_i = x_parent`
- `CENTER`：左右留白相等
- `FLEX_END`：`x_i + w_i = x_parent + W_parent`

#### Row（交叉轴是 Y）
- `FLEX_START`：`y_i = y_parent`
- `CENTER`：上下留白相等
- `FLEX_END`：`y_i + h_i = y_parent + H_parent`

## 4. 主轴公式（重点）

### 4.1 Column 主轴链式公式（Y 方向）

第一个子项：
- 非 `SPACE_*`：
  - `y_0 = y_parent + top_0 * spaceScale + mainAxisOffset`
- `SPACE_*`：
  - `y_0 = y_parent + mainAxisOffset`

后续子项（`i > 0`）：
- 非 `SPACE_*`：
  - `y_i = y_{i-1} + h_{i-1} + bottom_{i-1} * spaceScale`
- `SPACE_*`：
  - `y_i = y_{i-1} + h_{i-1} + betweenGap`

### 4.2 Row 主轴链式公式（X 方向）

第一个子项：
- 非 `SPACE_*`：
  - `x_0 = x_parent + left_0 * spaceScale + mainAxisOffset`
- `SPACE_*`：
  - `x_0 = x_parent + mainAxisOffset`

后续子项（`i > 0`）：
- 非 `SPACE_*`：
  - `x_i = x_{i-1} + w_{i-1} + right_{i-1} * spaceScale`
- `SPACE_*`：
  - `x_i = x_{i-1} + w_{i-1} + betweenGap`

## 5. 主轴对齐公式（含三种 SPACE）

设：
- `startPos` = 内容块起点（首子项主轴起点）
- `endPos` = 内容块终点（尾子项主轴终点）
- `parentStart` / `parentEnd` = 父容器主轴起终点

### `FLEX_START`
- `startPos - parentStart = headBase * spaceScale`
- `mainAxisOffset = 0`
- `betweenGap = 0`

### `CENTER`
- `startPos - parentStart = parentEnd - endPos`
- `betweenGap = 0`

### `FLEX_END`
- `parentEnd - endPos = tailBase * spaceScale`
- `betweenGap = 0`

### `SPACE_BETWEEN`
- `startPos = parentStart`
- `endPos = parentEnd`
- 中间间距统一由 `betweenGap` 提供

### `SPACE_AROUND`
- `startPos - parentStart = betweenGap / 2`
- `parentEnd - endPos = betweenGap / 2`

### `SPACE_EVENLY`
- `startPos - parentStart = betweenGap`
- `parentEnd - endPos = betweenGap`

## 6. 优化目标

当前目标函数：
- `maximize(spaceScale)`（一级目标）
- `maximize(sizeScale)`（二级目标）

含义：
- 在满足所有约束时，优先保持间距按统一比例缩放；
- 在 `spaceScale` 最优前提下，再尽量让子项尺寸更大；
- `betweenGap` 作为 `SPACE_*` 模式下的可行性调节量。

## 7. 代码对应关系

- 主流程：`PerformSmartLayout`
- 基础约束：`AddDefaultConstraints`
- 交叉轴约束：`AddCrossAxisAlignmentConstraints`
- 主轴对齐约束：`AddMainAxisAlignmentConstraints`
- 方向约束组装：`addColumnLayout` / `addRowLayout`

## 8. 已落地优化清单

1. `mainAxisAlign_` 完整支持：
- `FLEX_START / CENTER / FLEX_END / SPACE_BETWEEN / SPACE_AROUND / SPACE_EVENLY`

2. Row 模式补齐 `spaceScale` 上下界：
- `0 <= spaceScale <= 1`

3. 空间充足短路：
- `start/start` 且空间足够时跳过求解器
- 短路返回前会将子项 `transformScale` 复位到 `1.0`，避免沿用上次缩小结果

4. 间距字段一致化：
- Column 使用 `top/bottom`
- Row 使用 `left/right`

5. 交叉轴默认回退值：
- `crossAxisAlign_ = GetCrossAxisAlignValue(FlexAlign::FLEX_START)`

6. 大量中文注释：
- 约束、变量、阶段、回写流程都已补充

7. 调试日志：
- 在短路分支与求解回写分支输出子项位置日志，便于按元素 id 排查偏移问题
- 求解回写日志包含 `initOffset/finalOffset/delta/initSize/finalSize`
- 主轴排查字段：`parentMainSize/initTailGap/finalTailGap`
- 新增子项结构化字段：
  - `finalXYWH`：子项最终 `x/y/width/height`
  - `spaceFromPrevBottom`：相对前一个子项底部（Row 时为相对前一个右侧）的主轴间距
  - `firstTopSpaceY`：首个子项相对父容器顶部的 `y`
  - `sizeScale/spaceScale` 与 `scaleDelta`
  - `initSize/finalSize/sizeDelta/childScaleXY`（对比初始宽高与缩放变化）

8. 放大回涨稳定性修复：
- 在进入求解前，先把所有子项 `transformScale` 复位为 `1.0` 并重新布局
- 避免“上一次缩小后的尺寸”被当作下一次求解基准，导致父容器放大后子项仍不回涨

9. 首尾间距改为父容器相对坐标：
- `top/left` 不再直接用子项绝对偏移，而是 `childOffset - parentOffset`
- `bottom/right` 也按父容器相对坐标计算，避免 `initTailGap` 在父组件位移后出现负值假象

## 9. 一个简短例子（SPACE_EVENLY）

父容器主轴长度记为 `L`，三个子项主轴总长度为 `S`：
- 内容外共有 4 段等距（前、两中间、后）
- 每段就是 `betweenGap`
- 所以有：`S + 4 * betweenGap = L`

求解器会自动求出 `betweenGap`，同时满足边界与尺寸缩放约束。

## 10. 手算示例（一步一步代入）

### 10.1 Row + SPACE_EVENLY（完整）

已知：
- 父容器宽度：`W_parent = 500`
- 3 个子项原始宽度：`w_0 = 80`, `w_1 = 120`, `w_2 = 100`
- 假设空间足够，求解后 `sizeScale = 1`（即不缩放）
- `mainAxisAlign_ = SPACE_EVENLY`

第 1 步：先算内容总宽 `S`
- `S = 80 + 120 + 100 = 300`

第 2 步：`SPACE_EVENLY` 的间距关系
- 3 个子项会把主轴分成 4 段等距（前、两中间、后）
- 设每段都是 `betweenGap = g`
- 方程：`S + 4g = W_parent`
- 代入：`300 + 4g = 500`
- 解得：`g = 50`

第 3 步：按链式公式算每个 `x`
- 首项：`x_0 = x_parent + g`
  - 若 `x_parent = 0`，则 `x_0 = 50`
- 第二项：`x_1 = x_0 + w_0 + g = 50 + 80 + 50 = 180`
- 第三项：`x_2 = x_1 + w_1 + g = 180 + 120 + 50 = 350`

第 4 步：验算尾部间距
- 第三项右边界：`x_2 + w_2 = 350 + 100 = 450`
- 右侧剩余：`500 - 450 = 50 = g`
- 与 `SPACE_EVENLY` 一致，成立。

最终输出（主轴）：
- `betweenGap = 50`
- `x_0 = 50`, `x_1 = 180`, `x_2 = 350`

### 10.2 Column + SPACE_AROUND（简版）

已知：
- 父容器高度：`H_parent = 420`
- 两个子项高度：`h_0 = 100`, `h_1 = 140`
- `sizeScale = 1`
- `mainAxisAlign_ = SPACE_AROUND`

内容总高：
- `S = 100 + 140 = 240`

`SPACE_AROUND` 规则：
- 两端留白是内部间距的一半
- 设内部间距为 `g = betweenGap`，两端各 `g/2`
- 方程：`S + g + g/2 + g/2 = H_parent`，即 `S + 2g = H_parent`
- `240 + 2g = 420`，解得 `g = 90`
- 所以顶部留白和底部留白都为 `45`

主轴位置：
- `y_0 = y_parent + 45`
- `y_1 = y_0 + h_0 + 90`
