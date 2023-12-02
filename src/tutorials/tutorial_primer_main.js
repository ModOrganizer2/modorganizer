//TL Overview#5


var tooltips = []

function tooltipWidget(widgetName, explanation, maxheight, clickable) {
    var component = Qt.createComponent("TooltipArea.qml")
    if (component.status === Component.Ready)
        finishCreation(component, widgetName, explanation, maxheight, clickable);
    else
        component.statusChanged.connect(function() {
            finishCreation(component, widgetName, explanation, maxheight, clickable);
        });
}

function finishCreation(component, widgetName, explanation, maxheight, clickable) {
  if (component.status === Component.Ready) {
    var rect = tutorialControl.getRect(widgetName)
    if (typeof clickable === 'undefined') {
        clickable = false
    }
    if ((typeof maxheight === 'undefined') || maxheight === 0) {
        maxheight = rect.height
    }

    var obj = component.createObject(tutToplevel,
        {
            "x": rect.x,
            "y": rect.y,
            "width": rect.width,
            "height": maxheight
        })
    obj.tooltipText = explanation
    obj.clickable = clickable
    obj.visible = true

    tooltips.push(obj)
  } else if (component.status === Component.Error) {
      console.log("Error loading component: " + component.errorString())
  }
}

function tooltipAction(actionName, explanation, maxheight, clickable) {
  var rect = tutorialControl.getActionRect(actionName)
  var offsetRect = tutorialControl.getMenuRect(actionName)
  var component = Qt.createComponent("TooltipArea.qml")
  if (typeof clickable === 'undefined') {
    clickable = false
  }
  if ((typeof maxheight === 'undefined') || maxheight === 0) {
    maxheight = rect.height
  }
  var obj = component.createObject(tutToplevel,
                                   { "x" : rect.x,
                                     "y" : rect.y + offsetRect.height,
                                     "width" : rect.width,
                                     "height" : maxheight
                                   })
  obj.tooltipText = explanation
  obj.clickable = clickable
  obj.visible = true

  tooltips.push(obj)
}

function setupTooptips() {
  for (var tip in tooltips) {
    tooltips[tip].destroy()
  }
  tooltips = []

  tooltipWidget("modList", qsTr("This window shows all the mods that are installed. The column headers can be used for sorting. Only checked mods are active in the current profile."))
  tooltipWidget("profileBox", qsTr("Each profile is a separate set of enabled mods and ini settings."))
  tooltipWidget("listOptionsBtn", qsTr("Perform various actions on your mod list, such as refreshing data and checking for mod updates."))
  tooltipWidget("openFolderMenu", qsTr("Quick access to various directories, such as your MO2 mods, profiles, saves, and your active game location."))
  tooltipWidget("restoreModsButton", qsTr("Restore a mod list backup."))
  tooltipWidget("saveModsButton", qsTr("Create a backup of your current mod list."))
  tooltipWidget("activeModsCounter", qsTr("Running counter of your active mods. Hover to see a more detailed breakdown."))
  tooltipWidget("groupCombo", qsTr("The dropdown allows various ways of grouping the mods shown in the mod list."))
  tooltipWidget("displayCategoriesBtn", qsTr("Show/hide the category pane."))
  tooltipWidget("modFilterEdit", qsTr("Quickly filter the mod list as you type."))
  tooltipWidget("qt_tabwidget_tabbar", qsTr("Switch between information views."), 0, true)
  tooltipWidget("categoriesGroup", qsTr("This shows mod categories and some meta categories (in angle-brackets). Select some to filter the mod list. For example select \"<Checked>\" to show only active mods."))
  tooltipWidget("executablesListBox", qsTr("Customizable list for choosing the program to run."))
  tooltipWidget("startButton", qsTr("When this button is clicked, Mod Organizer creates a virtual directory structure then runs the program selected to the left."))
  tooltipWidget("linkButton", qsTr("Will create a shortcut for quick access. The shortcut can be placed in the toolbar at the top, in the Start Menu or on the Windows Desktop."))
  tooltipWidget("logList", qsTr("Log messages produced by MO. Please note that messages with a light bulb usually don't require your attention."))
  tooltipWidget("apistats", qsTr("Indicator of your current NexusMods API request limits."))

  tooltipAction("actionChange_Game", qsTr("Change/manage MO2 instances or switch to portable mode."))
  tooltipAction("actionInstallMod", qsTr("Browse to and manually install a mod from an archive on your computer."))
  tooltipAction("actionNexus", qsTr("Automatically open NexusMods to browse and install mods via the API."))
  tooltipAction("actionModPage", qsTr("Automatically open NexusMods to browse and install mods via the API."))
  tooltipAction("actionAdd_Profile", qsTr("Manage your MO2 profiles."))
  tooltipAction("action_Refresh", qsTr("Refresh everything."))
  tooltipAction("actionModify_Executables", qsTr("Open the executable editor to add and modify applications you wish to run with MO2."))
  tooltipAction("actionTool", qsTr("Select from a collection of additional tools, such as an INI editor, integrated FNIS updater, and more."))
  tooltipAction("actionSettings", qsTr("Configure Mod Organizer."))
  tooltipAction("actionEndorseMO", qsTr("See the status of and/or endorse MO2 on NexusMods."))
  tooltipAction("actionNotifications", qsTr("Notifications about the current setup."))
  tooltipAction("actionUpdate", qsTr("Activates if there is an update for MO. Please note that if, for any reason, MO can't communicate with NMM, this will not work either."))
  tooltipAction("actionHelp", qsTr("Access more information about MO2, including these tutorials, a link to the development discord, information about the devs and dependencies."))

  switch (tutorialControl.getTabName("tabWidget")) {
    case "espTab":
      tooltipWidget("espList", qsTr("Plugins (esp/esm/esl files) of the mods in the current profile. They need to be checked to be loaded."))
      tooltipWidget("sortButton", qsTr("Automatically sort plugins using the bundled LOOT application."))
      tooltipWidget("restoreButton", qsTr("Restore a backup of your plugin list order."))
      tooltipWidget("saveButton", qsTr("Save a backup of your plugin list order."))
      tooltipWidget("activePluginsCounter", qsTr("Counter of your total active plugins. Hover to see a breakdown of plugin types."))
      tooltipWidget("espFilterEdit", qsTr("Quickly filter plugin list as you type."))
      break
    case "bsaTab":
      tooltipWidget("bsaList", qsTr("All the asset archives (.bsa files) for all active mods."))
      break
    case "dataTab":
      tooltipWidget("dataTree", qsTr("The directory tree and all files that the program will see."))
      break
    case "savesTab":
      tooltipWidget("savegameList", qsTr("Save game browser. Shows all the saves for the current profile and whether or not the current mod-load status is correct."))
      break
    case "downloadTab":
      tooltipWidget("downloadView", qsTr("Shows the mods that have been downloaded and if they’ve been installed."))
      break
  }
}

function getTutorialSteps() {
    tutorialCanceller.visible = false
    return [
        function() {
          tutorial.text = qsTr("Click to quit")

          setupTooptips()

          onTabChanged(setupTooptips)

          waitForClick()
        }
    ]
}
