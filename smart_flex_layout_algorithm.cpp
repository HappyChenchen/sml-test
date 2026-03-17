#include "smart_flex_layout_algorithm.h"

#include <algorithm>

namespace HHHH::HHH::HH {

namespace {

const char* DirectionToString(SmartFlexDirection direction)
{
    return (direction == SmartFlexDirection::ROW) ? "FLEX_ROW" : "FLEX_COLUMN";
}

const char* MainAlignToString(FlexAlign align)
{
    switch (align) {
        case FlexAlign::FLEX_START:
            return "FLEX_START";
        case FlexAlign::CENTER:
            return "CENTER";
        case FlexAlign::FLEX_END:
            return "FLEX_END";
        case FlexAlign::SPACE_BETWEEN:
            return "SPACE_BETWEEN";
        case FlexAlign::SPACE_AROUND:
            return "SPACE_AROUND";
        case FlexAlign::SPACE_EVENLY:
            return "SPACE_EVENLY";
        default:
            return "UNKNOWN";
    }
}

const char* ItemAlignToString(SmartItemAlign align)
{
    switch (align) {
        case SmartItemAlign::AUTO:
            return "AUTO";
        case SmartItemAlign::START:
            return "START";
        case SmartItemAlign::CENTER:
            return "CENTER";
        case SmartItemAlign::END:
            return "END";
        case SmartItemAlign::STRETCH:
            return "STRETCH";
        case SmartItemAlign::BASELINE:
            return "BASELINE";
        default:
            return "UNKNOWN";
    }
}

bool IsSpaceAlign(FlexAlign align)
{
    return align == FlexAlign::SPACE_BETWEEN || align == FlexAlign::SPACE_AROUND || align == FlexAlign::SPACE_EVENLY;
}

} // namespace

float SmartFlexLayoutAlgorithm::Clamp01(float value)
{
    return std::max(0.0f, std::min(1.0f, value));
}

float SmartFlexLayoutAlgorithm::GetMainSize(const ChildSnapshot& child, SmartFlexDirection direction)
{
    return (direction == SmartFlexDirection::ROW) ? child.baseWidth : child.baseHeight;
}

float SmartFlexLayoutAlgorithm::GetCrossSize(const ChildSnapshot& child, SmartFlexDirection direction)
{
    return (direction == SmartFlexDirection::ROW) ? child.baseHeight : child.baseWidth;
}

float SmartFlexLayoutAlgorithm::GetMainSizeAfterScale(const ChildSnapshot& child, SmartFlexDirection direction)
{
    return (direction == SmartFlexDirection::ROW) ? child.baseWidth * child.scaleX : child.baseHeight * child.scaleY;
}

float SmartFlexLayoutAlgorithm::GetCrossSizeAfterScale(const ChildSnapshot& child, SmartFlexDirection direction)
{
    return (direction == SmartFlexDirection::ROW) ? child.baseHeight * child.scaleY : child.baseWidth * child.scaleX;
}

SmartItemAlign SmartFlexLayoutAlgorithm::ResolveItemAlign(const SmartFlexConfig& config)
{
    if (config.itemAlign != SmartItemAlign::AUTO) {
        return config.itemAlign;
    }
    return (config.containerItemAlign == SmartItemAlign::AUTO) ? SmartItemAlign::START : config.containerItemAlign;
}

void SmartFlexLayoutAlgorithm::ApplyScale(const RefPtr<LayoutWrapper>& item, float scaleX, float scaleY)
{
    CHECK_NULL_VOID(item);
    auto hostNode = item->GetHostNode();
    CHECK_NULL_VOID(hostNode);
    auto renderContext = hostNode->GetRenderContext();
    CHECK_NULL_VOID(renderContext);

    Dimension offsetX(0.0, DimensionUnit::PERCENT);
    Dimension offsetY(0.0, DimensionUnit::PERCENT);
    renderContext->UpdateTransformCenter(DimensionOffset(offsetX, offsetY));
    renderContext->UpdateTransformScale({ scaleX, scaleY });
}

void SmartFlexLayoutAlgorithm::ResetChildrenToIdentity(const std::vector<RefPtr<LayoutWrapper>>& children) const
{
    for (const auto& child : children) {
        ApplyScale(child, 1.0f, 1.0f);
        if (child->GetHostNode() != nullptr) {
            child->GetHostNode()->MarkDirtyNode(NG::PROPERTY_UPDATE_LAYOUT);
        }
        child->Layout();
    }
}

void SmartFlexLayoutAlgorithm::BuildSnapshots(
    const std::vector<RefPtr<LayoutWrapper>>& children,
    std::vector<ChildSnapshot>& snapshots) const
{
    snapshots.clear();
    snapshots.reserve(children.size());

    for (const auto& child : children) {
        auto geometry = child->GetGeometryNode();
        if (geometry == nullptr) {
            continue;
        }

        auto offset = geometry->GetFrameOffset();
        auto size = geometry->GetFrameSize();

        ChildSnapshot snapshot;
        snapshot.wrapper = child;
        snapshot.childId = child->GetHostNode() ? child->GetHostNode()->GetId() : -1;
        snapshot.baseX = offset.GetX();
        snapshot.baseY = offset.GetY();
        snapshot.baseWidth = size.Width();
        snapshot.baseHeight = size.Height();

        // 一阶段基线兜底：默认使用“底边”作为基线。
        snapshot.baselineOffset = size.Height();

        snapshots.emplace_back(snapshot);
    }
}

void SmartFlexLayoutAlgorithm::BuildLines(
    const std::vector<ChildSnapshot>& snapshots,
    float parentMain,
    const SmartFlexConfig& config,
    std::vector<FlexLine>& lines) const
{
    lines.clear();
    if (snapshots.empty()) {
        return;
    }

    FlexLine currentLine;
    float currentMain = 0.0f;

    auto FlushLine = [&]() {
        if (currentLine.childIndices.empty()) {
            return;
        }
        lines.emplace_back(currentLine);
        currentLine = FlexLine();
        currentMain = 0.0f;
    };

    const bool canWrap = (config.wrapMode != SmartFlexWrapMode::NO_WRAP);

    const SmartItemAlign resolvedItemAlign = ResolveItemAlign(config);
    for (size_t i = 0; i < snapshots.size(); ++i) {
        const auto& child = snapshots[i];
        const float childMain = GetMainSize(child, config.direction);
        const float childCross = GetCrossSize(child, config.direction);

        float predictedMain = childMain;
        if (!currentLine.childIndices.empty()) {
            predictedMain = currentMain + config.mainGap + childMain;
        }

        if (canWrap && !currentLine.childIndices.empty() && predictedMain > parentMain + EPS) {
            FlushLine();
        }

        if (!currentLine.childIndices.empty()) {
            currentMain += config.mainGap;
        }
        currentMain += childMain;

        currentLine.childIndices.emplace_back(i);
        currentLine.baseMainSum += childMain;
        currentLine.baseCrossMax = std::max(currentLine.baseCrossMax, childCross);

        if (config.direction == SmartFlexDirection::ROW && resolvedItemAlign == SmartItemAlign::BASELINE) {
            const float ascent = child.baselineOffset;
            const float descent = std::max(0.0f, childCross - child.baselineOffset);
            currentLine.lineAscentMax = std::max(currentLine.lineAscentMax, ascent);
            currentLine.lineDescentMax = std::max(currentLine.lineDescentMax, descent);
        }
    }

    FlushLine();
}

void SmartFlexLayoutAlgorithm::SolveMainAxisForLine(
    FlexLine& line,
    std::vector<ChildSnapshot>& snapshots,
    float parentMain,
    const SmartFlexConfig& config) const
{
    if (line.childIndices.empty()) {
        return;
    }

    const size_t childCount = line.childIndices.size();
    const float rawMainSum = line.baseMainSum;
    const float rawGapTotal = (childCount > 1) ? config.mainGap * static_cast<float>(childCount - 1) : 0.0f;

    float sizeScale = 1.0f;
    float spaceScale = 1.0f;

    if (IsSpaceAlign(config.mainAlign)) {
        if (rawMainSum > parentMain + EPS && rawMainSum > EPS) {
            sizeScale = Clamp01(parentMain / rawMainSum);
        }
        spaceScale = 1.0f;
    } else {
        float used = rawMainSum + rawGapTotal;
        if (used > parentMain + EPS) {
            if (rawMainSum < parentMain - EPS && rawGapTotal > EPS) {
                const float spaceBudget = std::max(0.0f, parentMain - rawMainSum);
                spaceScale = Clamp01(spaceBudget / rawGapTotal);
            } else {
                spaceScale = 0.0f;
            }

            used = rawMainSum + rawGapTotal * spaceScale;
            if (used > parentMain + EPS && rawMainSum > EPS) {
                sizeScale = Clamp01(parentMain / rawMainSum);
            }
        }
    }

    float scaledMainSum = 0.0f;
    for (const auto idx : line.childIndices) {
        auto& child = snapshots[idx];
        child.lineSizeScale = sizeScale;
        child.lineSpaceScale = spaceScale;
        child.scaleX = sizeScale;
        child.scaleY = sizeScale;
        scaledMainSum += GetMainSizeAfterScale(child, config.direction);
    }

    float solvedGap = 0.0f;
    float leading = 0.0f;

    const float scaledRawGapTotal = rawGapTotal * spaceScale;
    float usedMain = scaledMainSum + scaledRawGapTotal;
    float freeSpace = std::max(0.0f, parentMain - usedMain);

    if (config.mainAlign == FlexAlign::FLEX_START) {
        solvedGap = config.mainGap * spaceScale;
        leading = 0.0f;
    } else if (config.mainAlign == FlexAlign::CENTER) {
        solvedGap = config.mainGap * spaceScale;
        leading = freeSpace / 2.0f;
    } else if (config.mainAlign == FlexAlign::FLEX_END) {
        solvedGap = config.mainGap * spaceScale;
        leading = freeSpace;
    } else if (config.mainAlign == FlexAlign::SPACE_BETWEEN) {
        solvedGap = (childCount > 1) ? freeSpace / static_cast<float>(childCount - 1) : 0.0f;
        leading = 0.0f;
    } else if (config.mainAlign == FlexAlign::SPACE_AROUND) {
        solvedGap = (childCount > 0) ? freeSpace / static_cast<float>(childCount) : 0.0f;
        leading = solvedGap / 2.0f;
    } else if (config.mainAlign == FlexAlign::SPACE_EVENLY) {
        solvedGap = freeSpace / static_cast<float>(childCount + 1);
        leading = solvedGap;
    } else {
        solvedGap = config.mainGap * spaceScale;
        leading = 0.0f;
    }

    line.solvedMainGap = solvedGap;
    line.solvedLeading = leading;
    line.solvedSizeScale = sizeScale;
    line.solvedSpaceScale = spaceScale;

    const SmartItemAlign resolvedItemAlign = ResolveItemAlign(config);
    if (config.direction == SmartFlexDirection::ROW && resolvedItemAlign == SmartItemAlign::BASELINE) {
        line.solvedCrossSize = (line.lineAscentMax + line.lineDescentMax) * sizeScale;
    } else {
        line.solvedCrossSize = line.baseCrossMax * sizeScale;
    }
}

void SmartFlexLayoutAlgorithm::PlaceLinesOnCrossAxis(
    std::vector<FlexLine>& lines,
    std::vector<ChildSnapshot>& snapshots,
    float parentCross,
    const SmartFlexConfig& config) const
{
    if (lines.empty()) {
        return;
    }

    float totalCross = 0.0f;
    for (const auto& line : lines) {
        totalCross += line.solvedCrossSize;
    }
    if (lines.size() > 1) {
        totalCross += config.crossGap * static_cast<float>(lines.size() - 1);
    }

    float crossScale = 1.0f;
    if (totalCross > parentCross + EPS && totalCross > EPS) {
        crossScale = Clamp01(parentCross / totalCross);
    }

    const float solvedCrossGap = config.crossGap * crossScale;
    for (auto& line : lines) {
        line.solvedCrossSize *= crossScale;
    }

    // wrap-reverse：第一行从侧轴末端开始放置。
    float consumedCross = 0.0f;
    for (size_t i = 0; i < lines.size(); ++i) {
        auto& line = lines[i];
        float crossStart = 0.0f;
        if (config.wrapMode == SmartFlexWrapMode::WRAP_REVERSE) {
            crossStart = parentCross - consumedCross - line.solvedCrossSize;
        } else {
            crossStart = consumedCross;
        }
        line.solvedCrossStart = crossStart;
        consumedCross += line.solvedCrossSize;
        if (i + 1 < lines.size()) {
            consumedCross += solvedCrossGap;
        }
    }

    for (const auto& line : lines) {
        PlaceChildrenInLine(line, snapshots, config);
    }
}

void SmartFlexLayoutAlgorithm::PlaceChildrenInLine(
    const FlexLine& line,
    std::vector<ChildSnapshot>& snapshots,
    const SmartFlexConfig& config) const
{
    if (line.childIndices.empty()) {
        return;
    }

    const SmartItemAlign resolvedItemAlign = ResolveItemAlign(config);
    float mainCursor = line.solvedLeading;
    const bool baselineSupported =
        (config.direction == SmartFlexDirection::ROW && resolvedItemAlign == SmartItemAlign::BASELINE);

    const float lineAscentScaled = line.lineAscentMax * line.solvedSizeScale;

    for (size_t i = 0; i < line.childIndices.size(); ++i) {
        auto& child = snapshots[line.childIndices[i]];

        const float childMain = GetMainSizeAfterScale(child, config.direction);
        float childCross = GetCrossSizeAfterScale(child, config.direction);
        float childCrossPos = line.solvedCrossStart;

        if (baselineSupported) {
            const float ascentScaled = child.baselineOffset * child.lineSizeScale;
            childCrossPos = line.solvedCrossStart + (lineAscentScaled - ascentScaled);
        } else if (resolvedItemAlign == SmartItemAlign::CENTER) {
            childCrossPos = line.solvedCrossStart + (line.solvedCrossSize - childCross) / 2.0f;
        } else if (resolvedItemAlign == SmartItemAlign::END) {
            childCrossPos = line.solvedCrossStart + (line.solvedCrossSize - childCross);
        } else if (resolvedItemAlign == SmartItemAlign::STRETCH) {
            // stretch 通过非等比缩放实现。若原始 cross 尺寸为 0，则退化为 START。
            const float rawCross = GetCrossSize(child, config.direction);
            if (rawCross > EPS) {
                const float targetCrossScale = line.solvedCrossSize / rawCross;
                if (config.direction == SmartFlexDirection::ROW) {
                    child.scaleY = targetCrossScale;
                } else {
                    child.scaleX = targetCrossScale;
                }
                childCross = GetCrossSizeAfterScale(child, config.direction);
                childCrossPos = line.solvedCrossStart;
            }
        }

        if (config.direction == SmartFlexDirection::ROW) {
            child.solveX = mainCursor;
            child.solveY = childCrossPos;
        } else {
            child.solveX = childCrossPos;
            child.solveY = mainCursor;
        }

        mainCursor += childMain;
        if (i + 1 < line.childIndices.size()) {
            mainCursor += line.solvedMainGap;
        }
    }
}

void SmartFlexLayoutAlgorithm::WriteBack(
    LayoutWrapper* layoutWrapper,
    const std::vector<ChildSnapshot>& snapshots,
    const SmartFlexConfig& config) const
{
    CHECK_NULL_VOID(layoutWrapper);
    auto parentGeometry = layoutWrapper->GetGeometryNode();
    CHECK_NULL_VOID(parentGeometry);

    const auto parentOffset = parentGeometry->GetFrameOffset();
    const auto parentSize = parentGeometry->GetFrameSize();
    const int32_t parentId = layoutWrapper->GetHostNode() ? layoutWrapper->GetHostNode()->GetId() : -1;

    const SmartItemAlign resolvedItemAlign = ResolveItemAlign(config);
    const char* layoutStr = DirectionToString(config.direction);
    const char* mainStr = MainAlignToString(config.mainAlign);
    const char* crossStr = ItemAlignToString(resolvedItemAlign);

    for (const auto& child : snapshots) {
        auto geometry = child.wrapper->GetGeometryNode();
        if (geometry == nullptr) {
            continue;
        }

        const float finalX = parentOffset.GetX() + child.solveX;
        const float finalY = parentOffset.GetY() + child.solveY;
        geometry->SetFrameOffset(OffsetF(finalX, finalY));

        ApplyScale(child.wrapper, child.scaleX, child.scaleY);
        if (child.wrapper->GetHostNode() != nullptr) {
            child.wrapper->GetHostNode()->MarkDirtyNode(NG::PROPERTY_UPDATE_LAYOUT);
        }
        child.wrapper->Layout();

        const float solvedWidth = child.baseWidth * child.scaleX;
        const float solvedHeight = child.baseHeight * child.scaleY;
        LOGE("smart_layout compact layout:%{public}s main:%{public}s cross:%{public}s "
             "childId:%{public}d parentId:%{public}d "
             "parentXYWH:[%{public}.2f %{public}.2f %{public}.2f %{public}.2f] "
             "childXYWH:[%{public}.2f %{public}.2f %{public}.2f %{public}.2f] "
             "spacescale:%{public}.4f sizescale:%{public}.4f",
            layoutStr,
            mainStr,
            crossStr,
            child.childId,
            parentId,
            parentOffset.GetX(),
            parentOffset.GetY(),
            parentSize.Width(),
            parentSize.Height(),
            finalX,
            finalY,
            solvedWidth,
            solvedHeight,
            child.lineSpaceScale,
            child.lineSizeScale);
    }
}

void SmartFlexLayoutAlgorithm::Layout(LayoutWrapper* layoutWrapper, const SmartFlexConfig& config)
{
    CHECK_NULL_VOID(layoutWrapper);
    auto parentGeometry = layoutWrapper->GetGeometryNode();
    CHECK_NULL_VOID(parentGeometry);

    const auto parentSize = parentGeometry->GetFrameSize();
    const auto& children = layoutWrapper->GetAllChildrenWithBuild(false);
    if (children.empty()) {
        return;
    }

    ResetChildrenToIdentity(children);

    std::vector<ChildSnapshot> snapshots;
    BuildSnapshots(children, snapshots);
    if (snapshots.empty()) {
        return;
    }

    const float parentMain = (config.direction == SmartFlexDirection::ROW) ? parentSize.Width() : parentSize.Height();
    const float parentCross = (config.direction == SmartFlexDirection::ROW) ? parentSize.Height() : parentSize.Width();

    std::vector<FlexLine> lines;
    BuildLines(snapshots, parentMain, config, lines);

    for (auto& line : lines) {
        SolveMainAxisForLine(line, snapshots, parentMain, config);
    }

    PlaceLinesOnCrossAxis(lines, snapshots, parentCross, config);
    WriteBack(layoutWrapper, snapshots, config);
}

} // namespace HHHH::HHH::HH
