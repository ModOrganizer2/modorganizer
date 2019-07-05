//TL First Steps#0
function getTutorialSteps()
{
  return [
    function() {
        tutorial.text = qsTr("Welcome to the ModOrganizer Tutorial! This will guide you through the most common features of MO."
                           + "\nPlease go along with the tutorial because some things can't be demonstrated if you skip something.")
        waitForClick()
    },

    function() {
        tutorial.text = qsTr("Before we continue with the step-by-step tutorial, I'd like to tell you about the other ways you can receive help on ModOrganizer.")
        waitForClick()
    },

    function() {
        tutorial.text = qsTr("The highlighted button provides hints on solving potential problems MO recognized automatically.")
        if (tutorialControl.waitForAction("actionNotifications")) {
            tutorial.text += qsTr("\nThere IS a notification now but you may want to hold off on clearing it until after completing the tutorial.")
            highlightAction("actionNotifications", true)
        } else {
            highlightAction("actionNotifications", false)
            waitForClick()
        }
    },

    function() {
        console.log("next")
        tutorial.text = qsTr("This button provides multiple sources of information and further tutorials.")
        if (tutorialControl.waitForButton("actionHelp")) {
          highlightItem("actionHelp", true)
        } else {
          console.error("help button broken")
          waitForClick()
        }
    },

    function() {
        tutorial.text = qsTr("Finally there are tooltips on almost every part of Mod Organizer. If there is a control "
                           + "in MO you don't understand, please try hovering over it to get a short description or "
                           + "use \"Help on UI\" from the help menu to get a longer explanation")
        waitForClick()
    },

    function() {
        tutorial.text = qsTr("This list displays all mods installed through MO. It also displays installed DLCs and some mods "
                           + "installed outside MO but you have limited control over those in MO.")
        highlightItem("modList", false)
        waitForClick()
    },

    function() {
        tutorial.text = qsTr("Before we start installing mods, let's have a quick look at the settings.")
        manager.activateTutorial("SettingsDialog", "tutorial_firststeps_settings.js")
        if (tutorialControl.waitForAction("actionSettings")) {
            highlightAction("actionSettings", true)
        } else {
          console.error("settings action broken")
          waitForClick()
        }
    },

    function() {
        unhighlight()
        tutorial.text = qsTr("Now it's time to install a few mods!"
                            + "Please go along with this because we need a few mods installed to demonstrate other features")
        waitForClick()
    },

    function() {
        tutorial.text = qsTr("There are a few ways to get mods into ModOrganizer. "
                           + "If you associated MO with NXM links in the settings you can now use your regular browser to send downloads from Nexus to MO. "
                           + "Click on \"Nexus\" to open nexus, find a mod and click the green download buttons on Nexus saying \"Download with Manager\".")
        if (tutorialControl.waitForAction("actionNexus")) {
            highlightAction("actionNexus", true)
        } else {
          console.error("browser action broken")
          waitForClick()
        }
    },

    function() {
        tutorial.text = qsTr("You can also install mods from disk using the \"Install Mod\" button.")
        highlightAction("actionInstallMod", false)
        waitForClick()
    },

    function() {
        tutorial.text = qsTr("Downloads will appear on the \"Downloads\"-tab here. You have to download and install at least one mod to proceed.")
        organizer.modInstalled.connect(nextStep)
        highlightItem("tabWidget", true)
    },

    function() {
        unhighlight()
        organizer.modInstalled.disconnect(nextStep)
        tutorial.text = qsTr("Great, you just installed your first mod. Please note that the installation procedure may differ based on how a mod was packaged.")
        waitForClick()
    },

    function() {
        unhighlight()
        tutorial.text = qsTr("Now you know all about downloading and installing mods but they are not enabled yet...")
        waitForClick()
    },

    function() {
        tutorial.text = qsTr("Install a few more mods if you want, then enable mods by checking them in the left pane. "
                             + "Mods that aren't enabled have no effect on the game whatsoever. ")
        highlightItem("modList", true)
        modList.tutorialModlistUpdate.connect(nextStep)
    },

    function() {
        modList.tutorialModlistUpdate.disconnect(nextStep)
        unhighlight()
        tutorial.text = qsTr("For some mods, enabling it on the left pane is all you have to do...")
        waitForClick()
    },

    function() {
        tutorial.text = qsTr("...but most contain plugins. These are plugins for the game and are required "
                            +"to add stuff to the game (new weapons, armors, quests, areas, ...). "
                            +"Please open the \"Plugins\"-tab to get a list of plugins.")
        if (tutorialControl.waitForTabOpen("tabWidget", "espTab")) {
            highlightItem("tabWidget", true)
        } else {
            waitForClick()
        }
    },

    function() {
        tutorial.text = qsTr("You will notice some plugins are grayed out. These are part of the main game and can't be "
                            +"disabled.")
        waitForClick()
    },

    function() {
        tutorial.text = qsTr("A single mod may contain zero, one or multiple esps. Some or all may be optional. "
                              + "If in doubt, please consult the documentation of the individual mod. "
                              + "To do so, right-click the mod and select \"Information\".")
        highlightItem("modList", true)
        manager.activateTutorial("ModInfoDialog", "tutorial_firststeps_modinfo.js")
        applicationWindow.modInfoDisplayed.connect(nextStep)
    },

    function() {
        tutorial.text = qsTr("Now you know how to download, install and enable mods.\n"
                           + "It's important you always start the game from inside MO, otherwise "
                           + "the mods you installed here won't work.")
        highlightItem("startGroup", false)
        waitForClick()
    },

    function() {
        tutorial.text = qsTr("This combobox lets you choose <i>what</i> to start. This way you can start the game, Launcher, "
                              + "Script Extender, the Creation Kit or other tools. If you miss a tool you can also configure this list "
                              + "but that is an advanced topic.")
        highlightItem("executablesListBox", false)
        waitForClick()
    },

    function() {
        tutorial.text = qsTr("This completes the basic tutorial. As homework go play a bit! After you "
                           + "have installed more mods you may want to read the tutorial on conflict resolution.")
        highlightItem("startButton", false)
        waitForClick()
    }
  ]
}
