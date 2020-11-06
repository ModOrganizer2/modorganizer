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
	sanitychecks
	selfupdater
	singleinstance
)

add_filter(NAME src/browser GROUPS
	browserdialog
	browserview
)

add_filter(NAME src/core GROUPS
	categories
	archivefiletree
	installationmanager
	loadmechanism
	nexusinterface
	nxmaccessmanager
	organizercore
	plugincontainer
	apiuseraccount
	processrunner
	qdirfiletree
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
	downloadlistsortproxy
	downloadlistwidget
	downloadmanager
)

add_filter(NAME src/env GROUPS
	env
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
	iconfetcher
	filetree
	filetreeitem
	filetreemodel
	filterlist
	mainwindow
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
add_filter(NAME src/modlist GROUPS
	modlist
	modlistsortproxy
	modlistview
)

add_filter(NAME src/plugins GROUPS
	pluginlist
	pluginlistsortproxy
	pluginlistview
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
	settingsdialogsteam
	settingsdialogworkarounds
)

add_filter(NAME src/utilities GROUPS
	shared/appconfig
	bbcode
	csvbuilder
	shared/error_report
	shared/leaktrace
	persistentcookiejar
	serverinfo
	spawn
	shared/stackdata
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
	modflagicondelegate
	modconflicticondelegate
	modidlineedit
	noeditdelegate
	qtgroupingproxy
	texteditor
	viewmarkingscrollbar
)


requires_project(game_features githubpp bsatk esptk archive usvfs)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
	${modorganizer_super_path}/lootcli/include
)


install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/dlls.manifest
        DESTINATION bin/dlls
        RENAME dlls.manifest)

install(FILES ${qm_files} DESTINATION bin/resources/translations)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/stylesheets
		DESTINATION bin)
		
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tutorials
		DESTINATION bin/resources)

deploy_qt(BINARIES ModOrganizer.exe uibase.dll plugins/bsa_packer.dll)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../qdds.dll DESTINATION bin/dlls/imageformats)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/resources/markdown.html DESTINATION bin/resources)
