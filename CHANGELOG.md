# 变更日志

用于记录项目关键功能、关键改动和关键代码位置（含历史补录）。

## 2026-03-17

### 新增
- 初始化仓库基础文件：
  - `.gitignore`
  - `README.md`
- 新增智能布局求解核心：
  - `smart_layout_algorithm.cpp`
  - `smart_layout_algorithm.h`
  - `smart_layout_node.h`
- 新增算法说明文档：
  - `SMART_LAYOUT_GUIDE.md`
- 新增布局调试日志（短路分支 + 求解分支）：
  - 元素位置与尺寸
  - 初始值/最终值/偏移量
  - 主轴尾部间距

### 修改
- 完善主轴与交叉轴对齐约束逻辑（含 `FLEX_START/CENTER/FLEX_END/SPACE_*`）。
- `spaceScale` 目标调整为二级优化（在 `sizeScale` 最优前提下尽量保留间距比例）。
- 交叉轴默认回退值调整为 `FLEX_START`。
- `start/start` 且空间充足时，短路返回前恢复子项缩放到 `1.0`。
- 持续补充日志与文档，便于定位“贴底/间距变化/缩放残留”等问题。

### 修复
- 修复 `space == 0` 相关场景问题。
- 增强“末尾元素贴底”问题排查能力（通过日志增加主轴尾部间距字段）。

### 关键代码
- [smart_layout_algorithm.cpp](/d:/0_Work/sml-test/smart_layout_algorithm.cpp)
- [smart_layout_algorithm.h](/d:/0_Work/sml-test/smart_layout_algorithm.h)
- [smart_layout_node.h](/d:/0_Work/sml-test/smart_layout_node.h)
- [SMART_LAYOUT_GUIDE.md](/d:/0_Work/sml-test/SMART_LAYOUT_GUIDE.md)

## 2026-03-17（本次工作区改动）

### 修改
- `FLEX_END` 主轴约束由“强制贴尾”改为“尾部间距按 `spaceScale` 同比缩放”：
  - 原：`endPos = parentEnd`
  - 现：`parentEnd - endPos = tailBase * spaceScale`
- 目标优先级调整为“先间距、后尺寸”：
  - 先 `maximize(spaceScale)`
  - 再 `maximize(sizeScale)`

### 影响文件
- [smart_layout_algorithm.cpp](/d:/0_Work/sml-test/smart_layout_algorithm.cpp)
- [SMART_LAYOUT_GUIDE.md](/d:/0_Work/sml-test/SMART_LAYOUT_GUIDE.md)

## 2026-03-17（本次追加：flex_start 同步 + 放大回涨修复）
### 修改
- 主轴 `FLEX_START` 对齐同步改为“首部间距按 `spaceScale` 同比缩放”：
  - `startPos - parentStart = headBase * spaceScale`
- 求解前统一先将子元素 `transformScale` 复位为 `1.0` 并 `Layout()`，防止上一轮缩小结果成为下一轮基准。
- `edgesSpace` 的首尾间距计算改为相对父容器坐标（`childOffset - parentOffset`），避免父组件偏移后尾距日志失真。
- `solve_result` 和短路日志里的主轴偏移/尾距计算同步切换为父容器相对坐标。

### 影响文件
- [smart_layout_algorithm.cpp](/d:/0_Work/sml-test/smart_layout_algorithm.cpp)
- [SMART_LAYOUT_GUIDE.md](/d:/0_Work/sml-test/SMART_LAYOUT_GUIDE.md)
- [CHANGELOG.md](/d:/0_Work/sml-test/CHANGELOG.md)
- [gitlog.md](/d:/0_Work/sml-test/gitlog.md)

## 2026-03-17（本次追加：日志字段增强）
### 修改
- `solve_result` 日志改为输出子容器核心指标：
  - `finalXYWH`（最终 x/y/宽/高）
  - `spaceFromPrevBottom`（相对前一个子容器底部的主轴间距；首个元素按父顶部 y）
  - `firstTopSpaceY`（首元素相对父顶部）
  - `sizeScale/spaceScale` 与 `scaleDelta`
  - `initSize/finalSize/sizeDelta/childScaleXY`（相对初始宽高和缩放变化）
- 保留原有 `initOffset/modelOffset/finalOffset/delta` 与 `initTailGap/finalTailGap`，便于兼容原排查流程。

### 影响文件
- [smart_layout_algorithm.cpp](/d:/0_Work/sml-test/smart_layout_algorithm.cpp)
- [SMART_LAYOUT_GUIDE.md](/d:/0_Work/sml-test/SMART_LAYOUT_GUIDE.md)
- [CHANGELOG.md](/d:/0_Work/sml-test/CHANGELOG.md)
- [gitlog.md](/d:/0_Work/sml-test/gitlog.md)

## 2026-03-17（本次修复：日志编译报错）
### 修复
- 将 `solve_result` 超长单条日志拆分为 3 条，避免日志宏参数过多导致编译失败：
  - `solve_result_basic`
  - `solve_result_scale`
  - `solve_result_offset`
- 保留你要求的所有字段，只是分批输出，便于阅读和排查。

### 影响文件
- [smart_layout_algorithm.cpp](/d:/0_Work/sml-test/smart_layout_algorithm.cpp)
- [SMART_LAYOUT_GUIDE.md](/d:/0_Work/sml-test/SMART_LAYOUT_GUIDE.md)
- [CHANGELOG.md](/d:/0_Work/sml-test/CHANGELOG.md)
- [gitlog.md](/d:/0_Work/sml-test/gitlog.md)

## 2026-03-17（本次追加：日志公共参数抽取）
### 修改
- `solve_result` 日志继续拆分并抽取公共参数，减少重复：
  - 新增公共上下文 `logCtx`（`type/child/prevChild/isFirst`）
  - 日志拆分为 `solve_result_common / solve_result_geom / solve_result_scale / solve_result_offset`
- 保留原有全部排查字段，仅调整组织方式，降低单条日志复杂度。

### 影响文件
- [smart_layout_algorithm.cpp](/d:/0_Work/sml-test/smart_layout_algorithm.cpp)
- [SMART_LAYOUT_GUIDE.md](/d:/0_Work/sml-test/SMART_LAYOUT_GUIDE.md)
- [CHANGELOG.md](/d:/0_Work/sml-test/CHANGELOG.md)
- [gitlog.md](/d:/0_Work/sml-test/gitlog.md)

## 2026-03-17（本次追加：日志改为最简字段）
### 修改
- `solve_result` 日志改为单条精简输出 `smart_layout compact`，仅保留：
  - 父容器 `parentXYWH`
  - 子容器相对父容器 `childRelXY` 与自身 `childWH`
  - `spaceScale`、`sizeScale`
- 移除旧的多条细分日志与不再使用的中间计算，降低日志噪音。

### 影响文件
- [smart_layout_algorithm.cpp](/d:/0_Work/sml-test/smart_layout_algorithm.cpp)
- [SMART_LAYOUT_GUIDE.md](/d:/0_Work/sml-test/SMART_LAYOUT_GUIDE.md)
- [CHANGELOG.md](/d:/0_Work/sml-test/CHANGELOG.md)
- [gitlog.md](/d:/0_Work/sml-test/gitlog.md)

## 2026-03-17（本次追加：侧轴对齐枚举重构 + 显式 else）
### 修改
- `AddCrossAxisAlignmentConstraints` 改为显式 `if (...) { ... } else { ... }` 结构，Row 逻辑不再依赖早返回的隐式分支。
- 侧轴对齐改为方向专属枚举：
  - `COLUMN`：`HorizontalAlign::START/CENTER/END`
  - `ROW`：`VerticalAlign::TOP/CENTER/BOTTOM`
- 在 `PerformSmartLayout` 中将 `GetCrossAxisAlignValue(FlexAlign::FLEX_START)` 结果映射到对应枚举，并用于短路判定 `isCrossStart`。

### 影响文件
- [smart_layout_algorithm.h](/d:/0_Work/sml-test/smart_layout_algorithm.h)
- [smart_layout_algorithm.cpp](/d:/0_Work/sml-test/smart_layout_algorithm.cpp)
- [SMART_LAYOUT_GUIDE.md](/d:/0_Work/sml-test/SMART_LAYOUT_GUIDE.md)
- [CHANGELOG.md](/d:/0_Work/sml-test/CHANGELOG.md)

## 2026-03-17（本次修改：交叉轴默认值同步为 CENTER）
### 修改
- 对齐成员默认值统一为居中：
  - `horizontalAlign_ = HorizontalAlign::CENTER`
  - `verticalAlign_ = VerticalAlign::CENTER`
- 交叉轴配置读取默认值改为 `CENTER`：
  - `GetCrossAxisAlignValue(FlexAlign::CENTER)`
- 枚举映射兜底同步为居中：
  - `ToHorizontalAlign` 默认返回 `HorizontalAlign::CENTER`
  - `ToVerticalAlign` 默认返回 `VerticalAlign::CENTER`
- 同步更新 `SMART_LAYOUT_GUIDE.md` 的默认回退说明。

### 影响文件
- [smart_layout_algorithm.h](/d:/0_Work/sml-test/smart_layout_algorithm.h)
- [smart_layout_algorithm.cpp](/d:/0_Work/sml-test/smart_layout_algorithm.cpp)
- [SMART_LAYOUT_GUIDE.md](/d:/0_Work/sml-test/SMART_LAYOUT_GUIDE.md)
- [CHANGELOG.md](/d:/0_Work/sml-test/CHANGELOG.md)
- [gitlog.md](/d:/0_Work/sml-test/gitlog.md)

## 2026-03-17（本次修改：VerticalAlign 枚举命名调整）
### 修改
- `VerticalAlign` 命名从 `START/CENTER/END` 调整为 `TOP/CENTER/BOTTOM`。
- 代码侧同步更新：
  - `ToVerticalAlign` 映射：`FLEX_START -> TOP`，`FLEX_END -> BOTTOM`
  - Row 交叉轴约束判断改为 `TOP/CENTER/BOTTOM`
  - `start/start` 短路判定中的 Row 分支改为 `verticalAlign_ == TOP`
- 文档侧同步更新 `SMART_LAYOUT_GUIDE.md` 中的 Row 交叉轴枚举说明。

### 影响文件
- [smart_layout_algorithm.h](/d:/0_Work/sml-test/smart_layout_algorithm.h)
- [smart_layout_algorithm.cpp](/d:/0_Work/sml-test/smart_layout_algorithm.cpp)
- [SMART_LAYOUT_GUIDE.md](/d:/0_Work/sml-test/SMART_LAYOUT_GUIDE.md)
- [CHANGELOG.md](/d:/0_Work/sml-test/CHANGELOG.md)
