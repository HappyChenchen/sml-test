#include "smart_layout_node.h"

namespace HHHH::HHH::HH {


enum class LayoutType {
    COLUMN,
    ROW，
    FLEX
};

class SmartLayoutAlgorithm {
public:
    SmartLayoutAlgorithm() = default;
    ~SmartLayoutAlgorithm() = default;

    // 对外入口：按布局方向触发智能重排。
    // 调用方无需关心约束细节，只需传入当前布局树的根 wrapper。
    void SmartColumnLayout(LayoutWrapper* layoutWrapper);
    void SmartRowLayout(LayoutWrapper* layoutWrapper);

private:
    // 统一主流程：收集数据 -> 建模 -> 求解 -> 回写。
    // 参数 layoutType 决定主轴方向与侧轴映射规则。
    void PerformSmartLayout(LayoutWrapper* layoutWrapper, LayoutType layoutType);

    // 对齐配置（来自 FlexLayoutProperty）：
    // - mainAxisAlign_：主轴对齐策略（含 SPACE_*）
    // - horizontalAlign_：Column 模式侧轴对齐策略
    // - verticalAlign_：Row 模式侧轴对齐策略
    FlexAlign mainAxisAlign_ = FlexAlign::FLEX_START;
    HorizontalAlign horizontalAlign_ = HorizontalAlign::CENTER;
    VerticalAlign verticalAlign_ = VerticalAlign::CENTER;

    // 方向入口函数：
    // - addColumnLayout / addRowLayout：轻封装，只做方向分发
    // - addLinearLayout：Column/Row 共用主轴建模核心，避免双分支重复
    void addColumnLayout(z3::optimize& solver, std::shared_ptr<SmartLayoutNode> parent);
    void addRowLayout(z3::optimize& solver, std::shared_ptr<SmartLayoutNode> parent);
    void addLinearLayout(z3::optimize& solver, std::shared_ptr<SmartLayoutNode> parent, LayoutType layoutType);

    // 通用基础约束：
    // 1) 坐标、尺寸非负；
    // 2) 子节点 frame 必须落在父节点 frame 内部。
    void AddDefaultConstraints(
        z3::optimize& solver, std::shared_ptr<SmartLayoutNode> parent, std::shared_ptr<SmartLayoutNode> child);

    // 侧轴对齐约束：
    // - Column：在 X 方向应用 HorizontalAlign
    // - Row：在 Y 方向应用 VerticalAlign
    void AddCrossAxisAlignmentConstraints(
        z3::optimize& solver,
        std::shared_ptr<SmartLayoutNode> parent,
        std::shared_ptr<SmartLayoutNode> child,
        LayoutType layoutType);

    // 主轴对齐约束：
    // - 统一处理 FLEX_START/CENTER/FLEX_END/SPACE_*；
    // - mainAxisOffset：内容块整体偏移；
    // - betweenGap：SPACE_* 模式的统一间距变量。
    void AddMainAxisAlignmentConstraints(
        z3::optimize& solver,
        std::shared_ptr<SmartLayoutNode> parent,
        LayoutType layoutType,
        const z3::expr& mainAxisOffset,
        const z3::expr& betweenGap);

    // 当“主轴 start + 侧轴 start”且空间足够时，跳过求解器以减少开销。
    bool IsColumnSpaceEnough(LayoutWrapper* layoutWrapper);
    bool IsRowSpaceEnough(LayoutWrapper* layoutWrapper);

    // 将求得的 sizeScale 应用到真实节点（transform scale）。
    SizeF ItermScale(const RefPtr<LayoutWrapper>& iterm, float scale);
};

} 