#include <array>
#include <chrono>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

#include "engine/core/Color.h"
#include "engine/core/Rect.h"
#include "engine/core/Vec2.h"
#include "engine/platform/SdlStartupPresenter.h"
#include "engine/platform/SdlWindow.h"
#include "engine/render/webgpu/WebGpuContext.h"
#include "engine/render/webgpu/WebGpuRenderer.h"
#include "engine/ui/Layout.h"
#include "engine/ui/Style.h"
#include "engine/ui/UiContext.h"

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

Rect MakeRectangle(float x, float y, float width, float height)
{
    return Rect{
        .position = Vec2{x, y},
        .size = Vec2{width, height},
    };
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

    float buttonTop = bounds.position.y + 112.0f;
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

void DrawSystemHealthPanel(UiContext& uiContext, const Rect& bounds)
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
    const float cardWidth = (bounds.size.x - 44.0f - cardGap * 3.0f) / 4.0f;
    for (std::size_t index = 0; index < MetricCards.size(); ++index)
    {
        DrawMetricCard(uiContext,
                       MakeRectangle(bounds.position.x + 22.0f + (cardWidth + cardGap) * static_cast<float>(index),
                                     cardTop,
                                     cardWidth,
                                     132.0f),
                       MetricCards[index]);
    }

    const Rect summaryPanel = MakeRectangle(bounds.position.x + 22.0f, bounds.position.y + 248.0f, bounds.size.x - 44.0f, bounds.size.y - 270.0f);
    uiContext.Panel(summaryPanel, MakePanelStyle(SurfaceSoftColor));
    DrawText(uiContext, "Operational Posture", summaryPanel.position.x + 18.0f, summaryPanel.position.y + 16.0f, 240.0f, 24.0f, 18.0f, TextPrimaryColor);
    DrawText(uiContext, "Nominal service envelope with two warning observations pending review.", summaryPanel.position.x + 18.0f, summaryPanel.position.y + 48.0f, summaryPanel.size.x - 36.0f, 24.0f, 15.0f, TextSecondaryColor);
    uiContext.Panel(MakeRectangle(summaryPanel.position.x + 18.0f, summaryPanel.position.y + 88.0f, summaryPanel.size.x - 36.0f, 18.0f),
                    RectangleStyle{
                        .fillColor = Color{0.050f, 0.065f, 0.085f, 0.92f},
                        .cornerRadius = 9.0f,
                        .borderColor = Color{0.250f, 0.310f, 0.390f, 0.48f},
                        .borderThickness = 1.0f,
                    });
    uiContext.Panel(MakeRectangle(summaryPanel.position.x + 18.0f, summaryPanel.position.y + 88.0f, (summaryPanel.size.x - 36.0f) * 0.76f, 18.0f),
                    RectangleStyle{
                        .fillColor = GreenAccentColor,
                        .cornerRadius = 9.0f,
                    });
}

void DrawAlertQueuePanel(UiContext& uiContext, const Rect& bounds)
{
    constexpr std::array<AlertItem, 3> Alerts{
        AlertItem{"Warning", "Load variance", "Node group Delta above baseline", AmberAccentColor},
        AlertItem{"Warning", "Delayed check", "Asset Echo pending confirmation", AmberAccentColor},
        AlertItem{"Offline", "Standby node", "Node Hotel unavailable", RedAccentColor},
    };

    uiContext.Panel(bounds, MakePanelStyle(SurfaceColor));
    DrawSectionTitle(uiContext, MakeRectangle(bounds.position.x + 20.0f, bounds.position.y + 18.0f, 260.0f, 54.0f), "Alert Queue", "Prioritized generic observations");

    float itemTop = bounds.position.y + 86.0f;
    for (const AlertItem& alert : Alerts)
    {
        const Rect alertBounds = MakeRectangle(bounds.position.x + 18.0f, itemTop, bounds.size.x - 36.0f, 76.0f);
        uiContext.Panel(alertBounds, MakePanelStyle(SurfaceRaisedColor));
        DrawAccentRule(uiContext, alertBounds, alert.accentColor);
        DrawText(uiContext, alert.severity, alertBounds.position.x + 16.0f, alertBounds.position.y + 12.0f, 90.0f, 20.0f, 13.0f, alert.accentColor);
        DrawText(uiContext, alert.title, alertBounds.position.x + 16.0f, alertBounds.position.y + 33.0f, alertBounds.size.x - 30.0f, 22.0f, 17.0f, TextPrimaryColor);
        DrawText(uiContext, alert.detail, alertBounds.position.x + 16.0f, alertBounds.position.y + 56.0f, alertBounds.size.x - 30.0f, 18.0f, 12.0f, TextSecondaryColor);
        itemTop += 90.0f;
    }
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

    float buttonTop = bounds.position.y + 74.0f;
    for (std::size_t index = 0; index < ActionNames.size(); ++index)
    {
        const bool isSelected = dashboardState.selectedActionIndex == static_cast<int>(index);
        const Rect buttonBounds = MakeRectangle(bounds.position.x + 18.0f, buttonTop, bounds.size.x - 36.0f, 38.0f);
        if (uiContext.Button(std::string{"action-"} + std::to_string(index), ActionNames[index], buttonBounds, MakeInteractivePanelStyle(isSelected, index == 2U ? GreenAccentColor : BlueAccentColor)))
        {
            dashboardState.selectedActionIndex = static_cast<int>(index);
        }
        buttonTop += 48.0f;
    }
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
    const float cardWidth = (bounds.size.x - 44.0f - cardGap * 3.0f) / 4.0f;
    for (std::size_t index = 0; index < AssetCards.size(); ++index)
    {
        const AssetCard& asset = AssetCards[index];
        const bool isSelected = dashboardState.selectedAssetIndex == static_cast<int>(index);
        const Rect cardBounds = MakeRectangle(bounds.position.x + 22.0f + (cardWidth + cardGap) * static_cast<float>(index),
                                              bounds.position.y + 78.0f,
                                              cardWidth,
                                              bounds.size.y - 96.0f);
        if (uiContext.Button(std::string{"asset-"} + std::to_string(index), "", cardBounds, MakeInteractivePanelStyle(isSelected, asset.accentColor)))
        {
            dashboardState.selectedAssetIndex = static_cast<int>(index);
        }

        uiContext.Panel(MakeRectangle(cardBounds.position.x + 16.0f, cardBounds.position.y + 14.0f, 38.0f, 4.0f),
                        RectangleStyle{
                            .fillColor = asset.accentColor,
                            .cornerRadius = 3.0f,
                        });
        DrawText(uiContext, asset.name, cardBounds.position.x + 16.0f, cardBounds.position.y + 32.0f, cardBounds.size.x - 32.0f, 26.0f, 20.0f, TextPrimaryColor);
        DrawText(uiContext, asset.status, cardBounds.position.x + 16.0f, cardBounds.position.y + 60.0f, cardBounds.size.x - 32.0f, 22.0f, 15.0f, asset.accentColor);
        DrawText(uiContext, asset.detail, cardBounds.position.x + 16.0f, cardBounds.position.y + 84.0f, cardBounds.size.x - 32.0f, 24.0f, 12.0f, TextSecondaryColor);
    }
}

void BuildControlRoomUi(UiContext& uiContext, int windowWidth, int windowHeight, DashboardState& dashboardState)
{
    const float width = static_cast<float>(windowWidth);
    const float height = static_cast<float>(windowHeight);
    const float margin = 24.0f;
    const float headerHeight = 88.0f;
    const float navigationWidth = 178.0f;
    const float gap = 18.0f;

    uiContext.DrawFilledRectangle(MakeRectangle(0.0f, 0.0f, width, height), BackgroundColor);

    const Rect headerBounds = MakeRectangle(margin, margin, width - margin * 2.0f, headerHeight);
    DrawHeader(uiContext, headerBounds);

    const float contentTop = margin + headerHeight + gap;
    const float contentHeight = height - contentTop - margin;
    const Rect navigationBounds = MakeRectangle(margin, contentTop, navigationWidth, contentHeight);
    DrawNavigationRail(uiContext, navigationBounds, dashboardState);

    const float mainLeft = margin + navigationWidth + gap;
    const float mainWidth = width - mainLeft - margin;
    const float rightColumnWidth = 330.0f;
    const float centerWidth = mainWidth - rightColumnWidth - gap;
    const float bottomPanelHeight = 196.0f;

    DrawSystemHealthPanel(uiContext, MakeRectangle(mainLeft, contentTop, centerWidth, contentHeight - bottomPanelHeight - gap));
    DrawAssetStatusPanel(uiContext, MakeRectangle(mainLeft, height - margin - bottomPanelHeight, centerWidth, bottomPanelHeight), dashboardState);
    DrawAlertQueuePanel(uiContext, MakeRectangle(mainLeft + centerWidth + gap, contentTop, rightColumnWidth, contentHeight * 0.58f));
    DrawControlActionsPanel(uiContext,
                            MakeRectangle(mainLeft + centerWidth + gap, contentTop + contentHeight * 0.58f + gap, rightColumnWidth, contentHeight * 0.42f - gap),
                            dashboardState);
}

} // namespace

int main()
{
    try
    {
        std::cout << "Starting Greenfield sandbox\n";

        SdlWindow window{"Greenfield Sandbox", 1280, 720};
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
        DashboardState dashboardState{};

        while (!window.ShouldClose())
        {
            window.PollEvents();
            if (window.ShouldClose())
            {
                break;
            }

            renderer.BeginFrame();

            const Layout layout{
                .bounds =
                    Rect{
                        .position = Vec2{0.0f, 0.0f},
                        .size = Vec2{static_cast<float>(window.GetWidth()), static_cast<float>(window.GetHeight())},
                    },
                .padding = 16.0f,
            };

            uiContext.BeginFrame(layout, window.GetInputState());
            BuildControlRoomUi(uiContext, window.GetWidth(), window.GetHeight(), dashboardState);

            const auto& renderCommands = uiContext.EndFrame();
            renderer.Submit(renderCommands);
            renderer.EndFrame();

            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }

        return 0;
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
