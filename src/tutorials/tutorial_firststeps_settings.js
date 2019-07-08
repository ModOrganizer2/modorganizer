function getTutorialSteps()
{
    return [
        function() {
            highlightItem("tabWidget", true)
            tutorial.text = qsTr("You can use your regular browser to download from Nexus.\nPlease open the \"Nexus\"-tab")
            tutorialControl.waitForTabOpen("tabWidget", "nexusTab")
        },

        function() {
            highlightItem("associateButton", false)
            tutorial.text = qsTr("Click this button so that \"DOWNLOAD WITH MANAGER\"-buttons "
                                +"are download with Mod Organizer.")
            waitForClick()
        },

        function() {
            highlightItem("nexusBox", false)
            tutorial.text = qsTr("Use this interface to obtain an API key from NexusMods. "
                                +"This is used for all API connections - downloads, updates "
                                +"etc. MO2 uses the Windows Credential Manager to store "
                                +"this data securely. If the SSO page on Nexus is failing, "
                                +"use the manual entry and copy the API key from your profile.")
            waitForClick()
        }
    ]
}
