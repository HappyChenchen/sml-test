/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FOUNDATION_ACE_FRAMEWORKS_CORE_COMPONENTS_NG_PATTERN_FLEX_SMART_LAYOUT_NODE_H
#define FOUNDATION_ACE_FRAMEWORKS_CORE_COMPONENTS_NG_PATTERN_FLEX_SMART_LAYOUT_NODE_H

#include <memory>
#include <string>
#include <vector>
#include "localsmt/localsmt.h"

namespace OHOS::Ace::NG {

struct EdgesSpace {
    float top = 0, bottom = 0, left = 0, right = 0;
};

// 使用 localsmt 的 Expr 类型替换 z3::expr
struct ExprAndValue {
    localsmt::Expr expr;
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
    localsmt::Expr space;
    EdgesSpace edgesSpace_;
    SmartLayoutNode* parentNode = {nullptr};
    std::vector<std::shared_ptr<SmartLayoutNode>> childNode = {};

    // 构造函数用于在 LocalSMT Engine 中初始化表达式
    SmartLayoutNode(localsmt::Engine& engine, const std::string& name)
        : name_(name),
          position_({{engine.var((name_ + "_x").c_str(), 0, 10000), 0}, 
                     {engine.var((name_ + "_y").c_str(), 0, 10000), 0}}),
          size_({{engine.var((name_ + "_w").c_str(), 0, 10000), 0}, 
                  {engine.var((name_ + "_h").c_str(), 0, 10000), 0}}),
          scaleInfo_({{engine.var((name_ + "spaceScale").c_str(), 0, 1), 0},
                      {engine.var((name_ + "sizeScale").c_str(), 0, 1), 0}}),
          space(engine.var((name_ + "space").c_str(), 0, 1000))
    {
    }

    // 用于设置固定尺寸约束
    void setFixedSizeConstraint(localsmt::Engine& engine, float width, float height)
    {
        size_.width.value = width;
        size_.height.value = height;
        engine.add(size_.width.expr == static_cast<double>(width));
        engine.add(size_.height.expr == static_cast<double>(height));
    }

    void setChildren(std::vector<std::shared_ptr<SmartLayoutNode>> children)
    {
        for (auto& child : children) {
            child->parentNode = this;
            childNode.push_back(child);
        }
    }

    void syncData(localsmt::Engine& engine)
    {
        position_.offsetX.value = static_cast<float>(engine.getVariable(name_ + "_x"));
        position_.offsetY.value = static_cast<float>(engine.getVariable(name_ + "_y"));
        size_.width.value = static_cast<float>(engine.getVariable(name_ + "_w"));
        size_.height.value = static_cast<float>(engine.getVariable(name_ + "_h"));
        if (childNode.size() > 0) {
            scaleInfo_.sizeScale.value = static_cast<float>(engine.getVariable(name_ + "sizeScale"));
            scaleInfo_.spaceScale.value = static_cast<float>(engine.getVariable(name_ + "spaceScale"));
        }
    }

private:
    // 允许外部访问这些辅助函数
    friend class SmartLayoutAlgorithm;
};

} // namespace OHOS::Ace::NG

#endif // FOUNDATION_ACE_FRAMEWORKS_CORE_COMPONENTS_NG_PATTERN_FLEX_SMART_LAYOUT_NODE_H