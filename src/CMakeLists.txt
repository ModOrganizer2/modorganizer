cmake_minimum_required(VERSION 3.16)

find_package(mo2-cmake CONFIG REQUIRED)

find_package(usvfs CONFIG REQUIRED)

find_package(mo2-uibase CONFIG REQUIRED)
find_package(mo2-archive CONFIG REQUIRED)
find_package(mo2-lootcli-header CONFIG REQUIRED)
find_package(mo2-bsatk CONFIG REQUIRED)
find_package(mo2-esptk CONFIG REQUIRED)
find_package(mo2-dds-header CONFIG REQUIRED)
find_package(mo2-libbsarch CONFIG REQUIRED)

find_package(Qt6 REQUIRED COMPONENTS WebEngineWidgets WebSockets)
find_package(Boost CONFIG REQUIRED COMPONENTS program_options thread interprocess signals2 uuid accumulators)
find_package(7zip CONFIG REQUIRED)
find_package(lz4 CONFIG REQUIRED)
find_package(ZLIB REQUIRED)

add_executable(organizer)
set_target_properties(organizer PROPERTIES
	OUTPUT_NAME "ModOrganizer"
	WIN32_EXECUTABLE TRUE)

# disable translations because we want to be able to install somewhere else if
# required
mo2_configure_target(organizer WARNINGS 4 TRANSLATIONS OFF)

# we add translations "manually" to handle MO2_INSTALL_IS_BIN
mo2_add_translations(organizer
	INSTALL_RELEASE
	INSTALL_DIRECTORY "${MO2_INSTALL_BIN}/translations"
	SOURCES ${CMAKE_CURRENT_SOURCE_DIR})

mo2_set_project_to_run_from_install(
	organizer EXECUTABLE ${CMAKE_INSTALL_PREFIX}/${MO2_INSTALL_BIN}/ModOrganizer.exe)

target_link_libraries(organizer PRIVATE
	Shlwapi Bcrypt
	usvfs::usvfs mo2::uibase mo2::archive mo2::libbsarch
	mo2::bsatk mo2::esptk mo2::lootcli-header
	Boost::program_options Boost::signals2 Boost::uuid Boost::accumulators
	Qt6::WebEngineWidgets Qt6::WebSockets Version Dbghelp)

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/dlls.manifest.qt6"
		DESTINATION ${MO2_INSTALL_BIN}/dlls
		CONFIGURATIONS Release RelWithDebInfo
		RENAME dlls.manifest)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/dlls.manifest.debug.qt6"
		DESTINATION ${MO2_INSTALL_BIN}/dlls
		CONFIGURATIONS Debug
		RENAME dlls.manifest)

if (NOT MO2_SKIP_TUTORIALS_INSTALL)
	install(
		DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tutorials"
		DESTINATION ${MO2_INSTALL_BIN})
endif()

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/resources/markdown.html"
	DESTINATION ${MO2_INSTALL_BIN}/resources)

# install ModOrganizer.exe itself
install(FILES $<TARGET_FILE:organizer> DESTINATION ${MO2_INSTALL_BIN})

# install dependencies DLLs
install(FILES $<TARGET_FILE:mo2::libbsarch> DESTINATION ${MO2_INSTALL_BIN}/dlls)
install(FILES $<TARGET_FILE:mo2::archive> DESTINATION ${MO2_INSTALL_BIN}/dlls)
install(FILES $<TARGET_FILE:7zip::7zip> DESTINATION ${MO2_INSTALL_BIN}/dlls)

# this may copy over the ones from uibase/usvfs
# - when building with mob, this should not matter as the files should be identical
# - when building standalone, this should help having matching USVFS DLL between the
#   build and the installation
# - this may cause issue with uibase in standalone mode if the installed version does
#   not match the one used for the build, but there would be other issue anyway (e.g.
#   different uibase.dll between modorganizer and plugins)
#
install(FILES
	$<TARGET_FILE:mo2::uibase>
	$<TARGET_FILE:usvfs_x64::usvfs_dll>
	$<TARGET_FILE:usvfs_x86::usvfs_dll>
	$<TARGET_FILE:usvfs_x64::usvfs_proxy>
	$<TARGET_FILE:usvfs_x86::usvfs_proxy>
DESTINATION ${MO2_INSTALL_BIN})

# do not install PDB if CMAKE_INSTALL_PREFIX is "bin"
if (NOT MO2_INSTALL_IS_BIN)
	install(FILES $<TARGET_PDB_FILE:organizer> DESTINATION pdb)
endif()

mo2_deploy_qt(
	DIRECTORY ${MO2_INSTALL_BIN}
	BINARIES ModOrganizer.exe $<TARGET_FILE_NAME:mo2::uibase>)

# set source groups for VS
mo2_add_filter(NAME src/application GROUPS
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

mo2_add_filter(NAME src/browser GROUPS
	browserdialog
	browserview
)

mo2_add_filter(NAME src/categories GROUPS
	categories
	categoriestable
	categoriesdialog
	categoryimportdialog
)

mo2_add_filter(NAME src/core GROUPS
	archivefiletree
	github
	inibakery
	installationmanager
	nexusinterface
	nxmaccessmanager
	organizercore
	apiuseraccount
	processrunner
	qdirfiletree
	virtualfiletree
	uilocker
)

mo2_add_filter(NAME src/extensions GROUPS
	game_features
	thememanager
	translationmanager
	extensionmanager
	extensionwatcher
	pluginmanager
	proxyqt
)

mo2_add_filter(NAME src/dialogs GROUPS
	aboutdialog
	activatemodsdialog
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

mo2_add_filter(NAME src/downloads GROUPS
	downloadlist
	downloadlistview
	downloadmanager
)

mo2_add_filter(NAME src/env GROUPS
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

mo2_add_filter(NAME src/executables GROUPS
	executableslist
	editexecutablesdialog
)

mo2_add_filter(NAME src/instances GROUPS
	createinstancedialog
	createinstancedialogpages
	instancemanager
	instancemanagerdialog
)

mo2_add_filter(NAME src/loot GROUPS
	loot
	lootdialog
)

mo2_add_filter(NAME src/mainwindow GROUPS
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

mo2_add_filter(NAME src/modinfo GROUPS
	modinfo
	modinfobackup
	modinfoforeign
	modinfooverwrite
	modinforegular
	modinfoseparator
	modinfowithconflictinfo
)

mo2_add_filter(NAME src/modinfo/dialog GROUPS
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

mo2_add_filter(NAME src/modinfo/dialog/widgets GROUPS
	modidlineedit
)

mo2_add_filter(NAME src/modlist GROUPS
	modlist
	modlistdropinfo
	modlistsortproxy
	modlistbypriorityproxy
)

mo2_add_filter(NAME src/modlist/view GROUPS
	filterlist
	modlistview
	modlistviewactions
	modlistcontextmenu
	modflagicondelegate
	modcontenticondelegate
	modconflicticondelegate
	modlistversiondelegate
)

mo2_add_filter(NAME src/plugins GROUPS
	pluginlist
	pluginlistsortproxy
	pluginlistview
	pluginlistcontextmenu
)

mo2_add_filter(NAME src/previews GROUPS
	previewdialog
	previewgenerator
)

mo2_add_filter(NAME src/profiles GROUPS
	profile
	profileinputdialog
	profilesdialog
)

mo2_add_filter(NAME src/proxies GROUPS
	downloadmanagerproxy
	gamefeaturesproxy
	modlistproxy
	extensionlistproxy
	organizerproxy
	pluginlistproxy
	proxyutils
)

mo2_add_filter(NAME src/register GROUPS
	shared/directoryentry
	shared/fileentry
	shared/filesorigin
	shared/fileregister
	shared/fileregisterfwd
	shared/originconnection
	directoryrefresher
)

mo2_add_filter(NAME src/settings GROUPS
	settings
	settingsutilities
)

mo2_add_filter(NAME src/settingsdialog GROUPS
	settingsdialog
	settingsdialogdiagnostics
	settingsdialoggeneral
	settingsdialognexus
	settingsdialogpaths
	settingsdialogextensions
	settingsdialogextensionrow
	settingsdialogextensioninfo
	settingsdialogworkarounds
	settingsdialogmodlist
	settingsdialogtheme
	disableproxyplugindialog
)

mo2_add_filter(NAME src/utilities GROUPS
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

mo2_add_filter(NAME src/widgets GROUPS
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
