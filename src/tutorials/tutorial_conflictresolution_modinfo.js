function getTutorialSteps() {
    return [
        function() {
            tutorial.text = qsTr("Please switch to the \"Conflicts\"-Tab.")
            highlightItem("tabWidget", true)
            if (!tutorialControl.waitForTabOpen("tabWidget", "tabConflicts")) {
                nextStep()
            }
        },
        function() {
            tutorial.text = qsTr("Here you can see a list of files in this mod that out-prioritize others "
                                 +"in a file conflict and one with files where this mod is overridden.")
            waitForClick()
        },
        function() {
            tutorial.text = qsTr("Please close the information dialog again.")
            waitForClick()
        }

    ]
}
