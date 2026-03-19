#include <memory>
#include <string>
#include <vector>

#include "z3/include/z3++.h"

namespace HHHH::HHH::HH {

// 四边间距信息：用于记录元素之间的初始几何留白
struct EdgesSpace {
    float top = 0;
    float bottom = 0;
    float left = 0;
    float right = 0;
};

// 同时保存“表达式”和“求解后的值”
// expr：给 Z3 的未知量
// value：模型求解后回填的数值
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

// 全局缩放信息
// spaceScale：间距缩放
// sizeScale：尺寸缩放
struct ScaleInfo {
    ExprAndValue spaceScale;
    ExprAndValue sizeScale;
    ExprAndValue crossSpaceScale;
};

// 智能布局节点：是求解器侧的中间结构，不是实际渲染节点
class SmartLayoutNode {
public:
    std::string name_;
    NodePosition position_;
    NodeSize size_;
    ScaleInfo scaleInfo_;
    EdgesSpace edgesSpace_;
    EdgesSpace marginSpace_;

    SmartLayoutNode* parentNode = { nullptr };
    std::vector<std::shared_ptr<SmartLayoutNode>> childNode = {};

    // 在 Z3 context 中创建该节点所需的符号变量
    SmartLayoutNode(z3::context& ctx, const std::string& name)
        : name_(name),
          position_({ { ctx.real_const((name_ + ".x").c_str()), 0 }, { ctx.real_const((name_ + ".y").c_str()), 0 } }),
          size_({ { ctx.real_const((name_ + ".w").c_str()), 0 }, { ctx.real_const((name_ + ".h").c_str()), 0 } }),
          scaleInfo_({ { ctx.real_const((name_ + ".spaceScale").c_str()), 0 },
              { ctx.real_const((name_ + ".sizeScale").c_str()), 0 },
              { ctx.real_const((name_ + ".crossSpaceScale").c_str()), 0 } })
    {
    }

    // 给父节点添加固定尺寸约束（通常用于 root）
    void setFixedSizeConstraint(z3::optimize& solver, float width, float height)
    {
        z3::expr realWidth = Float2Expr(solver, width);
        z3::expr realHeight = Float2Expr(solver, height);
        solver.add(size_.width.expr == realWidth);
        solver.add(size_.height.expr == realHeight);
    }

    // 建立父子关系
    void setChildren(std::vector<std::shared_ptr<SmartLayoutNode>> children)
    {
        for (auto& child : children) {
            child->parentNode = this;
            childNode.push_back(child);
        }
    }

    // 从模型里同步数值，供真实布局引擎使用
    void syncData(z3::model& m)
    {
        position_.offsetX.value = RealExpr2Float(m, position_.offsetX.expr);
        position_.offsetY.value = RealExpr2Float(m, position_.offsetY.expr);
        size_.width.value = RealExpr2Float(m, size_.width.expr);
        size_.height.value = RealExpr2Float(m, size_.height.expr);

        // 约定：有子节点时才读取 scale（通常只有 root 需要）
        if (!childNode.empty()) {
            scaleInfo_.sizeScale.value = RealExpr2Float(m, scaleInfo_.sizeScale.expr);
            scaleInfo_.spaceScale.value = RealExpr2Float(m, scaleInfo_.spaceScale.expr);
            scaleInfo_.crossSpaceScale.value = RealExpr2Float(m, scaleInfo_.crossSpaceScale.expr);
        }
    }

private:
    // Z3 real -> float
    float RealExpr2Float(z3::model& m, z3::expr realData)
    {
        return static_cast<float>(m.get_const_interp(realData.decl()).numerator().as_int64()) /
            static_cast<float>(m.get_const_interp(realData.decl()).denominator().as_int64());
    }

    // float -> Z3 real
    z3::expr Float2Expr(z3::optimize& solver, float value)
    {
        return solver.ctx().real_val(std::to_string(value).c_str());
    }
};

} // namespace HHHH::HHH::HH
