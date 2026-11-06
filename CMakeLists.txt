cmake_minimum_required(VERSION 3.30)
project(apache VERSION 0.1.0 LANGUAGES C CXX)
if(APPLE)
  enable_language(OBJC)
endif()
option(APACHE_STATIC_RUNTIME "Link compiler runtimes statically" ${MINGW})
if(APACHE_STATIC_RUNTIME)
  if(MINGW)
    add_link_options(-static)
  else()
    add_link_options("$<$<LINK_LANG_AND_ID:CXX,GNU>:-static-libgcc;-static-libstdc++>")
  endif()
endif()
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
include(CTest)

add_library(apache_core
  src/configuration/graphics.cpp src/configuration/graphics.h
  src/render/display_layout.cpp src/render/display_layout.h
  src/frontend/multiplayer_chat.cpp src/frontend/multiplayer_chat.h
  src/multiplayer/peer_group.cpp src/multiplayer/peer_group.h
  src/multiplayer/connections.cpp src/multiplayer/connections.h
  src/multiplayer/lobby.cpp src/multiplayer/lobby.h
  src/platform/datagram.cpp src/platform/datagram.h
  src/multiplayer/transport.cpp src/multiplayer/transport.h
  src/multiplayer/packet.cpp src/multiplayer/packet.h
  src/multiplayer/crew.cpp src/multiplayer/crew.h
  src/multiplayer/replication.cpp src/multiplayer/replication.h
  src/multiplayer/lifecycle.cpp src/multiplayer/lifecycle.h
  src/multiplayer/casualties.cpp src/multiplayer/casualties.h
  src/multiplayer/rules.cpp src/multiplayer/rules.h
  src/simulation/remote_weapons.cpp src/simulation/remote_weapons.h
  src/simulation/mission_network.cpp src/simulation/mission_network.h
  src/simulation/mission_crew.cpp src/simulation/mission_crew.h
  src/simulation/mission_end.cpp src/simulation/mission_end.h
  src/audio/engine.cpp src/audio/engine.h
  src/audio/effects.cpp src/audio/effects.h src/audio/stream.cpp src/audio/stream.h
  src/frontend/score_menu.cpp
  src/frontend/score_menu.h
  src/simulation/quick_start.h
  src/math/geometry.cpp
  src/math/geometry.h
  src/math/fixed.h
  src/math/trigonometry.cpp
  src/math/trigonometry.h
  src/resources/features.cpp
  src/resources/features.h
  src/render/sector_scan.cpp
  src/render/sector_scan.h
  src/render/feature_renderer.cpp
  src/render/feature_renderer.h
  src/resources/model_metadata.cpp
  src/resources/missions.cpp
  src/resources/missions.h
  src/resources/models.cpp
  src/resources/models.h
  src/frontend/pilot_menu.cpp
  src/frontend/pilot_menu.h
  src/frontend/mission_frontend_network.cpp
  src/frontend/mission_frontend.cpp
  src/frontend/mission_frontend.h
  src/frontend/network_menu.cpp src/frontend/network_menu.h
  src/frontend/mission_link.cpp src/frontend/mission_link.h
  src/frontend/mission_session.cpp
  src/frontend/mission_session_network.cpp
  src/frontend/mission_session.h
  src/frontend/briefing_map.cpp
  src/frontend/briefing_map.h
  src/frontend/map_editor.cpp src/frontend/map_editor.h
  src/frontend/map_camera.cpp
  src/frontend/map_camera.h
  src/frontend/map_view.cpp
  src/frontend/map_view.h
  src/frontend/map_routes.cpp
  src/frontend/map_routes.h
  src/frontend/map_layers.cpp
  src/frontend/map_layers.h
  src/frontend/presentation.cpp src/frontend/presentation.h
  src/frontend/help_menu.cpp src/frontend/help_menu.h
  src/frontend/menu.cpp
  src/frontend/menu.h
  src/frontend/widget.cpp
  src/frontend/widget.h
  src/render/terrain_renderer.cpp
  src/render/terrain_renderer.h
  src/render/terrain_shading.cpp
  src/render/terrain_shading.h
  src/resources/terrain_textures.cpp
  src/resources/terrain_textures.h
  src/render/terrain_projection.cpp
  src/render/terrain_projection.h
  src/render/draw_queue.cpp
  src/render/draw_queue.h
  src/render/mobile_scene.cpp
  src/render/mobile_scene.h
  src/render/shadows.cpp
  src/render/shadows.h
  src/render/model_renderer.cpp
  src/render/model_renderer.h
  src/render/avionics.cpp
  src/render/avionics.h
  src/render/helmet_display.cpp
  src/render/helmet_display.h
  src/render/mfd_flight_plan.cpp
  src/render/mfd_radar.cpp
  src/render/mfd_weapons.cpp
  src/render/map_renderer.cpp
  src/render/map_renderer.h
  src/render/mfd.cpp
  src/render/mfd.h
  src/render/panel_instruments.cpp
  src/render/panel_instruments.h
  src/render/cockpit.cpp
  src/render/cockpit.h
  src/render/instrument_font.cpp
  src/render/instrument_font.h
  src/render/palette_effects.cpp
  src/render/palette_effects.h
  src/resources/executable.cpp
  src/resources/executable.h
  src/render/scene_palette.cpp
  src/render/scene_palette.h
  src/simulation/mission_debrief.cpp
  src/simulation/mission_debrief.h
  src/simulation/debrief.cpp
  src/simulation/debrief.h
  src/simulation/pilot_log.cpp
  src/simulation/pilot_log.h
  src/simulation/communications.cpp
  src/simulation/communications.h
  src/simulation/mission_player_weapons.cpp
  src/simulation/mission_player_weapons.h
  src/simulation/view_modes.cpp
  src/simulation/player_views.cpp
  src/simulation/mission_instruments.cpp
  src/simulation/mission_instruments.h
  src/simulation/player_views.h
  src/simulation/daylight.h
  src/simulation/sight_geometry.h
  src/simulation/daylight.cpp
  src/simulation/sight_geometry.cpp
  src/simulation/view_modes.h
  src/simulation/camera_geometry.cpp
  src/simulation/camera_geometry.h
  src/simulation/player_route.cpp
  src/simulation/player_route.h
  src/simulation/player_sensors.cpp
  src/simulation/player_sensors.h
  src/simulation/player_targeting.cpp
  src/simulation/player_targeting.h
  src/simulation/player_warnings.cpp
  src/simulation/player_warnings.h
  src/simulation/player_sight.cpp
  src/simulation/player_sight.h
  src/simulation/player_weapon_mounts.cpp
  src/simulation/player_weapon_mounts.h
  src/simulation/player_weapons.cpp
  src/simulation/player_weapons.h
  src/simulation/projectile_dynamics.cpp
  src/simulation/projectile_dynamics.h
  src/simulation/projectile_launch.cpp
  src/simulation/projectile_launch.h
  src/simulation/mission_combat.cpp src/simulation/mission_weapons.cpp src/simulation/mission_damage.cpp src/simulation/mission_flight.cpp src/simulation/mission_player_damage.cpp src/simulation/mission_runtime.cpp src/simulation/control_input.cpp
  src/simulation/mission_targets.cpp
  src/simulation/mission_spatial.cpp
  src/simulation/mission_radar.cpp
  src/simulation/mission_scenery.cpp
  src/simulation/combat_update.cpp
  src/simulation/war_grid.cpp
  src/simulation/battles.cpp
  src/simulation/combat_air_patrol.cpp
  src/simulation/actor_targeting.cpp
  src/simulation/actor_reactions.cpp
  src/simulation/actor_firing.cpp
  src/simulation/firing_solution.cpp
  src/simulation/firing_solution.h
  src/simulation/combat_effects.cpp
  src/simulation/combat_effects.h
  src/simulation/blast_damage.cpp
  src/simulation/blast_damage.h
  src/simulation/damage.cpp
  src/simulation/damage.h
  src/simulation/countermeasures.cpp
  src/simulation/countermeasures.h
  src/simulation/projectile_tracking.cpp
  src/simulation/projectile_tracking.h
  src/simulation/projectiles.cpp
  src/simulation/projectiles.h
  src/simulation/terrain_visibility.cpp
  src/simulation/terrain_visibility.h
  src/simulation/target_search.cpp
  src/simulation/target_visibility.cpp
  src/simulation/target_visibility.h
  src/simulation/targets.cpp
  src/simulation/targets.h
  src/simulation/seeker.cpp
  src/simulation/seeker.h
  src/simulation/collisions.cpp
  src/simulation/collisions.h
  src/simulation/actor_movement.cpp
  src/simulation/actor_movement.h
  src/simulation/actor_navigation.cpp
  src/simulation/actor_navigation.h
  src/simulation/ground_movement.cpp
  src/simulation/ground_movement.h
  src/simulation/mission_actors.cpp
  src/simulation/mission_actors.h
  src/simulation/weapon_info.cpp
  src/simulation/weapon_info.h
  src/simulation/weapon_rules.cpp
  src/simulation/weapon_rules.h
  src/simulation/actor_types.cpp
  src/simulation/actor_types.h
  src/simulation/arcade_flight.cpp
  src/simulation/arcade_flight.h
  src/simulation/realistic_flight.cpp
  src/simulation/realistic_flight.h
  src/simulation/rotor_dynamics.cpp
  src/simulation/rotor_dynamics.h
  src/simulation/flight_systems.cpp
  src/simulation/flight_systems.h
  src/simulation/instrument_state.cpp
  src/simulation/instrument_state.h
  src/simulation/mfd_state.cpp
  src/simulation/mfd_state.h
  src/simulation/fragments.cpp
  src/simulation/fragments.h
  src/render/fragment_renderer.cpp
  src/render/fragment_renderer.h
  src/simulation/craters.cpp
  src/simulation/craters.h
  src/render/crater_renderer.cpp
  src/render/crater_renderer.h
  src/simulation/effects.cpp
  src/simulation/effects.h
  src/render/effect_renderer.cpp
  src/render/effect_renderer.h
  src/render/clouds.cpp
  src/render/clouds.h
  src/render/celestial.cpp
  src/render/celestial.h
  src/render/landscape.cpp
  src/render/landscape.h
  src/render/horizon.cpp
  src/render/horizon.h
  src/render/projection2.cpp
  src/render/projection2.h
  src/render/projection.cpp
  src/render/projection.h
  src/render/perspective_rasterizer.cpp
  src/render/rasterizer.cpp
  src/render/rasterizer.h
  src/render/framebuffer.cpp
  src/render/framebuffer.h
  src/render/menu_font.cpp
  src/render/menu_font.h
  src/resources/reader.cpp
  src/resources/reader.h
  src/resources/installation.cpp src/resources/installation.h src/resources/reference_files.inc
  src/resources/data_store.cpp
  src/resources/data_store.h
  src/resources/animation.cpp src/resources/animation.h
  src/resources/image.cpp
  src/resources/image.h
  src/resources/texture.cpp
  src/resources/texture.h
  src/resources/terrain.cpp
  src/resources/terrain.h
  src/configuration/settings.cpp
  src/configuration/settings.h
  src/frontend/preferences_menu.cpp
  src/frontend/preferences_menu.h
  src/configuration/profiles.cpp
  src/configuration/profiles.h
  src/configuration/preferences.cpp
  src/configuration/preferences.h
)
target_include_directories(apache_core PUBLIC src)
target_compile_options(apache_core PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Wconversion;-ffp-contract=off>
)

include(cmake/dependencies.cmake)
add_library(pcm_device STATIC src/platform/miniaudio_implementation.c)
target_include_directories(pcm_device SYSTEM PUBLIC "${miniaudio_SOURCE_DIR}")
target_compile_definitions(pcm_device PUBLIC MA_NO_ENGINE MA_NO_RESOURCE_MANAGER MA_NO_NODE_GRAPH MA_NO_ENCODING MA_NO_GENERATION MA_NO_DECODING MA_NO_NULL)
target_link_libraries(pcm_device PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
if(UNIX)
  target_link_libraries(pcm_device PRIVATE m)
endif()
if(APPLE)
  set_source_files_properties(src/platform/miniaudio_implementation.c PROPERTIES LANGUAGE OBJC)
endif()
target_link_libraries(apache_core PUBLIC Threads::Threads)
add_executable(apache
  src/platform/audio_output.cpp
  src/platform/audio_output.h
  src/main.cpp
  src/platform/key_map.cpp
  src/platform/key_map.h
  src/platform/window.cpp
  src/platform/window.h
)
target_link_libraries(apache PRIVATE apache_core glfw OpenGL::GL pcm_device)
target_compile_options(apache PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Wconversion>)

if(WIN32)
  target_link_libraries(apache_core PUBLIC ws2_32)
endif()

add_executable(apache_inspect src/inspect_chat.cpp src/inspect_network_live.cpp src/inspect_network.cpp src/inspect_network.h src/inspect_briefing.cpp src/inspect_briefing.h src/inspect_audio.cpp src/inspect_audio.h src/inspect_menu.cpp src/inspect_menu.h src/inspect_debrief.cpp src/inspect_debrief.h src/inspect_logbook.cpp src/inspect_logbook.h src/inspect_session.cpp src/inspect_session.h src/inspect_cameras.cpp src/inspect_cameras.h src/inspect_sensors.cpp src/inspect_sensors.h src/inspect_targeting.cpp src/inspect_targeting.h src/inspect_player_weapons.cpp src/inspect_controls.cpp src/inspect.cpp src/inspect_flight.cpp src/inspect_flight.h src/inspect_arcade.cpp src/inspect_arcade.h src/inspect_weapons.cpp src/inspect_weapons.h src/inspect_navigation.cpp src/inspect_navigation.h src/inspect_collisions.cpp src/inspect_collisions.h src/inspect_seekers.cpp src/inspect_seekers.h src/inspect_projectiles.cpp src/inspect_projectiles.h src/inspect_projectile_pool.cpp src/inspect_projectile_pool.h src/inspect_targets.cpp src/inspect_targets.h src/inspect_tracking.cpp src/inspect_tracking.h src/inspect_countermeasures.cpp src/inspect_countermeasures.h src/inspect_damage.cpp src/inspect_damage.h src/inspect_blasts.cpp src/inspect_blasts.h src/inspect_combat_effects.cpp src/inspect_combat_effects.h src/inspect_mission_combat.cpp src/inspect_mission_targets.cpp src/inspect_mission_spatial.cpp src/inspect_mission_radar.cpp src/inspect_mission_scenery.cpp src/inspect_combat_update.cpp src/inspect_war_grid.cpp src/inspect_battles.cpp src/inspect_patrol.cpp src/inspect_actor_targeting.cpp src/inspect_target_search.cpp src/inspect_actor_reactions.cpp src/inspect_actor_firing.cpp src/inspect_firing.cpp src/inspect_firing.h)
target_link_libraries(apache_inspect PRIVATE apache_core)

if(BUILD_TESTING)
  add_executable(redux_aircraft_check tests/redux_aircraft.cpp)
  target_link_libraries(redux_aircraft_check PRIVATE apache_core)
  add_executable(graphics_scene_check tests/graphics_scene.cpp)
  target_link_libraries(graphics_scene_check PRIVATE apache_core)
  add_executable(graphics_tests tests/graphics.cpp)
  target_link_libraries(graphics_tests PRIVATE apache_core)
  add_test(NAME graphics_tests COMMAND graphics_tests)
  add_executable(transport_tests tests/transport.cpp)
  target_link_libraries(transport_tests PRIVATE apache_core)
  add_test(NAME transport_tests COMMAND transport_tests)
  add_executable(network_tests tests/network.cpp)
  target_link_libraries(network_tests PRIVATE apache_core)
  add_test(NAME network_tests COMMAND network_tests)
  add_executable(profile_tests tests/profiles.cpp)
  target_link_libraries(profile_tests PRIVATE apache_core)
  add_test(NAME profile_tests COMMAND profile_tests)
  add_executable(resource_tests tests/resources.cpp)
  target_link_libraries(resource_tests PRIVATE apache_core)
  add_test(NAME resource_tests COMMAND resource_tests)
  add_executable(scene_tests tests/scene.cpp)
  target_link_libraries(scene_tests PRIVATE apache_core)
  add_test(NAME scene_tests COMMAND scene_tests)
endif()

include(cmake/install.cmake)
