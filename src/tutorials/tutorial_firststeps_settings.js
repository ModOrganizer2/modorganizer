function getTutorialSteps()
{
    return [
        function() {
            highlightItem("tabWidget", true)
            tutorial.text = qsTr("You can use your regular browser to download from Nexus.\nPlease open the \"Nexus\"-tab")
            tutorialControl.waitForTabOpen("tabWidget", 1)
        },

        function() {
            highlightItem("associateButton", false)
            tutorial.text = qsTr("Click this button so that \"DOWNLOAD WITH MANAGER\"-buttons "
                                +"are download with Mod Organizer.")
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
