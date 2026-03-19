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

#include "core/components_ng/pattern/blank/blank_layout_property.h"
#include "core/components_ng/pattern/flex/flex_layout_pattern.h"
#include "core/components_ng/pattern/linear_layout/linear_layout_pattern.h"
#include "core/components_ng/pattern/linear_layout/linear_layout_property.h"
#include "core/components_ng/pattern/navigation/navigation_group_node.h"
#include "parameter.h"

#include <algorithm>
#include <cstdint>

#include "frameworks/core/components_ng/pattern/smart_layout/smart_layout_algorithm.h"

namespace OHOS::Ace::NG {

void SmartLayoutAlgorithm::SmartColumnLayout(LayoutWrapper* layoutWrapper)
{
    PerformSmartLayout(layoutWrapper, LayoutType::COLUMN);
}

void SmartLayoutAlgorithm::SmartRowLayout(LayoutWrapper* layoutWrapper)
{
    PerformSmartLayout(layoutWrapper, LayoutType::ROW);
}

// Private helpers

void SmartLayoutAlgorithm::PerformSmartLayout(LayoutWrapper* layoutWrapper, LayoutType layoutType)
{
    if (layoutWrapper == nullptr) {
        LOGE("cy_debug PerformSmartLayout aborted: layoutWrapper is null");
        return;
    }

    LayoutContext context;
    context.layoutType = layoutType;
    LOGD("cy_debug PerformSmartLayout begin layoutType=%{public}d", static_cast<int32_t>(layoutType));

    // 1) initialize
    if (!InitializeLayoutContext(context, layoutWrapper, layoutType)) {
        LOGE("cy_debug PerformSmartLayout init failed");
        ApplyMinimalFallback(layoutWrapper, layoutType);
        return;
    }
    LOGD("cy_debug init done parentSize=(%{public}f,%{public}f)",
        context.parentSize.Width(), context.parentSize.Height());
    // 2) collect child nodes
    ProcessLayoutChildren(context, layoutWrapper);
    LOGD("cy_debug collect children done count=%{public}zu", context.childrenLayoutNodes.size());
    if (context.childrenLayoutNodes.empty()) {
        LOGE("cy_debug no valid children after collect, fallback");
        ApplyMinimalFallback(layoutWrapper, layoutType);
        return;
    }
    // 3) build constraints according to layout type
    ApplyLayoutConstraints(context);
    LOGD("cy_debug constraints built");
    // 4) solve
    if (!SolveLayout(context)) {
        LOGE("cy_debug solver failed, apply minimal fallback");
        ApplyMinimalFallback(layoutWrapper, layoutType);
        return;
    }
    LOGD("cy_debug solver success, apply result");
    // 5) apply results
    ApplyLayoutResults(context, layoutWrapper);
    LOGD("cy_debug PerformSmartLayout end");
}

double SmartLayoutAlgorithm::CalculateChildSpacing(const RefPtr<LayoutWrapper>& child1,
    const RefPtr<LayoutWrapper>& child2, LayoutType layoutType)
{
    auto offset1 = child1->GetGeometryNode()->GetFrameOffset();
    auto size1 = child1->GetGeometryNode()->GetFrameSize();
    auto offset2 = child2->GetGeometryNode()->GetFrameOffset();

    if (layoutType == LayoutType::COLUMN) {
        return offset2.GetY() - (offset1.GetY() + size1.Height());
    } else {
        return offset2.GetX() - (offset1.GetX() + size1.Width());
    }
}

void SmartLayoutAlgorithm::ProcessLayoutChildren(LayoutContext& context, LayoutWrapper* layoutWrapper)
{
    // Build SmartLayoutNode list from layoutWrapper children in index order
    context.childrenLayoutNodes.clear();
    if (layoutWrapper == nullptr) {
        return;
    }
    const auto& children = layoutWrapper->GetAllChildrenWithBuild(false);
    if (children.empty()) {
        return;
    }
    for (auto it = children.begin(); it != children.end(); ++it) {
        auto childWrapper = *it;
        if (childWrapper == nullptr) {
            LOGE("cy_debug ProcessLayoutChildren skip null childWrapper");
            continue;
        }
        if (childWrapper->GetGeometryNode() == nullptr) {
            LOGE("cy_debug ProcessLayoutChildren skip child id=%{public}d: geometry null",
                childWrapper->GetHostNode() ? childWrapper->GetHostNode()->GetId() : -1);
            continue;
        }
        std::string childName = "child_" + std::to_string(childWrapper->GetHostNode()->GetId());
        auto childNode = std::make_shared<SmartLayoutNode>(context.engine, childName);

        bool isFirst = (it == children.begin());
        auto nestIt = std::next(it);
        bool isLast = (nestIt == children.end());

        // 设置第一个组件距离父容器起点的间距。
        if (isFirst) {
            if (context.layoutType == LayoutType::COLUMN) {
                childNode->space_.top = childWrapper->GetGeometryNode()->GetFrameOffset().GetY();
            } else {
                childNode->space_.left = childWrapper->GetGeometryNode()->GetFrameOffset().GetX();
            }
        }
        // 计算相邻组件间距。
        if (!isLast) {
            const auto& nextChild = *nestIt;
            childNode->space_.bottom = CalculateChildSpacing(childWrapper, nextChild, context.layoutType);
            if (!isFirst) {
                childNode->space_.top = context.childrenLayoutNodes.back()->space_.bottom;
                childNode->space_.left = context.childrenLayoutNodes.back()->space_.right;
            }
        }
        // 设置最后一个组件距离父容器终点的间距。
        if (isLast) {
            if (context.layoutType == LayoutType::COLUMN) {
                childNode->space_.bottom = context.parentSize.Height() -
                    (childWrapper->GetGeometryNode()->GetFrameOffset().GetY() +
                    childWrapper->GetGeometryNode()->GetFrameSize().Height());
            } else {
                childNode->space_.right = context.parentSize.Width() -
                    (childWrapper->GetGeometryNode()->GetFrameOffset().GetX() +
                    childWrapper->GetGeometryNode()->GetFrameSize().Width());
            }
        }

        // 设置尺寸信息。
        auto childSize = childWrapper->GetGeometryNode()->GetFrameSize();
        childNode->size_.width.value = childSize.Width();
        childNode->size_.height.value = childSize.Height();

        // 处理 Blank 节点。
        if (childWrapper->GetHostTag() == "Blank") {
            childNode->space_.bottom += childNode->size_.height.value;
            childNode->size_.height.value = 0;
        }

        context.childrenLayoutNodes.push_back(childNode);
    }

    if (!context.childrenLayoutNodes.empty()) {
        context.currentLayoutNode->SetChildren(context.childrenLayoutNodes);
    }
}

void SmartLayoutAlgorithm::ApplyLayoutConstraints(LayoutContext& context)
{
    // Add main-axis specific constraints
    if (context.layoutType == LayoutType::COLUMN) {
        AddColumnConstraints(context, context.currentLayoutNode);
    } else {
        AddRowConstraints(context, context.currentLayoutNode);
    }
}

bool SmartLayoutAlgorithm::SolveLayout(LayoutContext& context)
{
    auto constraints = context.engine.getConstraints();
    auto clauses = context.engine.getClauses();
    LOGD("cy_debug solve begin constraints=%{public}zu clauses=%{public}zu",
        constraints.size(), clauses.size());
    auto result = context.engine.solve();
    if (result == false) {
        LOGE("cy_debug solve failed: localsmt no solution");
    } else {
        context.currentLayoutNode->SyncData(context.engine);
        LOGD("cy_debug solve success scales space=%{public}f size=%{public}f",
            context.currentLayoutNode->scaleInfo_.spaceScale.value,
            context.currentLayoutNode->scaleInfo_.sizeScale.value);
        auto results = context.engine.getResults();
        for (const auto& var : results) {
            LOGD("cy_debug var %{public}s = %{public}f", var.name.c_str(), var.value);
        }
    }
    return result;
}

void SmartLayoutAlgorithm::ApplyLayoutResults(LayoutContext& context, LayoutWrapper* layoutWrapper)
{
    if (layoutWrapper == nullptr) {
        LOGE("cy_debug ApplyLayoutResults aborted: layoutWrapper is null");
        return;
    }

    uint32_t index = 0;
    for (const auto& child : layoutWrapper->GetAllChildrenWithBuild(false)) {
        if (child == nullptr || child->GetGeometryNode() == nullptr) {
            LOGE("cy_debug ApplyLayoutResults skip invalid child index=%{public}u", index);
            continue;
        }
        if (index >= context.childrenLayoutNodes.size()) {
            LOGE("cy_debug ApplyLayoutResults index overflow index=%{public}u size=%{public}zu",
                index, context.childrenLayoutNodes.size());
            break;
        }

        context.childrenLayoutNodes[index]->SyncData(context.engine);

        float offset1 = 0.0f;
        float offset2 = 0.0f;
        if (context.layoutType == LayoutType::COLUMN) {
            offset1 = child->GetGeometryNode()->GetMargin()->left.value_or(0);
        } else {
            offset2 = child->GetGeometryNode()->GetMargin()->top.value_or(0);
        }

        OffsetF offset(context.childrenLayoutNodes[index]->position_.offsetX.value + offset1,
            context.childrenLayoutNodes[index]->position_.offsetY.value + offset2);
        child->GetGeometryNode()->SetFrameOffset(offset);

        ItemScaling(child, context.currentLayoutNode->scaleInfo_.sizeScale.value);

        if (child->GetHostNode() != nullptr) {
            child->GetHostNode()->MarkDirtyNode(PROPERTY_UPDATE_LAYOUT);
        }
        child->Layout();

        LOGD("cy_debug apply child %{public}s: offset=(%{public}f,%{public}f), size=(%{public}f,%{public}f)",
            context.childrenLayoutNodes[index]->name_.c_str(),
            context.childrenLayoutNodes[index]->position_.offsetX.value,
            context.childrenLayoutNodes[index]->position_.offsetY.value,
            context.childrenLayoutNodes[index]->size_.width.value,
            context.childrenLayoutNodes[index]->size_.height.value);
        index++;
    }
}

bool SmartLayoutAlgorithm::InitializeLayoutContext(LayoutContext& context, LayoutWrapper* layoutWrapper,
    LayoutType layoutType)
{
    if (layoutWrapper == nullptr) {
        LOGE("cy_debug InitializeLayoutContext failed: layoutWrapper null");
        return false;
    }
    const auto& children = layoutWrapper->GetAllChildrenWithBuild(false);
    if (children.empty()) {
        LOGE("cy_debug InitializeLayoutContext failed: no children");
        return false;
    }

    context.layoutType = layoutType;

    // parent size：优先取 geometry node，否则回落为 0。
    auto geo = layoutWrapper->GetGeometryNode();
    if (geo) {
        context.parentSize = geo->GetFrameSize();
    } else {
        // fallback: set to zero
        context.parentSize = SizeF(0.0f, 0.0f);
        LOGE("cy_debug InitializeLayoutContext: parent geometry null, fallback size zero");
    }

    // try to extract alignment from layout property if this is a LinearLayout
    auto layoutProp = layoutWrapper->GetLayoutProperty();
    if (layoutProp) {
        // attempt dynamic cast to FlexLayoutProperty to retrieve axis align
        auto flexProp = AceType::DynamicCast<FlexLayoutProperty>(layoutProp);
        if (flexProp) {
            context.mainAxisAlign = flexProp->GetMainAxisAlign().value_or(FlexAlign::FLEX_START);
            context.crossAxisAlign = flexProp->GetCrossAxisAlign().value_or(FlexAlign::CENTER);
            switch (context.crossAxisAlign) {
                case FlexAlign::FLEX_START:
                    context.horizontalAlign = HorizontalAlign::START;
                    context.verticalAlign = VerticalAlign::TOP;
                    break;
                case FlexAlign::FLEX_END:
                    context.horizontalAlign = HorizontalAlign::END;
                    context.verticalAlign = VerticalAlign::BOTTOM;
                    break;
                case FlexAlign::CENTER:
                default:
                    context.horizontalAlign = HorizontalAlign::CENTER;
                    context.verticalAlign = VerticalAlign::CENTER;
                    break;
            }
        }
    }
    LOGD("cy_debug InitializeLayoutContext align main=%{public}d cross=%{public}d hAlign=%{public}d vAlign=%{public}d",
        static_cast<int32_t>(context.mainAxisAlign),
        static_cast<int32_t>(context.crossAxisAlign),
        static_cast<int32_t>(context.horizontalAlign),
        static_cast<int32_t>(context.verticalAlign));

    context.currentLayoutNode = std::make_shared<SmartLayoutNode>(context.engine, "root");
    context.currentLayoutNode->SetFixedSizeConstraints(context.engine, static_cast<double>(context.parentSize.Width()),
                                                      static_cast<double>(context.parentSize.Height()));
    return true;
}

void SmartLayoutAlgorithm::AddColumnConstraints(LayoutContext& context, const std::shared_ptr<SmartLayoutNode> parent)
{
    if (parent == nullptr) {
        LOGE("cy_debug AddColumnConstraints skipped: parent null");
        return;
    }
    AddDefaultConstraints(context, parent);
    double sumOfAllChildHeight = GetSumOfAllChildHeight(parent);
    double compressibleGapSum = 0.0;
    if (!parent->children_.empty()) {
        compressibleGapSum += std::max(0.0, parent->children_.front()->space_.top);
        for (size_t i = 1; i < parent->children_.size(); ++i) {
            const auto& prev = parent->children_[i - 1];
            if (prev == nullptr) {
                continue;
            }
            compressibleGapSum += std::max(0.0, prev->space_.bottom);
        }
    }
    const double totalNeed = sumOfAllChildHeight + compressibleGapSum;
    const double parentMainSize = context.parentSize.Height();
    if (totalNeed > parentMainSize) {
        if (compressibleGapSum > 0.0) {
            const double targetScale = (parentMainSize - sumOfAllChildHeight) / compressibleGapSum;
            context.engine.add(parent->scaleInfo_.spaceScale.expr == ClampValue(static_cast<float>(targetScale), 0.0f, 1.0f));
        } else {
            context.engine.add(parent->scaleInfo_.spaceScale.expr == ((sumOfAllChildHeight > parentMainSize) ? 0.0 : 1.0));
        }
    } else {
        context.engine.add(parent->scaleInfo_.spaceScale.expr == 1.0);
    }

    // Ensure parent's spaceScale is in [0,1]
    context.engine.add(parent->scaleInfo_.spaceScale.expr >= 0.0);
    context.engine.add(parent->scaleInfo_.spaceScale.expr <= 1.0);
    LOGD("cy_debug AddColumnConstraints childCount=%{public}zu sizeSum=%{public}f gapSum=%{public}f totalNeed=%{public}f parentH=%{public}f",
        parent->children_.size(), sumOfAllChildHeight, compressibleGapSum, totalNeed, context.parentSize.Height());

    for (size_t i = 0; i < parent->children_.size(); ++i) {
        auto& child = parent->children_[i];
        if (child == nullptr) {
            continue;
        }
        if (sumOfAllChildHeight > context.parentSize.Height()) {
            context.engine.add(child->size_.width.expr * sumOfAllChildHeight ==
                child->size_.width.expr * context.parentSize.Height());
            context.engine.add(child->size_.height.expr * sumOfAllChildHeight ==
                child->size_.height.expr * context.parentSize.Height());
        } else {
            context.engine.add(child->size_.width.expr == child->size_.width.expr);
            context.engine.add(child->size_.height.expr == child->size_.height.expr);
        }

        if (context.horizontalAlign == HorizontalAlign::START) {
            // left align
            context.engine.add(child->position_.offsetX.expr == parent->position_.offsetX.expr);
        } else if (context.horizontalAlign == HorizontalAlign::END) {
            // right align
            context.engine.add(child->position_.offsetX.expr + child->size_.width.expr ==
                               parent->position_.offsetX.expr + parent->size_.width.expr);
        } else if (context.horizontalAlign == HorizontalAlign::CENTER) {
            // center align
            context.engine.add((2.0 * child->position_.offsetX.expr) + child->size_.width.expr ==
                (2.0 * parent->position_.offsetX.expr) + parent->size_.width.expr);
        }

        if (i == 0) {
            context.engine.add(child->position_.offsetY.expr ==
                parent->position_.offsetY.expr + child->space_.top * parent->scaleInfo_.spaceScale.expr);
        } else {
            auto& prev = parent->children_[i - 1];
            context.engine.add(child->position_.offsetY.expr ==
                prev->position_.offsetY.expr + prev->size_.height.expr +
                prev->space_.bottom * parent->scaleInfo_.spaceScale.expr);
        }

        if (i == parent->children_.size() - 1) {
            context.engine.add(child->position_.offsetY.expr + child->size_.height.expr + 1 >=
                parent->position_.offsetY.expr + parent->size_.height.expr);
            context.engine.add(child->position_.offsetY.expr + child->size_.height.expr <=
                parent->position_.offsetY.expr + parent->size_.height.expr);
        }
    }
}

void SmartLayoutAlgorithm::AddRowConstraints(LayoutContext& context, const std::shared_ptr<SmartLayoutNode> parent)
{
    if (parent == nullptr) {
        LOGE("cy_debug AddRowConstraints skipped: parent null");
        return;
    }
    AddDefaultConstraints(context, parent);
    // compute sum of all child widths
    double sumOfAllChildWidth = 0.0;
    for (const auto& c : parent->children_) {
        if (c == nullptr) {
            continue;
        }
        sumOfAllChildWidth += c->size_.width.value;
    }
    double compressibleGapSum = 0.0;
    if (!parent->children_.empty()) {
        compressibleGapSum += std::max(0.0, parent->children_.front()->space_.left);
        for (size_t i = 1; i < parent->children_.size(); ++i) {
            const auto& prev = parent->children_[i - 1];
            if (prev == nullptr) {
                continue;
            }
            compressibleGapSum += std::max(0.0, prev->space_.right);
        }
    }
    const double totalNeed = sumOfAllChildWidth + compressibleGapSum;
    const double parentMainSize = context.parentSize.Width();
    if (totalNeed > parentMainSize) {
        if (compressibleGapSum > 0.0) {
            const double targetScale = (parentMainSize - sumOfAllChildWidth) / compressibleGapSum;
            context.engine.add(parent->scaleInfo_.spaceScale.expr == ClampValue(static_cast<float>(targetScale), 0.0f, 1.0f));
        } else {
            context.engine.add(parent->scaleInfo_.spaceScale.expr == ((sumOfAllChildWidth > parentMainSize) ? 0.0 : 1.0));
        }
    } else {
        context.engine.add(parent->scaleInfo_.spaceScale.expr == 1.0);
    }

    // Ensure parent's spaceScale is in [0,1]
    context.engine.add(parent->scaleInfo_.spaceScale.expr >= 0.0);
    context.engine.add(parent->scaleInfo_.spaceScale.expr <= 1.0);
    LOGD("cy_debug AddRowConstraints childCount=%{public}zu sizeSum=%{public}f gapSum=%{public}f totalNeed=%{public}f parentW=%{public}f",
        parent->children_.size(), sumOfAllChildWidth, compressibleGapSum, totalNeed, context.parentSize.Width());

    for (size_t i = 0; i < parent->children_.size(); ++i) {
        auto& child = parent->children_[i];
        if (child == nullptr) {
            continue;
        }

        if (sumOfAllChildWidth > context.parentSize.Width()) {
            context.engine.add(child->size_.width.expr * sumOfAllChildWidth ==
                child->size_.width.expr * context.parentSize.Width());
            context.engine.add(child->size_.height.expr * sumOfAllChildWidth ==
                child->size_.height.expr * context.parentSize.Width());
        } else {
            context.engine.add(child->size_.width.expr == child->size_.width.expr);
            context.engine.add(child->size_.height.expr == child->size_.height.expr);
        }

        // Cross-axis (vertical) alignment by VerticalAlign
        if (context.verticalAlign == VerticalAlign::TOP) {
            // top align
            context.engine.add(child->position_.offsetY.expr == parent->position_.offsetY.expr);
        } else if (context.verticalAlign == VerticalAlign::BOTTOM) {
            // bottom align
            context.engine.add(child->position_.offsetY.expr + child->size_.height.expr ==
                               parent->position_.offsetY.expr + parent->size_.height.expr);
        } else if (context.verticalAlign == VerticalAlign::CENTER) {
            // center align
            context.engine.add((2.0 * child->position_.offsetY.expr) + child->size_.height.expr ==
                               (2.0 * parent->position_.offsetY.expr) + parent->size_.height.expr);
        }

        if (i == 0) {
            context.engine.add(child->position_.offsetX.expr ==
                parent->position_.offsetX.expr + child->space_.left * parent->scaleInfo_.spaceScale.expr);
        } else {
            auto& prev = parent->children_[i - 1];
            context.engine.add(child->position_.offsetX.expr ==
                prev->position_.offsetX.expr + prev->size_.width.expr +
                prev->space_.right * parent->scaleInfo_.spaceScale.expr);
        }

        if (i == parent->children_.size() - 1) {
            // parent right minus last child's right <= 1
            context.engine.add(child->position_.offsetX.expr + child->size_.width.expr + 1 >=
                parent->position_.offsetX.expr + parent->size_.width.expr);
            context.engine.add(child->position_.offsetX.expr + child->size_.width.expr <=
                parent->position_.offsetX.expr + parent->size_.width.expr);
        }
    }
}

void SmartLayoutAlgorithm::AddDefaultConstraints(LayoutContext& context, const std::shared_ptr<SmartLayoutNode> parent)
{
    if (!parent) {
        return;
    }

    // Ensure parent non-negative
    context.engine.add(parent->size_.width.expr >= 0.0);
    context.engine.add(parent->size_.height.expr >= 0.0);
    context.engine.add(parent->position_.offsetX.expr >= 0.0);
    context.engine.add(parent->position_.offsetY.expr >= 0.0);

    // For every direct child, ensure size/offset non-negative and child is inside parent
    for (const auto& child : parent->children_) {
        // non-negativity for child
        context.engine.add(child->size_.width.expr >= 0.0);
        context.engine.add(child->size_.height.expr >= 0.0);
        context.engine.add(child->position_.offsetX.expr >= 0.0);
        context.engine.add(child->position_.offsetY.expr >= 0.0);

        // child offset must be no less than parent offset (stay inside)
        context.engine.add(child->position_.offsetX.expr >= parent->position_.offsetX.expr);
        context.engine.add(child->position_.offsetY.expr >= parent->position_.offsetY.expr);

        // child end (offset + size) must not exceed parent end (parent offset + parent size)
        context.engine.add(child->position_.offsetX.expr + child->size_.width.expr <=
                           parent->position_.offsetX.expr + parent->size_.width.expr);
        context.engine.add(child->position_.offsetY.expr + child->size_.height.expr <=
                           parent->position_.offsetY.expr + parent->size_.height.expr);
    }
}

void SmartLayoutAlgorithm::ItemScaling(const RefPtr<LayoutWrapper>& item, double scale)
{
    Dimension offsetX(0.0, DimensionUnit::PERCENT);
    Dimension offsetY(0.0, DimensionUnit::PERCENT);
    item->GetHostNode()->GetRenderContext()->UpdateTransformCenter(DimensionOffset(offsetX, offsetY));
    item->GetHostNode()->GetRenderContext()->UpdateTransformScale({scale, scale});
}

double SmartLayoutAlgorithm::GetSumOfAllChildHeight(const std::shared_ptr<SmartLayoutNode> parent)
{
    double sumOfAllChildHeight = 0.0;
    for (const auto& c : parent->children_) {
        if (c == nullptr) {
            continue;
        }
        sumOfAllChildHeight += c->size_.height.value;
    }
    return sumOfAllChildHeight;
}

float SmartLayoutAlgorithm::ClampValue(float value, float low, float high)
{
    if (low > high) {
        return low;
    }
    return std::max(low, std::min(value, high));
}

void SmartLayoutAlgorithm::ApplyMinimalFallback(LayoutWrapper* layoutWrapper, LayoutType layoutType)
{
    if (layoutWrapper == nullptr) {
        LOGE("cy_debug fallback aborted: layoutWrapper null");
        return;
    }
    auto parentGeo = layoutWrapper->GetGeometryNode();
    if (parentGeo == nullptr) {
        LOGE("cy_debug fallback aborted: parent geometry null");
        return;
    }

    const auto parentOffset = parentGeo->GetFrameOffset();
    const auto parentSize = parentGeo->GetFrameSize();
    float cursorMain = (layoutType == LayoutType::COLUMN) ? parentOffset.GetY() : parentOffset.GetX();
    uint32_t index = 0;

    for (const auto& child : layoutWrapper->GetAllChildrenWithBuild(false)) {
        if (child == nullptr || child->GetGeometryNode() == nullptr) {
            LOGE("cy_debug fallback skip invalid child index=%{public}u", index);
            continue;
        }
        auto childGeo = child->GetGeometryNode();
        const auto childSize = childGeo->GetFrameSize();

        double safeScale = 1.0;
        if (childSize.Width() > 0.0f && parentSize.Width() > 0.0f) {
            safeScale = std::min(safeScale, static_cast<double>(parentSize.Width() / childSize.Width()));
        }
        if (childSize.Height() > 0.0f && parentSize.Height() > 0.0f) {
            safeScale = std::min(safeScale, static_cast<double>(parentSize.Height() / childSize.Height()));
        }
        safeScale = std::max(0.0, std::min(1.0, safeScale));

        const float scaledW = static_cast<float>(childSize.Width() * safeScale);
        const float scaledH = static_cast<float>(childSize.Height() * safeScale);
        const auto oldOffset = childGeo->GetFrameOffset();
        float x = oldOffset.GetX();
        float y = oldOffset.GetY();

        if (layoutType == LayoutType::COLUMN) {
            y = ClampValue(cursorMain, parentOffset.GetY(), parentOffset.GetY() + parentSize.Height() - scaledH);
            x = ClampValue(x, parentOffset.GetX(), parentOffset.GetX() + parentSize.Width() - scaledW);
            cursorMain = y + scaledH;
        } else {
            x = ClampValue(cursorMain, parentOffset.GetX(), parentOffset.GetX() + parentSize.Width() - scaledW);
            y = ClampValue(y, parentOffset.GetY(), parentOffset.GetY() + parentSize.Height() - scaledH);
            cursorMain = x + scaledW;
        }

        childGeo->SetFrameOffset(OffsetF(x, y));
        ItemScaling(child, safeScale);
        if (child->GetHostNode() != nullptr) {
            child->GetHostNode()->MarkDirtyNode(PROPERTY_UPDATE_LAYOUT);
        }
        child->Layout();
        LOGD("cy_debug fallback apply index=%{public}u scale=%{public}f offset=(%{public}f,%{public}f)",
            index, safeScale, x, y);
        index++;
    }
}

} // namespace OHOS::Ace::NG


