#ifndef FOUNDATION_ACE_FRAMEWORKS_CORE_COMPONENTS_NG_PATTERN_FLEX_SMART_LAYOUT_NODE_H
#define FOUNDATION_ACE_FRAMEWORKS_CORE_COMPONENTS_NG_PATTERN_FLEX_SMART_LAYOUT_NODE_H

#include <memory>
#include <string>
#include <vector>
#include "core/components_ng/pattern/z3/include/z3++.h"

namespace HHHH::HHH::HH {

struct EdgesSpace {
    float top = 0, bottom = 0, left = 0, right = 0;
};

struct ExprAndValue {
    z3::expr expr;
    float value;
};

struct NodePosition {
    ExprAndValue offsetX;
    ExprAndValue offsetY;
};

struct NodeSize {
    ExprAndValue width;
    ExprAndValue height;
};

struct ScaleInfo {
    ExprAndValue spaceScale;
    ExprAndValue sizeScale;
};


// Smart layout node used in smart layout algorithm
class SmartLayoutNode {
public:
    std::string name_;
    NodePosition position_;
    NodeSize size_;
    ScaleInfo scaleInfo_;
    EdgesSpace edgesSpace_;
    SmartLayoutNode* parentNode = {nullptr};
    std::vector<std::shared_ptr<SmartLayoutNode>> childNode = {};

    // 构造函数用于在 Z3 context 中初始化表达式
    SmartLayoutNode(z3::context &ctx, const std::string &name)
        : name_(name),
          position_({{ctx.real_const((name_ + ".x").c_str()), 0}, {ctx.real_const((name_ + ".y").c_str()), 0}}),
          size_({{ctx.real_const((name_ + ".w").c_str()), 0}, {ctx.real_const((name_ + ".h").c_str()), 0}}),
          scaleInfo_({{ctx.real_const((name_ + ".spaceScale").c_str()), 0},
                      {ctx.real_const((name_ + ".sizeScale").c_str()), 0}})
    {
    }

    // 用于设置固定尺寸约束
    void setFixedSizeConstraint(z3::optimize& solver, float width, float height)
    {
        z3::expr real_width = Float2Expr(solver, width);
        z3::expr real_height = Float2Expr(solver, height);
        solver.add(size_.width.expr == real_width);
        solver.add(size_.height.expr == real_height);
    }

    void setChildren(std::vector<std::shared_ptr<SmartLayoutNode>> children)
    {
        for (auto& child : children) {
            child->parentNode = this;
            childNode.push_back(child);
        }
    }

    void syncData(z3::model& m)
    {
        position_.offsetX.value = RealExpr2Float(m, position_.offsetX.expr);
        position_.offsetY.value = RealExpr2Float(m, position_.offsetY.expr);
        size_.width.value = RealExpr2Float(m, size_.width.expr);
        size_.height.value = RealExpr2Float(m, size_.height.expr);
        if (childNode.size() > 0) {
            scaleInfo_.sizeScale.value = RealExpr2Float(m, scaleInfo_.sizeScale.expr);
            scaleInfo_.spaceScale.value = RealExpr2Float(m, scaleInfo_.spaceScale.expr);
        }
    }

private:
    float RealExpr2Float(z3::model& m, z3::expr realData)
    {
        return (float)(m.get_const_interp(realData.decl()).numerator().as_int64()) /
            (float)(m.get_const_interp(realData.decl()).denominator().as_int64());
    }

    z3::expr Float2Expr(z3::optimize& solver, float value)
    {
        return solver.ctx().real_val(std::to_string(value).c_str());
    }
};

} // namespace HHHH::HHH::HH

#endif // FOUNDATION_ACE_FRAMEWORKS_CORE_COMPONENTS_NG_PATTERN_FLEX_SMART_LAYOUT_NODE_H  加一下注释