# localsmt 原实现思路说明（基于当前 smart_layout 代码）

本文描述的是当前 `smart_layout/smart_layout_algorithm.cpp` + `smart_layout/smart_layout_node.h` 的实现思路，不是理想版设计。

## 1. 总体目标

- 对 `Linear` 容器做约束求解布局（`COLUMN` / `ROW`）。
- 使用 `localsmt::Engine` 建变量与约束，求解后回写子组件坐标，并统一做 transform scale。

入口：
- `SmartColumnLayout(...)`
- `SmartRowLayout(...)`

主流程函数：
- `PerformSmartLayout(...)`

## 2. 求解模型

### 2.1 变量（每个节点）

来自 `SmartLayoutNode`：

- 位置：
  - `x` / `y`
- 尺寸：
  - `w` / `h`
- 缩放：
  - `spaceScale`（间距缩放）
  - `sizeScale`（尺寸缩放）

根节点（`root`）固定：
- `root.w == parentWidth`
- `root.h == parentHeight`

### 2.2 边距/间距数据（space_）

对子节点会提取：
- `top/left`：首元素到父容器起点的距离
- `bottom/right`：当前元素到下一个兄弟的间距
- 末元素额外计算到父容器终点的剩余距离（`bottom/right`）

`Blank` 特殊处理（当前实现）：
- 将 `Blank` 高度置 `0`
- 并把 `Blank` 原高度并入 `space_.bottom`

## 3. 约束构建逻辑

### 3.1 默认硬约束（所有方向共用）

`AddDefaultConstraints(...)` 为父子建立：

- 非负：
  - `parent/child` 的 `x,y,w,h >= 0`
- 子项在父内：
  - `child.x >= parent.x`
  - `child.y >= parent.y`
  - `child.x + child.w <= parent.x + parent.w`
  - `child.y + child.h <= parent.y + parent.h`

### 3.2 Column 方向（`AddColumnConstraints`）

1. 先算 `sumOfAllChildHeight`
2. 主轴间距缩放：
   - 若 `sumHeight > parentHeight`：`spaceScale = parentHeight / sumHeight`
   - 否则：`spaceScale = 1`
   - 且 `spaceScale ∈ [0,1]`
3. 尺寸约束（按当前代码字面）：
   - 若溢出：  
     `child.w * sumHeight == child.w * parentHeight`  
     `child.h * sumHeight == child.h * parentHeight`
   - 未溢出：`child.w == child.w`、`child.h == child.h`（恒真）
4. 侧轴对齐（X 方向）：
   - `FLEX_START`: `child.x == parent.x`
   - `FLEX_END`: `child.x + child.w == parent.x + parent.w`
   - `CENTER`: `2*child.x + child.w == parent.w`（当前代码写法）
5. 主轴链：
   - 首元素：`child0.y == parent.y + child0.space.top * spaceScale`
   - 后续：`child_i.y == prev.y + prev.h + prev.space.bottom * spaceScale`
6. 末元素贴底（近似）：
   - `last.y + last.h + 1 >= parent.y + parent.h`
   - `last.y + last.h <= parent.y + parent.h`

### 3.3 Row 方向（`AddRowConstraints`）

与 Column 对称：

1. 计算 `sumOfAllChildWidth`
2. `spaceScale`：
   - 溢出：`spaceScale = parentWidth / sumWidth`
   - 否则：`1`
   - 且 `[0,1]`
3. 尺寸约束（与 Column 同模式）：
   - 溢出时用 `child.w * sumWidth == child.w * parentWidth` 等式
4. 侧轴对齐（Y 方向）：
   - `FLEX_START`: `child.y == parent.y`
   - `FLEX_END`: `child.y + child.h == parent.y + parent.h`
   - `CENTER`: `2*child.y + child.h == parent.h`（当前代码写法）
5. 主轴链：
   - 首元素：`child0.x == parent.x + child0.space.left * spaceScale`
   - 后续：`child_i.x == prev.x + prev.w + prev.space.right * spaceScale`
6. 末元素贴右（近似）：
   - `last.x + last.w + 1 >= parent.x + parent.w`
   - `last.x + last.w <= parent.x + parent.w`

## 4. 求解与回写

### 4.1 求解

- 调用 `context.engine.solve()`
- 失败：直接返回，不做降级求解
- 成功：`SyncData(...)` 同步变量值并打印变量日志

### 4.2 回写

- 把求解后的 `(x,y)` 回写到 `GeometryNode`。
- 统一调用 `ItemScaling(..., root.sizeScale)` 做 transform 缩放。
- 调用 `MarkDirtyNode(PROPERTY_UPDATE_LAYOUT)` + `child->Layout()`。

注：当前代码里 margin 回写逻辑分支判断写反（`GetGeometryNode()==nullptr` 时反而解引用），实际会有空指针风险。

## 5. 这个版本的核心思想（一句话）

- 先用主轴 `spaceScale` 压间距；
- 再用一组代数等式“尝试”限制尺寸；
- 同时维持子项不越界与末元素贴底/贴右；
- 侧轴按 start/end/center 对齐。

## 6. 排查时最容易踩的点

- 溢出时尺寸等式是乘法约束，行为可能与“按比例缩放尺寸”预期不一致。
- `CENTER` 公式写法是 `2*x + w == parent.w`（或 `2*y + h == parent.h`），不是标准的 `2*(x-parent.x)+w==parent.w`。
- 回写阶段有潜在空指针分支问题（margin 分支）。
- 失败时没有二阶段 fallback。

