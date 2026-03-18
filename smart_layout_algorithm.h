#pragma once

#include "smart_flex_layout_algorithm.h"
#include "smart_layout_node.h"

namespace HHHH::HHH::HH {

enum class LayoutType {
    COLUMN,
    ROW,
    FLEX,
};

class SmartLayoutAlgorithm {
public:
    SmartLayoutAlgorithm() = default;
    ~SmartLayoutAlgorithm() = default;

    // 现有线性布局入口（不换行）。
    void SmartColumnLayout(LayoutWrapper* layoutWrapper);
    void SmartRowLayout(LayoutWrapper* layoutWrapper);

    // 一阶段 Flex 独立入口（支持 wrap / wrap-reverse / baseline）。
    void SmartFlexLayout(LayoutWrapper* layoutWrapper, const SmartFlexConfig& config);

private:
    // 通用流程：读取属性 -> 建模 -> 求解 -> 回写。
    void PerformSmartLayout(LayoutWrapper* layoutWrapper, LayoutType layoutType);

    // Column/Row 主轴、侧轴对齐配置。
    FlexAlign mainAxisAlign_ = FlexAlign::FLEX_START;
    HorizontalAlign horizontalAlign_ = HorizontalAlign::CENTER;
    VerticalAlign verticalAlign_ = VerticalAlign::CENTER;

    // Column/Row 共用主轴建模。
    void addColumnLayout(z3::optimize& solver, std::shared_ptr<SmartLayoutNode> parent);
    void addRowLayout(z3::optimize& solver, std::shared_ptr<SmartLayoutNode> parent);
    void addLinearLayout(z3::optimize& solver, std::shared_ptr<SmartLayoutNode> parent, LayoutType layoutType);

    // 基础约束与对齐约束。
    void AddDefaultConstraints(
        z3::optimize& solver, std::shared_ptr<SmartLayoutNode> parent, std::shared_ptr<SmartLayoutNode> child);
    void AddCrossAxisAlignmentConstraints(
        z3::optimize& solver,
        std::shared_ptr<SmartLayoutNode> parent,
        std::shared_ptr<SmartLayoutNode> child,
        LayoutType layoutType);
    void AddMainAxisAlignmentConstraints(
        z3::optimize& solver,
        std::shared_ptr<SmartLayoutNode> parent,
        LayoutType layoutType,
        const z3::expr& mainAxisOffset,
        const z3::expr& betweenGap);

    bool IsColumnSpaceEnough(LayoutWrapper* layoutWrapper);
    bool IsRowSpaceEnough(LayoutWrapper* layoutWrapper);
    bool IsColumnCrossSpaceEnough(LayoutWrapper* layoutWrapper);
    bool IsRowCrossSpaceEnough(LayoutWrapper* layoutWrapper);

    SizeF ItermScale(const RefPtr<LayoutWrapper>& iterm, float scale);

    SmartFlexLayoutAlgorithm smartFlexLayoutAlgorithm_;
};

} // namespace HHHH::HHH::HH
