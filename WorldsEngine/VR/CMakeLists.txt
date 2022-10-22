set(worlds_vr_src VR/FakeVRInterface.cpp)

if(WORLDS_USE_OPENVR)
    list(APPEND worlds_vr_src VR/OpenVRInterface.cpp)
endif()

if(WORLDS_USE_OPENXR)
    list(APPEND worlds_vr_src VR/OpenXRInterface.cpp)
endif()