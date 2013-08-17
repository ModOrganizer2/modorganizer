//TL First Steps#0
function getTutorialSteps()
{
  return [
    function() {
        tutorial.text = qsTr("Welcome to the ModOrganizer Tutorial! This will guide you through the most important features of the program.")
        waitForClick()
    },

    function() {
        tutorial.text = qsTr("Before we continue with the step-by-step tutorial, I'd like to tell you about the other ways you can receive help on ModOrganizer.")
        waitForClick()
    },

    function() {
        tutorial.text = qsTr("The highlighted button provides hints on solving problems MO recognized automatically.")
        if (!tutorialControl.waitForAction("actionProblems")) {
            highlightAction("actionProblems", false)
            waitForClick()
        } else {
            tutorial.text += qsTr("\nThere IS a problem now but you may want to hold off on fixing it until after completing the tutorial.")
            highlightAction("actionProblems", true)
        }
    },

    function() {
        tutorial.text = qsTr("This button provides multiple sources of information and further tutorials.")
        highlightItem("actionHelp", true)
        tutorialControl.waitForButton("actionHelp")
    },

    function() {
        tutorial.text = qsTr("Finally there are tooltips on almost every part of Mod Organizer. If there is a control "
                           + "in MO you don't understand, please try hovering over it to get a short description or "
                           + "use \"Help on UI\" from the help menu to get a longer explanation")
        waitForClick()
    },

    function() {
        tutorial.text = qsTr("This list displays all mods installed through MO. The first point in our agenda will be adding some stuff to it.")
        highlightItem("modList", false)
        waitForClick()
    },

    function() {
        tutorial.text = qsTr("There are a few ways to get mods into ModOrganizer. You can use your regular browser to send download from nexusmods to MO."
                           + "Click on \"Nexus\" to open the appropriate nexusmods page. This will also register ModOrganizer as the downloader "
                           + "for \"nxm links\" for the game MO is managing. \"nxm links\" are the green buttons on Nexus saying \"Download with Manager\".")
        highlightAction("actionNexus", true)
        tutorialControl.waitForAction("actionNexus")
    },

    function() {
        tutorial.text = qsTr("Downloads will appear here. Double click one to install it.")
        applicationWindow.modInstalled.connect(nextStep)
        highlightItem("downloadView", true)
    },

    function() {
        unhighlight()
        applicationWindow.modInstalled.disconnect(nextStep)
        tutorial.text = qsTr("Great, you just installed your first mod. Please note that the installation procedure may differ based on how a mod was packaged.")
        waitForClick()
    },

    function() {
        tutorial.text = qsTr("This is how to get mods from Nexus.\n"
                             + "You can also install mods from disk using the \"Install Mod\" button.")
        highlightAction("actionInstallMod", false)
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
        modList.modlist_changed.connect(nextStep)
    },

    function() {
        modList.modlist_changed.disconnect(nextStep)
        unhighlight()
        tutorial.text = qsTr("For some mods, enabling it on the left pane is all you have to do...")
        waitForClick()
    },

    function() {
        tutorial.text = qsTr("...but most contain plugins. These are plugins for the game and are required "
                            +"to add stuff to the game (new weapons, armors, quests, areas, ...). "
                            +"Please open the \"Plugins\"-tab to get a list of plugins.")
        if (tutorialControl.waitForTabOpen("tabWidget", 0)) {
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
                              + "If in doubt, please consult the documentation of the indiviual mod. "
                              + "To do so, right-click the mod and select \"Information\".")
        highlightItem("modList", true)
        manager.activateTutorial("ModInfoDialog", "tutorial_firststeps_modinfo.js")
        applicationWindow.modInfoDisplayed.connect(nextStep)
    },

    function() {
        tutorial.text = qsTr("Another special type of files are BSAs. These are bundles of game resources. "
                             + "Please open the \"Archives\"-tab.")
        if (tutorialControl.waitForTabOpen("tabWidget", 1)) {
            highlightItem("tabWidget", true)
        } else {
            waitForClick()
        }
    },

    function() {
        tutorial.text = qsTr("These archives can be a real headache because the way bsas interact "
                           + "with non-bundled resources is complicated. The game can even crash if required "
                           + "archives are not loaded or ordered incorrectly.")
        waitForClick()
    },

    function() {
        tutorial.text = qsTr("MO applies some \"magic\" to make all BSAs that are checked in this list load in "
                           + "the correct order interleaved with the non-bundled resources. Usually it's best "
                           + "to check all bsas that have an exclamation mark at the side.")
        waitForClick()
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
