/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FOUNDATION_ACE_FRAMEWORKS_CORE_COMPONENTS_NG_PATTERN_FLEX_SMART_LAYOUT_ALGORITHM_H
#define FOUNDATION_ACE_FRAMEWORKS_CORE_COMPONENTS_NG_PATTERN_FLEX_SMART_LAYOUT_ALGORITHM_H

#include <memory>
#include <vector>

#include "core/components/common/layout/constants.h"
#include "core/components_ng/layout/layout_wrapper.h"
#include "core/components_ng/pattern/flex/flex_layout_styles.h"
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

} // namespace OHOS::Ace::NG

#endif // FOUNDATION_ACE_FRAMEWORKS_CORE_COMPONENTS_NG_PATTERN_FLEX_SMART_LAYOUT_ALGORITHM_H