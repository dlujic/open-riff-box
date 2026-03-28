#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_plugin_client/juce_audio_plugin_client.h>

#if JUCE_DEBUG
#include "tools/OfflineProcessor.h"
#endif

#if JucePlugin_Build_Standalone
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

#include "ui/Theme.h"
//==============================================================================
// Custom title bar button — larger and styled for amp aesthetic
class TitleBarButton : public juce::Button
{
public:
    TitleBarButton(const juce::String& name, int type)
        : juce::Button(name), buttonType(type) {}

    void paintButton(juce::Graphics& g, bool isOver, bool isDown) override
    {
        auto area = getLocalBounds().toFloat().reduced(2.0f);

        if (buttonType == juce::DocumentWindow::closeButton && isOver)
        {
            g.setColour(juce::Colour(0xffcc3333).withAlpha(isDown ? 0.6f : 0.35f));
            g.fillRoundedRectangle(area, 4.0f);
        }
        else if (isOver)
        {
            g.setColour(juce::Colours::white.withAlpha(isDown ? 0.15f : 0.08f));
            g.fillRoundedRectangle(area, 4.0f);
        }

        auto colour = isOver ? Theme::Colours::textPrimary
                             : Theme::Colours::textSecondary;
        g.setColour(colour);

        auto centre = area.getCentre();

        if (buttonType == juce::DocumentWindow::closeButton)
        {
            float s = 5.0f;
            g.drawLine(centre.x - s, centre.y - s, centre.x + s, centre.y + s, 1.8f);
            g.drawLine(centre.x + s, centre.y - s, centre.x - s, centre.y + s, 1.8f);
        }
        else if (buttonType == juce::DocumentWindow::minimiseButton)
        {
            float s = 5.0f;
            g.drawLine(centre.x - s, centre.y + 2.0f, centre.x + s, centre.y + 2.0f, 1.8f);
        }
    }

private:
    int buttonType;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TitleBarButton)
};

//==============================================================================
// Window LookAndFeel — custom title bar matching amp aesthetic
class WindowLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawDocumentWindowTitleBar(juce::DocumentWindow&, juce::Graphics& g,
                                   int w, int h, int /*titleSpaceX*/, int titleSpaceW,
                                   const juce::Image*, bool) override
    {
        auto bounds = juce::Rectangle<int>(0, 0, w, h);

        g.setGradientFill(juce::ColourGradient(
            Theme::Colours::topBarTop, 0.0f, 0.0f,
            Theme::Colours::topBarBottom, 0.0f, static_cast<float>(h),
            false));
        g.fillRect(bounds);

        // Brushed horizontal lines
        for (int y = 0; y < h; y += 2)
        {
            float alpha = (y % 4 == 0) ? 0.06f : 0.03f;
            g.setColour(juce::Colours::white.withAlpha(alpha));
            g.drawHorizontalLine(y, 0.0f, static_cast<float>(w));
        }

        Theme::paintNoise(g, bounds, 0.05f);

        // Highlight at top edge
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.drawHorizontalLine(0, 0.0f, static_cast<float>(w));

        // Shadow at bottom edge
        g.setColour(juce::Colours::black.withAlpha(0.4f));
        g.drawHorizontalLine(h - 1, 0.0f, static_cast<float>(w));

        // Title text — Metal Mania font, gold
        g.setColour(Theme::Colours::goldText);
        g.setFont(Theme::Fonts::appTitle());
        g.drawText("Open Riff Box", 12, 0, titleSpaceW, h,
                   juce::Justification::centredLeft);
    }

    juce::Button* createDocumentWindowButton(int buttonType) override
    {
        auto name = (buttonType == juce::DocumentWindow::closeButton) ? "close" : "minimise";
        return new TitleBarButton(name, buttonType);
    }

    void positionDocumentWindowButtons(juce::DocumentWindow&,
                                       int titleBarX, int titleBarY,
                                       int titleBarW, int titleBarH,
                                       juce::Button* minimiseButton,
                                       juce::Button* maximiseButton,
                                       juce::Button* closeButton,
                                       bool) override
    {
        const int btnSize = 32;
        const int margin = 8;
        int x = titleBarX + titleBarW;

        if (closeButton != nullptr)
        {
            x -= btnSize + margin;
            closeButton->setBounds(x, titleBarY + (titleBarH - btnSize) / 2,
                                   btnSize, btnSize);
        }
        if (minimiseButton != nullptr)
        {
            x -= btnSize + 4;
            minimiseButton->setBounds(x, titleBarY + (titleBarH - btnSize) / 2,
                                      btnSize, btnSize);
        }
        if (maximiseButton != nullptr)
            maximiseButton->setBounds(0, 0, 0, 0);
    }
};

//==============================================================================
// Custom standalone window — removes native title bar and JUCE Options menu,
// replaces with our brushed-metal title bar + styled close/minimise buttons.
class OpenRiffBoxWindow : public juce::StandaloneFilterWindow
{
public:
    OpenRiffBoxWindow(juce::PropertySet* settings)
        : StandaloneFilterWindow(
              juce::JUCEApplication::getInstance()->getApplicationName(),
              juce::Colours::darkgrey,
              settings,
              true)
    {
        // Apply our custom window LookAndFeel
        setLookAndFeel(&windowLF);

        // Replace native title bar with our styled one
        setUsingNativeTitleBar(false);
        setTitleBarHeight(Theme::Dims::topBarHeight);

        // Recreate buttons with our LookAndFeel (close + minimise only)
        setTitleBarButtonsRequired(
            juce::DocumentWindow::minimiseButton | juce::DocumentWindow::closeButton,
            false);

        // Hide the JUCE "Options" TextButton.
        for (int i = getNumChildComponents(); --i >= 0;)
        {
            if (auto* btn = dynamic_cast<juce::TextButton*>(getChildComponent(i)))
            {
                btn->setVisible(false);
                break;
            }
        }

        if (auto* holder = getPluginHolder())
            holder->getMuteInputValue().setValue(false);
    }

    ~OpenRiffBoxWindow() override
    {
        setLookAndFeel(nullptr);
    }

private:
    WindowLookAndFeel windowLF;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenRiffBoxWindow)
};

//==============================================================================
class OpenRiffBoxApplication : public juce::JUCEApplication
{
public:
    OpenRiffBoxApplication() = default;

    const juce::String getApplicationName()    override { return "OpenRiffBox"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed()           override { return false; }

    void initialise(const juce::String& commandLine) override
    {
#if JUCE_DEBUG
        // Offline file processor: --process-file input.flac output.flac [options]
        // Processes audio through AmpSimGold/AmpSimPlatinum without launching the GUI.
        if (runOfflineProcessor(getCommandLineParameterArray()))
        {
            quit();
            return;
        }
#endif

        juce::PropertiesFile::Options options;
        options.applicationName     = "OpenRiffBox";
        options.filenameSuffix      = ".settings";
        options.osxLibrarySubFolder = "Application Support";
        options.folderName          = "OpenRiffBox";

        // Portable mode: store settings next to the exe if the directory is writable.
        // Falls back to %APPDATA%/OpenRiffBox/ if not (e.g. installed in Program Files).
        auto exeDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                          .getParentDirectory();
        auto portableFile = exeDir.getChildFile("OpenRiffBox.settings");

        if (portableFile.existsAsFile() || exeDir.hasWriteAccess())
        {
            // Use portable settings file directly
            portableSettings = std::make_unique<juce::PropertiesFile>(portableFile, options);
        }
        else
        {
            // Fall back to standard OS location
            appProperties.setStorageParameters(options);
        }

        auto* settings = portableSettings ? portableSettings.get()
                                          : appProperties.getUserSettings();

        mainWindow = std::make_unique<OpenRiffBoxWindow>(settings);
        mainWindow->setVisible(true);
    }

    void shutdown() override
    {
        // Save settings before destroying the window (which triggers ASIO teardown)
        if (portableSettings)
            portableSettings->saveIfNeeded();
        else
            appProperties.saveIfNeeded();

        // Watchdog: if ASIO driver teardown hangs, force-exit after 3 seconds.
        // ExitProcess() releases the single-instance mutex so relaunch works.
        std::thread([] {
            juce::Thread::sleep(3000);
            juce::Process::terminate();
        }).detach();

        mainWindow = nullptr;
    }

    void systemRequestedQuit() override
    {
        quit();
    }

private:
    juce::ApplicationProperties appProperties;
    std::unique_ptr<juce::PropertiesFile> portableSettings;
    std::unique_ptr<OpenRiffBoxWindow> mainWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenRiffBoxApplication)
};

START_JUCE_APPLICATION(OpenRiffBoxApplication)
#endif
