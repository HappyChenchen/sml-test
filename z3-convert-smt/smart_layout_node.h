#pragma once

#include <memory>
#include <string>
#include <vector>

#include "../smart_layout/localsmt/localsmt.h"

namespace HHHH::HHH::HH {

struct EdgesSpace {
    float top = 0.0f;
    float bottom = 0.0f;
    float left = 0.0f;
    float right = 0.0f;
};

struct ExprAndValue {
    localsmt::Expr expr;
    float value = 0.0f;
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
    ExprAndValue crossSpaceScale;
};

class SmartLayoutNode {
public:
    std::string name_;
    NodePosition position_;
    NodeSize size_;
    ScaleInfo scaleInfo_;
    EdgesSpace edgesSpace_;
    EdgesSpace marginSpace_;

    SmartLayoutNode* parentNode = nullptr;
    std::vector<std::shared_ptr<SmartLayoutNode>> childNode = {};

    SmartLayoutNode(localsmt::Engine& engine, const std::string& name)
        : name_(name),
          position_({ { engine.var(name_ + ".x"), 0.0f }, { engine.var(name_ + ".y"), 0.0f } }),
          size_({ { engine.var(name_ + ".w"), 0.0f }, { engine.var(name_ + ".h"), 0.0f } }),
          scaleInfo_({ { engine.var(name_ + ".spaceScale"), 1.0f },
              { engine.var(name_ + ".sizeScale"), 1.0f },
              { engine.var(name_ + ".crossSpaceScale"), 1.0f } })
    {}

    ~SmartLayoutNode() = default;

    void setFixedSizeConstraint(localsmt::Engine& engine, float width, float height)
    {
        size_.width.value = width;
        size_.height.value = height;
        engine.add(size_.width.expr == static_cast<double>(width));
        engine.add(size_.height.expr == static_cast<double>(height));
    }

    void setChildren(const std::vector<std::shared_ptr<SmartLayoutNode>>& children)
    {
        childNode = children;
        for (auto child : childNode) {
            if (child == nullptr) {
                continue;
            }
            child->parentNode = this;
        }
    }

    void syncData(localsmt::Engine& engine)
    {
        position_.offsetX.value = static_cast<float>(engine.getVariable(name_ + ".x"));
        position_.offsetY.value = static_cast<float>(engine.getVariable(name_ + ".y"));
        size_.width.value = static_cast<float>(engine.getVariable(name_ + ".w"));
        size_.height.value = static_cast<float>(engine.getVariable(name_ + ".h"));
        if (!childNode.empty()) {
            scaleInfo_.sizeScale.value = static_cast<float>(engine.getVariable(name_ + ".sizeScale"));
            scaleInfo_.spaceScale.value = static_cast<float>(engine.getVariable(name_ + ".spaceScale"));
            scaleInfo_.crossSpaceScale.value = static_cast<float>(engine.getVariable(name_ + ".crossSpaceScale"));
        }
    }
};

} // namespace HHHH::HHH::HH
