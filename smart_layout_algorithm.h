#ifndef FOUNDATION_ACE_FRAMEWORKS_CORE_COMPONENTS_NG_PATTERN_FLEX_SMART_LAYOUT_ALGORITHM_H
#define FOUNDATION_ACE_FRAMEWORKS_CORE_COMPONENTS_NG_PATTERN_FLEX_SMART_LAYOUT_ALGORITHM_H

#include "core/components/common/layout/constants.h"
#include "core/components_ng/layout/layout_wrapper.h"
#include "core/components_ng/pattern/flex/flex_layout_styles.h"
#include "core/components_ng/pattern/z3/include/z3++.h"

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

    // Main smart layout functions
    void SmartColumnLayout(LayoutWrapper* layoutWrapper);
    void SmartRowLayout(LayoutWrapper* layoutWrapper);

private:
    // Helper function for common layout logic
    void PerformSmartLayout(LayoutWrapper* layoutWrapper, LayoutType layoutType);

    FlexAlign mainAxisAlign_ = FlexAlign::FLEX_START;
    FlexAlign crossAxisAlign_ = FlexAlign::FLEX_START;

    // Smart layout constraint generation functions
    void addColumnLayout(z3::optimize& solver, std::shared_ptr<SmartLayoutNode> parent);
    void addRowLayout(z3::optimize& solver, std::shared_ptr<SmartLayoutNode> parent);
    void AddDefaultConstraints(z3::optimize& solver, std::shared_ptr<SmartLayoutNode> parent, std::shared_ptr<SmartLayoutNode> child);
    
    // Utility functions
    bool IsColumnSpaceEnough(LayoutWrapper* layoutWrapper);
    SizeF ItermScale(const RefPtr<LayoutWrapper>& iterm, float scale);
};

} // namespace HHHH::HHH::HH

#endif // FOUNDATION_ACE_FRAMEWORKS_CORE_COMPONENTS_NG_PATTERN_FLEX_SMART_LAYOUT_ALGORITHM_H