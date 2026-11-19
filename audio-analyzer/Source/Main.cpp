#include <juce_gui_extra/juce_gui_extra.h>
#include "AnalyzerComponent.h"

// Songsmith Audio Analyzer: a standalone application (REC-02). It does not need a recording program.
//  - Double-click it, then drop a file on the window or choose one.
//  - A file given on the command line, dropped on the app's icon or opened with it from Finder is analysed straight away.
namespace
{
    class AnalyzerWindow : public juce::DocumentWindow
    {
    public:
        AnalyzerWindow()
            : DocumentWindow (ANALYZER_PRODUCT_NAME, Songsmith::Analyzer::Colours::accent.withBrightness (0.1f), DocumentWindow::allButtons)
        {
            using namespace Songsmith::Analyzer;
            setUsingNativeTitleBar (true);
            setContentOwned (new AnalyzerRoot(), true);
            setResizable (true, true);
            // The window scales as a whole, between 75 % and 200 %, keeping its shape.
            const int minWidth = Layout::designWidth * 3 / 4, minHeight = Layout::designHeight * 3 / 4;
            setResizeLimits (minWidth, minHeight, Layout::designWidth * 2, Layout::designHeight * 2);
            getConstrainer()->setFixedAspectRatio ((double) Layout::designWidth / (double) Layout::designHeight);
            centreWithSize (Layout::designWidth, Layout::designHeight + getTitleBarHeight());
            setVisible (true);
        }

        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }

        Songsmith::Analyzer::AnalyzerComponent& analyzer()
        {
            return static_cast<Songsmith::Analyzer::AnalyzerRoot*> (getContentComponent())->content;
        }
    };
}

class AudioAnalyzerApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return ANALYZER_PRODUCT_NAME; }
    const juce::String getApplicationVersion() override { return ANALYZER_VERSION; }
    bool moreThanOneInstanceAllowed() override          { return false; }

    void initialise (const juce::String& commandLine) override
    {
        window = std::make_unique<AnalyzerWindow>();
        openFromCommandLine (commandLine);
    }

    void shutdown() override { window.reset(); }

    void anotherInstanceStarted (const juce::String& commandLine) override
    {
        if (window != nullptr)
        {
            window->toFront (true);
            openFromCommandLine (commandLine);
        }
    }

    void systemRequestedQuit() override { quit(); }

private:
    void openFromCommandLine (const juce::String& commandLine)
    {
        const auto args = juce::StringArray::fromTokens (commandLine, true);
        for (auto arg : args)
        {
            arg = arg.unquoted();
            if (arg.isEmpty() || arg.startsWith ("-"))
                continue;
            window->analyzer().analyse (juce::File::getCurrentWorkingDirectory().getChildFile (arg));
            return;
        }
    }

    std::unique_ptr<AnalyzerWindow> window;
};

START_JUCE_APPLICATION (AudioAnalyzerApplication)
