pushd "UE4"
git clean -ffxd
popd
git clean -ffxd

pushd "UE4"
./Setup.sh
./GenerateProjectFiles.sh
popd