echo "Configuring and building companion project..."

echo "Updating submodules..."
git submodule init 
git submodule update --recursive
cd modules/mavlink

git submodule init 
git submodule update --recursive

cd ../..


echo "Generating mavlink headders..."
python ./modules/mavlink/pymavlink/tools/mavgen.py --lang C++11 ./modules/mavlink/message_definitions/v1.0/ardupilotmega.xml -o ./generated/mavlink --wire-protocol=2.0

echo "Doing the build..."
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j4

