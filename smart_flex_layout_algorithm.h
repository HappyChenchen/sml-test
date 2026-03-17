#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "smart_layout_node.h"

namespace HHHH::HHH::HH {

enum class SmartFlexDirection {
    ROW,
    COLUMN,
};

enum class SmartFlexWrapMode {
    NO_WRAP,
    WRAP,
    WRAP_REVERSE,
};

enum class SmartItemAlign {
    AUTO,
    START,
    CENTER,
    END,
    STRETCH,
    BASELINE,
};

struct SmartFlexConfig {
    SmartFlexDirection direction = SmartFlexDirection::ROW;
    SmartFlexWrapMode wrapMode = SmartFlexWrapMode::NO_WRAP;
    FlexAlign mainAlign = FlexAlign::FLEX_START;
    SmartItemAlign itemAlign = SmartItemAlign::AUTO;
    SmartItemAlign containerItemAlign = SmartItemAlign::START;
    float mainGap = 0.0f;
    float crossGap = 0.0f;
};

// 一阶段 Flex 独立引擎：
// - 支持 row/column + wrap/wrap-reverse
// - 主轴对齐使用 FlexAlign
// - 侧轴对齐使用 ItemAlign（含 baseline）
// - 溢出策略延续“先压间距，再压尺寸”的智能缩放
class SmartFlexLayoutAlgorithm {
public:
    SmartFlexLayoutAlgorithm() = default;
    ~SmartFlexLayoutAlgorithm() = default;

    void Layout(LayoutWrapper* layoutWrapper, const SmartFlexConfig& config);

private:
    struct ChildSnapshot {
        RefPtr<LayoutWrapper> wrapper;
        int32_t childId = -1;

        float baseX = 0.0f;
        float baseY = 0.0f;
        float baseWidth = 0.0f;
        float baseHeight = 0.0f;

        float baselineOffset = 0.0f; // 相对子项顶部的基线偏移

        float solveX = 0.0f;
        float solveY = 0.0f;
        float scaleX = 1.0f;
        float scaleY = 1.0f;

        float lineSizeScale = 1.0f;
        float lineSpaceScale = 1.0f;
    };

    struct FlexLine {
        std::vector<size_t> childIndices;

        float baseMainSum = 0.0f;
        float baseCrossMax = 0.0f;

        float lineAscentMax = 0.0f;
        float lineDescentMax = 0.0f;

        float solvedMainGap = 0.0f;
        float solvedLeading = 0.0f;
        float solvedSizeScale = 1.0f;
        float solvedSpaceScale = 1.0f;
        float solvedCrossSize = 0.0f;
        float solvedCrossStart = 0.0f;
    };

    static constexpr float EPS = 1e-4f;

    static float Clamp01(float value);
    static float GetMainSize(const ChildSnapshot& child, SmartFlexDirection direction);
    static float GetCrossSize(const ChildSnapshot& child, SmartFlexDirection direction);

    static float GetMainSizeAfterScale(const ChildSnapshot& child, SmartFlexDirection direction);
    static float GetCrossSizeAfterScale(const ChildSnapshot& child, SmartFlexDirection direction);

    static void ApplyScale(const RefPtr<LayoutWrapper>& item, float scaleX, float scaleY);
    static SmartItemAlign ResolveItemAlign(const SmartFlexConfig& config);

    void ResetChildrenToIdentity(const std::vector<RefPtr<LayoutWrapper>>& children) const;
    void BuildSnapshots(const std::vector<RefPtr<LayoutWrapper>>& children, std::vector<ChildSnapshot>& snapshots) const;

    void BuildLines(
        const std::vector<ChildSnapshot>& snapshots,
        float parentMain,
        const SmartFlexConfig& config,
        std::vector<FlexLine>& lines) const;

    void SolveMainAxisForLine(
        FlexLine& line,
        std::vector<ChildSnapshot>& snapshots,
        float parentMain,
        const SmartFlexConfig& config) const;

    void PlaceLinesOnCrossAxis(
        std::vector<FlexLine>& lines,
        std::vector<ChildSnapshot>& snapshots,
        float parentCross,
        const SmartFlexConfig& config) const;

    void PlaceChildrenInLine(
        const FlexLine& line,
        std::vector<ChildSnapshot>& snapshots,
        const SmartFlexConfig& config) const;

    void WriteBack(
        LayoutWrapper* layoutWrapper,
        const std::vector<ChildSnapshot>& snapshots,
        const SmartFlexConfig& config) const;
};

} // namespace HHHH::HHH::HH
