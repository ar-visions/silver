
import: KhronosGroup:Vulkan-Headers

import: KhronosGroup:Vulkan-Loader
    -DVULKAN_HEADERS_INSTALL_DIR=$IMPORT

import: KhronosGroup:MoltenVK

import: KhronosGroup:Vulkan-Tools
    -DVULKAN_HEADERS_INSTALL_DIR=$IMPORT
    -DMOLTENVK_REPO_DIR=$CHECKOUT/MoltenVK
