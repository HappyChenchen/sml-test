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

#include <algorithm>
#include <codecvt>
#include <string>
#include "core/components_ng/pattern/flex/flex_layout_property.h"
#include "core/components_ng/pattern/flex/flex_layout_pattern.h"
#include "core/components_ng/pattern/linear_layout/linear_layout_pattern.h"
#include "core/components_ng/pattern/linear_layout/linear_layout_property.h"
#include "core/components_ng/pattern/navigation/navigation_group_node.h"
#include "core/components_ng/pattern/text_field/text_field_pattern.h"
#include "parameters.h"

#include "smart_layout_algorithm.h"

namespace OHOS::Ace::NG {

// static constexpr double MIN_SIZE_SCALE = 0.0;

void SmartLayoutAlgorithm::AddDefaultConstraints(localsmt::Engine& engine, std::shared_ptr<SmartLayoutNode> parent,
    std::shared_ptr<SmartLayoutNode> child)
{
    // 1. 控制元素尺寸不小于 0
    engine.add(child->size_.width.expr >= 0);
    engine.add(child->size_.height.expr >= 0);
    engine.add(child->position_.offsetX.expr >= 0);
    engine.add(child->position_.offsetY.expr >= 0);
    engine.add(parent->position_.offsetX.expr == 0);
    engine.add(parent->position_.offsetY.expr == 0);
    engine.add(parent->size_.width.expr >= 0);
    engine.add(parent->size_.height.expr >= 0);
    engine.add(parent->scaleInfo_.spaceScale.expr >= 0);
    engine.add(parent->space >= 0);

    // 子控件不能超出父控件内部
    engine.add(child->position_.offsetX.expr >= parent->position_.offsetX.expr);
    engine.add(child->position_.offsetY.expr >= parent->position_.offsetY.expr);
    engine.add(child->position_.offsetX.expr + child->size_.width.expr <=
        parent->position_.offsetX.expr + parent->size_.width.expr);
    engine.add(child->position_.offsetY.expr + child->size_.height.expr <=
        parent->position_.offsetY.expr + parent->size_.height.expr);
}

void SmartLayoutAlgorithm::addColumnLayout(LayoutContext& context, std::shared_ptr<SmartLayoutNode> parent)
{
    float sumOfAllChildHeight = GetSumOfAllChildHeight(parent);
    float maxChildWidth = 0;
    for (const auto& child : parent->childNode) {
        maxChildWidth = std::max(maxChildWidth, child->size_.width.value);
    }

    float mainAxisScale = 1.0f;
    if (sumOfAllChildHeight > parent->size_.height.value && sumOfAllChildHeight > 0) {
        mainAxisScale = parent->size_.height.value / sumOfAllChildHeight;
    }
    float crossAxisScale = 1.0f;
    if (maxChildWidth > parent->size_.width.value && maxChildWidth > 0) {
        crossAxisScale = parent->size_.width.value / maxChildWidth;
    }
    float finalScale = std::min(mainAxisScale, crossAxisScale);
    context.engine.add(parent->scaleInfo_.sizeScale.expr == static_cast<double>(finalScale));

    context.engine.add(parent->scaleInfo_.spaceScale.expr >= 0);
    context.engine.add(parent->scaleInfo_.spaceScale.expr <= 1);

    for (size_t i = 0; i < parent->childNode.size(); ++i) {
        std::shared_ptr<SmartLayoutNode> child = parent->childNode[i];
        context.engine.add(child->size_.width.expr ==
            static_cast<double>(child->size_.width.value) * parent->scaleInfo_.sizeScale.expr);
        context.engine.add(child->size_.height.expr ==
            static_cast<double>(child->size_.height.value) * parent->scaleInfo_.sizeScale.expr);

        // 默认约束
        AddDefaultConstraints(context.engine, parent, child);
        if (context.horizontalAlign_ == HorizontalAlign::START) {
            context.engine.add(child->position_.offsetX.expr == parent->position_.offsetX.expr);
        } else if (context.horizontalAlign_ == HorizontalAlign::END) {
            context.engine.add(child->position_.offsetX.expr + child->size_.width.expr ==
                parent->position_.offsetX.expr + parent->size_.width.expr);
        } else {
            context.engine.add(child->position_.offsetX.expr - parent->position_.offsetX.expr ==
                      parent->position_.offsetX.expr + parent->size_.width.expr -
                      child->position_.offsetX.expr - child->size_.width.expr);
        }

        // 垂直约束保证元素顺序排列
        if (i == 0) {
            context.engine.add(child->position_.offsetY.expr == parent->position_.offsetY.expr +
                      child->edgesSpace_.top * parent->scaleInfo_.spaceScale.expr);
        } else {
            std::shared_ptr<SmartLayoutNode> prev = parent->childNode[i - 1];
            // 当前 y = 上一节点的底部 + 间距
            context.engine.add(child->position_.offsetY.expr == prev->position_.offsetY.expr +
                      prev->size_.height.expr + prev->edgesSpace_.bottom * parent->scaleInfo_.spaceScale.expr);
        }

        if (i == parent->childNode.size() - 1) {
            context.engine.add(child->position_.offsetY.expr + child->size_.height.expr + 1 >=
                parent->position_.offsetY.expr + parent->size_.height.expr);
            context.engine.add(child->position_.offsetY.expr + child->size_.height.expr <=
                parent->position_.offsetY.expr + parent->size_.height.expr);
        }
    }
}

void SmartLayoutAlgorithm::addRowLayout(LayoutContext& context, std::shared_ptr<SmartLayoutNode> parent)
{
    float sumOfAllChildWidth = GetSumOfAllChildWidth(parent);
    float maxChildHeight = 0;
    for (const auto& child : parent->childNode) {
        maxChildHeight = std::max(maxChildHeight, child->size_.height.value);
    }

    float mainAxisScale = 1.0f;
    if (sumOfAllChildWidth > parent->size_.width.value && sumOfAllChildWidth > 0) {
        mainAxisScale = parent->size_.width.value / sumOfAllChildWidth;
    }
    float crossAxisScale = 1.0f;
    if (maxChildHeight > parent->size_.height.value && maxChildHeight > 0) {
        crossAxisScale = parent->size_.height.value / maxChildHeight;
    }
    float finalScale = std::min(mainAxisScale, crossAxisScale);
    context.engine.add(parent->scaleInfo_.sizeScale.expr == static_cast<double>(finalScale));

    context.engine.add(parent->scaleInfo_.spaceScale.expr >= 0);
    context.engine.add(parent->scaleInfo_.spaceScale.expr <= 1);

    for (size_t i = 0; i < parent->childNode.size(); ++i) {
        std::shared_ptr<SmartLayoutNode> child = parent->childNode[i];
        context.engine.add(child->size_.width.expr ==
            static_cast<double>(child->size_.width.value) * parent->scaleInfo_.sizeScale.expr);
        context.engine.add(child->size_.height.expr ==
            static_cast<double>(child->size_.height.value) * parent->scaleInfo_.sizeScale.expr);

        AddDefaultConstraints(context.engine, parent, child);

        if (context.verticalAlign_ == VerticalAlign::TOP) {
            context.engine.add(child->position_.offsetY.expr == parent->position_.offsetY.expr);
        } else if (context.verticalAlign_ == VerticalAlign::CENTER) {
            context.engine.add(child->position_.offsetY.expr - parent->position_.offsetY.expr ==
                      parent->position_.offsetY.expr + parent->size_.height.expr -
                      child->position_.offsetY.expr - child->size_.height.expr);
        } else if (context.verticalAlign_ == VerticalAlign::BOTTOM) {
            context.engine.add(child->position_.offsetY.expr + child->size_.height.expr ==
                parent->position_.offsetY.expr + parent->size_.height.expr);
        }

        if (i == 0) {
            context.engine.add(child->position_.offsetX.expr == parent->position_.offsetX.expr +
                child->edgesSpace_.left * parent->scaleInfo_.spaceScale.expr);
        } else {
            std::shared_ptr<SmartLayoutNode> prev = parent->childNode[i - 1];
            context.engine.add(child->position_.offsetX.expr == prev->position_.offsetX.expr +
                prev->size_.width.expr + prev->edgesSpace_.right * parent->scaleInfo_.spaceScale.expr);
        }

        if (i == parent->childNode.size() - 1) {
            context.engine.add(child->position_.offsetX.expr + child->size_.width.expr +
                child->edgesSpace_.right * parent->scaleInfo_.spaceScale.expr + 1 >=
                parent->position_.offsetX.expr + parent->size_.width.expr);
            context.engine.add(child->position_.offsetX.expr + child->size_.width.expr +
                child->edgesSpace_.right * parent->scaleInfo_.spaceScale.expr <=
                parent->position_.offsetX.expr + parent->size_.width.expr);
        }
    }
}

SizeF SmartLayoutAlgorithm::ItermScale(const RefPtr<LayoutWrapper>& iterm, float scale)
{
    Dimension offsetX(0.0, DimensionUnit::PERCENT);
    Dimension offsetY(0.0, DimensionUnit::PERCENT);
    iterm->GetHostNode()->GetRenderContext()->UpdateTransformCenter(DimensionOffset(offsetX, offsetY));
    iterm->GetHostNode()->GetRenderContext()->UpdateTransformScale({ scale, scale });
    return {0, 0};
}

float SmartLayoutAlgorithm::GetSumOfAllChildHeight(std::shared_ptr<SmartLayoutNode> parent)
{
    float sumOfAllChildHeight = 0;

    for (size_t i = 0; i < parent->childNode.size(); ++i) {
        std::shared_ptr<SmartLayoutNode> child = parent->childNode[i];
        sumOfAllChildHeight += child->size_.height.value;
    }
    return sumOfAllChildHeight;
}

float SmartLayoutAlgorithm::GetSumOfAllChildWidth(std::shared_ptr<SmartLayoutNode> parent)
{
    float sumOfAllChildWidth = 0;

    for (size_t i = 0; i < parent->childNode.size(); ++i) {
        std::shared_ptr<SmartLayoutNode> child = parent->childNode[i];
        sumOfAllChildWidth += child->size_.width.value;
    }
    return sumOfAllChildWidth;
}

bool SmartLayoutAlgorithm::IsColumnSpaceEnough(LayoutWrapper* layoutWrapper)
{
    uint32_t sumOfAllChildHeight = 0;

    const auto& children = layoutWrapper->GetAllChildrenWithBuild(false);
    if (children.empty()) {
        return true;
    }

    for (const auto& child : children) {
        auto childSize = child->GetGeometryNode()->GetFrameSize();
        sumOfAllChildHeight += childSize.Height();
    }

    return sumOfAllChildHeight <= layoutWrapper->GetGeometryNode()->GetFrameSize().Height();
}

float CalculateSpaceBetween(const RefPtr<LayoutWrapper>& child1, const RefPtr<LayoutWrapper>& child2,
    LayoutType layoutType)
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

void SmartLayoutAlgorithm::PerformSmartLayout(LayoutWrapper* layoutWrapper, LayoutType layoutType)
{
    LayoutContext context;
    // Initialize context and children.
    InitializeLayoutContext(context, layoutWrapper, layoutType);
    ProcessLayoutChildren(context, layoutWrapper);

    // Apply constraints and solve.
    ApplyLayoutConstraints(context);
    if (!SolveLayout(context)) {
        LOGE("cr_debug localsmt failed to find solution!");
        return;
    }

    // Apply solved result.
    ApplyLayoutResults(context, layoutWrapper);
}

bool SmartLayoutAlgorithm::InitializeLayoutContext(LayoutContext& context, LayoutWrapper* layoutWrapper,
    LayoutType layoutType)
{
    const auto& children = layoutWrapper->GetAllChildrenWithBuild(false);
    if (children.empty()) {
        return false;
    }

    context.layoutType = layoutType;

    // Read layout alignment properties.
    auto layoutProperty = AceType::DynamicCast<FlexLayoutProperty>(layoutWrapper->GetLayoutProperty());
    if (layoutProperty) {
        context.mainAxisAlign_ = layoutProperty->GetMainAxisAlignValue(FlexAlign::FLEX_START);
        const auto crossAxisAlign = layoutProperty->GetCrossAxisAlignValue(FlexAlign::CENTER);
        switch (crossAxisAlign) {
            case FlexAlign::FLEX_START:
                context.horizontalAlign_ = HorizontalAlign::START;
                context.verticalAlign_ = VerticalAlign::TOP;
                break;
            case FlexAlign::FLEX_END:
                context.horizontalAlign_ = HorizontalAlign::END;
                context.verticalAlign_ = VerticalAlign::BOTTOM;
                break;
            case FlexAlign::CENTER:
            default:
                context.horizontalAlign_ = HorizontalAlign::CENTER;
                context.verticalAlign_ = VerticalAlign::CENTER;
                break;
        }
    }

    // Read parent size.
    if (layoutWrapper->GetGeometryNode()) {
        context.parentSize = layoutWrapper->GetGeometryNode()->GetFrameSize();
    } else {
        context.parentSize = SizeF(0, 0);
        return false;
    }

    // Create root node.
    context.root = std::make_shared<SmartLayoutNode>(context.engine, "root");
    context.root->setFixedSizeConstraint(context.engine, context.parentSize.Width(), context.parentSize.Height());
    return true;
}

void SmartLayoutAlgorithm::ProcessLayoutChildren(LayoutContext& context, LayoutWrapper* layoutWrapper)
{
    const auto& children = layoutWrapper->GetAllChildrenWithBuild(false);
    
    for (auto it = children.begin(); it != children.end(); ++it) {
        const auto& child = *it;
        std::string childName = "child_" + std::to_string(child->GetHostNode()->GetId());
        auto item = std::make_shared<SmartLayoutNode>(context.engine, childName);

        bool isFirst = (it == children.begin());
        auto nextIt = std::next(it);
        bool isLast = (nextIt == children.end());

        // 设置初始间距
        if (isFirst) {
            if (context.layoutType == LayoutType::COLUMN) {
                item->edgesSpace_.top = child->GetGeometryNode()->GetFrameOffset().GetY();
            } else {
                item->edgesSpace_.left = child->GetGeometryNode()->GetFrameOffset().GetX();
            }
        }

        // 计算相邻间距
        if (!isLast) {
            const auto& nextChild = *nextIt;
            float spaceBetween = CalculateSpaceBetween(child, nextChild, context.layoutType);
            if (context.layoutType == LayoutType::COLUMN) {
                item->edgesSpace_.bottom = spaceBetween;
                if (!isFirst) {
                    item->edgesSpace_.top = context.childrenLayoutNode.back()->edgesSpace_.bottom;
                }
            } else {
                item->edgesSpace_.right = spaceBetween;
                if (!isFirst) {
                    item->edgesSpace_.left = context.childrenLayoutNode.back()->edgesSpace_.right;
                }
            }
        }

        // Set trailing space.
        if (isLast) {
            if (context.layoutType == LayoutType::COLUMN) {
                item->edgesSpace_.bottom =
                    context.parentSize.Height() - child->GetGeometryNode()->GetFrameOffset().GetY() -
                    child->GetGeometryNode()->GetFrameSize().Height();
            } else {
                item->edgesSpace_.right =
                    context.parentSize.Width() - child->GetGeometryNode()->GetFrameOffset().GetX() -
                    child->GetGeometryNode()->GetFrameSize().Width();
            }
        }

        // 设置尺寸信息
        auto childSize = child->GetGeometryNode()->GetFrameSize();
        item->size_.width.value = childSize.Width();
        item->size_.height.value = childSize.Height();

        // 处理空白节点
        if (child->GetHostTag() == "Blank") {
            if (context.layoutType == LayoutType::COLUMN) {
                item->edgesSpace_.bottom += item->size_.height.value;
                item->size_.height.value = 0;
            } else {
                item->edgesSpace_.right += item->size_.width.value;
                item->size_.width.value = 0;
            }
        }
        context.childrenLayoutNode.push_back(item);
    }

    if (!context.childrenLayoutNode.empty()) {
        context.root->setChildren(context.childrenLayoutNode);
    }
}

void SmartLayoutAlgorithm::ApplyLayoutConstraints(LayoutContext& context)
{
    if (context.layoutType == LayoutType::COLUMN) {
        addColumnLayout(context, context.root);
    } else {
        addRowLayout(context, context.root);
    }
}

bool SmartLayoutAlgorithm::SolveLayout(LayoutContext& context)
{
    auto Constraints = context.engine.getConstraints();
    LOGE("cr_debug: Constraints:");
    for (auto c : Constraints) {
        LOGE("  cr_debug: %{public}s", c.expr.c_str());
    }
    auto result = context.engine.solve();
    if (result == false) {
        LOGE("cr_debug localsmt failed to find solution!");
    } else {
        context.root->syncData(context.engine);
        LOGE("Variables: root scale:%{public}f", context.root->scaleInfo_.sizeScale.value);
        
        LOGE("===== Solver constraints =====");
        auto results = context.engine.getResults();
        LOGE("Engine has solved %zu variables:", results.size());
        for (const auto& var : results) {
            LOGE("  %s = %f", var.name.c_str(), var.value);
        }
    }
    return result;
}

void SmartLayoutAlgorithm::ApplyLayoutResults(LayoutContext& context, LayoutWrapper* layoutWrapper)
{
    uint32_t index = 0;
    const auto& children = layoutWrapper->GetAllChildrenWithBuild(false);
    for (const auto& child : children) {
        context.childrenLayoutNode[index]->syncData(context.engine);

        // Calculate final offset.
        float offset1 = 0;
        float offset2 = 0;
        if (context.layoutType == LayoutType::COLUMN) {
            // 列布局：使用左边距
            if (child->GetGeometryNode()->GetMargin()) {
                offset1 = child->GetGeometryNode()->GetMargin()->left.value_or(0);
            }
        } else {
            // 行布局：使用上边距
            if (child->GetGeometryNode()->GetMargin()) {
                offset2 = child->GetGeometryNode()->GetMargin()->top.value_or(0);
            }
        }
        
        OffsetF offset(context.childrenLayoutNode[index]->position_.offsetX.value + offset1,
                      context.childrenLayoutNode[index]->position_.offsetY.value + offset2);
        child->GetGeometryNode()->SetFrameOffset(offset);

        ItermScale(child, context.root->scaleInfo_.sizeScale.value);
        if (child->GetHostNode() != nullptr) {
            child->GetHostNode()->MarkDirtyNode(NG::PROPERTY_UPDATE_LAYOUT);
        }
        child->Layout();
        
        LOGE("Variables: name:%{public}s [%{public}f %{public}f %{public}f %{public}f]",
             context.childrenLayoutNode[index]->name_.c_str(),
             context.childrenLayoutNode[index]->position_.offsetX.value,
             context.childrenLayoutNode[index]->position_.offsetY.value,
             context.childrenLayoutNode[index]->size_.width.value,
             context.childrenLayoutNode[index]->size_.height.value);
        index++;
    }
}

void SmartLayoutAlgorithm::SmartColumnLayout(LayoutWrapper* layoutWrapper)
{
    PerformSmartLayout(layoutWrapper, LayoutType::COLUMN);
}

void SmartLayoutAlgorithm::SmartRowLayout(LayoutWrapper* layoutWrapper)
{
    PerformSmartLayout(layoutWrapper, LayoutType::ROW);
}

} // namespace OHOS::Ace::NG
