function getTutorialSteps()
{
    tutorialCanceller.visible = false
    return [
        function() {
            highlightItem("tabWidget", true)
            tutorial.text = qsTr("It is possible to download files directly from Nexus.\n\n"
                               + "Please open the \"Nexus\" tab.")
            tutorialControl.waitForTabOpen("tabWidget", "nexusTab")
        },

        function() {
            highlightItem("associateButton", false)
            tutorial.text = qsTr("Clicking on this button should register Nexus \"Download with Manager\" buttons "
                                +"to download with Mod Organizer.")
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
