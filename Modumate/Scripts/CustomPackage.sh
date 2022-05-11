#This file is only for reference. Jenkins does its own thing

oldDir=$(pwd)
cd ../../

GitDirectory=$(pwd)

python3 Modumate/Scripts/utils.py get_project_version > ~/Documents/BuildVersion.txt
python3 Modumate/Scripts/utils.py get_engine_version > ~/Documents/EngineVersion.txt
export BUILD_VERSION=`cat ~/Documents/BuildVersion.txt`
export ENGINE_VERSION=`cat ~/Documents/EngineVersion.txt`

#https://forums.unrealengine.com/t/build-broken-on-mac-with-built-from-changelist-non-zero/346102/2
./UE4/Engine/Build/BatchFiles/Mac/Build.sh UnrealHeaderTool Mac Development -clean
./UE4/Engine/Build/BatchFiles/Mac/Build.sh UnrealHeaderTool Mac Shipping -clean

#Because we cleaned the editor, we now have to rebuild it
./UE4/Engine/Build/BatchFiles/Mac/Build.sh ModumateEditor Mac Development -WaitMutex

#build the project
./UE4/Engine/Build/BatchFiles/RunUAT.sh -ScriptsForProject=$GitDirectory/Modumate/Modumate.uproject BuildCookRun -nop4 -project=$GitDirectory/Modumate/Modumate.uproject -cook -stage -archive -archivedirectory=~/Documents/$BUILD_VERSION/ModumatePackaged -package -ue4exe=$GitDirectory/UE4/Engine/Binaries/Mac/UE4Editor.app/Contents/MacOS/UE4Editor -nocompilededitor -compressed -ddc=DerivedDataBackendGraph -pak -preeqs -distribution -nodebuginfo -manifests -platform=Mac -build -CrashReporter -architecture=x86_64 -target=Modumate -clientconfig=Shipping -utf8output

#if it all succeeds, then we have to run the post build on the packaged app
if [ $? -eq 0 ]; then
    ./Modumate/Scripts/MacPostBuild.sh $GitDirectory/Modumate Modumate Shipping ~/Documents/$BUILD_VERSION/ModumatePackaged/MacNoEditor/Modumate-Mac-Shipping.app
fi

cd $oldDir