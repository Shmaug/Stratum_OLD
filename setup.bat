@echo off

set "STRATUM_DIR=%cd%"
set "ASSIMP_DIR=%cd%/ThirdParty/assimp"
set "SHADERC_DIR=%cd%/ThirdParty/shaderc"
set "SPIRV_CROSS_DIR=%cd%/ThirdParty/shaderc/third_party/spirv-cross"

echo Updating submodules...
git submodule update --init
echo Submodules updated.

echo Configuring Assimp...
cd ThirdParty/assimp
cmake CMakeLists.txt -GNinja -S "%ASSIMP_DIR%" -B "%ASSIMP_DIR%" -Wno-dev -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DASSIMP_BUILD_TESTS=OFF -DASSIMP_BUILD_ASSIMP_TOOLS=OFF -DASSIMP_BUILD_ZLIB=ON -DINJECT_DEBUG_POSTFIX=OFF -DLIBRARY_SUFFIX="" -DCMAKE_INSTALL_PREFIX="%ASSIMP_DIR%"
echo Assimp configured.

echo Configuring Shaderc...
cd ../shaderc
python utils/git-sync-deps
cmake CMakeLists.txt -GNinja -S "%SHADERC_DIR%" -B "%SHADERC_DIR%" -Wno-dev -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DSHADERC_SKIP_TESTS=ON -DBUILD_TESTING=OFF -DSPIRV_SKIP_EXECUTABLES=ON -DCMAKE_INSTALL_PREFIX="%SHADERC_DIR%"
echo Shaderc configured.

echo Configuring SPIRV-cross...
cd third_party/spirv-cross
cmake CMakeLists.txt -GNinja -S "%SPIRV_CROSS_DIR%" -B "%SPIRV_CROSS_DIR%" -Wno-dev -DCMAKE_BUILD_TYPE=Release -DSPIRV_CROSS_SHARED=OFF -DSPIRV_CROSS_ENABLE_TESTS=OFF -DCMAKE_INSTALL_PREFIX="%SPIRV_CROSS_DIR%" 
echo SPIRV-cross configured.


echo Building Assimp...
cd "%ASSIMP_DIR%"
cmake --build . --config Release --target install
echo Assimp built.

echo Building Shaderc...
cd "%SHADERC_DIR%"
cmake --build . --config Release --target add-copyright
cmake --build . --config Release --target install
echo Shaderc built.

echo Building SPIRV-cross...
cd "%SPIRV_CROSS_DIR%"
cmake --build . --config Release --target install
echo SPIRV-cross built.

cd %STRATUM_DIR%