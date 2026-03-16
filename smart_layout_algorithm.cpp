#include <string>

#include "smart_layout_algorithm.h"

namespace HHHH::HHH::HH {

namespace {

// 工具函数：把 float 常量转换成 Z3 实数表达式
z3::expr Float2Expr(z3::optimize& solver, float value)
{
    return solver.ctx().real_val(std::to_string(value).c_str());
}

// 计算两个相邻子项之间的间距（按布局方向）
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

} // namespace

void SmartLayoutAlgorithm::AddDefaultConstraints(
    z3::optimize& solver, std::shared_ptr<SmartLayoutNode> parent, std::shared_ptr<SmartLayoutNode> child)
{
    // 1) 几何值必须非负
    solver.add(child->size_.width.expr >= 0);
    solver.add(child->size_.height.expr >= 0);
    solver.add(child->position_.offsetX.expr >= 0);
    solver.add(child->position_.offsetY.expr >= 0);
    solver.add(parent->position_.offsetX.expr >= 0);
    solver.add(parent->position_.offsetY.expr >= 0);

    // 2) 子组件必须落在父组件矩形内
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
    // Column 的交叉轴是 X，Row 的交叉轴是 Y。
    if (layoutType == LayoutType::COLUMN) {
        if (crossAxisAlign_ == FlexAlign::FLEX_START) {
            solver.add(child->position_.offsetX.expr == parent->position_.offsetX.expr);
        } else if (crossAxisAlign_ == FlexAlign::CENTER) {
            // 左右留白相等
            solver.add(child->position_.offsetX.expr - parent->position_.offsetX.expr ==
                parent->position_.offsetX.expr + parent->size_.width.expr -
                child->position_.offsetX.expr - child->size_.width.expr);
        } else if (crossAxisAlign_ == FlexAlign::FLEX_END) {
            solver.add(child->position_.offsetX.expr + child->size_.width.expr ==
                parent->position_.offsetX.expr + parent->size_.width.expr);
        }
        return;
    }

    if (crossAxisAlign_ == FlexAlign::FLEX_START) {
        solver.add(child->position_.offsetY.expr == parent->position_.offsetY.expr);
    } else if (crossAxisAlign_ == FlexAlign::CENTER) {
        solver.add(child->position_.offsetY.expr - parent->position_.offsetY.expr ==
            parent->position_.offsetY.expr + parent->size_.height.expr -
            child->position_.offsetY.expr - child->size_.height.expr);
    } else if (crossAxisAlign_ == FlexAlign::FLEX_END) {
        solver.add(child->position_.offsetY.expr + child->size_.height.expr ==
            parent->position_.offsetY.expr + parent->size_.height.expr);
    }
}

void SmartLayoutAlgorithm::AddMainAxisAlignmentConstraints(
    z3::optimize& solver,
    std::shared_ptr<SmartLayoutNode> parent,
    LayoutType layoutType,
    const z3::expr& mainAxisOffset,
    const z3::expr& betweenGap)
{
    if (parent->childNode.empty()) {
        return;
    }

    const auto& first = parent->childNode.front();
    const auto& last = parent->childNode.back();

    // startPos/endPos 分别代表内容块在主轴方向的起点/终点。
    z3::expr startPos = (layoutType == LayoutType::COLUMN) ? first->position_.offsetY.expr : first->position_.offsetX.expr;
    z3::expr endPos = (layoutType == LayoutType::COLUMN)
        ? (last->position_.offsetY.expr + last->size_.height.expr)
        : (last->position_.offsetX.expr + last->size_.width.expr);

    z3::expr parentStart = (layoutType == LayoutType::COLUMN) ? parent->position_.offsetY.expr : parent->position_.offsetX.expr;
    z3::expr parentEnd = (layoutType == LayoutType::COLUMN)
        ? (parent->position_.offsetY.expr + parent->size_.height.expr)
        : (parent->position_.offsetX.expr + parent->size_.width.expr);

    // mainAxisOffset 的作用：在保持相对顺序不变的前提下整体平移。
    if (mainAxisAlign_ == FlexAlign::FLEX_START) {
        solver.add(mainAxisOffset == 0);
        solver.add(betweenGap == 0);
    } else if (mainAxisAlign_ == FlexAlign::CENTER) {
        // 头部留白 == 尾部留白
        solver.add(startPos - parentStart == parentEnd - endPos);
        solver.add(betweenGap == 0);
    } else if (mainAxisAlign_ == FlexAlign::FLEX_END) {
        // 内容尾部贴紧父容器尾部
        solver.add(endPos == parentEnd);
        solver.add(betweenGap == 0);
    } else if (mainAxisAlign_ == FlexAlign::SPACE_BETWEEN) {
        // 首尾贴边，内部间隔相等（通过 betweenGap 统一控制）
        solver.add(startPos == parentStart);
        solver.add(endPos == parentEnd);
    } else if (mainAxisAlign_ == FlexAlign::SPACE_AROUND) {
        // 两端留白 = 内部间隔的一半
        auto halfGap = betweenGap / solver.ctx().real_val("2");
        solver.add(startPos - parentStart == halfGap);
        solver.add(parentEnd - endPos == halfGap);
    } else if (mainAxisAlign_ == FlexAlign::SPACE_EVENLY) {
        // 两端留白 = 内部间隔
        solver.add(startPos - parentStart == betweenGap);
        solver.add(parentEnd - endPos == betweenGap);
    } else {
        // 其他对齐类型暂时回退到 FLEX_START，避免行为不可控。
        solver.add(mainAxisOffset == 0);
        solver.add(betweenGap == 0);
    }
}

void SmartLayoutAlgorithm::addColumnLayout(z3::optimize& solver, std::shared_ptr<SmartLayoutNode> parent)
{
    // sizeScale：组件尺寸缩放系数，目标是尽可能大（尽量少缩小）
    solver.add(parent->scaleInfo_.sizeScale.expr > 0);
    solver.add(parent->scaleInfo_.sizeScale.expr <= 1);
    solver.maximize(parent->scaleInfo_.sizeScale.expr);

    // spaceScale：间距缩放系数
    solver.add(parent->scaleInfo_.spaceScale.expr >= 0);
    solver.add(parent->scaleInfo_.spaceScale.expr <= 1);

    // 主轴平移量：用于支持 mainAxisAlign_ 的 center/end。
    z3::expr mainAxisOffset =
        solver.ctx().real_const((parent->name_ + ".columnMainAxisOffset").c_str());
    solver.add(mainAxisOffset >= 0);
    z3::expr betweenGap =
        solver.ctx().real_const((parent->name_ + ".columnBetweenGap").c_str());
    solver.add(betweenGap >= 0);

    for (size_t i = 0; i < parent->childNode.size(); ++i) {
        const auto& child = parent->childNode[i];

        // 尺寸统一按 sizeScale 缩放
        solver.add(child->size_.width.expr == Float2Expr(solver, child->size_.width.value) * parent->scaleInfo_.sizeScale.expr);
        solver.add(child->size_.height.expr == Float2Expr(solver, child->size_.height.value) * parent->scaleInfo_.sizeScale.expr);

        AddDefaultConstraints(solver, parent, child);
        AddCrossAxisAlignmentConstraints(solver, parent, child, LayoutType::COLUMN);

        bool isSpaceAlign = (mainAxisAlign_ == FlexAlign::SPACE_BETWEEN ||
            mainAxisAlign_ == FlexAlign::SPACE_AROUND ||
            mainAxisAlign_ == FlexAlign::SPACE_EVENLY);

        // 主轴（Y）顺序：第一个由“首间距 + 偏移”决定，其余接在前一个后面。
        // SPACE_* 模式用统一间隔 betweenGap，不沿用历史边距。
        if (i == 0) {
            z3::expr leadingGap = isSpaceAlign ? solver.ctx().real_val("0")
                : (Float2Expr(solver, child->edgesSpace_.top) * parent->scaleInfo_.spaceScale.expr);
            solver.add(child->position_.offsetY.expr ==
                parent->position_.offsetY.expr +
                leadingGap +
                mainAxisOffset);
        } else {
            const auto& prev = parent->childNode[i - 1];
            z3::expr innerGap = isSpaceAlign ? betweenGap
                : (Float2Expr(solver, prev->edgesSpace_.bottom) * parent->scaleInfo_.spaceScale.expr);
            solver.add(child->position_.offsetY.expr ==
                prev->position_.offsetY.expr + prev->size_.height.expr +
                innerGap);
        }
    }

    AddMainAxisAlignmentConstraints(solver, parent, LayoutType::COLUMN, mainAxisOffset, betweenGap);
}

void SmartLayoutAlgorithm::addRowLayout(z3::optimize& solver, std::shared_ptr<SmartLayoutNode> parent)
{
    // 与 Column 一致：先保证尺寸缩放在 (0, 1] 并最大化。
    solver.add(parent->scaleInfo_.sizeScale.expr > 0);
    solver.add(parent->scaleInfo_.sizeScale.expr <= 1);
    solver.maximize(parent->scaleInfo_.sizeScale.expr);

    // 优化点：Row 也补齐 spaceScale 约束，避免模型出现异常间距。
    solver.add(parent->scaleInfo_.spaceScale.expr >= 0);
    solver.add(parent->scaleInfo_.spaceScale.expr <= 1);

    z3::expr mainAxisOffset =
        solver.ctx().real_const((parent->name_ + ".rowMainAxisOffset").c_str());
    solver.add(mainAxisOffset >= 0);
    z3::expr betweenGap =
        solver.ctx().real_const((parent->name_ + ".rowBetweenGap").c_str());
    solver.add(betweenGap >= 0);

    for (size_t i = 0; i < parent->childNode.size(); ++i) {
        const auto& child = parent->childNode[i];

        solver.add(child->size_.width.expr == Float2Expr(solver, child->size_.width.value) * parent->scaleInfo_.sizeScale.expr);
        solver.add(child->size_.height.expr == Float2Expr(solver, child->size_.height.value) * parent->scaleInfo_.sizeScale.expr);

        AddDefaultConstraints(solver, parent, child);
        AddCrossAxisAlignmentConstraints(solver, parent, child, LayoutType::ROW);

        bool isSpaceAlign = (mainAxisAlign_ == FlexAlign::SPACE_BETWEEN ||
            mainAxisAlign_ == FlexAlign::SPACE_AROUND ||
            mainAxisAlign_ == FlexAlign::SPACE_EVENLY);

        // 主轴（X）顺序：SPACE_* 模式使用统一间隔 betweenGap。
        if (i == 0) {
            z3::expr leadingGap = isSpaceAlign ? solver.ctx().real_val("0")
                : (Float2Expr(solver, child->edgesSpace_.left) * parent->scaleInfo_.spaceScale.expr);
            solver.add(child->position_.offsetX.expr ==
                parent->position_.offsetX.expr +
                leadingGap +
                mainAxisOffset);
        } else {
            const auto& prev = parent->childNode[i - 1];
            z3::expr innerGap = isSpaceAlign ? betweenGap
                : (Float2Expr(solver, prev->edgesSpace_.right) * parent->scaleInfo_.spaceScale.expr);
            solver.add(child->position_.offsetX.expr ==
                prev->position_.offsetX.expr + prev->size_.width.expr +
                innerGap);
        }
    }

    AddMainAxisAlignmentConstraints(solver, parent, LayoutType::ROW, mainAxisOffset, betweenGap);
}

SizeF SmartLayoutAlgorithm::ItermScale(const RefPtr<LayoutWrapper>& iterm, float scale)
{
    Dimension offsetX(0.0, DimensionUnit::PERCENT);
    Dimension offsetY(0.0, DimensionUnit::PERCENT);
    iterm->GetHostNode()->GetRenderContext()->UpdateTransformCenter(DimensionOffset(offsetX, offsetY));
    iterm->GetHostNode()->GetRenderContext()->UpdateTransformScale({ scale, scale });
    return { 0, 0 };
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

bool SmartLayoutAlgorithm::IsRowSpaceEnough(LayoutWrapper* layoutWrapper)
{
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

void SmartLayoutAlgorithm::PerformSmartLayout(LayoutWrapper* layoutWrapper, LayoutType layoutType)
{
    // Step 1: 拉取 Flex 配置（主轴/交叉轴对齐）
    auto layoutProperty = AceType::DynamicCast<FlexLayoutProperty>(layoutWrapper->GetLayoutProperty());
    CHECK_NULL_VOID(layoutProperty);
    mainAxisAlign_ = layoutProperty->GetMainAxisAlignValue(FlexAlign::FLEX_START);
    crossAxisAlign_ = layoutProperty->GetCrossAxisAlignValue(FlexAlign::CENTER);

    // Step 2: 轻量短路优化
    // 仅在 start/start 场景且空间充足时跳过求解，减少求解器开销。
    if (mainAxisAlign_ == FlexAlign::FLEX_START && crossAxisAlign_ == FlexAlign::FLEX_START) {
        if ((layoutType == LayoutType::COLUMN && IsColumnSpaceEnough(layoutWrapper)) ||
            (layoutType == LayoutType::ROW && IsRowSpaceEnough(layoutWrapper))) {
            return;
        }
    }

    // Step 3: 创建求解器上下文
    z3::context ctx;
    z3::optimize solver(ctx);

    const auto& children = layoutWrapper->GetAllChildrenWithBuild(false);
    if (children.empty()) {
        return;
    }

    auto parentSize = layoutWrapper->GetGeometryNode()->GetFrameSize();

    // 根节点用于承载父容器尺寸与缩放变量
    auto root = std::make_shared<SmartLayoutNode>(ctx, "root");
    root->setFixedSizeConstraint(solver, parentSize.Width(), parentSize.Height());

    std::vector<std::shared_ptr<SmartLayoutNode>> childrenLayoutNode;

    // Step 4: 从真实节点抽取初始几何数据，构造 SmartLayoutNode
    for (auto it = children.begin(); it != children.end(); ++it) {
        const auto& child = *it;
        const std::string childName = "child_" + std::to_string(child->GetHostNode()->GetId());
        auto item = std::make_shared<SmartLayoutNode>(ctx, childName);

        const bool isFirst = (it == children.begin());
        const auto nextIt = std::next(it);
        const bool isLast = (nextIt == children.end());

        auto childOffset = child->GetGeometryNode()->GetFrameOffset();
        auto childSize = child->GetGeometryNode()->GetFrameSize();

        // 记录首元素相对父容器的初始留白
        if (isFirst) {
            item->edgesSpace_.top = childOffset.GetY();
            item->edgesSpace_.left = childOffset.GetX();
        }

        // 记录与下一个元素的间距（纵向/横向都记录，后续不同布局方向可复用）
        if (!isLast) {
            const auto& nextChild = *nextIt;
            item->edgesSpace_.bottom = CalculateSpaceBetween(child, nextChild, LayoutType::COLUMN);
            item->edgesSpace_.right = CalculateSpaceBetween(child, nextChild, LayoutType::ROW);

            if (!isFirst) {
                item->edgesSpace_.top = childrenLayoutNode.back()->edgesSpace_.bottom;
                item->edgesSpace_.left = childrenLayoutNode.back()->edgesSpace_.right;
            }
        }

        // 记录最后一个元素到父容器尾部的间距
        if (isLast) {
            item->edgesSpace_.bottom =
                parentSize.Height() - childOffset.GetY() - childSize.Height();
            item->edgesSpace_.right =
                parentSize.Width() - childOffset.GetX() - childSize.Width();
        }

        item->size_.width.value = childSize.Width();
        item->size_.height.value = childSize.Height();

        // Blank 视作“弹性空白”：把它转成间距，实体尺寸置 0
        if (child->GetHostTag() == "Blank") {
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

    root->setChildren(childrenLayoutNode);

    // Step 5: 按方向添加约束
    if (layoutType == LayoutType::COLUMN) {
        addColumnLayout(solver, root);
    } else {
        addRowLayout(solver, root);
    }

    // Step 6: 求解
    auto result = solver.check();
    if (result != z3::sat) {
        LOGE("cr_debug unsat!!!");
        return;
    }

    // Step 7: 回写模型结果到真实节点
    z3::model m = solver.get_model();
    root->syncData(m);

    uint32_t index = 0;
    for (const auto& child : children) {
        childrenLayoutNode[index]->syncData(m);

        float offsetX = childrenLayoutNode[index]->position_.offsetX.value;
        float offsetY = childrenLayoutNode[index]->position_.offsetY.value;

        // 与原逻辑保持一致：按方向附加 margin
        if (child->GetGeometryNode()->GetMargin()) {
            if (layoutType == LayoutType::COLUMN) {
                offsetX += child->GetGeometryNode()->GetMargin()->left.value_or(0);
            } else {
                offsetY += child->GetGeometryNode()->GetMargin()->top.value_or(0);
            }
        }

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
