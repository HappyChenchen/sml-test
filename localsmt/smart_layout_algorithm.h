#include "localsmt/localsmt.h"

#include "smart_layout_node.h"

namespace OHOS::Ace::NG {

enum class LayoutType {
    COLUMN,
    ROW
};

class LayoutContext {
public:
    LayoutContext() = default;
    localsmt::Engine engine;
    LayoutType layoutType;
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
    void ProcessLayoutChildren(LayoutContext& context, LayoutWrapper* layoutWrapper);
    void ApplyLayoutConstraints(LayoutContext& context);
    bool SolveLayout(LayoutContext& context);
    void ApplyLayoutResults(LayoutContext& context, LayoutWrapper* layoutWrapper);
    bool InitializeLayoutContext(LayoutContext& context, LayoutWrapper* layoutWrapper, LayoutType layoutType);

    // Smart layout constraint generation functions
    void addColumnLayout(LayoutContext& context, std::shared_ptr<SmartLayoutNode> parent);
    void addRowLayout(LayoutContext& context, std::shared_ptr<SmartLayoutNode> parent);
    void AddDefaultConstraints(localsmt::Engine& engine, std::shared_ptr<SmartLayoutNode> parent, std::shared_ptr<SmartLayoutNode> child);
    
    // Utility functions
    bool IsColumnSpaceEnough(LayoutWrapper* layoutWrapper);
    SizeF ItermScale(const RefPtr<LayoutWrapper>& iterm, float scale);
    float GetSumOfAllChildHeight(std::shared_ptr<SmartLayoutNode> parent);
};

}
