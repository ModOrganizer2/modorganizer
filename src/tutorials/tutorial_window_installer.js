//WIN InstallDialog
function getTutorialSteps()
{
    return [
        function() {
            tutorial.text = qsTr("This mod has been packaged in a way that Mod Organizer did not automatically "
                               + "recognize...")
            waitForClick()
        },
        function() {
            tutorial.text = qsTr("You can use drag&drop on this list to fix the structure of the mod.")
            highlightItem("treeContent", false)
            waitForClick()
        },
        function() {
            tutorial.text = qsTr("The correct structure replicates the data-directory of the game. That means "
                               + "esps, the \"meshes\"- or \"textures\"-directory and so on should be directly "
                               + "below \"<data>\".")
            waitForClick()
        },
        function() {
            tutorial.text = qsTr("You can also disable files and directories that you don't want to unpack.")
            waitForClick()
        },
        function() {
            tutorial.text = qsTr("From the context menu (right-click) you can open textfiles, in case "
                               + "you want to access a readme.")
            waitForClick()
        },
        function() {
            tutorial.text = qsTr("This text will turn green if MO thinks the structure looks good.")
            highlightItem("problemLabel", false)
            manager.finishWindowTutorial("InstallDialog")
            waitForClick()
        }
    ]
}
