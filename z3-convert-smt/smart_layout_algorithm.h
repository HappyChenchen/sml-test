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

    // Existing linear layout entry (no wrap).
    void SmartColumnLayout(LayoutWrapper* layoutWrapper);
    void SmartRowLayout(LayoutWrapper* layoutWrapper);

private:
    // Common flow: read attributes -> model -> solve -> apply.
    void PerformSmartLayout(LayoutWrapper* layoutWrapper, LayoutType layoutType);

    // Cross-axis baseline alignment:
    // - COLUMN -> HorizontalAlign
    // - ROW -> VerticalAlign
    HorizontalAlign horizontalAlign_ = HorizontalAlign::CENTER;
    VerticalAlign verticalAlign_ = VerticalAlign::CENTER;

    // Solved scale values written into model as hard equalities.
    float solvedMainSpaceScale_ = 1.0f;
    float solvedSizeScale_ = 1.0f;

    void addColumnLayout(localsmt::Engine& solver, const std::shared_ptr<SmartLayoutNode>& parent);
    void addRowLayout(localsmt::Engine& solver, const std::shared_ptr<SmartLayoutNode>& parent);
    void addLinearLayout(localsmt::Engine& solver, const std::shared_ptr<SmartLayoutNode>& parent, LayoutType layoutType);

    void AddDefaultConstraints(
        localsmt::Engine& solver, const std::shared_ptr<SmartLayoutNode>& parent, const std::shared_ptr<SmartLayoutNode>& child);
    void AddCrossAxisAlignmentConstraints(
        localsmt::Engine& solver,
        const std::shared_ptr<SmartLayoutNode>& parent,
        const std::shared_ptr<SmartLayoutNode>& child,
        LayoutType layoutType);

    void ComputeSolvedScales(const std::shared_ptr<SmartLayoutNode>& parent, LayoutType layoutType);
    SizeF ItermScale(const RefPtr<LayoutWrapper>& item, float scale);
};

} // namespace HHHH::HHH::HH
