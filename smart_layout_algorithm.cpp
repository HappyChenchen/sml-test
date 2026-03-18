#include "smart_layout_algorithm.h"

#include <algorithm>

namespace HHHH::HHH::HH {

namespace {

// 工具函数：将 float 常量转成 Z3 real 字面量。
// 统一通过该函数构造常量，避免不同位置使用不同精度/字符串形式。
z3::expr Float2Expr(z3::optimize& solver, float value)
{
    return solver.ctx().real_val(std::to_string(value).c_str());
}

float Clamp01(float value)
{
    return std::max(0.0f, std::min(1.0f, value));
}

} // namespace

void SmartLayoutAlgorithm::AddDefaultConstraints(
    z3::optimize& solver, std::shared_ptr<SmartLayoutNode> parent, std::shared_ptr<SmartLayoutNode> child)
{
    // 【默认约束】
    // 这组约束与布局方向无关，是任何求解都必须满足的几何底线：
    // 1) 节点尺寸、坐标非负；
    // 2) 子节点必须完全落在父节点边界内。
    solver.add(child->size_.width.expr >= 0);
    solver.add(child->size_.height.expr >= 0);
    solver.add(child->position_.offsetX.expr >= 0);
    solver.add(child->position_.offsetY.expr >= 0);
    solver.add(parent->position_.offsetX.expr >= 0);
    solver.add(parent->position_.offsetY.expr >= 0);

    // 子节点不能越界（包含四个方向约束）。
    solver.add(child->position_.offsetX.expr >= parent->position_.offsetX.expr);
    solver.add(child->position_.offsetY.expr >= parent->position_.offsetY.expr);
    solver.add(child->position_.offsetX.expr + child->size_.width.expr <=
        parent->position_.offsetX.expr + parent->size_.width.expr);
    solver.add(child->position_.offsetY.expr + child->size_.height.expr <=
        parent->position_.offsetY.expr + parent->size_.height.expr);
}

void SmartLayoutAlgorithm::AddCrossAxisAlignmentConstraints(
    z3::optimize& solver,
    std::shared_ptr<SmartLayoutNode> parent,
    std::shared_ptr<SmartLayoutNode> child,
    LayoutType layoutType)
{
    // 侧轴规则：不再使用对齐枚举与 margin，直接按“原始 offset 到父起点距离”缩放。
    // - COLUMN：缩放 X 方向前导间距
    // - ROW：缩放 Y 方向前导间距
    if (layoutType == LayoutType::COLUMN) {
        const float crossLeadingGapRaw = child->position_.offsetX.value - parent->position_.offsetX.value;
        const float crossLeadingGap = (crossLeadingGapRaw > 0.0f) ? crossLeadingGapRaw : 0.0f;
        solver.add(child->position_.offsetX.expr ==
            parent->position_.offsetX.expr +
            Float2Expr(solver, crossLeadingGap) * parent->scaleInfo_.crossSpaceScale.expr);
        return;
    }
    const float crossLeadingGapRaw = child->position_.offsetY.value - parent->position_.offsetY.value;
    const float crossLeadingGap = (crossLeadingGapRaw > 0.0f) ? crossLeadingGapRaw : 0.0f;
    solver.add(child->position_.offsetY.expr ==
        parent->position_.offsetY.expr +
        Float2Expr(solver, crossLeadingGap) * parent->scaleInfo_.crossSpaceScale.expr);
}
void SmartLayoutAlgorithm::addLinearLayout(
    z3::optimize& solver, std::shared_ptr<SmartLayoutNode> parent, LayoutType layoutType)
{
    // 【主轴建模核心（Column/Row 解耦后统一入口）】
    // 设计目的：
    // - 把 Column/Row 的重复建模逻辑合并为一份；
    // - 通过 isColumn 分流“主轴坐标、主轴尺寸、间距字段”的选取；
    // - 保证两种布局方向的求解流程一致，降低维护成本。
    const bool isColumn = (layoutType == LayoutType::COLUMN);

    // 目标 1：尽量保留原始尺寸（sizeScale 最大化）。
    solver.add(parent->scaleInfo_.sizeScale.expr > 0);
    solver.add(parent->scaleInfo_.sizeScale.expr <= 1);
    solver.add(parent->scaleInfo_.sizeScale.expr <= Float2Expr(solver, mergedSizeScaleUpperBound_));
    solver.maximize(parent->scaleInfo_.sizeScale.expr);

    // 目标 2：在尺寸最优前提下，尽量保留间距（spaceScale 最大化）。
    solver.add(parent->scaleInfo_.spaceScale.expr >= 0);
    solver.add(parent->scaleInfo_.spaceScale.expr <= 1);
    solver.maximize(parent->scaleInfo_.spaceScale.expr);
    solver.add(parent->scaleInfo_.crossSpaceScale.expr >= 0);
    solver.add(parent->scaleInfo_.crossSpaceScale.expr <= 1);
    solver.maximize(parent->scaleInfo_.crossSpaceScale.expr);

    const float parentMainSize = isColumn ? parent->size_.height.value : parent->size_.width.value;
    const float parentCrossSize = isColumn ? parent->size_.width.value : parent->size_.height.value;

    float totalMainSize = 0.0f;
    float totalMainGap = 0.0f;
    bool crossOverflow = false;
    for (size_t i = 0; i < parent->childNode.size(); ++i) {
        const auto& child = parent->childNode[i];
        const float childMainSize = isColumn ? child->size_.height.value : child->size_.width.value;
        const float childCrossSize = isColumn ? child->size_.width.value : child->size_.height.value;
        totalMainSize += std::max(childMainSize, 0.0f);

        const float childMainPos = isColumn ? child->position_.offsetY.value : child->position_.offsetX.value;
        const float parentMainPos = isColumn ? parent->position_.offsetY.value : parent->position_.offsetX.value;
        if (i == 0) {
            totalMainGap += std::max(0.0f, childMainPos - parentMainPos);
        } else {
            const auto& prev = parent->childNode[i - 1];
            const float prevMainPos = isColumn ? prev->position_.offsetY.value : prev->position_.offsetX.value;
            const float prevMainSize = isColumn ? prev->size_.height.value : prev->size_.width.value;
            totalMainGap += std::max(0.0f, childMainPos - (prevMainPos + prevMainSize));
        }

        const float childCrossPos = isColumn ? child->position_.offsetX.value : child->position_.offsetY.value;
        const float parentCrossPos = isColumn ? parent->position_.offsetX.value : parent->position_.offsetY.value;
        const float crossLead = std::max(0.0f, childCrossPos - parentCrossPos);
        if (crossLead + std::max(childCrossSize, 0.0f) > parentCrossSize) {
            crossOverflow = true;
        }
    }
    const bool mainOverflow = (totalMainSize + totalMainGap > parentMainSize);
    if (!mainOverflow) {
        solver.add(parent->scaleInfo_.spaceScale.expr == 1);
    }
    if (!crossOverflow) {
        solver.add(parent->scaleInfo_.crossSpaceScale.expr == 1);
    }

    for (size_t i = 0; i < parent->childNode.size(); ++i) {
        const auto& child = parent->childNode[i];

        // 所有子项尺寸统一按 sizeScale 缩放。
        solver.add(child->size_.width.expr == Float2Expr(solver, child->size_.width.value) * parent->scaleInfo_.sizeScale.expr);
        solver.add(child->size_.height.expr == Float2Expr(solver, child->size_.height.value) * parent->scaleInfo_.sizeScale.expr);

        // 通用约束 + 侧轴对齐
        AddDefaultConstraints(solver, parent, child);
        AddCrossAxisAlignmentConstraints(solver, parent, child, layoutType);

        // 根据方向定义“主轴坐标”表达式：
        // - Column: Y
        // - Row: X
        z3::expr childMainPos = isColumn ? child->position_.offsetY.expr : child->position_.offsetX.expr;
        z3::expr parentMainPos = isColumn ? parent->position_.offsetY.expr : parent->position_.offsetX.expr;

        if (i == 0) {
            // 首节点间距：首元素到父容器主轴起点距离。
            const float leadingBaseGapRaw = (isColumn ? child->position_.offsetY.value : child->position_.offsetX.value) -
                (isColumn ? parent->position_.offsetY.value : parent->position_.offsetX.value);
            const float leadingBaseGap = (leadingBaseGapRaw > 0.0f) ? leadingBaseGapRaw : 0.0f;
            z3::expr leadingGap = Float2Expr(solver, leadingBaseGap) * parent->scaleInfo_.spaceScale.expr;
            solver.add(childMainPos == parentMainPos + leadingGap);
            continue;
        }

        // 后续节点间距：当前 top/left 与上兄弟 bottom/right 的距离。
        const auto& prev = parent->childNode[i - 1];
        z3::expr prevMainPos = isColumn ? prev->position_.offsetY.expr : prev->position_.offsetX.expr;
        z3::expr prevMainSize = isColumn ? prev->size_.height.expr : prev->size_.width.expr;
        const float childMainPosBase = isColumn ? child->position_.offsetY.value : child->position_.offsetX.value;
        const float prevMainPosBase = isColumn ? prev->position_.offsetY.value : prev->position_.offsetX.value;
        const float prevMainSizeBase = isColumn ? prev->size_.height.value : prev->size_.width.value;
        const float innerBaseGapRaw = childMainPosBase - (prevMainPosBase + prevMainSizeBase);
        const float innerBaseGap = (innerBaseGapRaw > 0.0f) ? innerBaseGapRaw : 0.0f;
        z3::expr innerGap = Float2Expr(solver, innerBaseGap) * parent->scaleInfo_.spaceScale.expr;
        solver.add(childMainPos == prevMainPos + prevMainSize + innerGap);
    }
}

void SmartLayoutAlgorithm::addColumnLayout(z3::optimize& solver, std::shared_ptr<SmartLayoutNode> parent)
{
    addLinearLayout(solver, parent, LayoutType::COLUMN);
}

void SmartLayoutAlgorithm::addRowLayout(z3::optimize& solver, std::shared_ptr<SmartLayoutNode> parent)
{
    addLinearLayout(solver, parent, LayoutType::ROW);
}

SizeF SmartLayoutAlgorithm::ItermScale(const RefPtr<LayoutWrapper>& iterm, float scale)
{
    // 将节点缩放中心固定在左上角，再应用统一缩放比例。
    Dimension offsetX(0.0, DimensionUnit::PERCENT);
    Dimension offsetY(0.0, DimensionUnit::PERCENT);
    iterm->GetHostNode()->GetRenderContext()->UpdateTransformCenter(DimensionOffset(offsetX, offsetY));
    iterm->GetHostNode()->GetRenderContext()->UpdateTransformScale({ scale, scale });
    return { 0, 0 };
}

void SmartLayoutAlgorithm::PerformSmartLayout(LayoutWrapper* layoutWrapper, LayoutType layoutType)
{
    // 1：读取子节点
    const auto& children = layoutWrapper->GetAllChildrenWithBuild(false);
    if (children.empty()) {
        return;
    }

    auto parentGeometry = layoutWrapper->GetGeometryNode();
    CHECK_NULL_VOID(parentGeometry);
    const auto parentSize = parentGeometry->GetFrameSize();

    // 3：创建求解器与根节点
    z3::context ctx;
    z3::optimize solver(ctx);

    // root 是“求解器侧父节点”，不是实际渲染节点。
    auto root = std::make_shared<SmartLayoutNode>(ctx, "root");
    root->setFixedSizeConstraint(solver, parentSize.Width(), parentSize.Height());
    root->size_.width.value = parentSize.Width();
    root->size_.height.value = parentSize.Height();

    std::vector<std::shared_ptr<SmartLayoutNode>> childrenLayoutNode;

    // 4：提取子项初始几何，构建求解节点
    for (auto it = children.begin(); it != children.end(); ++it) {
        const auto& child = *it;
        const std::string childName = "child_" + std::to_string(child->GetHostNode()->GetId());
        auto item = std::make_shared<SmartLayoutNode>(ctx, childName);

        auto childOffset = child->GetGeometryNode()->GetFrameOffset();
        auto childSize = child->GetGeometryNode()->GetFrameSize();
        item->position_.offsetX.value = childOffset.GetX();
        item->position_.offsetY.value = childOffset.GetY();

        item->size_.width.value = childSize.Width();
        item->size_.height.value = childSize.Height();

        if (child->GetHostTag() == "Blank") {
            // Blank 视为空白间距：尺寸置 0，由后续“位置差值”转化为间距。
            if (layoutType == LayoutType::COLUMN) {
                item->size_.height.value = 0;
            } else {
                item->size_.width.value = 0;
            }
        }

        childrenLayoutNode.push_back(item);
    }

    float sumMainSize = 0.0f;
    float maxCrossSize = 0.0f;
    for (const auto& childNode : childrenLayoutNode) {
        const float mainSize = (layoutType == LayoutType::COLUMN) ? childNode->size_.height.value : childNode->size_.width.value;
        const float crossSize = (layoutType == LayoutType::COLUMN) ? childNode->size_.width.value : childNode->size_.height.value;
        sumMainSize += std::max(mainSize, 0.0f);
        maxCrossSize = std::max(maxCrossSize, std::max(crossSize, 0.0f));
    }
    const float parentMainSize = (layoutType == LayoutType::COLUMN) ? parentSize.Height() : parentSize.Width();
    const float parentCrossSize = (layoutType == LayoutType::COLUMN) ? parentSize.Width() : parentSize.Height();
    mainSizeScale_ = (sumMainSize > 0.0f) ? Clamp01(parentMainSize / sumMainSize) : 1.0f;
    crossSizeScale_ = (maxCrossSize > 0.0f) ? Clamp01(parentCrossSize / maxCrossSize) : 1.0f;
    mergedSizeScaleUpperBound_ = std::min(mainSizeScale_, crossSizeScale_);

    root->setChildren(childrenLayoutNode);

    // 5：布局方向
    if (layoutType == LayoutType::COLUMN) {
        addColumnLayout(solver, root);
    } else {
        addRowLayout(solver, root);
    }

    // 6：执行求解
    auto result = solver.check();
    if (result != z3::sat) {
        return;
    }

    // 7：回写求解结果
    z3::model m = solver.get_model();
    root->syncData(m);

    uint32_t index = 0;
    for (const auto& child : children) {
        childrenLayoutNode[index]->syncData(m);

        float offsetX = childrenLayoutNode[index]->position_.offsetX.value;
        float offsetY = childrenLayoutNode[index]->position_.offsetY.value;

        // 回写规则：直接使用求解坐标，不做额外 margin 偏移补偿。

        // 将求解坐标写回真实节点，并应用尺寸缩放。
        child->GetGeometryNode()->SetFrameOffset(OffsetF(offsetX, offsetY));
        ItermScale(child, root->scaleInfo_.sizeScale.value);
        if (child->GetHostNode() != nullptr) {
            child->GetHostNode()->MarkDirtyNode(NG::PROPERTY_UPDATE_LAYOUT);
        }
        child->Layout();

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

} // namespace HHHH::HHH::HH
