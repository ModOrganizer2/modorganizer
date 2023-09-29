function getTutorialSteps()
{
    tutorialCanceller.visible = false
    return [
        function() {
            tutorial.text = qsTr("This dialog tries to expose as much information about a mod as possible. "
                                +"Depending on the mod, this may include readmes, screenshots, optional plugins and so on. "
                                +"If a certain type of information was not found in a mod, the corresponding tab "
                                +"is grayed out.")
            highlightItem("tabWidget", false)
            waitForClick()
        },
        function() {
            tutorial.text = qsTr("If you installed the mod from Nexus, the corresponding tab should give you direct "
                                +"access to the mod page description, which can be refreshed directly from Nexus.")
            waitForClick()
        },
        function() {
            unhighlight()
            tutorial.text = qsTr("We may revisit this screen in later tutorials.")
            waitForClick()
        }
    ]
}
