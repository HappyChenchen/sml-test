#pragma once

#include "localsmt/localsmt.h"

#include "smart_layout_node.h"

namespace HHHH::HHH::HH {

enum class LayoutType {
    COLUMN,
    ROW,
};

class LayoutContext {
public:
    LayoutContext() = default;
    localsmt::Engine engine;
    LayoutType layoutType = LayoutType::COLUMN;
    FlexAlign mainAxisAlign_ = FlexAlign::FLEX_START;
    FlexAlign crossAxisAlign_ = FlexAlign::FLEX_START;
    SizeF parentSize;
    std::vector<std::shared_ptr<SmartLayoutNode>> childrenLayoutNode;
    std::shared_ptr<SmartLayoutNode> root;
};

class SmartLayoutAlgorithm {
public:
    SmartLayoutAlgorithm() = default;
    ~SmartLayoutAlgorithm() = default;

    // Main smart layout functions
    void SmartColumnLayout(LayoutWrapper* layoutWrapper);
    void SmartRowLayout(LayoutWrapper* layoutWrapper);

private:
    // Helper function for common layout logic
    void PerformSmartLayout(LayoutWrapper* layoutWrapper, LayoutType layoutType);
    bool InitializeLayoutContext(LayoutContext& context, LayoutWrapper* layoutWrapper, LayoutType layoutType);
    void ProcessLayoutChildren(LayoutContext& context, LayoutWrapper* layoutWrapper);
    void ApplyLayoutConstraints(LayoutContext& context);
    bool SolveLayout(LayoutContext& context);
    void ApplyLayoutResults(LayoutContext& context, LayoutWrapper* layoutWrapper);

    // Smart layout constraint generation functions
    void addColumnLayout(LayoutContext& context, std::shared_ptr<SmartLayoutNode> parent);
    void addRowLayout(LayoutContext& context, std::shared_ptr<SmartLayoutNode> parent);
    void addLinearLayout(LayoutContext& context, std::shared_ptr<SmartLayoutNode> parent, LayoutType layoutType);
    void AddDefaultConstraints(
        localsmt::Engine& engine, std::shared_ptr<SmartLayoutNode> parent, std::shared_ptr<SmartLayoutNode> child);
    void AddCrossAxisAlignmentConstraints(
        localsmt::Engine& engine,
        std::shared_ptr<SmartLayoutNode> parent,
        std::shared_ptr<SmartLayoutNode> child,
        LayoutType layoutType);

    // Utility functions
    SizeF ItermScale(const RefPtr<LayoutWrapper>& iterm, float scale);

    // 新增逻辑：保留 z3 目标语义（maximize(1-loss)）的等价求解上界
    HorizontalAlign horizontalAlign_ = HorizontalAlign::CENTER;
    VerticalAlign verticalAlign_ = VerticalAlign::CENTER;
    float mainSizeScale_ = 1.0f;
    float crossSizeScale_ = 1.0f;
    float mergedSizeScaleUpperBound_ = 1.0f;
};

} // namespace HHHH::HHH::HH
