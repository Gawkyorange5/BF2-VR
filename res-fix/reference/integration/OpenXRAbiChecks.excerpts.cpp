// Reference excerpts only: these blocks belong to different existing scopes.
// Do not compile this file as a standalone translation unit.

// Compile-time x86 ABI checks after including the OpenXR SDK headers.
// Origin: RFX/XrSessionClient.cpp, source line 16
// Shared ABI contract with the isolated C# x86 resolution probe.
static_assert(sizeof(XrInstance)==8&&sizeof(XrInstanceCreateInfo)==304,"OpenXR x86 instance ABI");
static_assert(offsetof(XrInstanceCreateInfo,applicationInfo)==16&&offsetof(XrApplicationInfo,apiVersion)==264,"OpenXR application offsets");
static_assert(sizeof(XrSystemGetInfo)==12&&sizeof(XrViewConfigurationView)==32,"OpenXR x86 view ABI");
static_assert(offsetof(XrViewConfigurationView,recommendedImageRectWidth)==8&&offsetof(XrViewConfigurationView,recommendedImageRectHeight)==16,"OpenXR view offsets");
static_assert(sizeof(XrInstanceProperties)==144&&offsetof(XrInstanceProperties,runtimeName)==16,"OpenXR runtime property ABI");
