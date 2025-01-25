bin_PROGRAMS = bzfs

AM_CPPFLAGS = -DINSIDE_BZ \
	-DINSTALL_LIB_DIR=\"$(libdir)/\" \
	$(LIBCURL_CPPFLAGS) \
	$(CONF_CPPFLAGS) \
	`pkg-config --cflags python-$(PYTHON_VERSION)` \
	-I$(top_srcdir)/include
AM_CFLAGS = $(CONF_CFLAGS)
AM_CXXFLAGS = $(CONF_CXXFLAGS) \
	      -Wno-redundant-decls

MAINTAINERCLEANFILES = \
	Makefile.in

if BUILD_PLUGINS
plugin_files = bzfsPlugins.h bzfsPlugins.cxx
else
plugin_files =
endif

bzfs_LDFLAGS = -export-dynamic -static

bzfs_SOURCES =				\
	${plugin_files}			\
	bzfsPython.cxx			\
	bzfsPython.h			\
	AccessControlList.cxx		\
	AccessControlList.h		\
	Authentication.cxx		\
	Authentication.h		\
	BanCommands.cxx			\
	base64.cxx			\
	base64.h			\
	bzfsAPI.cxx			\
	bzfsHTTPAPI.cxx			\
	BZWError.cxx			\
	BZWError.h			\
	BZWReader.cxx			\
	BZWReader.h			\
	CmdLineOptions.cxx		\
	CmdLineOptions.h		\
	CustomArc.cxx			\
	CustomArc.h			\
	CustomBase.cxx			\
	CustomBase.h			\
	CustomBox.cxx			\
	CustomBox.h			\
	CustomCone.cxx			\
	CustomCone.h			\
	CustomDynamicColor.cxx		\
	CustomDynamicColor.h		\
	CustomGate.cxx			\
	CustomGate.h			\
	CustomGroup.cxx			\
	CustomGroup.h			\
	CustomLink.cxx			\
	CustomLink.h			\
	CustomMaterial.h		\
	CustomMaterial.cxx		\
	CustomMesh.cxx			\
	CustomMesh.h			\
	CustomMeshFace.cxx		\
	CustomMeshFace.h		\
	CustomMeshTransform.cxx		\
	CustomMeshTransform.h		\
	CustomPhysicsDriver.cxx		\
	CustomPhysicsDriver.h		\
	CustomSphere.cxx		\
	CustomSphere.h			\
	CustomTextureMatrix.cxx		\
	CustomTextureMatrix.h		\
	CustomPyramid.cxx		\
	CustomPyramid.h			\
	CustomTetra.cxx			\
	CustomTetra.h			\
	CustomWaterLevel.cxx		\
	CustomWaterLevel.h		\
	CustomWeapon.cxx		\
	CustomWeapon.h			\
	CustomWorld.cxx			\
	CustomWorld.h			\
	CustomZone.cxx			\
	CustomZone.h			\
	DropGeometry.cxx		\
	DropGeometry.h			\
	EntryZones.cxx			\
	EntryZones.h			\
	Filter.cxx			\
	Filter.h			\
	FlagHistory.cxx			\
	FlagHistory.h			\
	FlagInfo.cxx			\
	FlagInfo.h			\
	GameKeeper.cxx			\
	GameKeeper.h			\
	ListServerConnection.cxx	\
	ListServerConnection.h		\
	MasterBanList.cxx		\
	MasterBanList.h			\
	PackVars.h			\
	ParseMaterial.cxx		\
	ParseMaterial.h			\
	Permissions.h			\
	Permissions.cxx			\
	RandomSpawnPolicy.h		\
	RandomSpawnPolicy.cxx		\
	RecordReplay.cxx		\
	RecordReplay.h			\
	RejoinList.cxx			\
	RejoinList.h			\
	Score.h				\
	Score.cxx			\
	ServerCommand.cxx		\
	ServerCommand.h			\
	ServerSidePlayer.cxx		\
	ShotManager.h			\
	ShotManager.cxx			\
	SpawnPolicy.cxx			\
	SpawnPolicy.h			\
	SpawnPosition.cxx		\
	SpawnPosition.h			\
	TeamBases.cxx			\
	TeamBases.h			\
	VotingArbiter.cxx			\
	VotingArbiter.h			\
	WorldFileLocation.cxx		\
	WorldFileLocation.h		\
	WorldFileObject.cxx		\
	WorldFileObject.h		\
	WorldFileObstacle.cxx		\
	WorldFileObstacle.h		\
	WorldGenerators.cxx		\
	WorldGenerators.h		\
	WorldInfo.cxx			\
	WorldInfo.h			\
	WorldWeapons.cxx		\
	WorldWeapons.h			\
	WorldEventManager.cxx		\
	commands.cxx			\
	commands.h			\
	bzfs.cxx			\
	bzfs.h

LDADD =					\
	../obstacle/libObstacle.la	\
	../game/libGame.la		\
	../net/libNet.la		\
	../common/libCommon.la		\
	$(LIBCURL)			\
	$(LIBCARES)			\
	$(LIBREGEX)			\
	$(LIBZ)				\
	$(LIBDL)			\
	$(LIBMINIUPNPC)			\
	-lpython$(PYTHON_VERSION)	\
	../date/libDate.la
