add_executable(pvz-rules-tests
	GameplayRulesTests.cpp
	../src/Lawn/Plant/PlantRules.cpp
	../src/Lawn/Rules/PlantingRules.cpp
	../src/Lawn/Projectile/ProjectileRules.cpp
	../src/Lawn/Rules/TargetingRules.cpp
	../src/Lawn/Rules/WaveRules.cpp
	../src/Lawn/Zombie/ZombieStatusRules.cpp
	../src/Lawn/Zombie/ZombieStrengthRules.cpp
	../src/Lawn/System/SaveGameFormat.cpp
)
target_compile_features(pvz-rules-tests PRIVATE cxx_std_20)
set_target_properties(pvz-rules-tests PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(pvz-rules-tests PRIVATE ../src)
target_link_libraries(pvz-rules-tests PRIVATE ZLIB::ZLIB)
foreach(suite IN ITEMS strength targeting-placement waves cold save-format)
	add_test(NAME ${suite} COMMAND pvz-rules-tests ${suite})
endforeach()
