#include "smart_layout_node.h"

namespace HHHH::HHH::HH {

enum class LayoutType {
    COLUMN,
    ROW
};

class SmartLayoutAlgorithm {
public:
    SmartLayoutAlgorithm() = default;
    ~SmartLayoutAlgorithm() = default;

    // 对外入口：分别处理纵向布局和横向布局
    void SmartColumnLayout(LayoutWrapper* layoutWrapper);
    void SmartRowLayout(LayoutWrapper* layoutWrapper);

private:
    // 统一主流程：收集数据 -> 建模 -> 求解 -> 回写
    void PerformSmartLayout(LayoutWrapper* layoutWrapper, LayoutType layoutType);

    // 对齐配置（来自 FlexLayoutProperty）
    FlexAlign mainAxisAlign_ = FlexAlign::FLEX_START;
    HorizontalAlign horizontalAlign_ = HorizontalAlign::CENTER;
    VerticalAlign verticalAlign_ = VerticalAlign::CENTER;

    // 为不同方向添加约束
    void addColumnLayout(z3::optimize& solver, std::shared_ptr<SmartLayoutNode> parent);
    void addRowLayout(z3::optimize& solver, std::shared_ptr<SmartLayoutNode> parent);

    // 通用基础约束：非负 + 子组件在父组件内
    void AddDefaultConstraints(
        z3::optimize& solver, std::shared_ptr<SmartLayoutNode> parent, std::shared_ptr<SmartLayoutNode> child);

    // 交叉轴对齐约束（例如 Column 的 X 对齐 / Row 的 Y 对齐）
    void AddCrossAxisAlignmentConstraints(
        z3::optimize& solver,
        std::shared_ptr<SmartLayoutNode> parent,
        std::shared_ptr<SmartLayoutNode> child,
        LayoutType layoutType);

    // 主轴对齐约束（FLEX_START/CENTER/FLEX_END）
    void AddMainAxisAlignmentConstraints(
        z3::optimize& solver,
        std::shared_ptr<SmartLayoutNode> parent,
        LayoutType layoutType,
        const z3::expr& mainAxisOffset,
        const z3::expr& betweenGap);

    // 快速判定：空间充足时可跳过求解器（仅在 start/start 场景）
    bool IsColumnSpaceEnough(LayoutWrapper* layoutWrapper);
    bool IsRowSpaceEnough(LayoutWrapper* layoutWrapper);

    // 把求得的 sizeScale 应用到真实节点
    SizeF ItermScale(const RefPtr<LayoutWrapper>& iterm, float scale);
};

} // namespace HHHH::HHH::HH

