#pragma once

#include "smart_layout_node.h"

namespace HHHH::HHH::HH {

enum class LayoutType {
    COLUMN,
    ROW,
};

class SmartLayoutAlgorithm {
public:
    SmartLayoutAlgorithm() = default;
    ~SmartLayoutAlgorithm() = default;

    // 现有线性布局入口（不换行）。
    void SmartColumnLayout(LayoutWrapper* layoutWrapper);
    void SmartRowLayout(LayoutWrapper* layoutWrapper);

private:
    // 通用流程：读取属性 -> 建模 -> 求解 -> 回写。
    void PerformSmartLayout(LayoutWrapper* layoutWrapper, LayoutType layoutType);

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

    SizeF ItermScale(const RefPtr<LayoutWrapper>& iterm, float scale);

    // 线性布局一轮求解的尺寸系数：
    // - mainSizeScale_: 主轴尺寸系数
    // - crossSizeScale_: 侧轴尺寸系数
    // - mergedSizeScaleUpperBound_: 统一尺寸系数（取两者最小）
    float mainSizeScale_ = 1.0f;
    float crossSizeScale_ = 1.0f;
    float mergedSizeScaleUpperBound_ = 1.0f;

};

} // namespace HHHH::HHH::HH
