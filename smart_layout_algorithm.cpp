#include <string>

#include "smart_layout_algorithm.h"

namespace HHHH::HHH::HH {

namespace {

z3::expr Float2Expr(z3::optimize& solver, float value)
{
    return solver.ctx().real_val(std::to_string(value).c_str());
}

HorizontalAlign ToHorizontalAlign(FlexAlign align)
{
    if (align == FlexAlign::FLEX_START) {
        return HorizontalAlign::START;
    } else if (align == FlexAlign::CENTER) {
        return HorizontalAlign::CENTER;
    } else if (align == FlexAlign::FLEX_END) {
        return HorizontalAlign::END;
    }
    return HorizontalAlign::CENTER;
}

VerticalAlign ToVerticalAlign(FlexAlign align)
{
    if (align == FlexAlign::FLEX_START) {
        return VerticalAlign::TOP;
    } else if (align == FlexAlign::CENTER) {
        return VerticalAlign::CENTER;
    } else if (align == FlexAlign::FLEX_END) {
        return VerticalAlign::BOTTOM;
    }
    return VerticalAlign::CENTER;
}

const char* FlexAlignToString(FlexAlign align)
{
    if (align == FlexAlign::FLEX_START) {
        return "FLEX_START";
    }
    if (align == FlexAlign::CENTER) {
        return "CENTER";
    }
    if (align == FlexAlign::FLEX_END) {
        return "FLEX_END";
    }
    if (align == FlexAlign::SPACE_BETWEEN) {
        return "SPACE_BETWEEN";
    }
    if (align == FlexAlign::SPACE_AROUND) {
        return "SPACE_AROUND";
    }
    if (align == FlexAlign::SPACE_EVENLY) {
        return "SPACE_EVENLY";
    }
    return "UNKNOWN";
}

const char* HorizontalAlignToString(HorizontalAlign align)
{
    if (align == HorizontalAlign::START) {
        return "START";
    }
    if (align == HorizontalAlign::CENTER) {
        return "CENTER";
    }
    if (align == HorizontalAlign::END) {
        return "END";
    }
    return "UNKNOWN";
}

const char* VerticalAlignToString(VerticalAlign align)
{
    if (align == VerticalAlign::TOP) {
        return "TOP";
    }
    if (align == VerticalAlign::CENTER) {
        return "CENTER";
    }
    if (align == VerticalAlign::BOTTOM) {
        return "BOTTOM";
    }
    return "UNKNOWN";
}

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
    solver.add(child->size_.width.expr >= 0);
    solver.add(child->size_.height.expr >= 0);
    solver.add(child->position_.offsetX.expr >= 0);
    solver.add(child->position_.offsetY.expr >= 0);
    solver.add(parent->position_.offsetX.expr >= 0);
    solver.add(parent->position_.offsetY.expr >= 0);

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
    if (layoutType == LayoutType::COLUMN) {
        if (horizontalAlign_ == HorizontalAlign::START) {
            solver.add(child->position_.offsetX.expr == parent->position_.offsetX.expr);
        } else if (horizontalAlign_ == HorizontalAlign::CENTER) {
            solver.add(child->position_.offsetX.expr - parent->position_.offsetX.expr ==
                parent->position_.offsetX.expr + parent->size_.width.expr -
                child->position_.offsetX.expr - child->size_.width.expr);
        } else if (horizontalAlign_ == HorizontalAlign::END) {
            solver.add(child->position_.offsetX.expr + child->size_.width.expr ==
                parent->position_.offsetX.expr + parent->size_.width.expr);
        }
    } else {
        if (verticalAlign_ == VerticalAlign::TOP) {
            solver.add(child->position_.offsetY.expr == parent->position_.offsetY.expr);
        } else if (verticalAlign_ == VerticalAlign::CENTER) {
            solver.add(child->position_.offsetY.expr - parent->position_.offsetY.expr ==
                parent->position_.offsetY.expr + parent->size_.height.expr -
                child->position_.offsetY.expr - child->size_.height.expr);
        } else if (verticalAlign_ == VerticalAlign::BOTTOM) {
            solver.add(child->position_.offsetY.expr + child->size_.height.expr ==
                parent->position_.offsetY.expr + parent->size_.height.expr);
        }
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

    z3::expr startPos = (layoutType == LayoutType::COLUMN) ? first->position_.offsetY.expr : first->position_.offsetX.expr;
    z3::expr endPos = (layoutType == LayoutType::COLUMN)
        ? (last->position_.offsetY.expr + last->size_.height.expr)
        : (last->position_.offsetX.expr + last->size_.width.expr);

    z3::expr parentStart = (layoutType == LayoutType::COLUMN) ? parent->position_.offsetY.expr : parent->position_.offsetX.expr;
    z3::expr parentEnd = (layoutType == LayoutType::COLUMN)
        ? (parent->position_.offsetY.expr + parent->size_.height.expr)
        : (parent->position_.offsetX.expr + parent->size_.width.expr);

    // Keep content block inside parent range on main axis.
    solver.add(startPos >= parentStart);
    solver.add(endPos <= parentEnd);

    float headBase = (layoutType == LayoutType::COLUMN) ? first->edgesSpace_.top : first->edgesSpace_.left;
    float tailBase = (layoutType == LayoutType::COLUMN) ? last->edgesSpace_.bottom : last->edgesSpace_.right;
    z3::expr scaledHeadGap = Float2Expr(solver, headBase) * parent->scaleInfo_.spaceScale.expr;
    z3::expr scaledTailGap = Float2Expr(solver, tailBase) * parent->scaleInfo_.spaceScale.expr;

    // mainAxisOffset controls block shift along main axis.
    if (mainAxisAlign_ == FlexAlign::FLEX_START) {
        solver.add(startPos - parentStart == scaledHeadGap);
        // Keep start aligned while allowing extra slack to stay at tail side.
        solver.add(parentEnd - endPos >= scaledTailGap);
        solver.add(mainAxisOffset == 0);
        solver.add(betweenGap == 0);
    } else if (mainAxisAlign_ == FlexAlign::CENTER) {
        solver.add(startPos - parentStart == scaledHeadGap + mainAxisOffset);
        solver.add(parentEnd - endPos == scaledTailGap + mainAxisOffset);
        solver.add(betweenGap == 0);
    } else if (mainAxisAlign_ == FlexAlign::FLEX_END) {
        // Keep tail-side gap scaled by tailBase * spaceScale.
        solver.add(startPos - parentStart == scaledHeadGap + mainAxisOffset);
        solver.add(parentEnd - endPos == scaledTailGap);
        solver.add(betweenGap == 0);
    } else if (mainAxisAlign_ == FlexAlign::SPACE_BETWEEN) {
        solver.add(startPos == parentStart);
        solver.add(endPos == parentEnd);
    } else if (mainAxisAlign_ == FlexAlign::SPACE_AROUND) {
        auto halfGap = betweenGap / solver.ctx().real_val("2");
        solver.add(startPos - parentStart == halfGap);
        solver.add(parentEnd - endPos == halfGap);
    } else if (mainAxisAlign_ == FlexAlign::SPACE_EVENLY) {
        solver.add(startPos - parentStart == betweenGap);
        solver.add(parentEnd - endPos == betweenGap);
    } else {
        // Fallback to FLEX_START for unsupported align types.
        solver.add(mainAxisOffset == 0);
        solver.add(betweenGap == 0);
    }
}

void SmartLayoutAlgorithm::addColumnLayout(z3::optimize& solver, std::shared_ptr<SmartLayoutNode> parent)
{
    // Keep sizeScale in [0,1].
    solver.add(parent->scaleInfo_.sizeScale.expr >= 0);
    solver.add(parent->scaleInfo_.sizeScale.expr <= 1);
    // Objective prefers larger sizeScale first.

    // Keep spaceScale in [0,1].
    solver.add(parent->scaleInfo_.spaceScale.expr >= 0);
    solver.add(parent->scaleInfo_.spaceScale.expr <= 1);
    const bool isMainAxisLinearAlign = (mainAxisAlign_ == FlexAlign::FLEX_START ||
        mainAxisAlign_ == FlexAlign::CENTER ||
        mainAxisAlign_ == FlexAlign::FLEX_END);
    if (isMainAxisLinearAlign) {
        // Linear main-axis align: shrink spacing to zero before shrinking size.
        solver.add(z3::implies(
            parent->scaleInfo_.sizeScale.expr < solver.ctx().real_val("1"),
            parent->scaleInfo_.spaceScale.expr == solver.ctx().real_val("0")));
    }
    solver.maximize(parent->scaleInfo_.sizeScale.expr);
    solver.maximize(parent->scaleInfo_.spaceScale.expr);

    z3::expr mainAxisOffset =
        solver.ctx().real_const((parent->name_ + ".columnMainAxisOffset").c_str());
    solver.add(mainAxisOffset >= 0);
    z3::expr betweenGap =
        solver.ctx().real_const((parent->name_ + ".columnBetweenGap").c_str());
    solver.add(betweenGap >= 0);
    for (size_t i = 0; i < parent->childNode.size(); ++i) {
        const auto& child = parent->childNode[i];

        // Scale child size by sizeScale.
        solver.add(child->size_.width.expr == Float2Expr(solver, child->size_.width.value) * parent->scaleInfo_.sizeScale.expr);
        solver.add(child->size_.height.expr == Float2Expr(solver, child->size_.height.value) * parent->scaleInfo_.sizeScale.expr);

        AddDefaultConstraints(solver, parent, child);
        AddCrossAxisAlignmentConstraints(solver, parent, child, LayoutType::COLUMN);

        bool isSpaceAlign = (mainAxisAlign_ == FlexAlign::SPACE_BETWEEN ||
            mainAxisAlign_ == FlexAlign::SPACE_AROUND ||
            mainAxisAlign_ == FlexAlign::SPACE_EVENLY);

        // SPACE_* uses uniform betweenGap on main axis.
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
    // Keep sizeScale in [0,1].
    solver.add(parent->scaleInfo_.sizeScale.expr >= 0);
    solver.add(parent->scaleInfo_.sizeScale.expr <= 1);
    // Objective prefers larger sizeScale first.

    // Keep spaceScale in [0,1].
    solver.add(parent->scaleInfo_.spaceScale.expr >= 0);
    solver.add(parent->scaleInfo_.spaceScale.expr <= 1);
    const bool isMainAxisLinearAlign = (mainAxisAlign_ == FlexAlign::FLEX_START ||
        mainAxisAlign_ == FlexAlign::CENTER ||
        mainAxisAlign_ == FlexAlign::FLEX_END);
    if (isMainAxisLinearAlign) {
        // Linear main-axis align: shrink spacing to zero before shrinking size.
        solver.add(z3::implies(
            parent->scaleInfo_.sizeScale.expr < solver.ctx().real_val("1"),
            parent->scaleInfo_.spaceScale.expr == solver.ctx().real_val("0")));
    }
    solver.maximize(parent->scaleInfo_.sizeScale.expr);
    solver.maximize(parent->scaleInfo_.spaceScale.expr);

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

        // SPACE_* uses uniform betweenGap on main axis.
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
    const auto& children = layoutWrapper->GetAllChildrenWithBuild(false);
    if (children.empty()) {
        return true;
    }

    const float parentStart = layoutWrapper->GetGeometryNode()->GetFrameOffset().GetY();
    const float parentEnd = parentStart + layoutWrapper->GetGeometryNode()->GetFrameSize().Height();
    float prevEnd = parentStart;
    bool firstChild = true;

    for (const auto& child : children) {
        auto childOffset = child->GetGeometryNode()->GetFrameOffset();
        auto childSize = child->GetGeometryNode()->GetFrameSize();
        const float childStart = childOffset.GetY();
        const float childEnd = childStart + childSize.Height();
        if (childStart < parentStart || childEnd > parentEnd) {
            return false;
        }
        if (!firstChild && childStart < prevEnd) {
            return false;
        }
        prevEnd = childEnd;
        firstChild = false;
    }

    return true;
}

bool SmartLayoutAlgorithm::IsRowSpaceEnough(LayoutWrapper* layoutWrapper)
{
    const auto& children = layoutWrapper->GetAllChildrenWithBuild(false);
    if (children.empty()) {
        return true;
    }

    const float parentStart = layoutWrapper->GetGeometryNode()->GetFrameOffset().GetX();
    const float parentEnd = parentStart + layoutWrapper->GetGeometryNode()->GetFrameSize().Width();
    float prevEnd = parentStart;
    bool firstChild = true;

    for (const auto& child : children) {
        auto childOffset = child->GetGeometryNode()->GetFrameOffset();
        auto childSize = child->GetGeometryNode()->GetFrameSize();
        const float childStart = childOffset.GetX();
        const float childEnd = childStart + childSize.Width();
        if (childStart < parentStart || childEnd > parentEnd) {
            return false;
        }
        if (!firstChild && childStart < prevEnd) {
            return false;
        }
        prevEnd = childEnd;
        firstChild = false;
    }

    return true;
}

void SmartLayoutAlgorithm::PerformSmartLayout(LayoutWrapper* layoutWrapper, LayoutType layoutType)
{
    const char* layoutTypeStr = (layoutType == LayoutType::COLUMN) ? "COLUMN" : "ROW";
    LOGI("smart_layout trigger_enter type:%{public}s wrapper:%{public}p", layoutTypeStr, layoutWrapper);

    // Step 1
    auto layoutProperty = AceType::DynamicCast<FlexLayoutProperty>(layoutWrapper->GetLayoutProperty());
    if (!layoutProperty) {
        LOGI("smart_layout trigger_skip type:%{public}s reason:no_layout_property", layoutTypeStr);
        return;
    }
    mainAxisAlign_ = layoutProperty->GetMainAxisAlignValue(FlexAlign::FLEX_START);
    auto crossAxisAlign = layoutProperty->GetCrossAxisAlignValue(FlexAlign::CENTER);
    if (layoutType == LayoutType::COLUMN) {
        horizontalAlign_ = ToHorizontalAlign(crossAxisAlign);
    } else {
        verticalAlign_ = ToVerticalAlign(crossAxisAlign);
    }
    if (layoutType == LayoutType::COLUMN) {
        LOGI("smart_layout align type:%{public}s main:%{public}s(%{public}d) crossFlex:%{public}s(%{public}d) crossResolved:%{public}s",
            layoutTypeStr,
            FlexAlignToString(mainAxisAlign_),
            static_cast<int>(mainAxisAlign_),
            FlexAlignToString(crossAxisAlign),
            static_cast<int>(crossAxisAlign),
            HorizontalAlignToString(horizontalAlign_));
    } else {
        LOGI("smart_layout align type:%{public}s main:%{public}s(%{public}d) crossFlex:%{public}s(%{public}d) crossResolved:%{public}s",
            layoutTypeStr,
            FlexAlignToString(mainAxisAlign_),
            static_cast<int>(mainAxisAlign_),
            FlexAlignToString(crossAxisAlign),
            static_cast<int>(crossAxisAlign),
            VerticalAlignToString(verticalAlign_));
    }
    const auto& children = layoutWrapper->GetAllChildrenWithBuild(false);
    if (children.empty()) {
        LOGI("smart_layout trigger_skip type:%{public}s reason:no_children", layoutTypeStr);
        return;
    }
    LOGI("smart_layout trigger_state type:%{public}s childCount:%{public}d mainAxisAlign:%{public}d crossAxisAlign:%{public}d",
        layoutTypeStr,
        static_cast<int>(children.size()),
        static_cast<int>(mainAxisAlign_),
        static_cast<int>(crossAxisAlign));
    auto parentSize = layoutWrapper->GetGeometryNode()->GetFrameSize();
    auto parentOffset = layoutWrapper->GetGeometryNode()->GetFrameOffset();
    for (const auto& child : children) {
        ItermScale(child, 1.0f);
        if (child->GetHostNode() != nullptr) {
            child->GetHostNode()->MarkDirtyNode(NG::PROPERTY_UPDATE_LAYOUT);
        }
        child->Layout();
    }

    // Step 2
    bool isCrossStart = (layoutType == LayoutType::COLUMN) ? (horizontalAlign_ == HorizontalAlign::START)
                                                            : (verticalAlign_ == VerticalAlign::TOP);
    if (mainAxisAlign_ == FlexAlign::FLEX_START && isCrossStart) {
        bool isSpaceEnough = (layoutType == LayoutType::COLUMN) ? IsColumnSpaceEnough(layoutWrapper)
                                                                 : IsRowSpaceEnough(layoutWrapper);
        if (isSpaceEnough) {
            LOGI("smart_layout trigger_skip type:%{public}s reason:short_circuit_start_start_space_enough", layoutTypeStr);
            for (const auto& child : children) {
                ItermScale(child, 1.0f);
                if (child->GetHostNode() != nullptr) {
                    child->GetHostNode()->MarkDirtyNode(NG::PROPERTY_UPDATE_LAYOUT);
                }
                child->Layout();

                auto childOffset = child->GetGeometryNode()->GetFrameOffset();
                auto childSize = child->GetGeometryNode()->GetFrameSize();
                auto childId = child->GetHostNode() ? child->GetHostNode()->GetId() : -1;
                float parentMainSize =
                    (layoutType == LayoutType::COLUMN) ? layoutWrapper->GetGeometryNode()->GetFrameSize().Height()
                                                       : layoutWrapper->GetGeometryNode()->GetFrameSize().Width();
                float childMainOffset = (layoutType == LayoutType::COLUMN) ? (childOffset.GetY() - parentOffset.GetY())
                                                                            : (childOffset.GetX() - parentOffset.GetX());
                float childMainSize =
                    (layoutType == LayoutType::COLUMN) ? childSize.Height() : childSize.Width();
                float tailGap = parentMainSize - (childMainOffset + childMainSize);
                LOGI("smart_layout short_circuit_reset type:%{public}s child:%{public}d "
                     "offset:[%{public}.2f %{public}.2f] size:[%{public}.2f %{public}.2f] "
                     "parentMainSize:%{public}.2f tailGap:%{public}.2f",
                    layoutTypeStr, childId, childOffset.GetX(), childOffset.GetY(), childSize.Width(), childSize.Height(),
                    parentMainSize, tailGap);
            }
            return;
        }
        LOGI("smart_layout trigger_continue type:%{public}s reason:start_start_but_space_not_enough", layoutTypeStr);
    } else {
        LOGI("smart_layout trigger_continue type:%{public}s reason:need_solver_for_align", layoutTypeStr);
    }

    // Step 3
    LOGI("smart_layout solver_start type:%{public}s", layoutTypeStr);
    z3::context ctx;
    z3::optimize solver(ctx);

    auto root = std::make_shared<SmartLayoutNode>(ctx, "root");
    root->setFixedSizeConstraint(solver, parentSize.Width(), parentSize.Height());

    std::vector<std::shared_ptr<SmartLayoutNode>> childrenLayoutNode;

    // Step 4
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
            item->edgesSpace_.top = childOffset.GetY() - parentOffset.GetY();
            item->edgesSpace_.left = childOffset.GetX() - parentOffset.GetX();
        }

        if (!isLast) {
            const auto& nextChild = *nextIt;
            item->edgesSpace_.bottom = CalculateSpaceBetween(child, nextChild, LayoutType::COLUMN);
            item->edgesSpace_.right = CalculateSpaceBetween(child, nextChild, LayoutType::ROW);

            if (!isFirst) {
                item->edgesSpace_.top = childrenLayoutNode.back()->edgesSpace_.bottom;
                item->edgesSpace_.left = childrenLayoutNode.back()->edgesSpace_.right;
            }
        }

        if (isLast) {
            item->edgesSpace_.bottom =
                parentSize.Height() - (childOffset.GetY() - parentOffset.GetY()) - childSize.Height();
            item->edgesSpace_.right =
                parentSize.Width() - (childOffset.GetX() - parentOffset.GetX()) - childSize.Width();
        }

        item->size_.width.value = childSize.Width();
        item->size_.height.value = childSize.Height();

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

    // Step 5
    if (layoutType == LayoutType::COLUMN) {
        addColumnLayout(solver, root);
    } else {
        addRowLayout(solver, root);
    }

    // Step 6
    auto result = solver.check();
    if (result != z3::sat) {
        LOGE("cr_debug unsat!!!");
        return;
    }

    // Step 7
    z3::model m = solver.get_model();
    root->syncData(m);

    uint32_t index = 0;
    for (const auto& child : children) {
        childrenLayoutNode[index]->syncData(m);

        float offsetX = childrenLayoutNode[index]->position_.offsetX.value;
        float offsetY = childrenLayoutNode[index]->position_.offsetY.value;

        if (child->GetGeometryNode()->GetMargin()) {
            if (layoutType == LayoutType::COLUMN) {
                offsetX += child->GetGeometryNode()->GetMargin()->left.value_or(0);
            } else {
                offsetY += child->GetGeometryNode()->GetMargin()->top.value_or(0);
            }
        }

        auto childId = child->GetHostNode() ? child->GetHostNode()->GetId() : -1;
        const float finalWidth = childrenLayoutNode[index]->size_.width.value;
        const float finalHeight = childrenLayoutNode[index]->size_.height.value;
        const float childRelativeX = offsetX - parentOffset.GetX();
        const float childRelativeY = offsetY - parentOffset.GetY();
        const float sizeScale = root->scaleInfo_.sizeScale.value;
        const float spaceScale = root->scaleInfo_.spaceScale.value;
        LOGI("smart_layout compact type:%{public}s child:%{public}d "
             "parentXYWH:[%{public}.2f %{public}.2f %{public}.2f %{public}.2f] "
             "childRelXY:[%{public}.2f %{public}.2f] childWH:[%{public}.2f %{public}.2f] "
             "spaceScale:%{public}.4f sizeScale:%{public}.4f",
            layoutTypeStr,
            childId,
            parentOffset.GetX(),
            parentOffset.GetY(),
            parentSize.Width(),
            parentSize.Height(),
            childRelativeX,
            childRelativeY,
            finalWidth,
            finalHeight,
            spaceScale,
            sizeScale);

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
    LOGI("smart_layout trigger_request type:COLUMN wrapper:%{public}p", layoutWrapper);
    PerformSmartLayout(layoutWrapper, LayoutType::COLUMN);
}

void SmartLayoutAlgorithm::SmartRowLayout(LayoutWrapper* layoutWrapper)
{
    LOGI("smart_layout trigger_request type:ROW wrapper:%{public}p", layoutWrapper);
    PerformSmartLayout(layoutWrapper, LayoutType::ROW);
}

} // namespace HHHH::HHH::HH

