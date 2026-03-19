/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#ifndef FOUNDATION_ACE_FRAMEWORKS_CORE_COMPONENTS_NG_PATTERN_SMART_LAYOUT_SMART_LAYOUT_ALGORITHM_H
#define FOUNDATION_ACE_FRAMEWORKS_CORE_COMPONENTS_NG_PATTERN_SMART_LAYOUT_SMART_LAYOUT_ALGORITHM_H

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
    ~LayoutContext() = default;
    localsmt::Engine engine;
    LayoutType layoutType;
    FlexAlign mainAxisAlign = FlexAlign::FLEX_START;
    FlexAlign crossAxisAlign = FlexAlign::FLEX_START;
    HorizontalAlign horizontalAlign = HorizontalAlign::CENTER;
    VerticalAlign verticalAlign = VerticalAlign::CENTER;
    SizeF parentSize;
    std::vector<std::shared_ptr<SmartLayoutNode>> childrenLayoutNodes;
    std::shared_ptr<SmartLayoutNode> currentLayoutNode;
};

class SmartLayoutAlgorithm {
public:
    SmartLayoutAlgorithm() = default;
    ~SmartLayoutAlgorithm() = default;

    void SmartColumnLayout(LayoutWrapper* layoutWrapper);
    void SmartRowLayout(LayoutWrapper* layoutWrapper);

private:
    void PerformSmartLayout(LayoutWrapper* layoutWrapper, LayoutType layoutType);
    void ProcessLayoutChildren(LayoutContext& context, LayoutWrapper* layoutWrapper);
    void ApplyLayoutConstraints(LayoutContext& context);
    bool SolveLayout(LayoutContext& context);
    void ApplyLayoutResults(LayoutContext& context, LayoutWrapper* layoutWrapper);
    bool InitializeLayoutContext(LayoutContext& context, LayoutWrapper* layoutWrapper, LayoutType layoutType);

    void AddColumnConstraints(LayoutContext& context, const std::shared_ptr<SmartLayoutNode> parent);
    void AddRowConstraints(LayoutContext& context, const std::shared_ptr<SmartLayoutNode> parent);
    void AddDefaultConstraints(LayoutContext& context, const std::shared_ptr<SmartLayoutNode> parent);

    // Calculate spacing between two child wrappers along the given layout axis
    double CalculateChildSpacing(const RefPtr<LayoutWrapper>& child1, const RefPtr<LayoutWrapper>& child2, LayoutType layoutType);

    void ItemScaling(const RefPtr<LayoutWrapper>& item, double scale);
    double GetSumOfAllChildHeight(const std::shared_ptr<SmartLayoutNode> parent);
    void ApplyMinimalFallback(LayoutWrapper* layoutWrapper, LayoutType layoutType);
    static float ClampValue(float value, float low, float high);
};

} // namespace OHOS::Ace::NG

#endif // FOUNDATION_ACE_FRAMEWORKS_CORE_COMPONENTS_NG_PATTERN_SMART_LAYOUT_SMART_LAYOUT_ALGORITHM_H
