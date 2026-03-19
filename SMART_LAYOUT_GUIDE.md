# Smart Layout Guide（当前 smart_layout 实现）

## 1. 范围
本文仅描述当前目录 `smart_layout/` 下的实现：
- `smart_layout/smart_layout_algorithm.cpp`
- `smart_layout/smart_layout_algorithm.h`
- `smart_layout/smart_layout_node.h`
- `smart_layout/localsmt/localsmt.*`（与求解行为相关）

## 2. 核心结构

### 2.1 LayoutContext
- `engine`: `localsmt::Engine`
- `layoutType`: `COLUMN / ROW`
- `mainAxisAlign_`
- `horizontalAlign_` / `verticalAlign_`
- `parentSize`
- `childrenLayoutNode`
- `root`

### 2.2 SmartLayoutNode
- 位置变量：`name_x`, `name_y`
- 尺寸变量：`name_w`, `name_h`
- 缩放变量：`namesizeScale`, `namespaceScale`
- 间距缓存：`edgesSpace_ {top, bottom, left, right}`

变量域（当前实现）：
- `x/y/w/h` 在 `[0, 10000]`
- `sizeScale` 在 `[0.3, 1]`
- `spaceScale` 在 `[0, 1]`

## 3. 布局流程
入口：
- `SmartColumnLayout(...)`
- `SmartRowLayout(...)`

统一进入 `PerformSmartLayout(...)`：
1. `InitializeLayoutContext`
2. `ProcessLayoutChildren`
3. `ApplyLayoutConstraints`
4. `SolveLayout`
5. `ApplyLayoutResults`

说明：当前为单轮求解，不做多轮回退。

## 4. 子节点预处理（ProcessLayoutChildren）
- 读取首项与相邻项间距
- `COLUMN` 使用 `top/bottom`
- `ROW` 使用 `left/right`
- 末项记录尾部剩余间距
- `Blank`：
  - `COLUMN`：高度并入 `bottom`，高度置 0
  - `ROW`：宽度并入 `right`，宽度置 0

## 5. 约束规则

### 5.1 默认约束（父子通用）
- 非负约束
- 子节点必须在父节点内
- 父节点位置固定到 `(0,0)`

### 5.2 主轴/侧轴溢出判定

#### COLUMN
- 主轴（Y）：`sum(childHeight)` 对比 `parentHeight`
- 侧轴（X）：`max(childWidth)` 对比 `parentWidth`
- 计算两个缩放候选：
  - `mainAxisScale`（主轴超出时 `<1`）
  - `crossAxisScale`（侧轴超出时 `<1`）
- 取 `finalScale = min(mainAxisScale, crossAxisScale)`
- 约束：`sizeScale == finalScale`

#### ROW
- 主轴（X）：`sum(childWidth)` 对比 `parentWidth`
- 侧轴（Y）：`max(childHeight)` 对比 `parentHeight`
- 同样取 `finalScale = min(mainAxisScale, crossAxisScale)`
- 约束：`sizeScale == finalScale`

### 5.3 子项尺寸
- `child.w = child.w0 * sizeScale`
- `child.h = child.h0 * sizeScale`

### 5.4 主轴/侧轴对齐方式

#### COLUMN（纵向）
- 主轴：`Y` 轴（链式从上到下）
- 侧轴：`X` 轴
- 侧轴枚举：`HorizontalAlign::START / CENTER / END`

#### ROW（横向）
- 主轴：`X` 轴（链式从左到右）
- 侧轴：`Y` 轴
- 侧轴枚举：`VerticalAlign::TOP / CENTER / BOTTOM`

说明：`mainAxisAlign_` 当前已读取，但主轴仍由链式规则和末尾约束驱动。

### 5.5 主轴链式规则
- `COLUMN`：按 `top/bottom` 链式累加
- `ROW`：按 `left/right` 链式累加

末项约束（当前为严格贴边窗口）：
- `COLUMN`: `last.y + last.h + 1 >= parentEndY` 且 `last.y + last.h <= parentEndY`
- `ROW`: `last.x + last.w + right*spaceScale + 1 >= parentEndX` 且 `... <= parentEndX`

## 6. 求解与回写

### 6.1 SolveLayout
- 打印约束
- 调用 `engine.solve()`
- 成功后同步变量并打印结果

### 6.2 ApplyLayoutResults
- 同步每个 child 的解
- 计算偏移（含 margin 修正）
- 写回 `GeometryNode`
- 应用 transform scale
- 标记脏并触发布局

## 7. 已知行为
- `localsmt` 返回 `false` 不一定是严格数学 UNSAT，也可能是搜索未命中。
- `sizeScale` 下界为 `0.3`，当理论所需缩放低于 `0.3` 时会直接无解。
