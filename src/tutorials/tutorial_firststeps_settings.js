function getTutorialSteps()
{
    return [
        function() {
            highlightItem("tabWidget", true)
            tutorial.text = qsTr("You can use your regular browser to download from Nexus.\nPlease open the \"Nexus\"-tab")
            tutorialControl.waitForTabOpen("tabWidget", 2)
        },

        function() {
            highlightItem("handleNXMBox", false)
            tutorial.text = qsTr("If this box is checked the \"DOWNLOAD WITH MANAGER\"-buttons "
                                +"in your regular browser will also download with Mod Organizer.")
            waitForClick()
        },

        function() {
            highlightItem("nexusBox", false)
            tutorial.text = qsTr("You can also store your Nexus-credentials "
                                +"here for automatic login. The password is "
                                +"stored unencrypted on your disk!")
            waitForClick()
        }
    ]
}
