r2_sources = []

subdir('Source')

r2_public_include_dir = include_directories('PublicInclude')
r2_private_include_dir = include_directories('PrivateInclude')

# extern include dir is needed for Vulkan headers
r2 = static_library('r2', r2_sources,
    include_directories: [extern_include_dir, r2_public_include_dir, r2_private_include_dir],
    cpp_args: '-DVK_USE_PLATFORM_WIN32_KHR'
)

r2_dep = declare_dependency(include_directories: r2_public_include_dir, link_with: r2)