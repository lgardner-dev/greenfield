#include <cstdlib>
#include <limits>
#include <span>
#include <variant>
#include <vector>

#include "engine/visualization/VisualizationCommandList.h"
#include "tests/TestHelpers.h"

namespace
{

[[nodiscard]] bool PointsMatch(greenfield::Vec2 firstPoint, greenfield::Vec2 secondPoint)
{
    return firstPoint.x == secondPoint.x && firstPoint.y == secondPoint.y;
}

[[nodiscard]] const greenfield::VisualizationLineCommand* GetLine(const greenfield::VisualizationCommand& command)
{
    return std::get_if<greenfield::VisualizationLineCommand>(&command);
}

[[nodiscard]] const greenfield::VisualizationPolylineCommand* GetPolyline(const greenfield::VisualizationCommand& command)
{
    return std::get_if<greenfield::VisualizationPolylineCommand>(&command);
}

[[nodiscard]] const greenfield::VisualizationPointMarkerCommand* GetPointMarker(
    const greenfield::VisualizationCommand& command)
{
    return std::get_if<greenfield::VisualizationPointMarkerCommand>(&command);
}

[[nodiscard]] greenfield::Color MakeAccentColor()
{
    return greenfield::Color{.red = 0.25f, .green = 0.5f, .blue = 0.75f, .alpha = 1.0f};
}

[[nodiscard]] bool TestNewCommandListBeginsEmpty()
{
    const greenfield::VisualizationCommandList commandList;

    return commandList.IsEmpty() && commandList.Size() == 0U && commandList.Commands().empty();
}

[[nodiscard]] bool TestClipBoundsCanBeSetAndRead()
{
    using namespace greenfield;
    using greenfield::tests::RectanglesMatch;

    VisualizationCommandList commandList;
    const Rect clipBounds{.position = Vec2{10.0f, 20.0f}, .size = Vec2{300.0f, 180.0f}};

    return commandList.SetClipBounds(clipBounds) && RectanglesMatch(commandList.ClipBounds(), clipBounds);
}

[[nodiscard]] bool TestReversedClipBoundsNormalize()
{
    using namespace greenfield;
    using greenfield::tests::RectanglesMatch;

    VisualizationCommandList commandList;

    return commandList.SetClipBounds(Rect{.position = Vec2{500.0f, 300.0f}, .size = Vec2{-200.0f, -120.0f}}) &&
           RectanglesMatch(commandList.ClipBounds(),
                           Rect{.position = Vec2{300.0f, 180.0f}, .size = Vec2{200.0f, 120.0f}});
}

[[nodiscard]] bool TestInvalidClipBoundsDoNotCorruptPreviousBounds()
{
    using namespace greenfield;
    using greenfield::tests::RectanglesMatch;

    VisualizationCommandList commandList;
    const Rect validClipBounds{.position = Vec2{2.0f, 4.0f}, .size = Vec2{10.0f, 20.0f}};

    return commandList.SetClipBounds(validClipBounds) &&
           !commandList.SetClipBounds(
               Rect{.position = Vec2{std::numeric_limits<float>::quiet_NaN(), 0.0f}, .size = Vec2{8.0f, 8.0f}}) &&
           RectanglesMatch(commandList.ClipBounds(), validClipBounds);
}

[[nodiscard]] bool TestValidLineIsAppended()
{
    using namespace greenfield;
    using greenfield::tests::ColorsMatch;

    VisualizationCommandList commandList;
    const Color strokeColor = MakeAccentColor();

    if (!commandList.AddLine(Vec2{1.0f, 2.0f}, Vec2{3.0f, 4.0f}, strokeColor, 2.5f) || commandList.Size() != 1U)
    {
        return false;
    }

    const VisualizationLineCommand* line = GetLine(commandList.Commands()[0]);
    return line != nullptr && PointsMatch(line->start, Vec2{1.0f, 2.0f}) && PointsMatch(line->end, Vec2{3.0f, 4.0f}) &&
           ColorsMatch(line->strokeColor, strokeColor) && line->strokeThickness == 2.5f;
}

[[nodiscard]] bool TestValidPolylineIsAppended()
{
    using namespace greenfield;
    using greenfield::tests::ColorsMatch;

    VisualizationCommandList commandList;
    const std::vector<Vec2> points{Vec2{1.0f, 2.0f}, Vec2{3.0f, 4.0f}, Vec2{5.0f, 6.0f}};
    const Color strokeColor = MakeAccentColor();

    if (!commandList.AddPolyline(points, strokeColor, 1.5f) || commandList.Size() != 1U)
    {
        return false;
    }

    const VisualizationPolylineCommand* polyline = GetPolyline(commandList.Commands()[0]);
    return polyline != nullptr && polyline->points.size() == 3U && PointsMatch(polyline->points[0], points[0]) &&
           PointsMatch(polyline->points[1], points[1]) && PointsMatch(polyline->points[2], points[2]) &&
           ColorsMatch(polyline->strokeColor, strokeColor) && polyline->strokeThickness == 1.5f;
}

[[nodiscard]] bool TestPolylineOwnsCopiedPoints()
{
    using namespace greenfield;

    VisualizationCommandList commandList;
    std::vector<Vec2> points{Vec2{1.0f, 2.0f}, Vec2{3.0f, 4.0f}};

    if (!commandList.AddPolyline(points, MakeAccentColor(), 1.0f))
    {
        return false;
    }

    points[0] = Vec2{100.0f, 200.0f};

    const VisualizationPolylineCommand* polyline = GetPolyline(commandList.Commands()[0]);
    return polyline != nullptr && PointsMatch(polyline->points[0], Vec2{1.0f, 2.0f});
}

[[nodiscard]] bool TestValidPointMarkerIsAppended()
{
    using namespace greenfield;
    using greenfield::tests::ColorsMatch;

    VisualizationCommandList commandList;
    const Color fillColor{.red = 1.0f, .green = 0.25f, .blue = 0.0f, .alpha = 0.75f};

    if (!commandList.AddPointMarker(Vec2{40.0f, 50.0f}, 6.0f, fillColor) || commandList.Size() != 1U)
    {
        return false;
    }

    const VisualizationPointMarkerCommand* pointMarker = GetPointMarker(commandList.Commands()[0]);
    return pointMarker != nullptr && PointsMatch(pointMarker->center, Vec2{40.0f, 50.0f}) &&
           pointMarker->radius == 6.0f && ColorsMatch(pointMarker->fillColor, fillColor);
}

[[nodiscard]] bool TestMixedCommandInsertionOrderIsPreserved()
{
    using namespace greenfield;

    VisualizationCommandList commandList;
    const std::vector<Vec2> points{Vec2{1.0f, 1.0f}, Vec2{2.0f, 2.0f}};

    return commandList.AddPointMarker(Vec2{5.0f, 5.0f}, 3.0f, MakeAccentColor()) &&
           commandList.AddLine(Vec2{0.0f, 0.0f}, Vec2{1.0f, 1.0f}, MakeAccentColor(), 1.0f) &&
           commandList.AddPolyline(points, MakeAccentColor(), 2.0f) && commandList.Size() == 3U &&
           GetPointMarker(commandList.Commands()[0]) != nullptr && GetLine(commandList.Commands()[1]) != nullptr &&
           GetPolyline(commandList.Commands()[2]) != nullptr;
}

[[nodiscard]] bool TestReadOnlyAccessAllowsTypeInspection()
{
    using namespace greenfield;

    VisualizationCommandList commandList;
    const VisualizationCommandList& readOnlyCommandList = commandList;

    return commandList.AddLine(Vec2{0.0f, 0.0f}, Vec2{10.0f, 10.0f}, MakeAccentColor(), 1.0f) &&
           std::holds_alternative<VisualizationLineCommand>(readOnlyCommandList.Commands()[0]);
}

[[nodiscard]] bool TestClearRemovesCommands()
{
    using namespace greenfield;

    VisualizationCommandList commandList;
    if (!commandList.AddLine(Vec2{0.0f, 0.0f}, Vec2{1.0f, 1.0f}, MakeAccentColor(), 1.0f))
    {
        return false;
    }

    commandList.Clear();

    return commandList.IsEmpty() && commandList.Size() == 0U;
}

[[nodiscard]] bool TestClearPreservesClipBounds()
{
    using namespace greenfield;
    using greenfield::tests::RectanglesMatch;

    VisualizationCommandList commandList;
    const Rect clipBounds{.position = Vec2{10.0f, 20.0f}, .size = Vec2{30.0f, 40.0f}};

    if (!commandList.SetClipBounds(clipBounds) ||
        !commandList.AddPointMarker(Vec2{11.0f, 22.0f}, 3.0f, MakeAccentColor()))
    {
        return false;
    }

    commandList.Clear();

    return commandList.IsEmpty() && RectanglesMatch(commandList.ClipBounds(), clipBounds);
}

[[nodiscard]] bool TestNonFiniteLineGeometryIsRejected()
{
    using namespace greenfield;

    VisualizationCommandList commandList;

    return !commandList.AddLine(Vec2{std::numeric_limits<float>::infinity(), 0.0f}, Vec2{1.0f, 1.0f}, MakeAccentColor(), 1.0f) &&
           commandList.IsEmpty();
}

[[nodiscard]] bool TestInvalidLineThicknessIsRejected()
{
    using namespace greenfield;

    VisualizationCommandList commandList;

    return !commandList.AddLine(Vec2{0.0f, 0.0f}, Vec2{1.0f, 1.0f}, MakeAccentColor(), 0.0f) &&
           !commandList.AddLine(Vec2{0.0f, 0.0f}, Vec2{1.0f, 1.0f}, MakeAccentColor(), -1.0f) &&
           !commandList.AddLine(Vec2{0.0f, 0.0f}, Vec2{1.0f, 1.0f}, MakeAccentColor(), std::numeric_limits<float>::quiet_NaN()) &&
           commandList.IsEmpty();
}

[[nodiscard]] bool TestInvalidPolylineIsRejected()
{
    using namespace greenfield;

    VisualizationCommandList commandList;
    const std::vector<Vec2> onePoint{Vec2{0.0f, 0.0f}};
    const std::vector<Vec2> nonFinitePoints{Vec2{0.0f, 0.0f},
                                            Vec2{std::numeric_limits<float>::quiet_NaN(), 1.0f}};
    const std::vector<Vec2> validPoints{Vec2{0.0f, 0.0f}, Vec2{1.0f, 1.0f}};

    return !commandList.AddPolyline(onePoint, MakeAccentColor(), 1.0f) &&
           !commandList.AddPolyline(nonFinitePoints, MakeAccentColor(), 1.0f) &&
           !commandList.AddPolyline(validPoints, MakeAccentColor(), 0.0f) &&
           !commandList.AddPolyline(validPoints, MakeAccentColor(), std::numeric_limits<float>::infinity()) &&
           commandList.IsEmpty();
}

[[nodiscard]] bool TestInvalidPointMarkerIsRejected()
{
    using namespace greenfield;

    VisualizationCommandList commandList;

    return !commandList.AddPointMarker(Vec2{std::numeric_limits<float>::infinity(), 5.0f}, 2.0f, MakeAccentColor()) &&
           !commandList.AddPointMarker(Vec2{5.0f, 5.0f}, 0.0f, MakeAccentColor()) &&
           !commandList.AddPointMarker(Vec2{5.0f, 5.0f}, -1.0f, MakeAccentColor()) &&
           !commandList.AddPointMarker(Vec2{5.0f, 5.0f}, std::numeric_limits<float>::quiet_NaN(), MakeAccentColor()) &&
           commandList.IsEmpty();
}

[[nodiscard]] bool TestRejectedCommandsDoNotChangeExistingOrder()
{
    using namespace greenfield;

    VisualizationCommandList commandList;
    const std::vector<Vec2> validPolyline{Vec2{2.0f, 2.0f}, Vec2{3.0f, 3.0f}};

    if (!commandList.AddLine(Vec2{0.0f, 0.0f}, Vec2{1.0f, 1.0f}, MakeAccentColor(), 1.0f) ||
        !commandList.AddPolyline(validPolyline, MakeAccentColor(), 1.0f))
    {
        return false;
    }

    const std::size_t previousSize = commandList.Size();
    const bool rejectedLine =
        !commandList.AddLine(Vec2{0.0f, 0.0f}, Vec2{1.0f, 1.0f}, MakeAccentColor(), -1.0f);
    const bool rejectedMarker = !commandList.AddPointMarker(Vec2{4.0f, 4.0f}, 0.0f, MakeAccentColor());

    return rejectedLine && rejectedMarker && commandList.Size() == previousSize &&
           GetLine(commandList.Commands()[0]) != nullptr && GetPolyline(commandList.Commands()[1]) != nullptr;
}

} // namespace

int main()
{
    if (!TestNewCommandListBeginsEmpty() || !TestClipBoundsCanBeSetAndRead() || !TestReversedClipBoundsNormalize() ||
        !TestInvalidClipBoundsDoNotCorruptPreviousBounds() || !TestValidLineIsAppended() ||
        !TestValidPolylineIsAppended() || !TestPolylineOwnsCopiedPoints() || !TestValidPointMarkerIsAppended() ||
        !TestMixedCommandInsertionOrderIsPreserved() || !TestReadOnlyAccessAllowsTypeInspection() ||
        !TestClearRemovesCommands() || !TestClearPreservesClipBounds() || !TestNonFiniteLineGeometryIsRejected() ||
        !TestInvalidLineThicknessIsRejected() || !TestInvalidPolylineIsRejected() || !TestInvalidPointMarkerIsRejected() ||
        !TestRejectedCommandsDoNotChangeExistingOrder())
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
