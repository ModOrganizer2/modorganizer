cmake_minimum_required(VERSION 3.16)

# appveyor does not build modorganizer in its standard location, so use
# DEPENDENCIES_DIR to find cmake_common
if(DEFINED DEPENDENCIES_DIR)
	include(${DEPENDENCIES_DIR}/modorganizer_super/cmake_common/src.cmake)
else()
	include(../cmake_common/src.cmake)
endif()


add_filter(NAME src/application GROUPS
	iuserinterface
	commandline
	main
	moapplication
	moshortcut
	multiprocess
	sanitychecks
	selfupdater
	updatedialog
)

add_filter(NAME src/browser GROUPS
	browserdialog
	browserview
)

add_filter(NAME src/core GROUPS
	categories
    categoriestable
	archivefiletree
	installationmanager
	nexusinterface
	nxmaccessmanager
	organizercore
	plugincontainer
	apiuseraccount
	processrunner
	qdirfiletree
	virtualfiletree
	uilocker
)

add_filter(NAME src/dialogs GROUPS
	aboutdialog
	activatemodsdialog
	categoriesdialog
	credentialsdialog
	filedialogmemory
	forcedloaddialog
	forcedloaddialogwidget
	listdialog
	messagedialog
	motddialog
	overwriteinfodialog
	queryoverwritedialog
	problemsdialog
	savetextasdialog
	selectiondialog
	syncoverwritedialog
	transfersavesdialog
	waitingonclosedialog
)

add_filter(NAME src/downloads GROUPS
	downloadlist
	downloadlistview
	downloadmanager
)

add_filter(NAME src/env GROUPS
	env
	envdump
	envfs
	envmetrics
	envmodule
	envsecurity
	envshell
	envshortcut
	envwindows
)

add_filter(NAME src/executables GROUPS
	executableslist
	editexecutablesdialog
)

add_filter(NAME src/instances GROUPS
	createinstancedialog
	createinstancedialogpages
	instancemanager
	instancemanagerdialog
)

add_filter(NAME src/loot GROUPS
	loot
	lootdialog
)

add_filter(NAME src/mainwindow GROUPS
	datatab
	downloadstab
	iconfetcher
	filetree
	filetreeitem
	filetreemodel
	mainwindow
	savestab
	statusbar
)

add_filter(NAME src/modinfo GROUPS
	modinfo
	modinfobackup
	modinfoforeign
	modinfooverwrite
	modinforegular
	modinfoseparator
	modinfowithconflictinfo
)

add_filter(NAME src/modinfo/dialog GROUPS
	modinfodialog
	modinfodialogcategories
	modinfodialogconflicts
	modinfodialogconflictsmodels
	modinfodialogesps
	modinfodialogfiletree
	modinfodialogfwd
	modinfodialogimages
	modinfodialognexus
	modinfodialogtab
	modinfodialogtextfiles
)

add_filter(NAME src/modinfo/dialog/widgets GROUPS
	modidlineedit
)

add_filter(NAME src/modlist GROUPS
	modlist
	modlistdropinfo
	modlistsortproxy
	modlistbypriorityproxy
)

add_filter(NAME src/modlist/view GROUPS
	filterlist
	modlistview
	modlistviewactions
	modlistcontextmenu
	modflagicondelegate
	modcontenticondelegate
	modconflicticondelegate
	modlistversiondelegate
)

add_filter(NAME src/plugins GROUPS
	pluginlist
	pluginlistsortproxy
	pluginlistview
	pluginlistcontextmenu
)

add_filter(NAME src/previews GROUPS
	previewdialog
	previewgenerator
)

add_filter(NAME src/profiles GROUPS
	profile
	profileinputdialog
	profilesdialog
)

add_filter(NAME src/proxies GROUPS
	downloadmanagerproxy
	modlistproxy
	organizerproxy
	pluginlistproxy
	proxyutils
)

add_filter(NAME src/register GROUPS
	shared/directoryentry
	shared/fileentry
	shared/filesorigin
	shared/fileregister
	shared/fileregisterfwd
	shared/originconnection
	directoryrefresher
)

add_filter(NAME src/settings GROUPS
	settings
	settingsutilities
)

add_filter(NAME src/settingsdialog GROUPS
	settingsdialog
	settingsdialogdiagnostics
	settingsdialoggeneral
	settingsdialognexus
	settingsdialogpaths
	settingsdialogplugins
	settingsdialogworkarounds
	settingsdialogmodlist
	settingsdialogtheme
	disableproxyplugindialog
)

add_filter(NAME src/utilities GROUPS
	shared/appconfig
	bbcode
	csvbuilder
	persistentcookiejar
	serverinfo
	spawn
	shared/util
	usvfsconnector
	shared/windows_error
	thread_utils
	json
	glob_matching
)

add_filter(NAME src/widgets GROUPS
	colortable
	genericicondelegate
	filerenamer
	icondelegate
	lcdnumber
	loglist
	loghighlighter
	noeditdelegate
	qtgroupingproxy
	texteditor
	viewmarkingscrollbar
	modelutils
	copyeventfilter
)


requires_project(game_features githubpp bsatk esptk archive usvfs)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
	${modorganizer_super_path}/lootcli/include
)


install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/dlls.manifest.qt5"
        DESTINATION bin/dlls
        RENAME dlls.manifest)

install(FILES ${qm_files} DESTINATION bin/translations)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/stylesheets"
                  "${CMAKE_CURRENT_SOURCE_DIR}/tutorials"
        DESTINATION bin)

deploy_qt(BINARIES ModOrganizer.exe uibase.dll plugins/bsa_packer.dll)

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/../qdds.dll" DESTINATION bin/dlls/imageformats)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/resources/markdown.html" DESTINATION bin/resources)
