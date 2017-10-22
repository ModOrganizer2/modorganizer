//TL Overview#5


var tooltips = []

function tooltipWidget(widgetName, explanation, maxheight, clickable) {
  var rect = tutorialControl.getRect(widgetName)
  var component = Qt.createComponent("TooltipArea.qml")
  if (component.status === Component.Error) {
    console.log("b" + component.errorString())
  }
  if (typeof clickable === 'undefined') {
    clickable = false
  }
  if ((typeof maxheight === 'undefined') || maxheight === 0) {
    maxheight = rect.height
  }

  var obj = component.createObject(tutToplevel,
                                   { "x" : rect.x,
                                     "y" : rect.y,
                                     "width" : rect.width,
                                     "height" : maxheight
                                   })
  obj.tooltipText = explanation
  obj.clickable = clickable
  obj.visible = true

  tooltips.push(obj)
}

function tooltipAction(actionName, explanation, maxheight, clickable) {
  var rect = tutorialControl.getActionRect(actionName)
  var component = Qt.createComponent("TooltipArea.qml")
  if (typeof clickable === 'undefined') {
    clickable = false
  }
  if ((typeof maxheight === 'undefined') || maxheight === 0) {
    maxheight = rect.height
  }
  var obj = component.createObject(tutToplevel,
                                   { "x" : rect.x,
                                     "y" : rect.y,
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
  tooltipWidget("groupCombo", qsTr("The dropdown allows various ways of grouping the mods shown in the mod list."))
  tooltipWidget("displayCategoriesBtn", qsTr("Show/hide the category pane."))
  tooltipWidget("modFilterEdit", qsTr("Quickly filter the mod list as you type."))
  tooltipWidget("qt_tabwidget_tabbar", qsTr("Switch between information views."), 0, true)
  tooltipWidget("categoriesList", qsTr("This shows mod categories and some meta categories (in angle-brackets). Select some to filter the mod list. For example select \"<Checked>\" to show only active mods."))
  tooltipWidget("executablesListBox", qsTr("Customizable list for choosing the program to run."))
  tooltipWidget("startButton", qsTr("When this button is clicked, Mod Organizer creates a virtual directory structure then runs the program selected to the left."))
  tooltipWidget("linkButton", qsTr("Will create a shortcut for quick access. The shortcut can be placed in the toolbar at the top, in the Start Menu or on the Windows Desktop."))
  tooltipWidget("logList", qsTr("Log messages produced by MO. Please note that messages with a light bulb usually don't require your attention."))

  tooltipAction("actionSettings", qsTr("Configure Mod Organizer."))
  tooltipAction("actionProblems", qsTr("Reports potential Problems about the current setup."))
  tooltipAction("actionUpdate", qsTr("Activates if there is an update for MO. Please note that if, for any reason, MO can't communicate with NMM, this will not work either."))

  switch (manager.findControl("tabWidget").currentIndex) {
    case 0:
      tooltipWidget("espList", qsTr("Plugins (esp/esm/esl files) of the mods in the current profile. They need to be checked to be loaded."))
      tooltipWidget("bossButton", qsTr("Automatically sort plugins using the bundled LOOT application."))
      tooltipWidget("espFilterEdit", qsTr("Quickly filter plugin list as you type."))
      break
    case 1:
      tooltipWidget("bsaList", qsTr("All the asset archives (.bsa files) for all active mods."))
      break
    case 2:
      tooltipWidget("dataTree", qsTr("The directory tree and all files that the program will see."))
      break
    case 3:
      tooltipWidget("savegameList", qsTr("Save game browser. Shows all the saves for the current profile and whether or not the current mod-load status is correct."))
      break
    case 4:
      tooltipWidget("downloadView", qsTr("Shows the mods that have been downloaded and if they’ve been installed."))
      break
  }
}

function getTutorialSteps() {
    return [
        function() {
          tutorial.text = qsTr("Click to quit")

          setupTooptips()

          onTabChanged(setupTooptips)

          waitForClick()
        }
    ]
}


