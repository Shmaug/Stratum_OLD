@echo off

set "STRATUM_DIR=%cd%"
set "ASSIMP_DIR=%cd%/ThirdParty/assimp"
set "SHADERC_DIR=%cd%/ThirdParty/shaderc"
set "SPIRV_CROSS_DIR=%cd%/ThirdParty/shaderc/third_party/spirv-cross"

echo Updating submodules...
git submodule update --init
echo Submodules updated.

echo Configuring Assimp...
cd "%ASSIMP_DIR%"
cmake CMakeLists.txt -S "%ASSIMP_DIR%" -B "%ASSIMP_DIR%" -Wno-dev -DCMAKE_BUILD_TYPE=Release -DASSIMP_BUILD_ASSIMP_TOOLS=OFF -DBUILD_SHARED_LIBS=OFF -DASSIMP_BUILD_TESTS=OFF -DASSIMP_BUILD_ZLIB=ON -DINJECT_DEBUG_POSTFIX=OFF -DLIBRARY_SUFFIX="" -DCMAKE_INSTALL_PREFIX="%ASSIMP_DIR%"
echo Assimp configured.
echo Building Assimp...
cmake --build . --config Release --target install
echo Assimp built.


cd "%SHADERC_DIR%"
python utils/git-sync-deps

echo Configuring Shaderc...
cmake CMakeLists.txt -S "%SHADERC_DIR%" -B "%SHADERC_DIR%" -Wno-dev -DCMAKE_BUILD_TYPE=Release -DSHADERC_ENABLE_SHARED_CRT=ON -DLLVM_USE_CRT_DEBUG=MDd -DLLVM_USE_CRT_MINSIZEREL=MD -DLLVM_USE_CRT_RELEASE=MD -DLLVM_USE_CRT_RELWITHDEBINFO=MD -DBUILD_SHARED_LIBS=OFF -DSHADERC_SKIP_TESTS=ON -DSPIRV_SKIP_EXECUTABLES=ON -DBUILD_TESTING=OFF -DCMAKE_INSTALL_PREFIX="%SHADERC_DIR%"
echo Shaderc configured.
echo Building Shaderc...
cmake --build . --config Release --target add-copyright
cmake --build . --config Release --target install
echo Shaderc built.

echo Configuring SPIRV-cross...
cd "%SPIRV_CROSS_DIR%"
cmake CMakeLists.txt -S "%SPIRV_CROSS_DIR%" -B "%SPIRV_CROSS_DIR%" -Wno-dev -DCMAKE_BUILD_TYPE=Release -DSPIRV_CROSS_SHARED=OFF -DSPIRV_CROSS_STATIC=ON -DSPIRV_CROSS_ENABLE_TESTS=OFF -DCMAKE_INSTALL_PREFIX="%SPIRV_CROSS_DIR%" 
echo SPIRV-cross configured.
echo Building SPIRV-cross...
cmake --build . --config Release --target install
echo SPIRV-cross built.

cd %STRATUM_DIR%