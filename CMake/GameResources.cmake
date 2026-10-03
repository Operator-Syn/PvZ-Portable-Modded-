option(PVZ_AUTO_EXTRACT_RESOURCES "Restore missing game resources from a local ZIP" ON)
set(PVZ_RESOURCE_ARCHIVE "${PROJECT_SOURCE_DIR}/game-resources.zip" CACHE FILEPATH "Game resource ZIP bundle")

if(PVZ_AUTO_EXTRACT_RESOURCES AND EXISTS "${PVZ_RESOURCE_ARCHIVE}")
	find_package(Python3 3.7 REQUIRED COMPONENTS Interpreter)
	set(PVZ_RESOURCE_SCRIPT "${PROJECT_SOURCE_DIR}/scripts/game_resources.py")
	execute_process(
		COMMAND "${Python3_EXECUTABLE}" "${PVZ_RESOURCE_SCRIPT}" unpack
			--root "${PROJECT_SOURCE_DIR}" --archive "${PVZ_RESOURCE_ARCHIVE}"
		RESULT_VARIABLE PVZ_RESOURCE_RESULT
	)
	if(NOT PVZ_RESOURCE_RESULT EQUAL 0)
		message(FATAL_ERROR "Could not prepare game resources from ${PVZ_RESOURCE_ARCHIVE}")
	endif()
	add_custom_target(pvz-resources
		COMMAND "${Python3_EXECUTABLE}" "${PVZ_RESOURCE_SCRIPT}" unpack
			--root "${PROJECT_SOURCE_DIR}" --archive "${PVZ_RESOURCE_ARCHIVE}"
		VERBATIM
	)
endif()
