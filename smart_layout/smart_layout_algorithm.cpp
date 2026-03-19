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
        return;
    }

    LayoutContext context;
    context.layoutType = layoutType;

    // 1) initialize
    if (!InitializeLayoutContext(context, layoutWrapper, layoutType)) {
        return;
    }
    // 2) collect child nodes
    ProcessLayoutChildren(context, layoutWrapper);
    // 3) build constraints according to layout type
    ApplyLayoutConstraints(context);
    // 4) solve
    if (!SolveLayout(context)) {
        // solver failed - fallback to simple layout (do nothing here)
        return;
    }
    // 5) apply results
    ApplyLayoutResults(context, layoutWrapper);
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
            continue;
        }
        std::string childName = "child_" + std::to_string(childWrapper->GetHostNode()->GetId());
        auto childNode = std::make_shared<SmartLayoutNode>(context.engine, childName);

        bool isFirst = (it == children.begin());
        auto nestIt = std::next(it);
        bool isLast = (nestIt == children.end());

        // 设置第一个组件距离父容器top的间距
        if (isFirst) {
            if (context.layoutType == LayoutType::COLUMN) {
                childNode->space_.top = childWrapper->GetGeometryNode()->GetFrameOffset().GetY();
            } else {
                childNode->space_.left = childWrapper->GetGeometryNode()->GetFrameOffset().GetX();
            }
        }
        // 计算相邻间距
        if (!isLast) {
            const auto& nextChild = *nestIt;
            childNode->space_.bottom = CalculateChildSpacing(childWrapper, nextChild, context.layoutType);
            if (!isFirst) {
                childNode->space_.top = context.childrenLayoutNodes.back()->space_.bottom;
                childNode->space_.left = context.childrenLayoutNodes.back()->space_.right;
            }
        }
        // 设置最后一个组件距离父容器bottom的间距
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

        // 设置尺寸信息
        auto childSize = childWrapper->GetGeometryNode()->GetFrameSize();
        childNode->size_.width.value = childSize.Width();
        childNode->size_.height.value = childSize.Height();

        // 处理空白节点
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
    auto result = context.engine.solve();
    if (result == false) {
        LOGE("localsmt failed to find a solution for the given constraints");
    } else {
        context.currentLayoutNode->SyncData(context.engine);
        auto results = context.engine.getResults();
        for (const auto& var : results) {
            LOGD("   %s = %f", var.name.c_str(), var.value);
        }
    }
    return result;
}

void SmartLayoutAlgorithm::ApplyLayoutResults(LayoutContext& context, LayoutWrapper* layoutWrapper)
{
    uint32_t index = 0;
    for (const auto& child : layoutWrapper->GetAllChildrenWithBuild(false)) {
        context.childrenLayoutNodes[index]->SyncData(context.engine);
        // 计算偏移
        float offset1 = 0, offset2 = 0;
        if (context.layoutType == LayoutType::COLUMN) {
            // column布局使用左边距
            if (child->GetGeometryNode() == nullptr) {
                offset1 = child->GetGeometryNode()->GetMargin()->left.value_or(0);
            }
        } else {
            // row布局使用上边距
            if (child->GetGeometryNode() == nullptr) {
                offset2 = child->GetGeometryNode()->GetMargin()->top.value_or(0);
            }
        }

        OffsetF offset(context.childrenLayoutNodes[index]->position_.offsetX.value + offset1,
            context.childrenLayoutNodes[index]->position_.offsetY.value + offset2);
        child->GetGeometryNode()->SetFrameOffset(offset);

        ItemScaling(child, context.currentLayoutNode->scaleInfo_.sizeScale.value);

        if (child->GetHostNode() != nullptr) {
            child->GetHostNode()->MarkDirtyNode(PROPERTY_UPDATE_LAYOUT);
        }
        child->Layout();

        LOGE("Applied layout for child %{public}s: offset=(%{public}f, %{public}f), size=(%{public}f, %{public}f)",
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
        return false;
    }
    const auto& children = layoutWrapper->GetAllChildrenWithBuild(false);
    if (children.empty()) {
        return false;
    }

    context.layoutType = layoutType;

    // parent size — take from geometry node if present, otherwise from layout constraint
    auto geo = layoutWrapper->GetGeometryNode();
    if (geo) {
        context.parentSize = geo->GetFrameSize();
    } else {
        // fallback: set to zero
        context.parentSize = SizeF(0.0f, 0.0f);
    }

    // try to extract alignment from layout property if this is a LinearLayout
    auto layoutProp = layoutWrapper->GetLayoutProperty();
    if (layoutProp) {
        // attempt dynamic cast to FlexLayoutProperty to retrieve axis align
        auto flexProp = AceType::DynamicCast<FlexLayoutProperty>(layoutProp);
        if (flexProp) {
            context.mainAxisAlign = flexProp->GetMainAxisAlign().value_or(FlexAlign::FLEX_START);
            context.crossAxisAlign = flexProp->GetCrossAxisAlign().value_or(FlexAlign::CENTER);
        }
    }

    context.currentLayoutNode = std::make_shared<SmartLayoutNode>(context.engine, "root");
    context.currentLayoutNode->SetFixedSizeConstraints(context.engine, static_cast<double>(context.parentSize.Width()),
                                                      static_cast<double>(context.parentSize.Height()));
    return true;
}

void SmartLayoutAlgorithm::AddColumnConstraints(LayoutContext& context, const std::shared_ptr<SmartLayoutNode> parent)
{
    if (parent == nullptr) {
        return;
    }
    AddDefaultConstraints(context, parent);
    double sumOfAllChildHeight = GetSumOfAllChildHeight(parent);
    if (sumOfAllChildHeight > context.parentSize.Height()) {
        context.engine.add(parent->scaleInfo_.spaceScale.expr == context.parentSize.Height() / sumOfAllChildHeight);
    } else {
        context.engine.add(parent->scaleInfo_.spaceScale.expr == 1.0);
    }

    // Ensure parent's spaceScale is in [0,1]
    context.engine.add(parent->scaleInfo_.spaceScale.expr >= 0.0);
    context.engine.add(parent->scaleInfo_.spaceScale.expr <= 1.0);

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

        if (context.crossAxisAlign == FlexAlign::FLEX_START) {
            // left align: child.x == parent.x
            context.engine.add(child->position_.offsetX.expr == parent->position_.offsetX.expr);
        } else if (context.crossAxisAlign == FlexAlign::FLEX_END) {
            // right align: child.x + child.w == parent.x + parent.w
            context.engine.add(child->position_.offsetX.expr + child->size_.width.expr ==
                               parent->position_.offsetX.expr + parent->size_.width.expr);
        } else if (context.crossAxisAlign == FlexAlign::CENTER) {
            // center align: 2 * child.x + child.w == 2 * parent.x + parent.w
            context.engine.add((2.0 * child->position_.offsetX.expr) + child->size_.width.expr ==
                parent->size_.width.expr);
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
        return;
    }
    AddDefaultConstraints(context, parent);
    // compute sum of all child widths
    double sumOfAllChildWidth = 0.0;
    for (const auto& c : parent->children_) {
        sumOfAllChildWidth += c->size_.width.value;
    }

    if (sumOfAllChildWidth > context.parentSize.Width()) {
        context.engine.add(parent->scaleInfo_.spaceScale.expr == context.parentSize.Width() / sumOfAllChildWidth);
    } else {
        context.engine.add(parent->scaleInfo_.spaceScale.expr == 1.0);
    }

    // Ensure parent's spaceScale is in [0,1]
    context.engine.add(parent->scaleInfo_.spaceScale.expr >= 0.0);
    context.engine.add(parent->scaleInfo_.spaceScale.expr <= 1.0);

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

        // Cross-axis (vertical) alignment
        if (context.crossAxisAlign == FlexAlign::FLEX_START) {
            // top align: child.y == parent.y
            context.engine.add(child->position_.offsetY.expr == parent->position_.offsetY.expr);
        } else if (context.crossAxisAlign == FlexAlign::FLEX_END) {
            // bottom align: child.y + child.h == parent.y + parent.h
            context.engine.add(child->position_.offsetY.expr + child->size_.height.expr ==
                               parent->position_.offsetY.expr + parent->size_.height.expr);
        } else if (context.crossAxisAlign == FlexAlign::CENTER) {
            // center align vertically: 2*child.y + child.h == 2*parent.y + parent.h
            context.engine.add((2.0 * child->position_.offsetY.expr) + child->size_.height.expr ==
                               parent->size_.height.expr);
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
        sumOfAllChildHeight += c->size_.height.value;
    }
    return sumOfAllChildHeight;
}

} // namespace OHOS::Ace::NG