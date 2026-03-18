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

// 计算两个相邻子节点在指定方向上的“原始间距”：
// - COLUMN：next.top - (curr.top + curr.height)
// - ROW：next.left - (curr.left + curr.width)
// 该值作为间距基线，后续会与 spaceScale 联动。
float CalculateSpaceBetween(const RefPtr<LayoutWrapper>& child1, const RefPtr<LayoutWrapper>& child2, LayoutType layoutType)
{
    auto offset1 = child1->GetGeometryNode()->GetFrameOffset();
    auto size1 = child1->GetGeometryNode()->GetFrameSize();
    auto offset2 = child2->GetGeometryNode()->GetFrameOffset();

    if (layoutType == LayoutType::COLUMN) {
        return offset2.GetY() - (offset1.GetY() + size1.Height());
    }
    return offset2.GetX() - (offset1.GetX() + size1.Width());
}

const char* LayoutTypeToString(LayoutType layoutType)
{
    switch (layoutType) {
        case LayoutType::COLUMN:
            return "COLUMN";
        case LayoutType::ROW:
            return "ROW";
        case LayoutType::FLEX:
            return "FLEX";
        default:
            return "UNKNOWN";
    }
}

const char* MainAlignToString(FlexAlign align)
{
    switch (align) {
        case FlexAlign::FLEX_START:
            return "FLEX_START";
        case FlexAlign::CENTER:
            return "CENTER";
        case FlexAlign::FLEX_END:
            return "FLEX_END";
        case FlexAlign::SPACE_BETWEEN:
            return "SPACE_BETWEEN";
        case FlexAlign::SPACE_AROUND:
            return "SPACE_AROUND";
        case FlexAlign::SPACE_EVENLY:
            return "SPACE_EVENLY";
        default:
            return "UNKNOWN";
    }
}

const char* CrossAlignToString(LayoutType layoutType, HorizontalAlign horizontalAlign, VerticalAlign verticalAlign)
{
    if (layoutType == LayoutType::COLUMN) {
        switch (horizontalAlign) {
            case HorizontalAlign::START:
                return "START";
            case HorizontalAlign::CENTER:
                return "CENTER";
            case HorizontalAlign::END:
                return "END";
            default:
                return "UNKNOWN";
        }
    }

    switch (verticalAlign) {
        case VerticalAlign::TOP:
            return "TOP";
        case VerticalAlign::CENTER:
            return "CENTER";
        case VerticalAlign::BOTTOM:
            return "BOTTOM";
        default:
            return "UNKNOWN";
    }
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
    const z3::expr marginLeft = Float2Expr(solver, child->marginSpace_.left) * parent->scaleInfo_.crossSpaceScale.expr;
    const z3::expr marginRight = Float2Expr(solver, child->marginSpace_.right) * parent->scaleInfo_.crossSpaceScale.expr;
    const z3::expr marginTop = Float2Expr(solver, child->marginSpace_.top) * parent->scaleInfo_.crossSpaceScale.expr;
    const z3::expr marginBottom = Float2Expr(solver, child->marginSpace_.bottom) * parent->scaleInfo_.crossSpaceScale.expr;
    if (layoutType == LayoutType::COLUMN) {
        if (horizontalAlign_ == HorizontalAlign::START) {
            solver.add(child->position_.offsetX.expr == parent->position_.offsetX.expr + marginLeft);
        } else if (horizontalAlign_ == HorizontalAlign::CENTER) {
            solver.add(child->position_.offsetX.expr - (parent->position_.offsetX.expr + marginLeft) ==
                (parent->position_.offsetX.expr + parent->size_.width.expr - marginRight) -
                (child->position_.offsetX.expr + child->size_.width.expr));
        } else if (horizontalAlign_ == HorizontalAlign::END) {
            solver.add(child->position_.offsetX.expr + child->size_.width.expr ==
                parent->position_.offsetX.expr + parent->size_.width.expr - marginRight);
        } else {
            solver.add(child->position_.offsetX.expr - (parent->position_.offsetX.expr + marginLeft) ==
                (parent->position_.offsetX.expr + parent->size_.width.expr - marginRight) -
                (child->position_.offsetX.expr + child->size_.width.expr));
        }
        return;
    }
    if (verticalAlign_ == VerticalAlign::TOP) {
        solver.add(child->position_.offsetY.expr == parent->position_.offsetY.expr + marginTop);
    } else if (verticalAlign_ == VerticalAlign::CENTER) {
        solver.add(child->position_.offsetY.expr - (parent->position_.offsetY.expr + marginTop) ==
            (parent->position_.offsetY.expr + parent->size_.height.expr - marginBottom) -
            (child->position_.offsetY.expr + child->size_.height.expr));
    } else if (verticalAlign_ == VerticalAlign::BOTTOM) {
        solver.add(child->position_.offsetY.expr + child->size_.height.expr ==
            parent->position_.offsetY.expr + parent->size_.height.expr - marginBottom);
    } else {
        solver.add(child->position_.offsetY.expr - (parent->position_.offsetY.expr + marginTop) ==
            (parent->position_.offsetY.expr + parent->size_.height.expr - marginBottom) -
            (child->position_.offsetY.expr + child->size_.height.expr));
    }
}
void SmartLayoutAlgorithm::AddMainAxisAlignmentConstraints(
    z3::optimize& solver,
    std::shared_ptr<SmartLayoutNode> parent,
    LayoutType layoutType,
    const z3::expr& mainAxisOffset,
    const z3::expr& betweenGap)
{
    // 【主轴对齐约束】
    // 处理“整体内容块（first..last）与父容器主轴边界”的关系。
    // 子项之间的顺序链式约束由 addLinearLayout 中逐项建立。
    if (parent->childNode.empty()) {
        return;
    }

    const auto& first = parent->childNode.front();
    const auto& last = parent->childNode.back();

    z3::expr startPos = (layoutType == LayoutType::COLUMN) ? first->position_.offsetY.expr : first->position_.offsetX.expr;
    z3::expr endPos = (layoutType == LayoutType::COLUMN)
        ? (last->position_.offsetY.expr + last->size_.height.expr)
        : (last->position_.offsetX.expr + last->size_.width.expr);

    z3::expr parentStart = (layoutType == LayoutType::COLUMN) ? parent->position_.offsetY.expr : parent->position_.offsetX.expr;
    z3::expr parentEnd = (layoutType == LayoutType::COLUMN)
        ? (parent->position_.offsetY.expr + parent->size_.height.expr)
        : (parent->position_.offsetX.expr + parent->size_.width.expr);
    const bool isSingleChild = (parent->childNode.size() == 1);

    // startPos/endPos：内容块在主轴上的起止位置。
    // parentStart/parentEnd：父容器在主轴上的起止边界。
    if (mainAxisAlign_ == FlexAlign::FLEX_START) {
        // 起始对齐：不引入额外偏移和等分间距。
        solver.add(mainAxisOffset == 0);
        solver.add(betweenGap == 0);
    } else if (mainAxisAlign_ == FlexAlign::CENTER) {
        // 居中：头尾留白相等。
        solver.add(startPos - parentStart == parentEnd - endPos);
        solver.add(betweenGap == 0);
    } else if (mainAxisAlign_ == FlexAlign::FLEX_END) {
        // 尾对齐：内容块末端贴父容器末端。
        solver.add(endPos == parentEnd);
        solver.add(betweenGap == 0);
    } else if (mainAxisAlign_ == FlexAlign::SPACE_BETWEEN) {
        if (isSingleChild) {
            // 单子项在 SPACE_BETWEEN 下退化为起始对齐，避免无解。
            solver.add(mainAxisOffset == 0);
            solver.add(betweenGap == 0);
        } else {
            // 两端贴边：内部间距等分由 betweenGap 承担。
            solver.add(startPos == parentStart);
            solver.add(endPos == parentEnd);
        }
    } else if (mainAxisAlign_ == FlexAlign::SPACE_AROUND) {
        // around：两端间距为内部间距的一半。
        auto halfGap = betweenGap / solver.ctx().real_val("2");
        solver.add(startPos - parentStart == halfGap);
        solver.add(parentEnd - endPos == halfGap);
    } else if (mainAxisAlign_ == FlexAlign::SPACE_EVENLY) {
        // evenly：两端间距与内部间距相等。
        solver.add(startPos - parentStart == betweenGap);
        solver.add(parentEnd - endPos == betweenGap);
    } else {
        // 未知主轴对齐按 FLEX_START 处理。
        solver.add(mainAxisOffset == 0);
        solver.add(betweenGap == 0);
    }
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

    // 主轴偏移量 + SPACE_* 间距变量。
    // 变量名带方向前缀，便于调试时在模型中区分 column/row。
    const std::string offsetVarName = parent->name_ + (isColumn ? ".columnMainAxisOffset" : ".rowMainAxisOffset");
    const std::string gapVarName = parent->name_ + (isColumn ? ".columnBetweenGap" : ".rowBetweenGap");
    z3::expr mainAxisOffset = solver.ctx().real_const(offsetVarName.c_str());
    z3::expr betweenGap = solver.ctx().real_const(gapVarName.c_str());
    solver.add(mainAxisOffset >= 0);
    solver.add(betweenGap >= 0);

    // SPACE_* 模式下，间距由 betweenGap 统一控制。
    const bool isSpaceAlign = (mainAxisAlign_ == FlexAlign::SPACE_BETWEEN ||
        mainAxisAlign_ == FlexAlign::SPACE_AROUND ||
        mainAxisAlign_ == FlexAlign::SPACE_EVENLY);

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
            // 首节点：
            // parentMainPos + leadingGap + mainAxisOffset
            const float leadingBaseGapRaw = isColumn ? child->edgesSpace_.top : child->edgesSpace_.left;
            const float leadingBaseGap = (leadingBaseGapRaw > 0.0f) ? leadingBaseGapRaw : 0.0f;
            z3::expr leadingGap = isSpaceAlign ? solver.ctx().real_val("0")
                : (Float2Expr(solver, leadingBaseGap) * parent->scaleInfo_.spaceScale.expr);
            solver.add(childMainPos == parentMainPos + leadingGap + mainAxisOffset);
            continue;
        }

        // 后续节点：
        // prevMainPos + prevMainSize + innerGap
        const auto& prev = parent->childNode[i - 1];
        z3::expr prevMainPos = isColumn ? prev->position_.offsetY.expr : prev->position_.offsetX.expr;
        z3::expr prevMainSize = isColumn ? prev->size_.height.expr : prev->size_.width.expr;
        const float innerBaseGapRaw = isColumn ? prev->edgesSpace_.bottom : prev->edgesSpace_.right;
        const float innerBaseGap = (innerBaseGapRaw > 0.0f) ? innerBaseGapRaw : 0.0f;
        z3::expr innerGap = isSpaceAlign ? betweenGap
            : (Float2Expr(solver, innerBaseGap) * parent->scaleInfo_.spaceScale.expr);
        solver.add(childMainPos == prevMainPos + prevMainSize + innerGap);
    }

    // 最后统一补主轴对齐约束（FLEX_*/SPACE_*）。
    AddMainAxisAlignmentConstraints(solver, parent, layoutType, mainAxisOffset, betweenGap);
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

bool SmartLayoutAlgorithm::IsColumnSpaceEnough(LayoutWrapper* layoutWrapper)
{
    // 快速估算：只按“子项高度总和 <= 父高度”判断是否有必要进求解器。
    // 注意：这里是轻量判定，不考虑复杂间距与对齐项。
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

bool SmartLayoutAlgorithm::IsColumnCrossSpaceEnough(LayoutWrapper* layoutWrapper)
{
    const auto& children = layoutWrapper->GetAllChildrenWithBuild(false);
    if (children.empty()) {
        return true;
    }

    const float parentWidth = layoutWrapper->GetGeometryNode()->GetFrameSize().Width();
    for (const auto& child : children) {
        const float childWidth = child->GetGeometryNode()->GetFrameSize().Width();
        float left = 0.0f;
        float right = 0.0f;
        if (child->GetGeometryNode()->GetMargin()) {
            const auto margin = child->GetGeometryNode()->GetMargin();
            const float leftRaw = margin->left.value_or(0);
            const float rightRaw = margin->right.value_or(0);
            left = (leftRaw > 0.0f) ? leftRaw : 0.0f;
            right = (rightRaw > 0.0f) ? rightRaw : 0.0f;
        }
        if (childWidth + left + right > parentWidth) {
            return false;
        }
    }
    return true;
}

bool SmartLayoutAlgorithm::IsRowSpaceEnough(LayoutWrapper* layoutWrapper)
{
    // 快速估算：只按“子项宽度总和 <= 父宽度”判断。
    uint32_t sumOfAllChildWidth = 0;
    const auto& children = layoutWrapper->GetAllChildrenWithBuild(false);
    if (children.empty()) {
        return true;
    }

    for (const auto& child : children) {
        auto childSize = child->GetGeometryNode()->GetFrameSize();
        sumOfAllChildWidth += childSize.Width();
    }

    return sumOfAllChildWidth <= layoutWrapper->GetGeometryNode()->GetFrameSize().Width();
}

bool SmartLayoutAlgorithm::IsRowCrossSpaceEnough(LayoutWrapper* layoutWrapper)
{
    const auto& children = layoutWrapper->GetAllChildrenWithBuild(false);
    if (children.empty()) {
        return true;
    }

    const float parentHeight = layoutWrapper->GetGeometryNode()->GetFrameSize().Height();
    for (const auto& child : children) {
        const float childHeight = child->GetGeometryNode()->GetFrameSize().Height();
        float top = 0.0f;
        float bottom = 0.0f;
        if (child->GetGeometryNode()->GetMargin()) {
            const auto margin = child->GetGeometryNode()->GetMargin();
            const float topRaw = margin->top.value_or(0);
            const float bottomRaw = margin->bottom.value_or(0);
            top = (topRaw > 0.0f) ? topRaw : 0.0f;
            bottom = (bottomRaw > 0.0f) ? bottomRaw : 0.0f;
        }
        if (childHeight + top + bottom > parentHeight) {
            return false;
        }
    }
    return true;
}

void SmartLayoutAlgorithm::PerformSmartLayout(LayoutWrapper* layoutWrapper, LayoutType layoutType)
{
    // 1：读取布局属性与对齐策略
    const char* layoutTypeStr = LayoutTypeToString(layoutType);

    auto layoutProperty = AceType::DynamicCast<FlexLayoutProperty>(layoutWrapper->GetLayoutProperty());
    CHECK_NULL_VOID(layoutProperty);

    mainAxisAlign_ = layoutProperty->GetMainAxisAlignValue(FlexAlign::FLEX_START);

    // 侧轴对齐根据布局方向映射到专用枚举：
    // - Column -> HorizontalAlign
    // - Row -> VerticalAlign
    FlexAlign crossAxisAlign = layoutProperty->GetCrossAxisAlignValue(FlexAlign::CENTER);
    switch (crossAxisAlign) {
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

    const auto& children = layoutWrapper->GetAllChildrenWithBuild(false);
    if (children.empty()) {
        return;
    }

    auto parentGeometry = layoutWrapper->GetGeometryNode();
    CHECK_NULL_VOID(parentGeometry);
    const auto parentOffset = parentGeometry->GetFrameOffset();
    const auto parentSize = parentGeometry->GetFrameSize();
    const int32_t parentId = layoutWrapper->GetHostNode() ? layoutWrapper->GetHostNode()->GetId() : -1;
    const char* mainAlignStr = MainAlignToString(mainAxisAlign_);
    const char* crossAlignStr = CrossAlignToString(layoutType, horizontalAlign_, verticalAlign_);

    // 3：创建求解器与根节点
    z3::context ctx;
    z3::optimize solver(ctx);

    // root 是“求解器侧父节点”，不是实际渲染节点。
    auto root = std::make_shared<SmartLayoutNode>(ctx, "root");
    root->setFixedSizeConstraint(solver, parentSize.Width(), parentSize.Height());

    std::vector<std::shared_ptr<SmartLayoutNode>> childrenLayoutNode;

    // 4：提取子项初始几何，构建求解节点
    for (auto it = children.begin(); it != children.end(); ++it) {
        const auto& child = *it;
        const std::string childName = "child_" + std::to_string(child->GetHostNode()->GetId());
        auto item = std::make_shared<SmartLayoutNode>(ctx, childName);

        const bool isFirst = (it == children.begin());
        const auto nextIt = std::next(it);
        const bool isLast = (nextIt == children.end());

        auto childOffset = child->GetGeometryNode()->GetFrameOffset();
        auto childSize = child->GetGeometryNode()->GetFrameSize();

        if (isFirst) {
            // 首节点的 top/left 作为头部间距基线。
            item->edgesSpace_.top = childOffset.GetY();
            item->edgesSpace_.left = childOffset.GetX();
        }

        if (!isLast) {
            // 非尾节点记录与下一节点之间的间距（纵向 + 横向都记录）。
            const auto& nextChild = *nextIt;
            item->edgesSpace_.bottom = CalculateSpaceBetween(child, nextChild, LayoutType::COLUMN);
            item->edgesSpace_.right = CalculateSpaceBetween(child, nextChild, LayoutType::ROW);

            if (!isFirst) {
                item->edgesSpace_.top = childrenLayoutNode.back()->edgesSpace_.bottom;
                item->edgesSpace_.left = childrenLayoutNode.back()->edgesSpace_.right;
            }
        }

        if (isLast) {
            // 尾节点记录到父容器尾边的剩余间距。
            item->edgesSpace_.bottom = parentSize.Height() - childOffset.GetY() - childSize.Height();
            item->edgesSpace_.right = parentSize.Width() - childOffset.GetX() - childSize.Width();
        }

        item->size_.width.value = childSize.Width();
        item->size_.height.value = childSize.Height();
        if (child->GetGeometryNode()->GetMargin()) {
            const auto margin = child->GetGeometryNode()->GetMargin();
            const float top = margin->top.value_or(0);
            const float bottom = margin->bottom.value_or(0);
            const float left = margin->left.value_or(0);
            const float right = margin->right.value_or(0);
            item->marginSpace_.top = (top > 0.0f) ? top : 0.0f;
            item->marginSpace_.bottom = (bottom > 0.0f) ? bottom : 0.0f;
            item->marginSpace_.left = (left > 0.0f) ? left : 0.0f;
            item->marginSpace_.right = (right > 0.0f) ? right : 0.0f;
        }

        if (child->GetHostTag() == "Blank") {
            // Blank 视为空白间距：把实体尺寸并入 gap，自身尺寸置 0。
            if (layoutType == LayoutType::COLUMN) {
                item->edgesSpace_.bottom += item->size_.height.value;
                item->size_.height.value = 0;
            } else {
                item->edgesSpace_.right += item->size_.width.value;
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

        // 回写前保留原逻辑：按方向附加 margin 偏移。
        // 侧轴 margin 已进入求解约束，不在回写阶段二次偏移。

        auto childId = child->GetHostNode() ? child->GetHostNode()->GetId() : -1;
        LOGE("smart_layout compact layout:%{public}s main:%{public}s cross:%{public}s "
             "childId:%{public}d parentId:%{public}d "
             "parentXYWH:[%{public}.2f %{public}.2f %{public}.2f %{public}.2f] "
             "childXYWH:[%{public}.2f %{public}.2f %{public}.2f %{public}.2f] "
             "spacescale:%{public}.4f crossspacescale:%{public}.4f sizescale:%{public}.4f",
            layoutTypeStr,
            mainAlignStr,
            crossAlignStr,
            childId,
            parentId,
            parentOffset.GetX(),
            parentOffset.GetY(),
            parentSize.Width(),
            parentSize.Height(),
            offsetX,
            offsetY,
            childrenLayoutNode[index]->size_.width.value,
            childrenLayoutNode[index]->size_.height.value,
            root->scaleInfo_.spaceScale.value,
            root->scaleInfo_.crossSpaceScale.value,
            root->scaleInfo_.sizeScale.value);

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

void SmartLayoutAlgorithm::SmartFlexLayout(LayoutWrapper* layoutWrapper, const SmartFlexConfig& config)
{
    smartFlexLayoutAlgorithm_.Layout(layoutWrapper, config);
}

} // namespace HHHH::HHH::HH
