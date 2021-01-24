sources = [
    'IOUtil.cpp',
    'imgui_impl_sdl.cpp',
    'imgui_impl_vulkan.cpp',
    'imgui_stdlib.cpp',
    'DefaultUITheme.cpp',
    'EarlySDLUtil.cpp',
    'LuaVM.cpp',
    'ShaderMetadata.cpp',
    'SplashWindow.cpp',
    'StyleEditor.cpp',
    'VTFLoader.cpp',
    'imgui.cpp',
    'imgui_demo.cpp',
    'imgui_draw.cpp',
    'imgui_widgets.cpp',
    'weldmesh.cpp',
    'GameControls.cpp',
    'Audio.cpp',
    'EntityList.cpp',
    'ImGuizmo.cpp',
    'physfs.cpp',
    'vkswapchain.cpp',
    'JobSystem.cpp',
    'WMDLLoader.cpp',
    'AssetDB.cpp',
    'AssetDBExplorer.cpp',
    'Assets.cpp',
    'Fatal.cpp',
    'ImGuiRenderPass.cpp',
    'Input.cpp',
    'ObjModelLoader.cpp',
    'RichPresence.cpp',
    'RobloxMeshLoader.cpp',
    'ShadowmapRenderPass.cpp',
    'TonemapRenderPass.cpp',
    'VRCullMeshRenderer.cpp',
    'BRDFLUTRenderer.cpp',
    'BlurRenderPass.cpp',
    'GTAORenderPass.cpp',
    'MaterialEditor.cpp',
    'PipelineCacheSerializer.cpp',
    'RenderTexture.cpp',
    'ShaderCache.cpp',
    'TextureLoader.cpp',
    'VKImGUIUtil.cpp',
    'ComponentFuncs.cpp',
    'Console.cpp',
    'CubemapConvoluter.cpp',
    'CubemapLoader.cpp',
    'D6Joint.cpp',
    'ESceneSerializationFunctions.cpp',
    'Editor.cpp',
    'Engine.cpp',
    'EntityEditor.cpp',
    'GuiUtil.cpp',
    'MaterialSlots.cpp',
    'OpenVRInterface.cpp',
    'Physics.cpp',
    'PolyRenderPass.cpp',
    'SceneSerialization.cpp',
    'SourceModelLoader.cpp',
    'vkrender.cpp',
    'imgui_impl_sdl.h',
    'tiny_obj_loader.h',
    'ImGuizmo.h',
    'imgui_impl_vulkan.h',
    'imgui_stdlib.h',
    'mikktspace.h',
    'weldmesh.h',
    'IconsFontAwesome5.h',
    'IconsFontaudio.h',
    'imconfig.h',
    'imgui.h',
    'imgui_internal.h',
    'imstb_rectpack.h',
    'imstb_textedit.h',
    'imstb_truetype.h',
    'spirv_reflect.h',
    'spirv_reflect.c',
    'stb_vorbis.c',
    'mikktspace.c',
    'tiny_obj_loader.cc'
]

cc = meson.get_compiler('cpp')

physx_lib_dir = meson.project_source_root() + '/External/Lib/linux64dbg'

if get_option('buildtype').startswith('release')
  physx_lib_dir = meson.project_source_root() + '/External/Lib/linux64rel'
endif

physx_deps = [
    cc.find_library('PhysXCharacterKinematic_static_64', dirs: physx_lib_dir),
    cc.find_library('PhysXCommon_static_64', dirs: physx_lib_dir),
    cc.find_library('PhysXCooking_static_64', dirs: physx_lib_dir),
    cc.find_library('PhysXExtensions_static_64', dirs: physx_lib_dir),
    cc.find_library('PhysXFoundation_static_64', dirs: physx_lib_dir),
    cc.find_library('PhysXPvdSDK_static_64', dirs: physx_lib_dir),
    cc.find_library('PhysX_static_64', dirs: physx_lib_dir),
    cc.find_library('PhysXVehicle_static_64', dirs: physx_lib_dir),
    cc.find_library('SampleBase_static_64', dirs: physx_lib_dir),
    cc.find_library('SampleFramework_static_64', dirs: physx_lib_dir),
    cc.find_library('SamplePlatform_static_64', dirs: physx_lib_dir),
    cc.find_library('SampleRenderer_static_64', dirs: physx_lib_dir),
    cc.find_library('SamplesToolkit_static_64', dirs: physx_lib_dir),
    cc.find_library('SnippetRender_static_64', dirs: physx_lib_dir),
    cc.find_library('SnippetUtils_static_64', dirs: physx_lib_dir)
]

worlds_inc = include_directories('.')
tracy_inc = include_directories('./tracy/')
physfs_dep = dependency('physfs')
vk_dep = dependency('vulkan')
lua_dep = dependency('lua')
tbb_dep = cc.find_library('tbb')
enet_dep = cc.find_library('enet')

dl_dep = cc.find_library('dl', required : false)
openvr_dep = cc.find_library('openvr_api')
discord_csdk_dep = cc.find_library('discord_game_sdk', dirs: meson.project_source_root() + '/External/Lib')

worlds = static_library('worlds', sources, 
  include_directories: [extern_include_dir, physx_include_dir, tracy_inc], 
  dependencies: [vk_dep, physfs_dep, sdl2_dep, physx_deps, dl_dep, openvr_dep, lua_dep, wmdl_dep, enet_dep, discord_csdk_dep, discord_sdk_dep, tbb_dep])
worlds_dep = declare_dependency(include_directories: worlds_inc, link_with: worlds)
