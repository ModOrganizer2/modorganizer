//TL Conflict Resolution#10
function getTutorialSteps() {
    return [
        function() {
            tutorial.text = qsTr("There are multiple types of conflicts you may encounter when dealing with Mods. "
                                +"This tutorial will try to cover and explain how to deal with all of them.")
            waitForClick()
        },
        function() {
            tutorial.text = qsTr("First up are file conflicts. These occur when two mods contain the same file. "
                                +"Most commonly this happens when several mods replace the same standard asset from "
                                +"the game, like the texture of an armor.")
            waitForClick()
        },
        function() {
            tutorial.text = qsTr("As an example, say you install \"ModA\" which contains stylish new iron and leather armor. "
                                +"Then you install \"ModB\" which contains sexy ebony and leather armor. Obviously there is a "
                                +"conflict now: which leather armor to use?")
            waitForClick()
        },
        function() {
            tutorial.text = qsTr("If you were to install the mods manually, when installing \"ModB\" you would be asked if you want "
                                 +"to overwrite conflicting files. If you choose yes, you get the leather armor from \"ModB\" otherwise "
                                 +"you keep the one from \"ModA\". If you later decide you made the wrong choice, "
                                 +"you have to reinstall one of the mods.")
            waitForClick()
        },
        function() {
            tutorial.text = qsTr("With MO, both ModA and ModB get installed completely (no overwrite dialog) and by default "
                                 +"\"ModB\" gets to provide the leather armor because it's automatically assigned the higher priority.")
            waitForClick()
        },
        function() {
            tutorial.text = qsTr("However, you can change the mod priority at any time using drag&drop on this list. "
                                 +"If you assign \"ModA\" a higher priority, it provides the leather armor, no re-installation required. "
                                 +"Since the priorities of mods in this list are treated as if the mods were installed in that order, "
                                 +"I tend to talk about \"installation order\".")
            highlightItem("modList", false)
            waitForClick()
        },
        function() {
            tutorial.text = qsTr("If the \"Flags\"-column is enabled in the mod list, it will show you which mods are involved in a conflict and how...")
            waitForClick()
        },
        function() {
            unhighlight()
            tutorial.text = qsTr("<img src=\"qrc:///MO/gui/emblem_conflict_overwrite\" /> indicates that the mod overwrites files that are also available in another mod.")
            waitForClick()
        },
        function() {
            tutorial.text = qsTr("<img src=\"qrc:///MO/gui/emblem_conflict_overwritten\" /> indicates that the mod is <b>partially</b> overwritten by another.")
            waitForClick()
        },
        function() {
            tutorial.text = qsTr("<img src=\"qrc:///MO/gui/emblem_conflict_mixed\" /> indicates that both of the above is true.")
            waitForClick()
        },
        function() {
            tutorial.text = qsTr("<img src=\"qrc:///MO/gui/emblem_conflict_redundant\" /> indicates that the mod is completely overwrtten by another. You could as well disable it.");
            waitForClick()
        },
        function() {
            tutorial.text = qsTr("There are two ways to see the individual files involved in a conflict:")
            waitForClick()
        },
        function() {
            tutorial.text = qsTr("Option A: Switch to the \"Data\"-tab if necessary")
            if (!tutorialControl.waitForTabOpen("tabWidget", 2)) {
                highlightItem("tabWidget", false)
                waitForClick()
            } else {
                highlightItem("tabWidget", true)
            }
        },
        function() {
            tutorial.text = qsTr("... here, if you mark the highlighted control, the tree will only display files in conflict. "
                                 +"In the right column, it says which mod currently provides the mod (because it has highest priority) "
                                 +"and if you hover your mouse over that info, it will list which other mods contains it.")
            highlightItem("conflictsCheckBox", false)
            waitForClick()
        },
        function() {
            tutorial.text = qsTr("Option B: Open the <i>Information</i>-Dialog of an <b>enabled</b> mod you're interested in by "
                                 +"either double-clicking it or selecting <i>Information...</i> from the right-click menu")
            highlightItem("modList", true)
            manager.activateTutorial("ModInfoDialog", "tutorial_conflictresolution_modinfo.js")
            applicationWindow.modInfoDisplayed.connect(nextStep)
        },
        function() {
            unhighlight()
            tutorial.text = qsTr("This was everything to know about file conflicts. The second type of conflict we have to deal with "
                                +"are \"record conflicts\".")
            waitForClick()
        },
        function() {
            tutorial.text = qsTr("I told you in the \"First Steps\" tutorial how the esp/esm/esl plugins contain changes to the game world "
                                 +"like modifications to the terrain or existing NPCs. Each change like this is stored in a record, hence the "
                                 +"name \"record conflict\". For example when two mods try to change the same location, only one change can become active.")
            waitForClick()
        },
        function() {
            tutorial.text = qsTr("As with file conflicts you can't really fix these conflicts, you have to choose which change you want. "
                                 +"This time around however, if you choose wrong, your game may become unstable because there may be "
                                 +"dependencies between the records of a mod.")
            waitForClick()
        },
        function() {
            tutorial.text = qsTr("Please open the \"Plugins\"-tab...")
            highlightItem("tabWidget", true)
            if (!tutorialControl.waitForTabOpen("tabWidget", 0)) {
                nextStep()
            }
        },
        function() {
            tutorial.text = qsTr("Again you can use drag&drop to change priorities of plugins, thus deciding which plugin takes "
                                 +"precedence in regards to conflicts. This is commonly called the \"load order\". "
                                 +"But how do you know how to order the plugins?")
            waitForClick()
        },
        function() {
            unhighlight()
            tutorial.text = qsTr("Unlike with file conflicts, MO does not provide help on finding conflicts. The good news is, there "
                                 +"already is a perfect tool for that called LOOT. LOOT is available on the Nexus and integrates "
                                 +"neatly with MO. Basically, if you don't have LOOT yet, install it once this tutorial is over.")
            waitForClick()
        },
        function() {
            tutorial.text = qsTr("After you installed LOOT in the default location (follow its instructions), start MO again and LOOT should automatically appear as an Executable...")
            highlightItem("startGroup", false)
            waitForClick()
        },
        function() {
            tutorial.text = qsTr("When you run LOOT, it will automatically re-organize plugins for best compatibility (overwriting your manual changes). "
                                 +"It will also open a report in your browser that warns about incompatibilities. You should read the report, at least "
                                 +"for new mods.")
            waitForClick()
        },
        function() {
            unhighlight()
            tutorial.text = qsTr("The final type of conflicts are also \"record conflicts\". Like the previous type. It's confusing, so "
                                 +"I'll just call them \"lists conflicts\" instead. The difference is the types of "
                                 +"records in conflict. The ones in question here can be merged so you may be able to get all modifications in your game.")
            waitForClick()
        },
        function() {
            tutorial.text = qsTr("One common example of such records are leveled lists that contain all the items that may spawn at a specific "
                                 +"character level. Traditionally, if multiple mods add items to such a list, only one is in effect...")
            waitForClick()
        },
        function() {
            tutorial.text = qsTr("... but there are tools to merge those mods so you can have the effects of all of them. Again, this "
                                 +"functionality is not integrated with MO because there are already great tools. For Oblivion and Skyrim "
                                 +"look for wrye bash, for fallout 3/nv it's wrye flash. For Skyrim there is also "
                                 +"\"SkyBash\". All of these can create a so-called \"bashed patch\" which is a plugin that contains the combined "
                                 +"mergeable records from all your mods.")
            waitForClick()
        },
        function() {
            tutorial.text = qsTr("This completes the tutorial.")
            waitForClick()
        }

    ]
}
