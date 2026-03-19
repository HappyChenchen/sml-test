# Smart Layout Guide（仅当前 smart_layout 目录）

## 1. 文档范围
本文只描述当前目录 `smart_layout/` 的线性布局解算实现：
- `smart_layout/smart_layout_algorithm.cpp`
- `smart_layout/smart_layout_algorithm.h`
- `smart_layout/smart_layout_node.h`

不包含 `z3/`、`z3-convert-smt/`、历史版本或理想化设计。

## 2. 核心数据结构

### 2.1 LayoutContext
来自 `smart_layout_algorithm.h`：
- `engine`：`localsmt::Engine`
- `layoutType`：`COLUMN / ROW`
- `mainAxisAlign_`：从 `FlexLayoutProperty` 读取
- `horizontalAlign_` / `verticalAlign_`：由 `crossAxisAlign(FlexAlign)` 映射得到
- `parentSize`
- `childrenLayoutNode`
- `root`

### 2.2 SmartLayoutNode
来自 `smart_layout_node.h`，每个节点包含：
- 位置变量：`name_x`, `name_y`
- 尺寸变量：`name_w`, `name_h`
- 缩放变量：`namespaceScale`, `namesizeScale`
- 边距缓存：`edgesSpace_ {top, bottom, left, right}`
- 额外变量：`namespace`

> 说明：当前变量命名使用下划线形式（如 `_x/_y/_w/_h`），缩放和 space 变量无分隔符（如 `rootsizeScale`）。

## 3. 执行流程

### 3.1 入口
- `SmartColumnLayout(...)` -> `PerformSmartLayout(..., COLUMN)`
- `SmartRowLayout(...)` -> `PerformSmartLayout(..., ROW)`

### 3.2 PerformSmartLayout
固定流程：
1. `InitializeLayoutContext`
2. `ProcessLayoutChildren`
3. `ApplyLayoutConstraints`
4. `SolveLayout`
5. `ApplyLayoutResults`

## 4. 初始化与建模

### 4.1 InitializeLayoutContext
- 读取 children；为空则返回 `false`
- 读取 `layoutType`
- 从 `FlexLayoutProperty` 读取：
  - `mainAxisAlign_ = GetMainAxisAlignValue(FLEX_START)`
  - `crossAxisAlign = GetCrossAxisAlignValue(CENTER)`
  - 映射：
    - `FLEX_START -> horizontalAlign_=START, verticalAlign_=TOP`
    - `CENTER -> horizontalAlign_=CENTER, verticalAlign_=CENTER`
    - `FLEX_END -> horizontalAlign_=END, verticalAlign_=BOTTOM`
- 读取父容器 `parentSize`（来自 `GeometryNode`）
- 创建 `root` 并固定：
  - `root.w == parentWidth`
  - `root.h == parentHeight`

### 4.2 ProcessLayoutChildren
逐个 child 创建 `SmartLayoutNode`，并提取：
- 初始 offset 与 size
- 相邻间距 `CalculateSpaceBetween(child, nextChild, layoutType)`
- `Blank` 特判：`height -> 0`，原高度并入 `edgesSpace_.bottom`

边距/间距写入规则（按当前代码）：
- 首元素：
  - `COLUMN` 取 `top`
  - `ROW` 取 `left`
- 非末元素：
  - 间距先写入 `edgesSpace_.bottom`
  - 非首元素再把上一元素的 `bottom/right` 复制到本元素 `top/left`
- 末元素：
  - 仅 `COLUMN` 写 `bottom` 到父容器底边剩余距离

最后 `root->setChildren(childrenLayoutNode)`。

## 5. 约束构建

### 5.1 默认约束 AddDefaultConstraints
对每个父子节点添加：
- 非负：`child.w/h/x/y >= 0`
- 父节点锚定：`parent.x == 0`, `parent.y == 0`
- 父节点与空间变量下界：`parent.w/h >= 0`, `parent.spaceScale >= 0`, `parent.space >= 0`
- 子节点在父节点内：
  - `child.x >= parent.x`
  - `child.y >= parent.y`
  - `child.x + child.w <= parent.x + parent.w`
  - `child.y + child.h <= parent.y + parent.h`

### 5.2 Column 约束 addColumnLayout
1. 计算 `sumOfAllChildHeight`
2. `sizeScale`：
   - 若 `sumHeight > parent.h`：`sizeScale = parent.h / sumHeight`
   - 否则 `sizeScale = 1`
3. `spaceScale` 仅加范围约束：`0 <= spaceScale <= 1`
4. 子项尺寸：
   - 溢出时：
     - `child.w * sumHeight == child.w0 * parent.h`
     - `child.h * sumHeight == child.h0 * parent.h`
   - 未溢出时：
     - `child.w == child.w0`
     - `child.h == child.h0`
5. 侧轴（X）：
   - `FLEX_START`：`child.x == parent.x`
   - 其他（当前统一同一公式）：`child` 在父内等距居中
6. 主轴（Y）链：
   - 首元素：`y0 = parent.y + top0 * spaceScale`
   - 后续：`yi = prev.y + prev.h + prev.bottom * spaceScale`
7. 末元素贴底近似：
   - `last.y + last.h + 1 >= parent.y + parent.h`
   - `last.y + last.h <= parent.y + parent.h`

### 5.3 Row 约束 addRowLayout
1. 使用 `sumOfAllChildHeight` 与 `parent.h` 计算 `sizeScale`
2. 子项尺寸统一：
   - `child.w = child.w0 * sizeScale`
   - `child.h = child.h0 * sizeScale`
3. 侧轴（Y）：
   - `FLEX_START`：顶部对齐
   - `CENTER`：居中对齐
   - `FLEX_END`：底部对齐
4. 主轴（X）链：
   - 首元素：`x0 = parent.x`
   - 后续：`xi = prev.x + prev.w + spaceScale`
5. 末元素贴右：
   - `last.x + last.w == parent.x + parent.w`

## 6. 求解与回写

### 6.1 SolveLayout
- 打印全部约束（日志）
- 调用 `engine.solve()`
- 成功后：
  - `root->syncData(engine)`
  - 打印所有变量结果

### 6.2 ApplyLayoutResults
按 children 顺序写回：
1. `childrenLayoutNode[index]->syncData(engine)`
2. 读取 margin 偏移：
   - `COLUMN` 用 `margin.left`
   - `ROW` 用 `margin.top`
3. 写 `GeometryNode->SetFrameOffset(...)`
4. 应用 transform scale：`ItermScale(child, root.sizeScale)`
5. `MarkDirtyNode(PROPERTY_UPDATE_LAYOUT)` + `child->Layout()`

## 7. 关键实现特征（当前代码语义）
- 主入口为线性两方向：`COLUMN/ROW`
- 求解器为 `localsmt::Engine`
- `root` 固定尺寸，位置在默认约束里固定为 `(0,0)`
- `spaceScale` 与 `sizeScale` 并存，但二者在 Column/Row 中的驱动方式不完全一致
- 回写采用“求解坐标 + margin 修正 + transform scale”策略
