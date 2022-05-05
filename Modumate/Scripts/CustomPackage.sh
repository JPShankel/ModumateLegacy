#This file is only for reference. Jenkins does its own thing

oldDir=$(pwd)
cd ../../
python3 Modumate/Scripts/utils.py get_project_version > ~/Documents/BuildVersion.txt
python3 Modumate/Scripts/utils.py get_engine_version > ~/Documents/EngineVersion.txt
export BUILD_VERSION=`cat ~/Documents/BuildVersion.txt`
export ENGINE_VERSION=`cat ~/Documents/EngineVersion.txt`

#https://forums.unrealengine.com/t/build-broken-on-mac-with-built-from-changelist-non-zero/346102/2
./UE4/Engine/Build/BatchFiles/Mac/Build.sh UnrealHeaderTool Mac Development -clean
./UE4/Engine/Build/BatchFiles/Mac/Build.sh UnrealHeaderTool Mac Shipping -clean

./UE4/Engine/Build/BatchFiles/RunUAT.sh -ScriptsForProject=/Users/nourij/Code/ModumateGit/Modumate/Modumate.uproject BuildCookRun -nop4 -project=/Users/nourij/Code/ModumateGit/Modumate/Modumate.uproject -nocompilededitor -cook -stage -archive -archivedirectory=/Users/nourij/Documents/$BUILD_VERSION/ModumatePackaged -package -ue4exe=/Users/nourij/Code/ModumateGit/UE4/Engine/Binaries/Mac/UE4Editor.app/Contents/MacOS/UE4Editor -compressed -ddc=DerivedDataBackendGraph -pak -preeqs -distribution -nodebuginfo -manifests -platform=Mac -build -CrashReporter -architecture=x86_64 -target=Modumate -clientconfig=Shipping -utf8output

if [ $? -eq 0 ]; then
    ./Modumate/Scripts/MacPostBuild.sh /Users/nourij/Code/ModumateGit/Modumate Modumate Shipping /Users/nourij/Documents/$BUILD_VERSION/ModumatePackaged/MacNoEditor/Modumate-Mac-Shipping.app
fi

cd $oldDir