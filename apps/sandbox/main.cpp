#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "engine/core/Color.h"
#include "engine/core/Rect.h"
#include "engine/core/Vec2.h"
#include "engine/input/InputState.h"
#include "engine/platform/SdlRasterPresenter.h"
#include "engine/platform/SdlStartupPresenter.h"
#include "engine/platform/SdlWindow.h"
#include "engine/render/RendererBackendKind.h"
#include "engine/render/fast2d/Fast2DRenderer.h"
#include "engine/render/webgpu/WebGpuContext.h"
#include "engine/render/webgpu/WebGpuRenderer.h"
#include "engine/ui/Layout.h"
#include "engine/ui/Style.h"
#include "engine/ui/UiContext.h"
#include "engine/visualization/Viewport2D.h"
#include "engine/visualization/VisualizationCommandList.h"

namespace
{

using namespace greenfield;

struct DashboardState
{
    int selectedNavigationIndex{0};
    int selectedAssetIndex{0};
    int selectedActionIndex{0};
};

struct MetricCard
{
    const char* label;
    const char* value;
    const char* detail;
    Color accentColor;
};

struct AlertItem
{
    const char* severity;
    const char* title;
    const char* detail;
    Color accentColor;
};

struct AssetCard
{
    const char* name;
    const char* status;
    const char* detail;
    Color accentColor;
};

struct SandboxWindowSize
{
    int width{1280};
    int height{720};
};

struct SandboxOptions
{
    SandboxWindowSize windowSize{};
    RendererBackendKind rendererBackendKind{RendererBackendKind::WebGpu};
    bool runHeadlessFast2D{false};
    std::string invalidRendererName{};
};

struct ControlRoomFrameRegions
{
    Rect visualizationPlotBounds{};
};

constexpr Color BackgroundColor{0.055f, 0.065f, 0.078f, 1.0f};
constexpr Color SurfaceColor{0.095f, 0.115f, 0.140f, 0.92f};
constexpr Color SurfaceSoftColor{0.120f, 0.145f, 0.175f, 0.76f};
constexpr Color SurfaceRaisedColor{0.135f, 0.165f, 0.205f, 0.86f};
constexpr Color BorderColor{0.310f, 0.390f, 0.480f, 0.38f};
constexpr Color BlueAccentColor{0.270f, 0.610f, 0.960f, 1.0f};
constexpr Color GreenAccentColor{0.180f, 0.770f, 0.560f, 1.0f};
constexpr Color AmberAccentColor{0.960f, 0.640f, 0.220f, 1.0f};
constexpr Color RedAccentColor{0.940f, 0.270f, 0.250f, 1.0f};
constexpr Color TextPrimaryColor{0.925f, 0.965f, 1.0f, 1.0f};
constexpr Color TextSecondaryColor{0.640f, 0.730f, 0.830f, 1.0f};
constexpr Color MutedTextColor{0.440f, 0.520f, 0.620f, 1.0f};

RectangleStyle MakePanelStyle(Color fillColor = SurfaceColor)
{
    return RectangleStyle{
        .fillColor = fillColor,
        .cornerRadius = 18.0f,
        .borderColor = BorderColor,
        .borderThickness = 1.0f,
    };
}

ButtonStyle MakeInteractivePanelStyle(bool isSelected, Color accentColor)
{
    const Color selectedFill{
        accentColor.red * 0.34f,
        accentColor.green * 0.34f,
        accentColor.blue * 0.34f,
        0.92f,
    };

    return ButtonStyle{
        .normal = isSelected ? selectedFill : SurfaceRaisedColor,
        .hovered = Color{0.175f, 0.220f, 0.275f, 0.96f},
        .pressed = Color{0.080f, 0.105f, 0.135f, 0.98f},
        .border = isSelected ? Color{accentColor.red, accentColor.green, accentColor.blue, 0.76f} : BorderColor,
        .textColor = TextPrimaryColor,
        .fontSize = 16.0f,
        .cornerRadius = 16.0f,
        .borderThickness = isSelected ? 1.5f : 1.0f,
    };
}

SliderStyle MakeRuntimeControlSliderStyle()
{
    return SliderStyle{
        .trackFill = Color{0.050f, 0.065f, 0.085f, 0.94f},
        .trackHovered = Color{0.095f, 0.130f, 0.170f, 0.96f},
        .trackPressed = Color{0.040f, 0.055f, 0.075f, 0.98f},
        .trackBorder = BorderColor,
        .progressFill = BlueAccentColor,
        .thumbFill = TextPrimaryColor,
        .textColor = TextSecondaryColor,
        .fontSize = 14.0f,
        .trackWidth = 150.0f,
        .trackHeight = 8.0f,
        .thumbWidth = 12.0f,
        .thumbHeight = 20.0f,
        .cornerRadius = 4.0f,
        .borderThickness = 1.0f,
    };
}

TextInputStyle MakeRuntimeTextInputStyle()
{
    return TextInputStyle{
        .fillColor = Color{0.050f, 0.065f, 0.085f, 0.94f},
        .hoveredFillColor = Color{0.095f, 0.130f, 0.170f, 0.96f},
        .borderColor = BorderColor,
        .textColor = TextPrimaryColor,
        .fontSize = 14.0f,
        .cornerRadius = 10.0f,
        .borderThickness = 1.0f,
        .horizontalTextInset = 12.0f,
    };
}

Rect MakeRectangle(float x, float y, float width, float height)
{
    return Rect{
        .position = Vec2{x, y},
        .size = Vec2{width, height},
    };
}

Rect InsetRectangleBy(const Rect& rectangle, float horizontalInset, float verticalInset)
{
    return MakeRectangle(rectangle.position.x + horizontalInset,
                         rectangle.position.y + verticalInset,
                         std::max(0.0f, rectangle.size.x - horizontalInset * 2.0f),
                         std::max(0.0f, rectangle.size.y - verticalInset * 2.0f));
}

Vec2 ConvertWorldPointToScreen(const Viewport2D& viewport, float x, float y)
{
    return viewport.WorldToScreen(Vec2{.x = x, .y = y});
}

float CalculateVisualizationScale(const Rect& plotBounds)
{
    constexpr float WorldWidth = 32.0f;
    constexpr float WorldHeight = 1.7f;

    const float horizontalScale = plotBounds.size.x / WorldWidth;
    const float verticalScale = plotBounds.size.y / WorldHeight;
    return std::max(1.0f, std::min(horizontalScale, verticalScale));
}

void DrawText(UiContext& uiContext, const char* text, float x, float y, float width, float height, float fontSize,
              Color color)
{
    uiContext.DrawText(text, MakeRectangle(x, y, width, height), fontSize, color);
}

void DrawAccentRule(UiContext& uiContext, const Rect& bounds, Color accentColor)
{
    uiContext.Panel(MakeRectangle(bounds.position.x, bounds.position.y, 4.0f, bounds.size.y),
                    RectangleStyle{
                        .fillColor = accentColor,
                        .cornerRadius = 4.0f,
                    });
}

void DrawSectionTitle(UiContext& uiContext, const Rect& bounds, const char* title, const char* detail)
{
    DrawText(uiContext, title, bounds.position.x, bounds.position.y, bounds.size.x, 28.0f, 20.0f, TextPrimaryColor);
    DrawText(uiContext, detail, bounds.position.x, bounds.position.y + 30.0f, bounds.size.x, 22.0f, 14.0f, TextSecondaryColor);
}

VisualizationCommandList BuildResponseTraceVisualizationCommands(const Rect& plotBounds)
{
    VisualizationCommandList visualizationCommands;
    (void)visualizationCommands.SetClipBounds(plotBounds);

    const Viewport2D viewport{
        plotBounds,
        Vec2{.x = 16.0f, .y = 0.0f},
        CalculateVisualizationScale(plotBounds),
    };

    std::vector<Vec2> primaryTracePoints;
    primaryTracePoints.reserve(25U);
    for (int pointIndex = 0; pointIndex <= 24; ++pointIndex)
    {
        const float x = static_cast<float>(pointIndex) * (32.0f / 24.0f);
        const float y = 0.58f * std::sin(x * 0.58f) + 0.22f * std::cos(x * 1.16f);
        primaryTracePoints.push_back(ConvertWorldPointToScreen(viewport, x, y));
    }

    std::vector<Vec2> referenceTracePoints;
    referenceTracePoints.reserve(13U);
    for (int pointIndex = 0; pointIndex <= 12; ++pointIndex)
    {
        const float x = static_cast<float>(pointIndex) * (32.0f / 12.0f);
        const float y = 0.34f * std::sin(x * 0.38f + 0.9f);
        referenceTracePoints.push_back(ConvertWorldPointToScreen(viewport, x, y));
    }

    (void)visualizationCommands.AddLine(ConvertWorldPointToScreen(viewport, -2.0f, -0.78f),
                                        ConvertWorldPointToScreen(viewport, 5.5f, -0.78f),
                                        Color{0.960f, 0.640f, 0.220f, 0.84f},
                                        3.0f);
    (void)visualizationCommands.AddLine(ConvertWorldPointToScreen(viewport, 28.0f, 0.88f),
                                        ConvertWorldPointToScreen(viewport, 34.0f, 0.88f),
                                        Color{0.180f, 0.770f, 0.560f, 0.76f},
                                        2.0f);
    (void)visualizationCommands.AddPolyline(referenceTracePoints,
                                            Color{0.640f, 0.730f, 0.830f, 0.58f},
                                            2.0f);
    (void)visualizationCommands.AddPolyline(primaryTracePoints, BlueAccentColor, 2.5f);

    constexpr std::array<int, 6> MarkerIndexes{2, 6, 10, 14, 18, 23};
    for (int markerIndex : MarkerIndexes)
    {
        (void)visualizationCommands.AddPointMarker(primaryTracePoints[static_cast<std::size_t>(markerIndex)],
                                                   4.5f,
                                                   Color{0.925f, 0.965f, 1.0f, 0.92f});
    }

    (void)visualizationCommands.AddPointMarker(ConvertWorldPointToScreen(viewport, 32.7f, 0.02f),
                                               5.5f,
                                               Color{0.940f, 0.270f, 0.250f, 0.82f});

    return visualizationCommands;
}

RenderCommandList BuildResponseTraceOverlayCommands(const Rect& plotBounds)
{
    RenderCommandList overlayCommands;

    const Rect badgeBounds = MakeRectangle(plotBounds.position.x + plotBounds.size.x * 0.55f,
                                           plotBounds.position.y + 8.0f,
                                           100.0f,
                                           26.0f);
    overlayCommands.AddFillRectangle(badgeBounds,
                                     Color{0.080f, 0.100f, 0.125f, 0.96f},
                                     10.0f,
                                     Color{0.270f, 0.610f, 0.960f, 0.62f},
                                     1.0f);
    overlayCommands.AddFillRectangle(MakeRectangle(badgeBounds.position.x + 10.0f,
                                                   badgeBounds.position.y + 9.0f,
                                                   18.0f,
                                                   8.0f),
                                     BlueAccentColor,
                                     4.0f);
    overlayCommands.AddText("Trace A",
                            MakeRectangle(badgeBounds.position.x + 34.0f,
                                          badgeBounds.position.y + 5.0f,
                                          58.0f,
                                          16.0f),
                            12.0f,
                            TextPrimaryColor);

    return overlayCommands;
}

std::string FindDefaultFontPath()
{
    const std::filesystem::path assetFontPath{"assets/fonts/Greenfield-Regular.ttf"};
    if (std::filesystem::exists(assetFontPath))
    {
        return assetFontPath.string();
    }

    const std::filesystem::path systemFontPath{"/usr/share/fonts/opentype/urw-base35/NimbusSans-Regular.otf"};
    if (std::filesystem::exists(systemFontPath))
    {
        return systemFontPath.string();
    }

    return {};
}

std::optional<SandboxWindowSize> ParseWindowSize(std::string_view text)
{
    const std::size_t separatorPosition = text.find('x');
    if (separatorPosition == std::string_view::npos)
    {
        return std::nullopt;
    }

    try
    {
        const int width = std::stoi(std::string{text.substr(0U, separatorPosition)});
        const int height = std::stoi(std::string{text.substr(separatorPosition + 1U)});
        if (width <= 0 || height <= 0)
        {
            return std::nullopt;
        }

        return SandboxWindowSize{.width = width, .height = height};
    }
    catch (const std::exception&)
    {
        return std::nullopt;
    }
}

SandboxOptions GetSandboxOptions(int argumentCount, char* argumentValues[])
{
    constexpr std::string_view WindowSizePrefix{"--window-size="};
    constexpr std::string_view RendererPrefix{"--renderer="};
    constexpr std::string_view HeadlessFlag{"--headless"};
    constexpr std::string_view DiagnosticFlag{"--diagnostic"};
    SandboxOptions sandboxOptions{};

    for (int argumentIndex = 1; argumentIndex < argumentCount; ++argumentIndex)
    {
        const std::string_view argument{argumentValues[argumentIndex]};
        if (argument.starts_with(WindowSizePrefix))
        {
            const std::optional<SandboxWindowSize> parsedWindowSize =
                ParseWindowSize(argument.substr(WindowSizePrefix.size()));
            if (parsedWindowSize.has_value())
            {
                sandboxOptions.windowSize = parsedWindowSize.value();
            }
            continue;
        }

        if (argument.starts_with(RendererPrefix))
        {
            const std::string_view rendererName = argument.substr(RendererPrefix.size());
            const std::optional<RendererBackendKind> parsedRendererBackendKind =
                ParseRendererBackendKind(rendererName);
            if (parsedRendererBackendKind.has_value())
            {
                sandboxOptions.rendererBackendKind = parsedRendererBackendKind.value();
                sandboxOptions.invalidRendererName.clear();
            }
            else
            {
                sandboxOptions.invalidRendererName = rendererName;
            }
            continue;
        }

        if (argument == HeadlessFlag || argument == DiagnosticFlag)
        {
            sandboxOptions.runHeadlessFast2D = true;
            continue;
        }
    }

    return sandboxOptions;
}

void DrawHeader(UiContext& uiContext, const Rect& bounds)
{
    uiContext.Panel(bounds, MakePanelStyle(Color{0.080f, 0.100f, 0.125f, 0.88f}));
    DrawText(uiContext, "Greenfield Control Room", bounds.position.x + 24.0f, bounds.position.y + 16.0f, 430.0f, 34.0f, 28.0f, TextPrimaryColor);
    DrawText(uiContext, "Generic operations dashboard sandbox", bounds.position.x + 26.0f, bounds.position.y + 52.0f, 360.0f, 22.0f, 15.0f, TextSecondaryColor);

    const Rect statusPill = MakeRectangle(bounds.position.x + bounds.size.x - 186.0f, bounds.position.y + 24.0f, 148.0f, 36.0f);
    uiContext.Panel(statusPill,
                    RectangleStyle{
                        .fillColor = Color{0.080f, 0.270f, 0.220f, 0.90f},
                        .cornerRadius = 18.0f,
                        .borderColor = Color{0.320f, 0.930f, 0.690f, 0.58f},
                        .borderThickness = 1.0f,
                    });
    DrawText(uiContext, "Nominal", statusPill.position.x + 24.0f, statusPill.position.y + 7.0f, 100.0f, 22.0f, 16.0f, TextPrimaryColor);
}

void DrawNavigationRail(UiContext& uiContext, const Rect& bounds, DashboardState& dashboardState)
{
    constexpr std::array<const char*, 5> NavigationItems{
        "Overview",
        "System Health",
        "Alert Queue",
        "Control Actions",
        "Asset Status",
    };

    uiContext.Panel(bounds, MakePanelStyle(Color{0.075f, 0.090f, 0.110f, 0.92f}));
    DrawText(uiContext, "GF", bounds.position.x + 28.0f, bounds.position.y + 22.0f, 60.0f, 28.0f, 24.0f, TextPrimaryColor);
    DrawText(uiContext, "Sandbox", bounds.position.x + 25.0f, bounds.position.y + 56.0f, 90.0f, 20.0f, 13.0f, MutedTextColor);

    const Rect autoReviewBounds = MakeRectangle(bounds.position.x + 14.0f, bounds.position.y + 94.0f, bounds.size.x - 28.0f, 32.0f);
    const bool autoReviewChanged = uiContext.Checkbox("navigation-auto-review", "Auto Review", autoReviewBounds);
    (void)autoReviewChanged;

    const Rect liveSyncBounds = MakeRectangle(bounds.position.x + 14.0f, autoReviewBounds.position.y + 42.0f, bounds.size.x - 28.0f, 32.0f);
    const bool liveSyncChanged = uiContext.Toggle("navigation-live-sync", "Live Sync", liveSyncBounds);
    (void)liveSyncChanged;

    float buttonTop = bounds.position.y + 194.0f;
    for (std::size_t index = 0; index < NavigationItems.size(); ++index)
    {
        const bool isSelected = dashboardState.selectedNavigationIndex == static_cast<int>(index);
        const Rect buttonBounds = MakeRectangle(bounds.position.x + 14.0f, buttonTop, bounds.size.x - 28.0f, 46.0f);
        if (uiContext.Button(std::string{"navigation-"} + std::to_string(index), "", buttonBounds, MakeInteractivePanelStyle(isSelected, BlueAccentColor)))
        {
            dashboardState.selectedNavigationIndex = static_cast<int>(index);
        }

        DrawText(uiContext, NavigationItems[index], buttonBounds.position.x + 16.0f, buttonBounds.position.y + 12.0f, buttonBounds.size.x - 24.0f, 20.0f, 14.0f, isSelected ? TextPrimaryColor : TextSecondaryColor);
        buttonTop += 58.0f;
    }
}

void DrawMetricCard(UiContext& uiContext, const Rect& bounds, const MetricCard& metricCard)
{
    uiContext.Panel(bounds, MakePanelStyle(SurfaceRaisedColor));
    uiContext.Panel(MakeRectangle(bounds.position.x + 16.0f, bounds.position.y + 16.0f, 42.0f, 4.0f),
                    RectangleStyle{
                        .fillColor = metricCard.accentColor,
                        .cornerRadius = 3.0f,
                    });
    DrawText(uiContext, metricCard.label, bounds.position.x + 16.0f, bounds.position.y + 32.0f, bounds.size.x - 32.0f, 22.0f, 14.0f, TextSecondaryColor);
    DrawText(uiContext, metricCard.value, bounds.position.x + 16.0f, bounds.position.y + 60.0f, bounds.size.x - 32.0f, 34.0f, 28.0f, TextPrimaryColor);
    DrawText(uiContext, metricCard.detail, bounds.position.x + 16.0f, bounds.position.y + 100.0f, bounds.size.x - 32.0f, 20.0f, 13.0f, MutedTextColor);
}

ControlRoomFrameRegions DrawSystemHealthPanel(UiContext& uiContext, const Rect& bounds)
{
    uiContext.Panel(bounds, MakePanelStyle(SurfaceColor));
    DrawSectionTitle(uiContext, MakeRectangle(bounds.position.x + 22.0f, bounds.position.y + 20.0f, 360.0f, 58.0f), "System Health", "Live synthetic state for a generic system");

    const Rect healthBadge = MakeRectangle(bounds.position.x + bounds.size.x - 152.0f, bounds.position.y + 24.0f, 112.0f, 34.0f);
    uiContext.Panel(healthBadge,
                    RectangleStyle{
                        .fillColor = Color{0.090f, 0.310f, 0.250f, 0.88f},
                        .cornerRadius = 17.0f,
                        .borderColor = Color{0.260f, 0.880f, 0.650f, 0.52f},
                        .borderThickness = 1.0f,
                    });
    DrawText(uiContext, "Nominal", healthBadge.position.x + 19.0f, healthBadge.position.y + 7.0f, 90.0f, 20.0f, 14.0f, TextPrimaryColor);

    constexpr std::array<MetricCard, 4> MetricCards{
        MetricCard{"Active Nodes", "128", "126 nominal", GreenAccentColor},
        MetricCard{"Throughput", "8.4K", "events per minute", BlueAccentColor},
        MetricCard{"Uptime", "99.98%", "rolling 30 days", GreenAccentColor},
        MetricCard{"Load", "62%", "steady utilization", AmberAccentColor},
    };

    const float cardTop = bounds.position.y + 94.0f;
    const float cardGap = 14.0f;
    const Rect cardRowBounds = MakeRectangle(bounds.position.x + 22.0f, cardTop, bounds.size.x - 44.0f, 132.0f);
    for (std::size_t index = 0; index < MetricCards.size(); ++index)
    {
        DrawMetricCard(uiContext, CalculateColumnRegion(cardRowBounds, index, MetricCards.size(), cardGap), MetricCards[index]);
    }

    const Rect visualizationPanel =
        MakeRectangle(bounds.position.x + 22.0f, bounds.position.y + 244.0f, bounds.size.x - 44.0f, bounds.size.y - 266.0f);
    uiContext.Panel(visualizationPanel, MakePanelStyle(SurfaceSoftColor));
    DrawText(uiContext,
             "Response Trace",
             visualizationPanel.position.x + 18.0f,
             visualizationPanel.position.y + 12.0f,
             180.0f,
             22.0f,
             17.0f,
             TextPrimaryColor);
    DrawText(uiContext,
             "Static viewport sample",
             visualizationPanel.position.x + visualizationPanel.size.x - 160.0f,
             visualizationPanel.position.y + 14.0f,
             136.0f,
             18.0f,
             12.0f,
             TextSecondaryColor);

    const Rect plotBounds = MakeRectangle(visualizationPanel.position.x + 18.0f,
                                          visualizationPanel.position.y + 40.0f,
                                          visualizationPanel.size.x - 36.0f,
                                          std::max(28.0f, visualizationPanel.size.y - 52.0f));
    uiContext.Panel(plotBounds,
                    RectangleStyle{
                        .fillColor = Color{0.050f, 0.065f, 0.085f, 0.92f},
                        .cornerRadius = 8.0f,
                        .borderColor = Color{0.250f, 0.310f, 0.390f, 0.48f},
                        .borderThickness = 1.0f,
                    });

    uiContext.Panel(MakeRectangle(plotBounds.position.x + plotBounds.size.x * 0.25f,
                                  plotBounds.position.y + 1.0f,
                                  1.0f,
                                  plotBounds.size.y - 2.0f),
                    RectangleStyle{
                        .fillColor = Color{0.250f, 0.310f, 0.390f, 0.42f},
                    });
    uiContext.Panel(MakeRectangle(plotBounds.position.x + plotBounds.size.x * 0.50f,
                                  plotBounds.position.y + 1.0f,
                                  1.0f,
                                  plotBounds.size.y - 2.0f),
                    RectangleStyle{
                        .fillColor = Color{0.250f, 0.310f, 0.390f, 0.42f},
                    });
    uiContext.Panel(MakeRectangle(plotBounds.position.x + plotBounds.size.x * 0.75f,
                                  plotBounds.position.y + 1.0f,
                                  1.0f,
                                  plotBounds.size.y - 2.0f),
                    RectangleStyle{
                        .fillColor = Color{0.250f, 0.310f, 0.390f, 0.42f},
                    });
    uiContext.Panel(MakeRectangle(plotBounds.position.x + 1.0f,
                                  plotBounds.position.y + plotBounds.size.y * 0.5f,
                                  plotBounds.size.x - 2.0f,
                                  1.0f),
                    RectangleStyle{
                        .fillColor = Color{0.250f, 0.310f, 0.390f, 0.42f},
                    });

    return ControlRoomFrameRegions{
        .visualizationPlotBounds = InsetRectangleBy(plotBounds, 1.0f, 1.0f),
    };
}

void DrawAlertQueuePanel(UiContext& uiContext, const Rect& bounds)
{
    constexpr std::array<AlertItem, 8> Alerts{
        AlertItem{"Warning", "Load variance", "Node group Delta above baseline", AmberAccentColor},
        AlertItem{"Warning", "Delayed check", "Asset Echo pending confirmation", AmberAccentColor},
        AlertItem{"Offline", "Standby node", "Node Hotel unavailable", RedAccentColor},
        AlertItem{"Notice", "Review queued", "Operator note awaiting acknowledgement", BlueAccentColor},
        AlertItem{"Warning", "Capacity drift", "Synthetic load shifted above target", AmberAccentColor},
        AlertItem{"Notice", "Schedule update", "Maintenance window ready for review", BlueAccentColor},
        AlertItem{"Warning", "Sensor variance", "Observation stream outside normal band", AmberAccentColor},
        AlertItem{"Offline", "Backup path", "Recovery check requested", RedAccentColor},
    };

    uiContext.Panel(bounds, MakePanelStyle(SurfaceColor));
    DrawSectionTitle(uiContext, MakeRectangle(bounds.position.x + 20.0f, bounds.position.y + 18.0f, 260.0f, 54.0f), "Alert Queue", "Prioritized generic observations");

    const float itemGap = 10.0f;
    const float itemHeight = 76.0f;
    const Rect alertListBounds =
        MakeRectangle(bounds.position.x + 18.0f, bounds.position.y + 86.0f, bounds.size.x - 36.0f, bounds.size.y - 104.0f);
    const float contentHeight = static_cast<float>(Alerts.size()) * itemHeight + static_cast<float>(Alerts.size() - 1U) * itemGap;
    const Rect contentBounds = uiContext.BeginVerticalScrollPanel("alert-queue", alertListBounds, contentHeight);
    for (std::size_t index = 0; index < Alerts.size(); ++index)
    {
        const AlertItem& alert = Alerts[index];
        const Rect alertBounds = MakeRectangle(contentBounds.position.x,
                                               contentBounds.position.y + (itemHeight + itemGap) * static_cast<float>(index),
                                               contentBounds.size.x,
                                               itemHeight);
        uiContext.Panel(alertBounds, MakePanelStyle(SurfaceRaisedColor));
        DrawAccentRule(uiContext, alertBounds, alert.accentColor);
        DrawText(uiContext, alert.severity, alertBounds.position.x + 16.0f, alertBounds.position.y + 9.0f, 90.0f, 18.0f, 12.0f, alert.accentColor);
        DrawText(uiContext, alert.title, alertBounds.position.x + 16.0f, alertBounds.position.y + 28.0f, alertBounds.size.x - 30.0f, 18.0f, 15.0f, TextPrimaryColor);
        DrawText(uiContext, alert.detail, alertBounds.position.x + 16.0f, alertBounds.position.y + 48.0f, alertBounds.size.x - 30.0f, 14.0f, 10.0f, TextSecondaryColor);
    }
    uiContext.EndVerticalScrollPanel();
}

void DrawControlActionsPanel(UiContext& uiContext, const Rect& bounds, DashboardState& dashboardState)
{
    constexpr std::array<const char*, 3> ActionNames{
        "Acknowledge",
        "Run Check",
        "Stabilize",
    };

    uiContext.Panel(bounds, MakePanelStyle(SurfaceColor));
    DrawSectionTitle(uiContext, MakeRectangle(bounds.position.x + 20.0f, bounds.position.y + 18.0f, 260.0f, 54.0f), "Control Actions", "Click actions to preview state");

    float buttonTop = bounds.position.y + 68.0f;
    for (std::size_t index = 0; index < ActionNames.size(); ++index)
    {
        const bool isSelected = dashboardState.selectedActionIndex == static_cast<int>(index);
        const Rect buttonBounds = MakeRectangle(bounds.position.x + 18.0f, buttonTop, bounds.size.x - 36.0f, 32.0f);
        if (uiContext.Button(std::string{"action-"} + std::to_string(index), ActionNames[index], buttonBounds, MakeInteractivePanelStyle(isSelected, index == 2U ? GreenAccentColor : BlueAccentColor)))
        {
            dashboardState.selectedActionIndex = static_cast<int>(index);
        }
        buttonTop += 40.0f;
    }

    const Rect sliderBounds = MakeRectangle(bounds.position.x + 18.0f, bounds.position.y + bounds.size.y - 42.0f, bounds.size.x - 36.0f, 30.0f);
    const Rect textInputBounds = MakeRectangle(bounds.position.x + 18.0f, bounds.position.y + bounds.size.y - 84.0f, bounds.size.x - 36.0f, 34.0f);
    const bool endpointChanged = uiContext.TextInput("runtime-endpoint-input", textInputBounds, MakeRuntimeTextInputStyle());
    (void)endpointChanged;
    DrawText(uiContext, "Endpoint", textInputBounds.position.x, textInputBounds.position.y - 18.0f, 120.0f, 16.0f, 11.0f, TextSecondaryColor);

    const bool mixChanged =
        uiContext.Slider("runtime-mix-slider", "Mix", sliderBounds, 0.0f, 1.0f, MakeRuntimeControlSliderStyle());
    (void)mixChanged;
}

void DrawAssetStatusPanel(UiContext& uiContext, const Rect& bounds, DashboardState& dashboardState)
{
    constexpr std::array<AssetCard, 4> AssetCards{
        AssetCard{"Atlas", "Nominal", "Primary active node", GreenAccentColor},
        AssetCard{"Beacon", "Nominal", "Healthy standby capacity", GreenAccentColor},
        AssetCard{"Cascade", "Warning", "Load trending upward", AmberAccentColor},
        AssetCard{"Delta", "Offline", "Awaiting recovery check", RedAccentColor},
    };

    uiContext.Panel(bounds, MakePanelStyle(SurfaceColor));
    DrawSectionTitle(uiContext, MakeRectangle(bounds.position.x + 22.0f, bounds.position.y + 18.0f, 300.0f, 54.0f), "Asset Status", "Interactive synthetic asset cards");

    const float cardGap = 14.0f;
    const Rect cardRowBounds =
        MakeRectangle(bounds.position.x + 22.0f, bounds.position.y + 78.0f, bounds.size.x - 44.0f, bounds.size.y - 96.0f);
    for (std::size_t index = 0; index < AssetCards.size(); ++index)
    {
        const AssetCard& asset = AssetCards[index];
        const bool isSelected = dashboardState.selectedAssetIndex == static_cast<int>(index);
        const Rect cardBounds = CalculateColumnRegion(cardRowBounds, index, AssetCards.size(), cardGap);
        if (uiContext.Button(std::string{"asset-"} + std::to_string(index), "", cardBounds, MakeInteractivePanelStyle(isSelected, asset.accentColor)))
        {
            dashboardState.selectedAssetIndex = static_cast<int>(index);
        }

        uiContext.Panel(MakeRectangle(cardBounds.position.x + 16.0f, cardBounds.position.y + 14.0f, 38.0f, 4.0f),
                        RectangleStyle{
                            .fillColor = asset.accentColor,
                            .cornerRadius = 3.0f,
                        });
        DrawText(uiContext, asset.name, cardBounds.position.x + 16.0f, cardBounds.position.y + 30.0f, cardBounds.size.x - 32.0f, 24.0f, 18.0f, TextPrimaryColor);
        DrawText(uiContext, asset.status, cardBounds.position.x + 16.0f, cardBounds.position.y + 56.0f, cardBounds.size.x - 32.0f, 18.0f, 13.0f, asset.accentColor);
        DrawText(uiContext, asset.detail, cardBounds.position.x + 16.0f, cardBounds.position.y + 76.0f, cardBounds.size.x - 32.0f, 16.0f, 10.0f, TextSecondaryColor);
    }
}

ControlRoomFrameRegions BuildControlRoomUi(UiContext& uiContext, const Rect& rootBounds, DashboardState& dashboardState)
{
    const float margin = ClampLayoutValue(PercentageWidth(rootBounds, 0.01875f), 24.0f, 36.0f);
    const float gap = ClampLayoutValue(PercentageWidth(rootBounds, 0.014f), 18.0f, 24.0f);
    const float headerHeight = ClampLayoutValue(PercentageHeight(rootBounds, 0.122f), 88.0f, 112.0f);

    uiContext.DrawFilledRectangle(rootBounds, BackgroundColor);

    const Rect dashboardBounds = InsetRectangle(rootBounds, margin);
    const LayoutSplit headerAndContent = SplitRectangleVerticallyFixedFlexible(dashboardBounds, headerHeight, gap);
    DrawHeader(uiContext, headerAndContent.first);

    const float navigationWidth = ClampLayoutValue(PercentageWidth(rootBounds, 0.14f), 178.0f, 220.0f);
    const LayoutSplit navigationAndMain =
        SplitRectangleHorizontallyFixedFlexible(headerAndContent.second, navigationWidth, gap);
    DrawNavigationRail(uiContext, navigationAndMain.first, dashboardState);

    const float rightColumnWidth = ClampLayoutValue(PercentageWidth(navigationAndMain.second, 0.30f), 330.0f, 420.0f);
    const LayoutSplit centerAndRight =
        SplitRectangleHorizontallyFixedFlexible(navigationAndMain.second, rightColumnWidth, gap, FixedRegion::Second);

    const float bottomPanelHeight = ClampLayoutValue(PercentageHeight(centerAndRight.first, 0.32f), 196.0f, 260.0f);
    const LayoutSplit healthAndAssets =
        SplitRectangleVerticallyFixedFlexible(centerAndRight.first, bottomPanelHeight, gap, FixedRegion::Second);
    const ControlRoomFrameRegions frameRegions = DrawSystemHealthPanel(uiContext, healthAndAssets.first);
    DrawAssetStatusPanel(uiContext, healthAndAssets.second, dashboardState);

    const LayoutSplit alertsAndActions = SplitRectangleVerticallyByPercentage(centerAndRight.second, 0.58f, gap);
    DrawAlertQueuePanel(uiContext, alertsAndActions.first);
    DrawControlActionsPanel(uiContext, alertsAndActions.second, dashboardState);

    return frameRegions;
}

void ConfigureControlRoomStyle(UiContext& uiContext)
{
    uiContext.SetStyle(Style{
        .windowBackground = BackgroundColor,
        .panelBackground = SurfaceColor,
        .panelBorder = BorderColor,
        .accent = BlueAccentColor,
        .textPrimary = TextPrimaryColor,
        .textSecondary = TextSecondaryColor,
        .panelCornerRadius = 18.0f,
        .panelBorderThickness = 1.0f,
    });
}

[[nodiscard]] Layout MakeSandboxLayout(float width, float height)
{
    return Layout{
        .bounds =
            Rect{
                .position = Vec2{0.0f, 0.0f},
                .size = Vec2{width, height},
            },
        .padding = 16.0f,
    };
}

ControlRoomFrameRegions BuildControlRoomFrame(UiContext& uiContext, const Layout& layout, DashboardState& dashboardState,
                                              const InputState& inputState = {})
{
    uiContext.BeginFrame(layout, inputState);
    return BuildControlRoomUi(uiContext, layout.bounds, dashboardState);
}

void SubmitControlRoomFrame(Fast2DRenderer& renderer,
                            const RenderCommandList& renderCommands,
                            const ControlRoomFrameRegions& frameRegions)
{
    VisualizationCommandList visualizationCommands =
        BuildResponseTraceVisualizationCommands(frameRegions.visualizationPlotBounds);
    RenderCommandList overlayCommands = BuildResponseTraceOverlayCommands(frameRegions.visualizationPlotBounds);

    renderer.Submit(renderCommands);
    renderer.SubmitVisualization(visualizationCommands);
    renderer.Submit(overlayCommands);
}

void SubmitControlRoomFrame(WebGpuRenderer& renderer,
                            const RenderCommandList& renderCommands,
                            const ControlRoomFrameRegions& frameRegions)
{
    VisualizationCommandList visualizationCommands =
        BuildResponseTraceVisualizationCommands(frameRegions.visualizationPlotBounds);
    RenderCommandList overlayCommands = BuildResponseTraceOverlayCommands(frameRegions.visualizationPlotBounds);

    renderer.Submit(renderCommands);
    renderer.SubmitVisualization(visualizationCommands);
    renderer.Submit(overlayCommands);
}

int RunFast2DDiagnosticSandbox(const SandboxWindowSize& initialWindowSize)
{
    std::cout << "Running Fast2D diagnostic renderer path.\n";

    Fast2DRenderer renderer{static_cast<std::size_t>(initialWindowSize.width),
                            static_cast<std::size_t>(initialWindowSize.height)};
    UiContext uiContext;
    ConfigureControlRoomStyle(uiContext);
    DashboardState dashboardState{};

    renderer.BeginFrame();

    const Layout layout =
        MakeSandboxLayout(static_cast<float>(initialWindowSize.width), static_cast<float>(initialWindowSize.height));
    const ControlRoomFrameRegions frameRegions = BuildControlRoomFrame(uiContext, layout, dashboardState);

    const auto& renderCommands = uiContext.EndFrame();
    SubmitControlRoomFrame(renderer, renderCommands, frameRegions);
    renderer.EndFrame();

    std::cout << "Fast2D diagnostic frame complete: " << renderer.CompletedFrameCommandCount()
              << " commands, " << renderer.SubmittedVisualizationCommandCount()
              << " submitted visualization commands, " << renderer.CompletedFrameVisualizationCommandCount()
              << " completed visualization commands, " << renderer.PreparedFillOperationCount() << " fill operations, "
              << renderer.DeferredTextCommandCount() << " deferred text commands, raster target "
              << renderer.RasterTargetWidth() << "x" << renderer.RasterTargetHeight() << ".\n";

    return 0;
}

int RunFast2DSandbox(const SandboxWindowSize& initialWindowSize)
{
    SdlWindow window{"Greenfield Sandbox - Fast2D",
                     initialWindowSize.width,
                     initialWindowSize.height,
                     SdlWindowVisibility::Hidden};
    Fast2DRenderer renderer{static_cast<std::size_t>(window.GetWidth()),
                            static_cast<std::size_t>(window.GetHeight())};
    SdlRasterPresenter presenter{window};
    UiContext uiContext;
    ConfigureControlRoomStyle(uiContext);
    DashboardState dashboardState{};

    while (!window.ShouldClose())
    {
        window.PollEvents();
        if (window.ShouldClose())
        {
            break;
        }

        if (window.GetWidth() <= 0 || window.GetHeight() <= 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            continue;
        }

        const auto rasterWidth = static_cast<std::size_t>(window.GetWidth());
        const auto rasterHeight = static_cast<std::size_t>(window.GetHeight());
        if (renderer.RasterTargetWidth() != rasterWidth || renderer.RasterTargetHeight() != rasterHeight)
        {
            renderer.ResizeRasterTarget(rasterWidth, rasterHeight);
        }

        renderer.BeginFrame();

        const Layout layout =
            MakeSandboxLayout(static_cast<float>(window.GetWidth()), static_cast<float>(window.GetHeight()));
        const ControlRoomFrameRegions frameRegions =
            BuildControlRoomFrame(uiContext, layout, dashboardState, window.GetInputState());

        const auto& renderCommands = uiContext.EndFrame();
        SubmitControlRoomFrame(renderer, renderCommands, frameRegions);
        renderer.EndFrame();
        if (!presenter.PresentRaster(renderer.RasterTargetWidth(), renderer.RasterTargetHeight(), renderer.RasterPixels()))
        {
            continue;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    return 0;
}

int RunWebGpuSandbox(const SandboxWindowSize& initialWindowSize)
{
    SdlWindow window{"Greenfield Sandbox", initialWindowSize.width, initialWindowSize.height};
    {
        SdlStartupPresenter startupPresenter{window};
        startupPresenter.DrawFrame();

        window.PollEvents();
        if (window.ShouldClose())
        {
            return 0;
        }
    }

    WebGpuContext webGpuContext{window};
    const std::string defaultFontPath = FindDefaultFontPath();
    if (defaultFontPath.empty())
    {
        std::cerr << "No default font found. Text commands will be recorded, but text will not render.\n";
    }
    else
    {
        std::cout << "Using sandbox font: " << defaultFontPath << '\n';
    }

    WebGpuRenderer renderer{webGpuContext, defaultFontPath};
    UiContext uiContext;
    ConfigureControlRoomStyle(uiContext);
    DashboardState dashboardState{};

    while (!window.ShouldClose())
    {
        window.PollEvents();
        if (window.ShouldClose())
        {
            break;
        }

        renderer.BeginFrame();

        const Layout layout =
            MakeSandboxLayout(static_cast<float>(window.GetWidth()), static_cast<float>(window.GetHeight()));
        const ControlRoomFrameRegions frameRegions =
            BuildControlRoomFrame(uiContext, layout, dashboardState, window.GetInputState());

        const auto& renderCommands = uiContext.EndFrame();
        SubmitControlRoomFrame(renderer, renderCommands, frameRegions);
        renderer.EndFrame();

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    return 0;
}

} // namespace

int main(int argumentCount, char* argumentValues[])
{
    try
    {
        std::cout << "Starting Greenfield sandbox\n";

        const SandboxOptions sandboxOptions = GetSandboxOptions(argumentCount, argumentValues);
        if (!sandboxOptions.invalidRendererName.empty())
        {
            std::cerr << "Unknown renderer backend '" << sandboxOptions.invalidRendererName
                      << "'. Expected --renderer=webgpu or --renderer=fast2d.\n";
            return 1;
        }

        if (sandboxOptions.rendererBackendKind == RendererBackendKind::Fast2D)
        {
            if (sandboxOptions.runHeadlessFast2D)
            {
                return RunFast2DDiagnosticSandbox(sandboxOptions.windowSize);
            }

            return RunFast2DSandbox(sandboxOptions.windowSize);
        }

        return RunWebGpuSandbox(sandboxOptions.windowSize);
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Greenfield sandbox failed: " << exception.what() << '\n';
    }
    catch (...)
    {
        std::cerr << "Greenfield sandbox failed with an unknown error.\n";
    }

    return 1;
}
