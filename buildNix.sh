#! /bin/sh

#call: buildNix.sh distributionName version
# e.g. buildNix.sh CentOS7 4.0.0.49

if [ `uname -m` = 'x86_64' ]; 
then
  ARCH=x64
else
  ARCH=Arm64
fi

rm -f OSPSuite.FuncParser4Nix.sln

dotnet restore

# copy the original solution file because it will be modified for dotnet build
cp -p -f OSPSuite.FuncParser.sln OSPSuite.FuncParser4Nix.sln

dotnet sln OSPSuite.FuncParser4Nix.sln remove src/OSPSuite.FuncParserNative/OSPSuite.FuncParserNative.vcxproj

for BuildType in Debug Release 
do 
  cmake -BBuild/${BuildType}/$ARCH/ -Hsrc/OSPSuite.FuncParserNative/ -DCMAKE_BUILD_TYPE=${BuildType} 
  make -C Build/${BuildType}/$ARCH 
  dotnet build OSPSuite.FuncParser4Nix.sln /property:Configuration=${BuildType} 
done

dotnet test OSPSuite.FuncParser4Nix.sln --no-build --no-restore --configuration:Release --logger:"html;LogFileName=../../../testLog_$1.html"

dotnet pack src/OSPSuite.FuncParser/ -p:PackageVersion=$2 -o:./
