sources = [
    'AssetDB.cpp',
    'AssetDBExplorer.cpp',
    'Assets.cpp',
    'Audio.cpp',
    'BlurRenderPass.cpp',
    'BRDFLUTRenderer.cpp',
    'ComponentFuncs.cpp',
    'Console.cpp',
    'CubemapConvoluter.cpp',
    'CubemapLoader.cpp',
    'CubemapSlots.cpp',
    'D6Joint.cpp',
    'DefaultUITheme.cpp',
    'EarlySDLUtil.cpp',
    'Editor.cpp',
    'Engine.cpp',
    'EntityEditor.cpp',
    'EntityList.cpp',
    'ESceneSerializationFunctions.cpp',
    'Fatal.cpp',
    'GameControls.cpp',
    'GTAORenderPass.cpp',
    'GuiUtil.cpp',
    'imgui.cpp',
    'ImGuiRenderPass.cpp',
    'ImGuizmo.cpp',
    'imgui_demo.cpp',
    'imgui_draw.cpp',
    'imgui_impl_sdl.cpp',
    'imgui_impl_vulkan.cpp',
    'imgui_stdlib.cpp',
    'imgui_widgets.cpp',
    'Input.cpp',
    'IOUtil.cpp',
    'JobSystem.cpp',
    'MaterialEditor.cpp',
    'MaterialSlots.cpp',
    'ObjModelLoader.cpp',
    'OpenVRInterface.cpp',
    'physfs.cpp',
    'Physics.cpp',
    'PipelineCacheSerializer.cpp',
    'PolyRenderPass.cpp',
    'RenderTexture.cpp',
    'RichPresence.cpp',
    'RobloxMeshLoader.cpp',
    'SceneSerialization.cpp',
    'ScriptBindClass.cpp',
    'ScriptUtil.cpp',
    'ShaderCache.cpp',
    'ShaderMetadata.cpp',
    'ShadowmapRenderPass.cpp',
    'SourceModelLoader.cpp',
    'SplashWindow.cpp',
    'StyleEditor.cpp',
    'TextureLoader.cpp',
    'TonemapRenderPass.cpp',
    'VKImGUIUtil.cpp',
    'vkrender.cpp',
    'vkswapchain.cpp',
    'VRCullMeshRenderer.cpp',
    'VTFLoader.cpp',
    'weldmesh.cpp',
    'WMDLLoader.cpp',
    'WrenVM.cpp',
    'AssetDB.hpp',
    'Audio.hpp',
    'Camera.hpp',
    'ComponentEditorUtil.hpp',
    'ComponentFuncs.hpp',
    'ComponentMetadata.hpp',
    'Console.hpp',
    'CreateModelObject.hpp',
    'CubemapLoader.hpp',
    'cxxopts.hpp',
    'D6Joint.hpp',
    'D6JointEditor.hpp',
    'EarlySDLUtil.hpp',
    'Editor.hpp',
    'EditorWindows.hpp',
    'Engine.hpp',
    'Fatal.hpp',
    'Frustum.hpp',
    'GlmStreamOps.hpp',
    'GuiUtil.hpp',
    'IGameEventHandler.hpp',
    'Input.hpp',
    'IOUtil.hpp',
    'ISystem.hpp',
    'IVRInterface.hpp',
    'JobSystem.hpp',
    'JsonUtil.hpp',
    'Log.hpp',
    'LogCategories.hpp',
    'MatUtil.hpp',
    'MeshManager.hpp',
    'NameComponent.hpp',
    'ObjModelLoader.hpp',
    'OpenVRInterface.hpp',
    'PackedMaterial.hpp',
    'PCH.hpp',
    'physfs.hpp',
    'Physics.hpp',
    'PhysicsActor.hpp',
    'Render.hpp',
    'RendererComponents.hpp',
    'RenderGraph.hpp',
    'RenderPasses.hpp',
    'ResourceSlots.hpp',
    'Result.hpp',
    'RichPresence.hpp',
    'RobloxMeshLoader.hpp',
    'SceneSerialization.hpp',
    'SceneSerializationFuncs.hpp',
    'ScriptComponent.hpp',
    'ScriptUtil.hpp',
    'ShaderCache.hpp',
    'ShaderMetadata.hpp',
    'SourceModelLoader.hpp',
    'SplashWindow.hpp',
    'TextureLoader.hpp',
    'TimingUtil.hpp',
    'Transform.hpp',
    'VKImGUIUtil.hpp',
    'WMDLLoader.hpp',
    'WrenVM.hpp',
    'IconsFontaudio.h',
    'IconsFontAwesome5.h',
    'imconfig.h',
    'imgui.h',
    'ImGuizmo.h',
    'imgui_impl_sdl.h',
    'imgui_impl_vulkan.h',
    'imgui_internal.h',
    'imgui_stdlib.h',
    'imstb_rectpack.h',
    'imstb_textedit.h',
    'imstb_truetype.h',
    'mikktspace.h',
    'spirv_reflect.h',
    'tiny_obj_loader.h',
    'weldmesh.h',
    'mikktspace.c',
    'spirv_reflect.c',
    'stb_vorbis.c',
    'tiny_obj_loader.cc',
]


cc = meson.get_compiler('cpp')

lib_dir = meson.project_source_root() + '/External/Lib/' + host_machine.system() + '64dbg'

if get_option('buildtype').startswith('release')
  lib_dir = meson.project_source_root() + '/External/Lib/' + host_machine.system() + '64rel'
endif

physx_deps = [
    cc.find_library('PhysXCommon_static_64', dirs: lib_dir),
    cc.find_library('PhysXCooking_static_64', dirs: lib_dir),
    cc.find_library('PhysXExtensions_static_64', dirs: lib_dir),
    cc.find_library('PhysXFoundation_static_64', dirs: lib_dir),
    cc.find_library('PhysXPvdSDK_static_64', dirs: lib_dir),
    cc.find_library('PhysX_static_64', dirs: lib_dir),
    cc.find_library('PhysXVehicle_static_64', dirs: lib_dir)
]

worlds_inc = include_directories('.')
tracy_inc = include_directories('./tracy/')
physfs_dep = dependency('physfs', fallback: ['physfs', 'physfs_dep'])
slib_dep = dependency('slib', fallback: ['slib', 'slib_dep'])
vk_dep = dependency('vulkan')
tbb_dep = cc.find_library('tbb', required: false)

enet_subproj = subproject('enet')

enetlib = enet_subproj.get_variable('enetlib')
enet_inc = enet_subproj.get_variable('incdir')

enet_dep = declare_dependency(link_with: enetlib, include_directories: enet_inc)

dl_dep = cc.find_library('dl', required : false)
openvr_dep = cc.find_library('openvr_api', dirs: lib_dir)
discord_csdk_dep = cc.find_library('discord_game_sdk', dirs: meson.project_source_root() + '/External/Lib')

wren_dep = dependency('wren', fallback: ['wren', 'wren_dep'])

platform_deps = []

if host_machine.system() == 'windows'
	platform_deps += cc.find_library('winmm')
	platform_deps += cc.find_library('ws2_32')
endif

worlds = static_library('worlds', sources, 
  include_directories: [extern_include_dir, physx_include_dir, tracy_inc], 
  dependencies: [vk_dep, physfs_dep, sdl2_dep, physx_deps, dl_dep, openvr_dep, wmdl_dep, discord_csdk_dep, discord_sdk_dep, tbb_dep, wren_dep, enet_dep, slib_dep, platform_deps])
worlds_dep = declare_dependency(include_directories: worlds_inc, link_with: worlds)
