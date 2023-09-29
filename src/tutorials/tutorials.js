var tutorialSteps = []
var waitingForClick = false


function highlightItem(widgetName, click) {
    var rect = tutorialControl.getRect(widgetName)
    highlight.x = rect.x - 1
    highlight.y = rect.y - 1
    highlight.width = rect.width + 2
    highlight.height = rect.height + 2
    if (click) {
        highlight.border.color = "green"
    } else {
        highlight.border.color = "blue"
    }
    highlight.visible = true
}

function highlightAction(actionName, click) {
    var rect = tutorialControl.getActionRect(actionName)
    var offsetRect = tutorialControl.getMenuRect(actionName)
    highlight.x = rect.x - 1
    highlight.y = rect.y + offsetRect.height
    highlight.width = rect.width + 2
    highlight.height = rect.height + 2
    if (click) {
        highlight.border.color = "green"
    } else {
        highlight.border.color = "blue"
    }
    highlight.visible = true
}

function unhighlight() {
    highlight.visible = false
}

function waitForClick() {
    waitingForClick = true;
    description.continueVisible = true
    // ui needs to be locked, otherwise the tutorial-view does not receive mouse-events!
    tutorialControl.lockUI(true)
}

function cancelTutorial() {
    tutorialControl.finish()
}

function clickNext() {
    if (waitingForClick) {
        nextStep()
    }
}

function nextStep() {
    waitingForClick = false;
    description.continueVisible = false
    if (step < tutorialSteps.length) {
        tutorialControl.lockUI(false)
        step++
        tutorialSteps[step - 1]()
    } else {
        tutorialControl.finish()
    }
}

function sameStep() {
    tutorialSteps[step - 1]()
}

function onTabChanged(func) {
    tutToplevel.tabChanged.connect(func)
}

function init() {
    var res = Qt.include("file:///" + scriptName)
    if (res.status !== 0) {
        console.log("failed to load " + scriptName + ": " + res.status)
        return
    }

    tutorialSteps = getTutorialSteps()
}
