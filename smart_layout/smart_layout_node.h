/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#ifndef FOUNDATION_ACE_FRAMEWORKS_CORE_COMPONENTS_NG_PATTERN_SMART_LAYOUT_SMART_LAYOUT_NODE_H
#define FOUNDATION_ACE_FRAMEWORKS_CORE_COMPONENTS_NG_PATTERN_SMART_LAYOUT_SMART_LAYOUT_NODE_H

#include <memory>
#include <string>
#include <vector>

#include "localsmt/localsmt.h"

namespace OHOS::Ace::NG {

struct EdgesSpaces {
    double left = 0.0;
    double right = 0.0;
    double top = 0.0;
    double bottom = 0.0;
};

struct ExprAndValue {
    localsmt::Expr expr;
    double value = 0.0;
};

struct NodePosition {
    ExprAndValue offsetX;
    ExprAndValue offsetY;
};

struct NodeSize {
    ExprAndValue width;
    ExprAndValue height;
};

struct ScaleInfo
{
    ExprAndValue spaceScale;
    ExprAndValue sizeScale;
};


class SmartLayoutNode {
public:
    std::string name_;
    NodePosition position_;
    NodeSize size_;
    ScaleInfo scaleInfo_;
    EdgesSpaces space_;
    SmartLayoutNode* parent_ = nullptr;
    std::vector<std::shared_ptr<SmartLayoutNode>> children_ = {};

    SmartLayoutNode(localsmt::Engine& engine, const std::string& name)
        : name_(name),
        position_{
            {engine.var((name_ + ".x").c_str()), 0.0},
            {engine.var((name_ + ".y").c_str()), 0.0}
        },
        size_{
            {engine.var((name_ + ".w").c_str()), 0.0},
            {engine.var((name_ + ".h").c_str()), 0.0}
        },
        scaleInfo_{
            {engine.var((name_ + ".spaceScale").c_str()), 1.0},
            {engine.var((name_ + ".sizeScale").c_str()), 1.0}
        }
    {}

    ~SmartLayoutNode() = default;

    void SetFixedSize(double width, double height)
    {
        size_.width.value = width;
        size_.height.value = height;
    }

    void SetFixedSizeConstraints(localsmt::Engine& engine, double width, double height)
    {
        size_.width.value = width;
        size_.height.value = height;
        engine.add(size_.width.expr == width);
        engine.add(size_.height.expr == height);
    }

    void SetChildren(const std::vector<std::shared_ptr<SmartLayoutNode>>& children)
    {
        children_ = children;
        for (auto& child : children_) {
            child->parent_ = this;
        }
    }

    void SyncData(localsmt::Engine& engine)
    {
        position_.offsetX.value = engine.getVariable(name_ + ".x");
        position_.offsetY.value = engine.getVariable(name_ + ".y");
        size_.width.value = engine.getVariable(name_ + ".w");
        size_.height.value = engine.getVariable(name_ + ".h");
        if (children_.size() > 0) {
            scaleInfo_.spaceScale.value = engine.getVariable(name_ + ".spaceScale");
            scaleInfo_.sizeScale.value = engine.getVariable(name_ + ".sizeScale");
        }
    }
};

} // namespace OHOS::Ace::NG

#endif // FOUNDATION_ACE_FRAMEWORKS_CORE_COMPONENTS_NG_PATTERN_SMART_LAYOUT_SMART_LAYOUT_NODE_H
