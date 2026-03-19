#include "smart_layout_algorithm.h"

#include <algorithm>
#include <cmath>

namespace HHHH::HHH::HH {

namespace {

// static constexpr double MIN_SIZE_SCALE = 0.0;

float Clamp01(float value)
{
    return std::max(0.0f, std::min(1.0f, value));
}

// 新增逻辑：主轴按“1-loss”目标等价计算可保留的 spaceScale 上界。
float ComputeMainSpaceScalePreferred(
    const std::shared_ptr<SmartLayoutNode>& parent, LayoutType layoutType, float sizeScale)
{
    const bool isColumn = (layoutType == LayoutType::COLUMN);
    const float parentMainSize = isColumn ? parent->size_.height.value : parent->size_.width.value;

    float totalScaledMainSize = 0.0f;
    float totalMainGap = 0.0f;
    for (size_t i = 0; i < parent->childNode.size(); ++i) {
        const auto& child = parent->childNode[i];
        const float childMainSize = isColumn ? child->size_.height.value : child->size_.width.value;
        totalScaledMainSize += std::max(childMainSize, 0.0f) * sizeScale;

        const float childMainPos = isColumn ? child->position_.offsetY.value : child->position_.offsetX.value;
        const float parentMainPos = isColumn ? parent->position_.offsetY.value : parent->position_.offsetX.value;
        if (i == 0) {
            totalMainGap += std::max(0.0f, childMainPos - parentMainPos);
            continue;
        }
        const auto& prev = parent->childNode[i - 1];
        const float prevMainPos = isColumn ? prev->position_.offsetY.value : prev->position_.offsetX.value;
        const float prevMainSize = isColumn ? prev->size_.height.value : prev->size_.width.value;
        totalMainGap += std::max(0.0f, childMainPos - (prevMainPos + prevMainSize));
    }

    const float remainForGap = parentMainSize - totalScaledMainSize;
    if (totalMainGap <= 1e-6f) {
        return (remainForGap >= 0.0f) ? 1.0f : 0.0f;
    }
    return Clamp01(remainForGap / totalMainGap);
}

// 新增逻辑：单个子项在侧轴上可行的 crossSpaceScale 区间求解。
float ComputeCrossScaleBoundsForChild(
    float baselineSolved, float restoredGap, float parentCrossSize, float childScaledCrossSize, float& lowerBound)
{
    float upperBound = 1.0f;
    if (std::abs(restoredGap) <= 1e-6f) {
        if (baselineSolved < -1e-6f || baselineSolved + childScaledCrossSize > parentCrossSize + 1e-6f) {
            lowerBound = 1.0f;
            return 0.0f;
        }
        return upperBound;
    }

    const float minPos = 0.0f;
    const float maxPos = parentCrossSize - childScaledCrossSize;
    float c1 = (minPos - baselineSolved) / restoredGap;
    float c2 = (maxPos - baselineSolved) / restoredGap;
    float localLower = std::min(c1, c2);
    float localUpper = std::max(c1, c2);

    lowerBound = std::max(lowerBound, localLower);
    upperBound = std::min(upperBound, localUpper);
    return upperBound;
}

// 新增逻辑：侧轴按“1-loss”目标等价计算可保留的 crossSpaceScale 上界。
float ComputeCrossSpaceScalePreferred(
    const std::shared_ptr<SmartLayoutNode>& parent, LayoutType layoutType, float sizeScale, HorizontalAlign hAlign,
    VerticalAlign vAlign)
{
    const bool isColumn = (layoutType == LayoutType::COLUMN);
    const float parentCrossSize = isColumn ? parent->size_.width.value : parent->size_.height.value;

    float globalLower = 0.0f;
    float globalUpper = 1.0f;

    for (const auto& child : parent->childNode) {
        const float childCrossRaw = isColumn ? child->size_.width.value : child->size_.height.value;
        const float childCrossScaled = childCrossRaw * sizeScale;
        const float childCrossPosRaw = isColumn ? child->position_.offsetX.value : child->position_.offsetY.value;

        float baselineRaw = 0.0f;
        float baselineSolved = 0.0f;
        if (isColumn) {
            if (hAlign == HorizontalAlign::CENTER) {
                baselineRaw = (parentCrossSize - childCrossRaw) * 0.5f;
                baselineSolved = (parentCrossSize - childCrossScaled) * 0.5f;
            } else if (hAlign == HorizontalAlign::END) {
                baselineRaw = parentCrossSize - childCrossRaw;
                baselineSolved = parentCrossSize - childCrossScaled;
            }
        } else {
            if (vAlign == VerticalAlign::CENTER) {
                baselineRaw = (parentCrossSize - childCrossRaw) * 0.5f;
                baselineSolved = (parentCrossSize - childCrossScaled) * 0.5f;
            } else if (vAlign == VerticalAlign::BOTTOM) {
                baselineRaw = parentCrossSize - childCrossRaw;
                baselineSolved = parentCrossSize - childCrossScaled;
            }
        }

        const float restoredGap = childCrossPosRaw - baselineRaw;
        float lower = globalLower;
        const float upper = ComputeCrossScaleBoundsForChild(
            baselineSolved, restoredGap, parentCrossSize, childCrossScaled, lower);
        globalLower = lower;
        globalUpper = std::min(globalUpper, upper);
    }

    if (globalUpper < globalLower) {
        return 0.0f;
    }
    return Clamp01(globalUpper);
}

} // namespace

void SmartLayoutAlgorithm::AddDefaultConstraints(
    localsmt::Engine& engine, std::shared_ptr<SmartLayoutNode> parent, std::shared_ptr<SmartLayoutNode> child)
{
    // 1. 控制元素尺寸不小于 0
    engine.add(child->size_.width.expr >= 0);
    engine.add(child->size_.height.expr >= 0);
    engine.add(child->position_.offsetX.expr >= 0);
    engine.add(child->position_.offsetY.expr >= 0);
    engine.add(parent->position_.offsetX.expr >= 0);
    engine.add(parent->position_.offsetY.expr >= 0);

    // 子控件不能超出父控件内部
    engine.add(child->position_.offsetX.expr >= parent->position_.offsetX.expr);
    engine.add(child->position_.offsetY.expr >= parent->position_.offsetY.expr);
    engine.add(child->position_.offsetX.expr + child->size_.width.expr <=
        parent->position_.offsetX.expr + parent->size_.width.expr);
    engine.add(child->position_.offsetY.expr + child->size_.height.expr <=
        parent->position_.offsetY.expr + parent->size_.height.expr);
}

void SmartLayoutAlgorithm::AddCrossAxisAlignmentConstraints(
    localsmt::Engine& engine,
    std::shared_ptr<SmartLayoutNode> parent,
    std::shared_ptr<SmartLayoutNode> child,
    LayoutType layoutType)
{
    if (layoutType == LayoutType::COLUMN) {
        const float parentCrossPosBase = parent->position_.offsetX.value;
        const float parentCrossSizeBase = parent->size_.width.value;
        const float childCrossPosBase = child->position_.offsetX.value;
        const float childCrossSizeBase = child->size_.width.value;

        float baselineBase = parentCrossPosBase;
        localsmt::Expr baselineExpr = parent->position_.offsetX.expr;
        if (horizontalAlign_ == HorizontalAlign::CENTER) {
            baselineBase = parentCrossPosBase + (parentCrossSizeBase - childCrossSizeBase) * 0.5f;
            baselineExpr = parent->position_.offsetX.expr + (parent->size_.width.expr - child->size_.width.expr) / 2.0;
        } else if (horizontalAlign_ == HorizontalAlign::END) {
            baselineBase = parentCrossPosBase + parentCrossSizeBase - childCrossSizeBase;
            baselineExpr = parent->position_.offsetX.expr + parent->size_.width.expr - child->size_.width.expr;
        }

        const float restoredGap = childCrossPosBase - baselineBase;
        engine.add(child->position_.offsetX.expr ==
            baselineExpr + static_cast<double>(restoredGap) * parent->scaleInfo_.crossSpaceScale.expr);
        return;
    }

    const float parentCrossPosBase = parent->position_.offsetY.value;
    const float parentCrossSizeBase = parent->size_.height.value;
    const float childCrossPosBase = child->position_.offsetY.value;
    const float childCrossSizeBase = child->size_.height.value;

    float baselineBase = parentCrossPosBase;
    localsmt::Expr baselineExpr = parent->position_.offsetY.expr;
    if (verticalAlign_ == VerticalAlign::CENTER) {
        baselineBase = parentCrossPosBase + (parentCrossSizeBase - childCrossSizeBase) * 0.5f;
        baselineExpr = parent->position_.offsetY.expr + (parent->size_.height.expr - child->size_.height.expr) / 2.0;
    } else if (verticalAlign_ == VerticalAlign::BOTTOM) {
        baselineBase = parentCrossPosBase + parentCrossSizeBase - childCrossSizeBase;
        baselineExpr = parent->position_.offsetY.expr + parent->size_.height.expr - child->size_.height.expr;
    }

    const float restoredGap = childCrossPosBase - baselineBase;
    engine.add(child->position_.offsetY.expr ==
        baselineExpr + static_cast<double>(restoredGap) * parent->scaleInfo_.crossSpaceScale.expr);
}

void SmartLayoutAlgorithm::addLinearLayout(
    LayoutContext& context, std::shared_ptr<SmartLayoutNode> parent, LayoutType layoutType)
{
    auto& engine = context.engine;
    const bool isColumn = (layoutType == LayoutType::COLUMN);

    // 新增逻辑：以 localsmt 结构承载 z3 目标语义（maximize(1-loss)）的等价闭式结果。
    const float sizeScalePreferred = Clamp01(mergedSizeScaleUpperBound_);
    const float mainSpaceScalePreferred = ComputeMainSpaceScalePreferred(parent, layoutType, sizeScalePreferred);
    const float crossSpaceScalePreferred =
        ComputeCrossSpaceScalePreferred(parent, layoutType, sizeScalePreferred, horizontalAlign_, verticalAlign_);
    const float sizeLoss = 1.0f - sizeScalePreferred;
    const float mainSpaceLoss = 1.0f - mainSpaceScalePreferred;
    const float crossSpaceLoss = 1.0f - crossSpaceScalePreferred;
    (void)sizeLoss;
    (void)mainSpaceLoss;
    (void)crossSpaceLoss;

    // localsmt 无 objective 接口时，这里直接固定到“最大可保留比例”，
    // 等价于 maximize(1-loss)：
    // maximize(1-sizeLoss), maximize(1-mainSpaceLoss), maximize(1-crossSpaceLoss)。
    engine.add(parent->scaleInfo_.sizeScale.expr == static_cast<double>(sizeScalePreferred));
    engine.add(parent->scaleInfo_.spaceScale.expr == static_cast<double>(mainSpaceScalePreferred));
    engine.add(parent->scaleInfo_.crossSpaceScale.expr == static_cast<double>(crossSpaceScalePreferred));

    for (size_t i = 0; i < parent->childNode.size(); ++i) {
        const auto& child = parent->childNode[i];

        engine.add(child->size_.width.expr ==
            static_cast<double>(child->size_.width.value) * parent->scaleInfo_.sizeScale.expr);
        engine.add(child->size_.height.expr ==
            static_cast<double>(child->size_.height.value) * parent->scaleInfo_.sizeScale.expr);

        // 默认约束
        AddDefaultConstraints(engine, parent, child);
        // 侧轴对齐（保留原有方式）
        AddCrossAxisAlignmentConstraints(engine, parent, child, layoutType);

        if (i == 0) {
            const float leadingBaseGapRaw =
                (isColumn ? child->position_.offsetY.value : child->position_.offsetX.value) -
                (isColumn ? parent->position_.offsetY.value : parent->position_.offsetX.value);
            const float leadingBaseGap = (leadingBaseGapRaw > 0.0f) ? leadingBaseGapRaw : 0.0f;
            if (isColumn) {
                // 垂直约束保证元素顺序排列
                engine.add(child->position_.offsetY.expr ==
                    parent->position_.offsetY.expr + static_cast<double>(leadingBaseGap) * parent->scaleInfo_.spaceScale.expr);
            } else {
                // 2. 垂直约束保证元素分水平排列（按顺序，每个元素在前一元素右侧）
                engine.add(child->position_.offsetX.expr ==
                    parent->position_.offsetX.expr + static_cast<double>(leadingBaseGap) * parent->scaleInfo_.spaceScale.expr);
            }
            continue;
        }

        const auto& prev = parent->childNode[i - 1];
        const float childMainPosBase = isColumn ? child->position_.offsetY.value : child->position_.offsetX.value;
        const float prevMainPosBase = isColumn ? prev->position_.offsetY.value : prev->position_.offsetX.value;
        const float prevMainSizeBase = isColumn ? prev->size_.height.value : prev->size_.width.value;
        const float innerBaseGapRaw = childMainPosBase - (prevMainPosBase + prevMainSizeBase);
        const float innerBaseGap = (innerBaseGapRaw > 0.0f) ? innerBaseGapRaw : 0.0f;

        if (isColumn) {
            // 当前 y = 上一节点的底部 + 间距
            engine.add(child->position_.offsetY.expr == prev->position_.offsetY.expr + prev->size_.height.expr +
                static_cast<double>(innerBaseGap) * parent->scaleInfo_.spaceScale.expr);
        } else {
            // 当前 x = 前一节点右侧 + 间距
            engine.add(child->position_.offsetX.expr == prev->position_.offsetX.expr + prev->size_.width.expr +
                static_cast<double>(innerBaseGap) * parent->scaleInfo_.spaceScale.expr);
        }
    }
}

void SmartLayoutAlgorithm::addColumnLayout(LayoutContext& context, std::shared_ptr<SmartLayoutNode> parent)
{
    addLinearLayout(context, parent, LayoutType::COLUMN);
}

void SmartLayoutAlgorithm::addRowLayout(LayoutContext& context, std::shared_ptr<SmartLayoutNode> parent)
{
    addLinearLayout(context, parent, LayoutType::ROW);
}

SizeF SmartLayoutAlgorithm::ItermScale(const RefPtr<LayoutWrapper>& iterm, float scale)
{
    Dimension offsetX(0.0, DimensionUnit::PERCENT);
    Dimension offsetY(0.0, DimensionUnit::PERCENT);
    iterm->GetHostNode()->GetRenderContext()->UpdateTransformCenter(DimensionOffset(offsetX, offsetY));
    iterm->GetHostNode()->GetRenderContext()->UpdateTransformScale({ scale, scale });
    return { 0, 0 };
}

bool SmartLayoutAlgorithm::InitializeLayoutContext(
    LayoutContext& context, LayoutWrapper* layoutWrapper, LayoutType layoutType)
{
    const auto& children = layoutWrapper->GetAllChildrenWithBuild(false);
    if (children.empty()) {
        return false;
    }

    context.layoutType = layoutType;
    // 初始化布局属性
    auto layoutProperty = AceType::DynamicCast<FlexLayoutProperty>(layoutWrapper->GetLayoutProperty());
    if (!layoutProperty) {
        return false;
    }
    context.mainAxisAlign_ = layoutProperty->GetMainAxisAlignValue(FlexAlign::FLEX_START);
    context.crossAxisAlign_ = layoutProperty->GetCrossAxisAlignValue(FlexAlign::CENTER);

    switch (context.crossAxisAlign_) {
        case FlexAlign::FLEX_START:
            horizontalAlign_ = HorizontalAlign::START;
            verticalAlign_ = VerticalAlign::TOP;
            break;
        case FlexAlign::FLEX_END:
            horizontalAlign_ = HorizontalAlign::END;
            verticalAlign_ = VerticalAlign::BOTTOM;
            break;
        case FlexAlign::CENTER:
        default:
            horizontalAlign_ = HorizontalAlign::CENTER;
            verticalAlign_ = VerticalAlign::CENTER;
            break;
    }

    // 设置父容器尺寸
    if (!layoutWrapper->GetGeometryNode()) {
        return false;
    }
    context.parentSize = layoutWrapper->GetGeometryNode()->GetFrameSize();
    // 创建根节点
    context.root = std::make_shared<SmartLayoutNode>(context.engine, "root");
    context.root->setFixedSizeConstraint(context.engine, context.parentSize.Width(), context.parentSize.Height());
    context.root->size_.width.value = context.parentSize.Width();
    context.root->size_.height.value = context.parentSize.Height();
    return true;
}

void SmartLayoutAlgorithm::ProcessLayoutChildren(LayoutContext& context, LayoutWrapper* layoutWrapper)
{
    const auto& children = layoutWrapper->GetAllChildrenWithBuild(false);
    for (const auto& child : children) {
        std::string childName = "child_" + std::to_string(child->GetHostNode()->GetId());
        auto item = std::make_shared<SmartLayoutNode>(context.engine, childName);

        // 设置尺寸信息
        auto childOffset = child->GetGeometryNode()->GetFrameOffset();
        auto childSize = child->GetGeometryNode()->GetFrameSize();
        item->position_.offsetX.value = childOffset.GetX();
        item->position_.offsetY.value = childOffset.GetY();
        item->size_.width.value = childSize.Width();
        item->size_.height.value = childSize.Height();

        // 处理空白节点
        if (child->GetHostTag() == "Blank") {
            if (context.layoutType == LayoutType::COLUMN) {
                item->size_.height.value = 0;
            } else {
                item->size_.width.value = 0;
            }
        }
        context.childrenLayoutNode.push_back(item);
    }

    float sumMainSize = 0.0f;
    float maxCrossSize = 0.0f;
    for (const auto& childNode : context.childrenLayoutNode) {
        const float mainSize = (context.layoutType == LayoutType::COLUMN) ? childNode->size_.height.value : childNode->size_.width.value;
        const float crossSize = (context.layoutType == LayoutType::COLUMN) ? childNode->size_.width.value : childNode->size_.height.value;
        sumMainSize += std::max(mainSize, 0.0f);
        maxCrossSize = std::max(maxCrossSize, std::max(crossSize, 0.0f));
    }

    const float parentMainSize = (context.layoutType == LayoutType::COLUMN) ? context.parentSize.Height() : context.parentSize.Width();
    const float parentCrossSize = (context.layoutType == LayoutType::COLUMN) ? context.parentSize.Width() : context.parentSize.Height();
    mainSizeScale_ = (sumMainSize > 0.0f) ? Clamp01(parentMainSize / sumMainSize) : 1.0f;
    crossSizeScale_ = (maxCrossSize > 0.0f) ? Clamp01(parentCrossSize / maxCrossSize) : 1.0f;
    mergedSizeScaleUpperBound_ = std::min(mainSizeScale_, crossSizeScale_);

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
    // 求解布局
    bool result = context.engine.solve();
    if (!result) {
        return false;
    }
    context.root->syncData(context.engine);
    return true;
}

void SmartLayoutAlgorithm::ApplyLayoutResults(LayoutContext& context, LayoutWrapper* layoutWrapper)
{
    uint32_t index = 0;
    const auto& children = layoutWrapper->GetAllChildrenWithBuild(false);
    for (const auto& child : children) {
        context.childrenLayoutNode[index]->syncData(context.engine);

        // 计算偏移量
        float offsetX = context.childrenLayoutNode[index]->position_.offsetX.value;
        float offsetY = context.childrenLayoutNode[index]->position_.offsetY.value;

        child->GetGeometryNode()->SetFrameOffset(OffsetF(offsetX, offsetY));
        ItermScale(child, context.root->scaleInfo_.sizeScale.value);
        if (child->GetHostNode() != nullptr) {
            child->GetHostNode()->MarkDirtyNode(NG::PROPERTY_UPDATE_LAYOUT);
        }
        child->Layout();
        index++;
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
        return;
    }
    // 应用结果
    ApplyLayoutResults(context, layoutWrapper);
}

void SmartLayoutAlgorithm::SmartColumnLayout(LayoutWrapper* layoutWrapper)
{
    PerformSmartLayout(layoutWrapper, LayoutType::COLUMN);
}

void SmartLayoutAlgorithm::SmartRowLayout(LayoutWrapper* layoutWrapper)
{
    PerformSmartLayout(layoutWrapper, LayoutType::ROW);
}

} // namespace HHHH::HHH::HH
