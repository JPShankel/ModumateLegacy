echo "Updating Nightly Build"

rm -f /Volumes/Samsung_T5/NightlyBuild/*(N)
cd /Users/buildy/Documents/$1/
zip -ry /Volumes/Samsung_T5/NightlyBuild/MacNightlyBuild.zip ./ModumatePackaged
cd -
aws s3 cp /Volumes/Samsung_T5/NightlyBuild/MacNightlyBuild.zip s3://modumate-latest/client/Nightly/ --acl public-read

