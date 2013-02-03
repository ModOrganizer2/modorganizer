function getTutorialSteps()
{
    return [
        function() {
            tutorial.text = qsTr("This is a fully featured browser that is set up to "
                                +"open the correct nexus page for your game. You can "
                                +"download any mod using the \"DOWNLOAD WITH MANAGER\"-button "
                                +"or the \"manual\"-link and it will be downloaded by MO.")
            waitForClick()
        }
    ]
}
