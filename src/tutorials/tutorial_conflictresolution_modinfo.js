function getTutorialSteps() {
    tutorialCanceller.visible = false
    return [
        function() {
            tutorial.text = qsTr("Please switch to the \"Conflicts\"-Tab.")
            highlightItem("tabWidget", true)
            if (!tutorialControl.waitForTabOpen("tabWidget", "tabConflicts")) {
                nextStep()
            }
        },
        function() {
            tutorial.text = qsTr("Here you can see two lists: a list of files that this mod overwrites that are also "
                               + "provided by other mods, and a list of files in this mod which are overwritten by "
                               + "one or more other mods.")
            waitForClick()
        },
        function() {
            tutorial.text = qsTr("Please close the information dialog.")
            waitForClick()
        }

    ]
}
