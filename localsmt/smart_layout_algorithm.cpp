#include "smart_layout_algorithm.h"

#include <algorithm>

namespace HHHH::HHH::HH {

namespace {

float Clamp01(float value)
{
    return std::max(0.0f, std::min(1.0f, value));
}

float ComputeMergedSizeScale(const std::shared_ptr<SmartLayoutNode>& parent, LayoutType layoutType)
{
    const bool isColumn = (layoutType == LayoutType::COLUMN);
    const float parentMain = isColumn ? parent->size_.height.value : parent->size_.width.value;
    const float parentCross = isColumn ? parent->size_.width.value : parent->size_.height.value;

    float sumMain = 0.0f;
    float maxCross = 0.0f;
    for (const auto& child : parent->childNode) {
        const float mainSize = isColumn ? child->size_.height.value : child->size_.width.value;
        const float crossSize = isColumn ? child->size_.width.value : child->size_.height.value;
        sumMain += std::max(mainSize, 0.0f);
        maxCross = std::max(maxCross, std::max(crossSize, 0.0f));
    }

    const float mainScale = (sumMain > 0.0f) ? Clamp01(parentMain / sumMain) : 1.0f;
    const float crossScale = (maxCross > 0.0f) ? Clamp01(parentCross / maxCross) : 1.0f;
    return std::min(mainScale, crossScale);
}

float ComputeMainSpaceScale(const std::shared_ptr<SmartLayoutNode>& parent, LayoutType layoutType, float sizeScale)
{
    const bool isColumn = (layoutType == LayoutType::COLUMN);
    const float parentMain = isColumn ? parent->size_.height.value : parent->size_.width.value;

    float scaledMainSum = 0.0f;
    for (const auto& child : parent->childNode) {
        const float mainSize = isColumn ? child->size_.height.value : child->size_.width.value;
        scaledMainSum += std::max(mainSize, 0.0f) * sizeScale;
    }

    float gapBase = 0.0f;
    if (isColumn) {
        if (!parent->childNode.empty()) {
            gapBase += std::max(0.0f, parent->childNode.front()->edgesSpace_.top);
            for (size_t i = 1; i < parent->childNode.size(); ++i) {
                gapBase += std::max(0.0f, parent->childNode[i - 1]->edgesSpace_.bottom);
            }
        }
    } else {
        if (parent->childNode.size() > 1) {
            gapBase = static_cast<float>(parent->childNode.size() - 1);
        }
    }

    const float remain = parentMain - scaledMainSum;
    if (gapBase <= 0.0f) {
        return (remain >= 0.0f) ? 1.0f : 0.0f;
    }
    return Clamp01(remain / gapBase);
}

void ApplyFallbackLayout(LayoutWrapper* layoutWrapper, float scale)
{
    const auto& children = layoutWrapper->GetAllChildrenWithBuild(false);
    for (const auto& child : children) {
        child->GetGeometryNode()->SetFrameOffset(OffsetF(0.0f, 0.0f));
        if (child->GetHostNode() != nullptr) {
            Dimension offsetX(0.0, DimensionUnit::PERCENT);
            Dimension offsetY(0.0, DimensionUnit::PERCENT);
            child->GetHostNode()->GetRenderContext()->UpdateTransformCenter(DimensionOffset(offsetX, offsetY));
            child->GetHostNode()->GetRenderContext()->UpdateTransformScale({ scale, scale });
            child->GetHostNode()->MarkDirtyNode(NG::PROPERTY_UPDATE_LAYOUT);
        }
        child->Layout();
    }
}

} // namespace

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
    const float sizeScale = ComputeMergedSizeScale(parent, LayoutType::COLUMN);
    const float spaceScale = ComputeMainSpaceScale(parent, LayoutType::COLUMN, sizeScale);
    context.engine.add(parent->scaleInfo_.sizeScale.expr == static_cast<double>(sizeScale));

    context.engine.add(parent->scaleInfo_.spaceScale.expr >= 0);
    context.engine.add(parent->scaleInfo_.spaceScale.expr <= 1);
    context.engine.add(parent->scaleInfo_.spaceScale.expr == static_cast<double>(spaceScale));

    for (size_t i = 0; i < parent->childNode.size(); ++i) {
        std::shared_ptr<SmartLayoutNode> child = parent->childNode[i];
        context.engine.add(child->size_.width.expr ==
            static_cast<double>(child->size_.width.value) * parent->scaleInfo_.sizeScale.expr);
        context.engine.add(child->size_.height.expr ==
            static_cast<double>(child->size_.height.value) * parent->scaleInfo_.sizeScale.expr);

        // 默认约束
        AddDefaultConstraints(context.engine, parent, child);
        if (context.crossAxisAlign_ == FlexAlign::FLEX_START) {
            context.engine.add(child->position_.offsetX.expr == parent->position_.offsetX.expr);
        } else if (context.crossAxisAlign_ == FlexAlign::FLEX_END) {
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

        // 末尾不再做“必须贴底边”的硬等式，避免无解。
    }
}

void SmartLayoutAlgorithm::addRowLayout(LayoutContext& context, std::shared_ptr<SmartLayoutNode> parent)
{
    const float sizeScale = ComputeMergedSizeScale(parent, LayoutType::ROW);
    const float spaceScale = ComputeMainSpaceScale(parent, LayoutType::ROW, sizeScale);
    context.engine.add(parent->scaleInfo_.sizeScale.expr == static_cast<double>(sizeScale));
    context.engine.add(parent->scaleInfo_.spaceScale.expr >= 0);
    context.engine.add(parent->scaleInfo_.spaceScale.expr <= 1);
    context.engine.add(parent->scaleInfo_.spaceScale.expr == static_cast<double>(spaceScale));

    for (size_t i = 0; i < parent->childNode.size(); ++i) {
        std::shared_ptr<SmartLayoutNode> child = parent->childNode[i];

        context.engine.add(child->size_.width.expr ==
            static_cast<double>(child->size_.width.value) * parent->scaleInfo_.sizeScale.expr);
        context.engine.add(child->size_.height.expr ==
            static_cast<double>(child->size_.height.value) * parent->scaleInfo_.sizeScale.expr);

        // 默认约束
        AddDefaultConstraints(context.engine, parent, child);

        // 水平约束保证元素垂直对齐
        if (context.crossAxisAlign_ == FlexAlign::FLEX_START) {
            context.engine.add(child->position_.offsetY.expr == parent->position_.offsetY.expr);
        } else if (context.crossAxisAlign_ == FlexAlign::CENTER) {
            context.engine.add(child->position_.offsetY.expr - parent->position_.offsetY.expr ==
                      parent->position_.offsetY.expr + parent->size_.height.expr -
                      child->position_.offsetY.expr - child->size_.height.expr);
        } else if (context.crossAxisAlign_ == FlexAlign::FLEX_END) {
            context.engine.add(child->position_.offsetY.expr + child->size_.height.expr ==
                parent->position_.offsetY.expr + parent->size_.height.expr);
        }

        // 2. 垂直约束保证元素分水平排列（按顺序，每个元素在前一元素右侧）
        if (i == 0) {
                context.engine.add(child->position_.offsetX.expr == parent->position_.offsetX.expr);
        } else {
            std::shared_ptr<SmartLayoutNode> prev = parent->childNode[i - 1];
            // 当前 x = 前一节点右侧 + 间距
                context.engine.add(child->position_.offsetX.expr == prev->position_.offsetX.expr +
                      prev->size_.width.expr + parent->scaleInfo_.spaceScale.expr);
        }

        // 末尾不再做“必须贴右边”的硬等式，避免无解。
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
    // 初始化布局上下文
    if (!InitializeLayoutContext(context, layoutWrapper, layoutType)) {
        return;
    }
    // 处理子节点
    ProcessLayoutChildren(context, layoutWrapper);
    // 应用约束
    ApplyLayoutConstraints(context);
    // 求解布局
    if (!SolveLayout(context)) {
        LOGE("cr_debug localsmt failed to find solution!");
        // 兜底：保证布局阶段可继续，避免无解导致流程中断。
        ApplyFallbackLayout(layoutWrapper, 0.0f);
        return;
    }
    
    // 应用结果
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
    // 初始化布局属性
    auto layoutProperty = AceType::DynamicCast<FlexLayoutProperty>(layoutWrapper->GetLayoutProperty());
    if (layoutProperty) {
        context.mainAxisAlign_ = layoutProperty->GetMainAxisAlignValue(FlexAlign::FLEX_START);
        context.crossAxisAlign_ = layoutProperty->GetCrossAxisAlignValue(FlexAlign::CENTER);
    }

    // 设置父容器尺寸
    if (layoutWrapper->GetGeometryNode()) {
        context.parentSize = layoutWrapper->GetGeometryNode()->GetFrameSize();
    } else {
        context.parentSize = SizeF(0, 0);
        return false;
    }
    
    // 创建根节点
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
            item->edgesSpace_.bottom = CalculateSpaceBetween(child, nextChild, context.layoutType);
            if (!isFirst) {
                item->edgesSpace_.top = context.childrenLayoutNode.back()->edgesSpace_.bottom;
                item->edgesSpace_.left = context.childrenLayoutNode.back()->edgesSpace_.right;
            }
        }

        // 设置最终间距
        if (isLast) {
            if (context.layoutType == LayoutType::COLUMN) {
                item->edgesSpace_.bottom =
                    context.parentSize.Height() - child->GetGeometryNode()->GetFrameOffset().GetY() -
                    child->GetGeometryNode()->GetFrameSize().Height();
            }
        }

        // 设置尺寸信息
        auto childSize = child->GetGeometryNode()->GetFrameSize();
        item->size_.width.value = childSize.Width();
        item->size_.height.value = childSize.Height();

        // 处理空白节点
        if (child->GetHostTag() == "Blank") {
            item->edgesSpace_.bottom += item->size_.height.value;
            item->size_.height.value = 0;
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
        // 计算偏移量
        float offset1 = 0, offset2 = 0;
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
