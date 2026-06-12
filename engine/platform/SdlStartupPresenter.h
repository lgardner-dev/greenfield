#pragma once

namespace greenfield
{

class SdlWindow;

class SdlStartupPresenter
{
public:
    explicit SdlStartupPresenter(SdlWindow& window) noexcept;
    ~SdlStartupPresenter();

    SdlStartupPresenter(const SdlStartupPresenter&) = delete;
    SdlStartupPresenter& operator=(const SdlStartupPresenter&) = delete;
    SdlStartupPresenter(SdlStartupPresenter&&) = delete;
    SdlStartupPresenter& operator=(SdlStartupPresenter&&) = delete;

    void DrawFrame();

private:
    void ReleaseWindowSurface() noexcept;

    SdlWindow* _window{nullptr};
    int _frameIndex{0};
};

} // namespace greenfield
