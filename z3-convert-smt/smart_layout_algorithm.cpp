#include "smart_layout_algorithm.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace HHHH::HHH::HH {

namespace {

constexpr double EPS = 1e-6;

double Clamp01(double value)
{
    return std::max(0.0, std::min(1.0, value));
}

double GetMainSize(const std::shared_ptr<SmartLayoutNode>& node, bool isColumn)
{
    return isColumn ? node->size_.height.value : node->size_.width.value;
}

double GetCrossSize(const std::shared_ptr<SmartLayoutNode>& node, bool isColumn)
{
    return isColumn ? node->size_.width.value : node->size_.height.value;
}

double GetMainPos(const std::shared_ptr<SmartLayoutNode>& node, bool isColumn)
{
    return isColumn ? node->position_.offsetY.value : node->position_.offsetX.value;
}

double GetCrossPos(const std::shared_ptr<SmartLayoutNode>& node, bool isColumn)
{
    return isColumn ? node->position_.offsetX.value : node->position_.offsetY.value;
}

double ComputeMainSizeSum(const std::shared_ptr<SmartLayoutNode>& parent, bool isColumn)
{
    double total = 0.0;
    for (const auto& child : parent->childNode) {
        if (child == nullptr) {
            continue;
        }
        total += std::max(0.0, GetMainSize(child, isColumn));
    }
    return total;
}

double ComputeInnerMainGapSum(const std::shared_ptr<SmartLayoutNode>& parent, bool isColumn)
{
    if (parent == nullptr || parent->childNode.empty()) {
        return 0.0;
    }

    double totalGap = 0.0;
    for (size_t i = 1; i < parent->childNode.size(); ++i) {
        const auto& child = parent->childNode[i];
        if (child == nullptr) {
            continue;
        }
        const auto& prev = parent->childNode[i - 1];
        if (prev == nullptr) {
            continue;
        }
        const double prevEnd = GetMainPos(prev, isColumn) + GetMainSize(prev, isColumn);
        const double innerGap = GetMainPos(child, isColumn) - prevEnd;
        totalGap += std::max(0.0, innerGap);
    }
    return totalGap;
}

double ComputeCrossSizeUpperBound(const std::shared_ptr<SmartLayoutNode>& parent, bool isColumn)
{
    if (parent == nullptr) {
        return 1.0;
    }
    const double parentCrossSize = std::max(0.0, GetCrossSize(parent, isColumn));
    double upperBound = 1.0;
    for (const auto& child : parent->childNode) {
        if (child == nullptr) {
            continue;
        }
        const double childCrossSize = std::max(0.0, GetCrossSize(child, isColumn));
        if (childCrossSize <= EPS) {
            continue;
        }
        upperBound = std::min(upperBound, Clamp01(parentCrossSize / childCrossSize));
    }
    return upperBound;
}

} // namespace

void SmartLayoutAlgorithm::AddDefaultConstraints(
    localsmt::Engine& solver, const std::shared_ptr<SmartLayoutNode>& parent, const std::shared_ptr<SmartLayoutNode>& child)
{
    if (parent == nullptr || child == nullptr) {
        return;
    }

    solver.add(child->size_.width.expr >= 0.0);
    solver.add(child->size_.height.expr >= 0.0);
    solver.add(child->position_.offsetX.expr >= 0.0);
    solver.add(child->position_.offsetY.expr >= 0.0);
    solver.add(parent->position_.offsetX.expr >= 0.0);
    solver.add(parent->position_.offsetY.expr >= 0.0);

    solver.add(child->position_.offsetX.expr >= parent->position_.offsetX.expr);
    solver.add(child->position_.offsetY.expr >= parent->position_.offsetY.expr);
    solver.add(child->position_.offsetX.expr + child->size_.width.expr <=
        parent->position_.offsetX.expr + parent->size_.width.expr);
    solver.add(child->position_.offsetY.expr + child->size_.height.expr <=
        parent->position_.offsetY.expr + parent->size_.height.expr);
}

void SmartLayoutAlgorithm::AddCrossAxisAlignmentConstraints(localsmt::Engine& solver,
    const std::shared_ptr<SmartLayoutNode>& parent, const std::shared_ptr<SmartLayoutNode>& child, LayoutType layoutType)
{
    if (parent == nullptr || child == nullptr) {
        return;
    }

    if (layoutType == LayoutType::COLUMN) {
        if (horizontalAlign_ == HorizontalAlign::CENTER) {
            solver.add(child->position_.offsetX.expr ==
                parent->position_.offsetX.expr + (parent->size_.width.expr - child->size_.width.expr) * 0.5);
        } else if (horizontalAlign_ == HorizontalAlign::END) {
            solver.add(child->position_.offsetX.expr ==
                parent->position_.offsetX.expr + parent->size_.width.expr - child->size_.width.expr);
        } else {
            solver.add(child->position_.offsetX.expr == parent->position_.offsetX.expr);
        }
        return;
    }

    if (verticalAlign_ == VerticalAlign::CENTER) {
        solver.add(child->position_.offsetY.expr ==
            parent->position_.offsetY.expr + (parent->size_.height.expr - child->size_.height.expr) * 0.5);
    } else if (verticalAlign_ == VerticalAlign::BOTTOM) {
        solver.add(child->position_.offsetY.expr ==
            parent->position_.offsetY.expr + parent->size_.height.expr - child->size_.height.expr);
    } else {
        solver.add(child->position_.offsetY.expr == parent->position_.offsetY.expr);
    }
}

void SmartLayoutAlgorithm::ComputeSolvedScales(const std::shared_ptr<SmartLayoutNode>& parent, LayoutType layoutType)
{
    solvedMainSpaceScale_ = 1.0f;
    solvedSizeScale_ = 1.0f;
    if (parent == nullptr || parent->childNode.empty()) {
        return;
    }

    const bool isColumn = (layoutType == LayoutType::COLUMN);
    const double parentMainSize = std::max(0.0, GetMainSize(parent, isColumn));
    const double totalMainSize = ComputeMainSizeSum(parent, isColumn);
    const double totalInnerMainGap = ComputeInnerMainGapSum(parent, isColumn);

    const double crossSizeUpper = ComputeCrossSizeUpperBound(parent, isColumn);
    // Use 1-xxx form for [0,1] variables:
    // sizeScale = 1 - sizeShrink, spaceScale = 1 - spaceShrink.
    double sizeShrink = Clamp01(1.0 - Clamp01(crossSizeUpper));
    double spaceShrink = 0.0;
    double sizeScale = 1.0 - sizeShrink;

    if (totalMainSize > EPS) {
        const double mainNeedWithCurrentScale = totalMainSize * sizeScale + totalInnerMainGap;
        if (mainNeedWithCurrentScale > parentMainSize + EPS) {
            if (totalInnerMainGap > EPS) {
                const double requiredSpaceShrink = (mainNeedWithCurrentScale - parentMainSize) / totalInnerMainGap;
                spaceShrink = Clamp01(requiredSpaceShrink);
            } else {
                spaceShrink = 1.0;
            }

            const double spaceScaleAfterShrink = 1.0 - spaceShrink;
            if (totalMainSize * sizeScale + totalInnerMainGap * spaceScaleAfterShrink > parentMainSize + EPS) {
                spaceShrink = 1.0;
                const double mainSizeUpper = Clamp01(parentMainSize / totalMainSize);
                const double mainSizeShrink = Clamp01(1.0 - mainSizeUpper);
                sizeShrink = std::max(sizeShrink, mainSizeShrink);
                sizeScale = 1.0 - sizeShrink;
            }
        }
    }

    const double spaceScale = 1.0 - spaceShrink;
    solvedSizeScale_ = static_cast<float>(sizeScale);
    solvedMainSpaceScale_ = static_cast<float>(spaceScale);
}

void SmartLayoutAlgorithm::addLinearLayout(
    localsmt::Engine& solver, const std::shared_ptr<SmartLayoutNode>& parent, LayoutType layoutType)
{
    if (parent == nullptr) {
        return;
    }
    const bool isColumn = (layoutType == LayoutType::COLUMN);

    solver.add(parent->scaleInfo_.sizeScale.expr >= 0.0);
    solver.add(parent->scaleInfo_.sizeScale.expr <= 1.0);
    solver.add(parent->scaleInfo_.spaceScale.expr >= 0.0);
    solver.add(parent->scaleInfo_.spaceScale.expr <= 1.0);
    solver.add(parent->scaleInfo_.crossSpaceScale.expr >= 0.0);
    solver.add(parent->scaleInfo_.crossSpaceScale.expr <= 1.0);

    // SAT-first: scales are solved outside and injected as constants to avoid non-deterministic/unsat optimize behavior.
    solver.add(parent->scaleInfo_.sizeScale.expr == solvedSizeScale_);
    solver.add(parent->scaleInfo_.spaceScale.expr == solvedMainSpaceScale_);
    solver.add(parent->scaleInfo_.crossSpaceScale.expr == 1.0);

    for (size_t i = 0; i < parent->childNode.size(); ++i) {
        const auto& child = parent->childNode[i];
        if (child == nullptr) {
            continue;
        }

        solver.add(child->size_.width.expr == child->size_.width.value * parent->scaleInfo_.sizeScale.expr);
        solver.add(child->size_.height.expr == child->size_.height.value * parent->scaleInfo_.sizeScale.expr);

        AddDefaultConstraints(solver, parent, child);
        AddCrossAxisAlignmentConstraints(solver, parent, child, layoutType);

        if (i == 0) {
            continue;
        }
        const localsmt::Expr childMainPos = isColumn ? child->position_.offsetY.expr : child->position_.offsetX.expr;
        const auto& prev = parent->childNode[i - 1];
        if (prev == nullptr) {
            continue;
        }
        const localsmt::Expr prevMainPos = isColumn ? prev->position_.offsetY.expr : prev->position_.offsetX.expr;
        const localsmt::Expr prevMainSize = isColumn ? prev->size_.height.expr : prev->size_.width.expr;
        const float childMainPosBase = isColumn ? child->position_.offsetY.value : child->position_.offsetX.value;
        const float prevMainPosBase = isColumn ? prev->position_.offsetY.value : prev->position_.offsetX.value;
        const float prevMainSizeBase = isColumn ? prev->size_.height.value : prev->size_.width.value;
        const float innerBaseGapRaw = childMainPosBase - (prevMainPosBase + prevMainSizeBase);
        const float innerBaseGap = (innerBaseGapRaw > 0.0f) ? innerBaseGapRaw : 0.0f;
        solver.add(childMainPos == prevMainPos + prevMainSize + innerBaseGap * parent->scaleInfo_.spaceScale.expr);
    }

    const auto& lastChild = parent->childNode.back();
    if (lastChild != nullptr) {
        if (isColumn) {
            solver.add(lastChild->position_.offsetY.expr + lastChild->size_.height.expr ==
                parent->position_.offsetY.expr + parent->size_.height.expr);
        } else {
            solver.add(lastChild->position_.offsetX.expr + lastChild->size_.width.expr ==
                parent->position_.offsetX.expr + parent->size_.width.expr);
        }
    }
}

void SmartLayoutAlgorithm::addColumnLayout(localsmt::Engine& solver, const std::shared_ptr<SmartLayoutNode>& parent)
{
    addLinearLayout(solver, parent, LayoutType::COLUMN);
}

void SmartLayoutAlgorithm::addRowLayout(localsmt::Engine& solver, const std::shared_ptr<SmartLayoutNode>& parent)
{
    addLinearLayout(solver, parent, LayoutType::ROW);
}

SizeF SmartLayoutAlgorithm::ItermScale(const RefPtr<LayoutWrapper>& item, float scale)
{
    Dimension offsetX(0.0, DimensionUnit::PERCENT);
    Dimension offsetY(0.0, DimensionUnit::PERCENT);
    item->GetHostNode()->GetRenderContext()->UpdateTransformCenter(DimensionOffset(offsetX, offsetY));
    item->GetHostNode()->GetRenderContext()->UpdateTransformScale({ scale, scale });
    return { 0, 0 };
}

void SmartLayoutAlgorithm::PerformSmartLayout(LayoutWrapper* layoutWrapper, LayoutType layoutType)
{
    if (layoutWrapper == nullptr) {
        return;
    }
    horizontalAlign_ = HorizontalAlign::CENTER;
    verticalAlign_ = VerticalAlign::CENTER;
    auto layoutProperty = AceType::DynamicCast<FlexLayoutProperty>(layoutWrapper->GetLayoutProperty());
    if (layoutProperty != nullptr) {
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
                break;
        }
    }

    const auto& children = layoutWrapper->GetAllChildrenWithBuild(false);
    if (children.empty()) {
        return;
    }

    auto parentGeometry = layoutWrapper->GetGeometryNode();
    if (parentGeometry == nullptr) {
        return;
    }
    const auto parentSize = parentGeometry->GetFrameSize();

    localsmt::Engine solver;
    auto root = std::make_shared<SmartLayoutNode>(solver, "root");
    root->setFixedSizeConstraint(solver, parentSize.Width(), parentSize.Height());
    root->size_.width.value = parentSize.Width();
    root->size_.height.value = parentSize.Height();
    root->position_.offsetX.value = 0.0f;
    root->position_.offsetY.value = 0.0f;
    solver.add(root->position_.offsetX.expr == 0.0);
    solver.add(root->position_.offsetY.expr == 0.0);

    std::vector<std::shared_ptr<SmartLayoutNode>> childrenLayoutNode;
    std::vector<RefPtr<LayoutWrapper>> orderedChildren;
    childrenLayoutNode.reserve(children.size());
    orderedChildren.reserve(children.size());

    int32_t childIndex = 0;
    for (const auto& child : children) {
        if (child == nullptr || child->GetGeometryNode() == nullptr) {
            continue;
        }
        const int32_t childId = child->GetHostNode() ? child->GetHostNode()->GetId() : -1;
        const std::string childName = "child_" + std::to_string(childId >= 0 ? childId : childIndex);
        auto item = std::make_shared<SmartLayoutNode>(solver, childName);

        auto childOffset = child->GetGeometryNode()->GetFrameOffset();
        auto childSize = child->GetGeometryNode()->GetFrameSize();
        item->position_.offsetX.value = childOffset.GetX();
        item->position_.offsetY.value = childOffset.GetY();
        item->size_.width.value = childSize.Width();
        item->size_.height.value = childSize.Height();

        if (child->GetHostTag() == "Blank") {
            if (layoutType == LayoutType::COLUMN) {
                item->size_.height.value = 0.0f;
            } else {
                item->size_.width.value = 0.0f;
            }
        }

        childrenLayoutNode.push_back(item);
        orderedChildren.push_back(child);
        ++childIndex;
    }

    if (childrenLayoutNode.empty()) {
        return;
    }

    root->setChildren(childrenLayoutNode);
    ComputeSolvedScales(root, layoutType);

    if (layoutType == LayoutType::COLUMN) {
        addColumnLayout(solver, root);
    } else {
        addRowLayout(solver, root);
    }

    auto applySolvedResult = [&](localsmt::Engine& solvedEngine, const std::shared_ptr<SmartLayoutNode>& solvedRoot,
                                 const std::vector<std::shared_ptr<SmartLayoutNode>>& solvedChildren) {
        solvedRoot->syncData(solvedEngine);
        for (uint32_t index = 0; index < orderedChildren.size(); ++index) {
            const auto& child = orderedChildren[index];
            solvedChildren[index]->syncData(solvedEngine);

            const float offsetX = solvedChildren[index]->position_.offsetX.value;
            const float offsetY = solvedChildren[index]->position_.offsetY.value;
            child->GetGeometryNode()->SetFrameOffset(OffsetF(offsetX, offsetY));
            ItermScale(child, solvedRoot->scaleInfo_.sizeScale.value);
            if (child->GetHostNode() != nullptr) {
                child->GetHostNode()->MarkDirtyNode(NG::PROPERTY_UPDATE_LAYOUT);
            }
            child->Layout();
        }
    };

    if (solver.solve()) {
        LOGD("cy_debug primary solve success");
        applySolvedResult(solver, root, childrenLayoutNode);
        return;
    }
    LOGE("cy_debug primary solve failed, entering fallback");

    // Hard fallback to keep SAT: degrade to zero-size, zero-gap placement at parent origin.
    localsmt::Engine fallbackSolver;
    auto fallbackRoot = std::make_shared<SmartLayoutNode>(fallbackSolver, "fallback_root");
    fallbackRoot->setFixedSizeConstraint(fallbackSolver, parentSize.Width(), parentSize.Height());
    fallbackRoot->position_.offsetX.value = 0.0f;
    fallbackRoot->position_.offsetY.value = 0.0f;
    fallbackSolver.add(fallbackRoot->position_.offsetX.expr == 0.0);
    fallbackSolver.add(fallbackRoot->position_.offsetY.expr == 0.0);
    fallbackSolver.add(fallbackRoot->scaleInfo_.sizeScale.expr == 0.0);
    fallbackSolver.add(fallbackRoot->scaleInfo_.spaceScale.expr == 0.0);
    fallbackSolver.add(fallbackRoot->scaleInfo_.crossSpaceScale.expr == 0.0);

    std::vector<std::shared_ptr<SmartLayoutNode>> fallbackChildren;
    fallbackChildren.reserve(orderedChildren.size());
    for (uint32_t index = 0; index < orderedChildren.size(); ++index) {
        const auto& child = orderedChildren[index];
        const int32_t childId = child->GetHostNode() ? child->GetHostNode()->GetId() : static_cast<int32_t>(index);
        auto fallbackNode = std::make_shared<SmartLayoutNode>(fallbackSolver, "fallback_child_" + std::to_string(childId));
        fallbackNode->size_.width.value = 0.0f;
        fallbackNode->size_.height.value = 0.0f;
        fallbackNode->position_.offsetX.value = 0.0f;
        fallbackNode->position_.offsetY.value = 0.0f;
        fallbackChildren.push_back(fallbackNode);
    }
    fallbackRoot->setChildren(fallbackChildren);
    for (const auto& child : fallbackChildren) {
        if (child == nullptr) {
            continue;
        }
        fallbackSolver.add(child->size_.width.expr == 0.0);
        fallbackSolver.add(child->size_.height.expr == 0.0);
        fallbackSolver.add(child->position_.offsetX.expr == fallbackRoot->position_.offsetX.expr);
        fallbackSolver.add(child->position_.offsetY.expr == fallbackRoot->position_.offsetY.expr);
        AddDefaultConstraints(fallbackSolver, fallbackRoot, child);
    }

    if (!fallbackSolver.solve()) {
        LOGE("cy_debug fallback solve failed");
        return;
    }
    LOGD("cy_debug fallback solve success");
    applySolvedResult(fallbackSolver, fallbackRoot, fallbackChildren);
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
