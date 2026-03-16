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

namespace HHHH::HHH::HH {

void SmartLayoutAlgorithm::AddDefaultConstraints(z3::optimize& solver, std::shared_ptr<SmartLayoutNode> parent, std::shared_ptr<SmartLayoutNode> child)
{
    // 1. 控制元素尺寸不小于 0
    solver.add(child->size_.width.expr >= 0);
    solver.add(child->size_.height.expr >= 0);
    solver.add(child->position_.offsetX.expr >= 0);
    solver.add(child->position_.offsetY.expr >= 0);
	solver.add(parent->position_.offsetX.expr >= 0);
    solver.add(parent->position_.offsetY.expr >= 0);
    solver.add(parent->scaleInfo_.spaceScale.expr >= 0);

    // 子控件不能超出父控件内部
    solver.add(child->position_.offsetX.expr >= parent->position_.offsetX.expr);
    solver.add(child->position_.offsetY.expr >= parent->position_.offsetY.expr);
    solver.add(child->position_.offsetX.expr + child->size_.width.expr <= parent->position_.offsetX.expr + parent->size_.width.expr);
    solver.add(child->position_.offsetY.expr + child->size_.height.expr <= parent->position_.offsetY.expr + parent->size_.height.expr);
}

z3::expr float2expr(z3::optimize& solver, float value)
{
    return solver.ctx().real_val(std::to_string(value).c_str());
}

void SmartLayoutAlgorithm::addColumnLayout(z3::optimize& solver, std::shared_ptr<SmartLayoutNode> parent)
{
    solver.add(parent->scaleInfo_.sizeScale.expr > 0);
    solver.add(parent->scaleInfo_.sizeScale.expr <= 1);
    solver.maximize(parent->scaleInfo_.sizeScale.expr);

    solver.add(parent->scaleInfo_.spaceScale.expr >= 0);
    solver.add(parent->scaleInfo_.spaceScale.expr <= 1);
    for (size_t i = 0; i < parent->childNode.size(); ++i) {
        std::shared_ptr<SmartLayoutNode> child = parent->childNode[i];

        z3::expr real_width = solver.ctx().real_val(std::to_string(child->size_.width.value).c_str());
        solver.add(child->size_.width.expr == real_width * parent->scaleInfo_.sizeScale.expr);
        z3::expr real_height = solver.ctx().real_val(std::to_string(child->size_.height.value).c_str());
        solver.add(child->size_.height.expr == real_height * parent->scaleInfo_.sizeScale.expr);

        // 默认约束
        AddDefaultConstraints(solver, parent, child);

        if (crossAxisAlign_ == FlexAlign::FLEX_START) {
            solver.add(child->position_.offsetX.expr == parent->position_.offsetX.expr);
        } else {
            solver.add(child->position_.offsetX.expr - parent->position_.offsetX.expr == 
                      parent->position_.offsetX.expr + parent->size_.width.expr - 
                      child->position_.offsetX.expr - child->size_.width.expr);
        }

        // 3. 垂直约束保证元素顺序排列（每个元素在上一节点底部 + 间距）
        if (i == 0) {
            solver.add(child->position_.offsetY.expr == parent->position_.offsetY.expr + 
                      float2expr(solver, child->edgesSpace_.top) * parent->scaleInfo_.spaceScale.expr);
        } else {
            std::shared_ptr<SmartLayoutNode> prev = parent->childNode[i - 1];
            // 当前 y = 前一节点底部 + 间距
            solver.add(child->position_.offsetY.expr == prev->position_.offsetY.expr + 
                      prev->size_.height.expr + 
                      float2expr(solver, prev->edgesSpace_.bottom) * parent->scaleInfo_.spaceScale.expr);
        }

        if (i == parent->childNode.size() - 1) {
            solver.add(child->position_.offsetY.expr + child->size_.height.expr == parent->position_.offsetY.expr + parent->size_.height.expr);
        }
    }
}

void SmartLayoutAlgorithm::addRowLayout(z3::optimize& solver, std::shared_ptr<SmartLayoutNode> parent)
{
    solver.add(parent->scaleInfo_.sizeScale.expr > 0);
    solver.add(parent->scaleInfo_.sizeScale.expr <= 1);
    solver.maximize(parent->scaleInfo_.sizeScale.expr);
    for (size_t i = 0; i < parent->childNode.size(); ++i) {
        std::shared_ptr<SmartLayoutNode> child = parent->childNode[i];

        z3::expr real_width = solver.ctx().real_val(std::to_string(child->size_.width.value).c_str());
        solver.add(child->size_.width.expr == real_width * parent->scaleInfo_.sizeScale.expr);
        z3::expr real_height = solver.ctx().real_val(std::to_string(child->size_.height.value).c_str());
        solver.add(child->size_.height.expr == real_height * parent->scaleInfo_.sizeScale.expr);

        // 默认约束
        AddDefaultConstraints(solver, parent, child);

        // 3. 水平约束保证元素垂直对齐（确保每个元素在高度上垂直分散）
        if (crossAxisAlign_ == FlexAlign::FLEX_START) {
            solver.add(child->position_.offsetY.expr == parent->position_.offsetY.expr);
        } else if (crossAxisAlign_ == FlexAlign::CENTER) {
            solver.add(child->position_.offsetY.expr - parent->position_.offsetY.expr == 
                      parent->position_.offsetY.expr + parent->size_.height.expr - 
                      child->position_.offsetY.expr - child->size_.height.expr);
        } else if (crossAxisAlign_ == FlexAlign::FLEX_END) {
            solver.add(child->position_.offsetY.expr + child->size_.height.expr == parent->position_.offsetY.expr + parent->size_.height.expr);
        }

        // 2. 垂直约束保证元素分水平排列（按顺序，每个元素在前一元素右侧）
        if (i == 0) {
            solver.add(child->position_.offsetX.expr == parent->position_.offsetX.expr);
        } else {
            std::shared_ptr<SmartLayoutNode> prev = parent->childNode[i - 1];
            // 当前 x = 前一节点右侧 + 间距
            solver.add(child->position_.offsetX.expr == prev->position_.offsetX.expr + 
                      prev->size_.width.expr + parent->scaleInfo_.spaceScale.expr);
        }

        if (i == parent->childNode.size() - 1) {
            solver.add(child->position_.offsetX.expr + child->size_.width.expr == parent->position_.offsetX.expr + parent->size_.width.expr);
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

float CalculateSpaceBetween(const RefPtr<LayoutWrapper>& child1, const RefPtr<LayoutWrapper>& child2, LayoutType layoutType)
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
    z3::context ctx;
    z3::optimize solver(ctx);
    
    const auto& children = layoutWrapper->GetAllChildrenWithBuild(false);
    if (children.empty()) {
        return;
    }

    auto layoutProperty = AceType::DynamicCast<FlexLayoutProperty>(layoutWrapper->GetLayoutProperty());
    CHECK_NULL_VOID(layoutProperty);
    mainAxisAlign_ = layoutProperty->GetMainAxisAlignValue(FlexAlign::FLEX_START);
    crossAxisAlign_ = layoutProperty->GetCrossAxisAlignValue(FlexAlign::CENTER);
    
    auto parentSize = layoutWrapper->GetGeometryNode()->GetFrameSize();
    // SmartLayoutNode root(ctx, "root");
    std::shared_ptr<SmartLayoutNode> root = std::make_shared<SmartLayoutNode>(ctx, "root");
    root->setFixedSizeConstraint(solver, parentSize.Width(), parentSize.Height());
    
    std::vector<std::shared_ptr<SmartLayoutNode>> childrenLayoutNode;
    uint32_t index = 1;

    for (auto it = children.begin(); it != children.end(); ++it) {
        const auto& child = *it; // 当前组件
        std::string childName = "child_" + std::to_string(child->GetHostNode()->GetId());
        std::shared_ptr<SmartLayoutNode> item = std::make_shared<SmartLayoutNode>(ctx, childName);

        bool isFirst = (it == children.begin());
        // 获取下一个迭代器（不会移动当前指针 it）
        auto nextIt = std::next(it);
        // 2. 判断是否是最后一个
        bool isLast = (nextIt == children.end());

        if (isFirst) {
            if (layoutType == LayoutType::COLUMN) {
                item->edgesSpace_.top = child->GetGeometryNode()->GetFrameOffset().GetY();
            } else {
                item->edgesSpace_.left = child->GetGeometryNode()->GetFrameOffset().GetX();
            }
        }

        if (!isLast) {
            const auto& nextChild = *nextIt;
            item->edgesSpace_.bottom = CalculateSpaceBetween(child, nextChild, layoutType);
            if (!isFirst) {
                item->edgesSpace_.top = childrenLayoutNode.back()->edgesSpace_.bottom;
                item->edgesSpace_.left = childrenLayoutNode.back()->edgesSpace_.right;
            }
        }

        if (isLast) {
            item->edgesSpace_.bottom = parentSize.Height() - child->GetGeometryNode()->GetFrameOffset().GetY() - child->GetGeometryNode()->GetFrameSize().Height();
        }
        LOGE("cr_debug edgesSpace_:[%{public}f %{public}f]", item->edgesSpace_.top, item->edgesSpace_.bottom);
        
        auto childSize = child->GetGeometryNode()->GetFrameSize();
        item->size_.width.value = childSize.Width();
        item->size_.height.value = childSize.Height();
        
        // 通过public edgesSpace设置直接访问（因为这是public的）
        if (child->GetHostTag() == "Blank") {
            item->edgesSpace_.bottom += item->size_.height.value;
            item->size_.height.value = 0;
        }

        childrenLayoutNode.push_back(item);
    }

    root->setChildren(childrenLayoutNode);
    
    // Apply layout type-specific constraints
    if (layoutType == LayoutType::COLUMN) {
        addColumnLayout(solver, root);
    } else {
        addRowLayout(solver, root);
    }
    LOGE("===== Solver constraints =====");
    z3::expr_vector constraints = solver.assertions(); // 获取约束列表
    // 遍历约束列表，逐个打印
    for (unsigned i = 0; i < constraints.size(); ++i) {
        LOGE("constraints[%{public}d]:  %{public}s",  i + 1 , constraints[i].to_string().c_str());
    }
    
    auto result = solver.check();
    if (result != z3::sat) {
        LOGE("cr_debug unsat!!!");
        return;
    }

    z3::model m = solver.get_model();
    std::string modelStr = Z3_model_to_string(ctx, m);

    index = 0;
    root->syncData(m);
    LOGE("Variables: root scale:%{public}f", root->scaleInfo_.sizeScale.value);
    for (const auto& child : children) {
        childrenLayoutNode[index]->syncData(m);
        
        // Calculate offset based on layout type
        float offset1 = 0, offset2 = 0;
        if (layoutType == LayoutType::COLUMN) {
            // Column layout: use left margin
            if (child->GetGeometryNode()->GetMargin()) {
                offset1 = child->GetGeometryNode()->GetMargin()->left.value_or(0);
            }
        } else {
            // Row layout: use top margin
            if (child->GetGeometryNode()->GetMargin()) {
                offset2 = child->GetGeometryNode()->GetMargin()->top.value_or(0);
            }
        }
        
        OffsetF offset(childrenLayoutNode[index]->position_.offsetX.value + offset1, 
                      childrenLayoutNode[index]->position_.offsetY.value + offset2);
        child->GetGeometryNode()->SetFrameOffset(offset);

        ItermScale(child, root->scaleInfo_.sizeScale.value);
        if (child->GetHostNode() != nullptr) {
            child->GetHostNode()->MarkDirtyNode(NG::PROPERTY_UPDATE_LAYOUT);
        }
        child->Layout();
        LOGE("Variables: name:%{public}s [%{public}f %{public}f %{public}f %{public}f]",
            childrenLayoutNode[index]->name_.c_str(), 
            childrenLayoutNode[index]->position_.offsetX.value,
            childrenLayoutNode[index]->position_.offsetY.value,
            childrenLayoutNode[index]->size_.width.value,
            childrenLayoutNode[index]->size_.height.value);
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