#!/bin/sh
# Must use LF line endings

#-------------------------------------
# Setup Variables

BUILD_CONFIG=$1 #Shipping or Development
SCRIPT_DIR=$(pwd);
MODUMATE_PATH="${SCRIPT_DIR}/../"
UE4_PATH="${SCRIPT_DIR}/../../UE4"

oldDir=$(pwd)
highlight=$(tput setaf 2)
normal=$(tput sgr0)

#-------------------------------------
# Setup Environment and Mono
cd "$UE4_PATH"
source Engine/Build/BatchFiles/Mac/SetupEnvironment.sh -mono Engine/Build/BatchFiles/Mac

#-------------------------------------
# Do the building
for target in "${@:2}"
do
	echo "${highlight}Building ${target} Mac ${BUILD_CONFIG}${normal}"
    mono Engine/Binaries/DotNET/UnrealBuildTool.exe $target Mac $BUILD_CONFIG $MODUMATE_PATH/Modumate.uproject -architecture=x86_64
	if [ $? -ne 0 ]; then
        exit $?
    fi
done