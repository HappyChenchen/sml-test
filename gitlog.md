# 提交记录补录

来源：`git log` 历史。  
说明：按时间从早到晚排列；“影响文件”使用无序列表。

## 1) 7eb001e | 2026-03-17 | ChenYun | Initial commit
影响文件：
- `.gitignore`
- `README.md`

变更说明：
- 初始化仓库基础结构。

## 2) 0f9f697 | 2026-03-17 | happychenchen | init
影响文件：
- `smart_layout_algorithm.cpp`
- `smart_layout_algorithm.h`
- `smart_layout_node.h`

变更说明：
- 首次引入智能布局求解核心实现。

## 3) 22dfed6 | 2026-03-17 | happychenchen | add desc
影响文件：
- `SMART_LAYOUT_GUIDE.md`
- `smart_layout_algorithm.cpp`
- `smart_layout_algorithm.h`
- `smart_layout_node.h`

变更说明：
- 补充算法说明文档并重构/整理核心代码。

## 4) 6dc39ba | 2026-03-17 | happychenchen | fix space==0 error
影响文件：
- `smart_layout_algorithm.cpp`

变更说明：
- 修复 `space==0` 相关问题。

## 5) 765b336 | 2026-03-17 | happychenchen | add log
影响文件：
- `SMART_LAYOUT_GUIDE.md`
- `smart_layout_algorithm.cpp`

变更说明：
- 增加调试日志并同步文档。

## 6) 3561e4d | 2026-03-17 | happychenchen | add log
影响文件：
- `SMART_LAYOUT_GUIDE.md`
- `smart_layout_algorithm.cpp`

变更说明：
- 扩展日志字段并同步文档。

## 7) d1f48fb | 2026-03-17 | happychenchen | add log
影响文件：
- `SMART_LAYOUT_GUIDE.md`
- `smart_layout_algorithm.cpp`

变更说明：
- 继续补充日志与文档说明。

## 8) 工作区未提交 | 2026-03-17 | 本地修改 | flex 对齐与目标优先级调整
影响文件：
- `smart_layout_algorithm.cpp`
- `SMART_LAYOUT_GUIDE.md`
- `CHANGELOG.md`
- `gitlog.md`

变更说明：
- `FLEX_END` 由贴尾改为尾部间距同比缩放（`tailBase * spaceScale`）。
- 目标顺序改为先 `spaceScale` 再 `sizeScale`，优先保证间距同比缩放。
- 同步更新文档与日志记录。

## 9) 工作区未提交 | 2026-03-17 | 本地修改 | flex_start 间距同比 + 子元素放大回涨修复
影响文件：
- `smart_layout_algorithm.cpp`
- `SMART_LAYOUT_GUIDE.md`
- `CHANGELOG.md`
- `gitlog.md`

变更说明：
- `FLEX_START` 主轴改为首部间距按 `spaceScale` 同比缩放，不再默认贴边。
- 求解前先重置子元素 `transformScale=1.0` 并重新布局，避免父组件放大后子元素仍停留在上次缩小尺寸。
- 首尾间距与 `tailGap` 日志改为父容器相对坐标计算，修复父偏移场景下 `initTailGap` 负值误判。

## 10) 工作区未提交 | 2026-03-17 | 本地修改 | solve_result 日志增强（子项相对间距与缩放变化）
影响文件：
- `smart_layout_algorithm.cpp`
- `SMART_LAYOUT_GUIDE.md`
- `CHANGELOG.md`
- `gitlog.md`

变更说明：
- `solve_result` 新增 `finalXYWH`、`spaceFromPrevBottom`、`firstTopSpaceY` 字段。
- 新增 `sizeScale/spaceScale` 与 `scaleDelta`，并输出 `initSize/finalSize/sizeDelta/childScaleXY`。
- 保留原偏移与 tailGap 字段，兼容现有日志分析流程。

## 11) 工作区未提交 | 2026-03-17 | 本地修复 | 拆分 solve_result 日志，修复编译报错
影响文件：
- `smart_layout_algorithm.cpp`
- `SMART_LAYOUT_GUIDE.md`
- `CHANGELOG.md`
- `gitlog.md`

变更说明：
- 原 `solve_result` 单条日志参数过多，拆分为 `solve_result_basic/solve_result_scale/solve_result_offset` 三条。
- 保留 `finalXYWH`、前后间距、scale 与初始对比等字段，不改变排查信息完整性。

## 12) 工作区未提交 | 2026-03-17 | 本地修改 | solve_result 日志抽公共参数并进一步拆分
影响文件：
- `smart_layout_algorithm.cpp`
- `SMART_LAYOUT_GUIDE.md`
- `CHANGELOG.md`
- `gitlog.md`

变更说明：
- 新增公共上下文 `logCtx`，统一承载 `type/child/prevChild/isFirst`。
- 日志拆分为 `solve_result_common/solve_result_geom/solve_result_scale/solve_result_offset`，减少重复参数并提升可读性。
