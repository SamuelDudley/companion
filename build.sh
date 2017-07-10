echo "Configuring and building companion project..."

python ./modules/mavlink/pymavlink/tools/mavgen.py --lang C++11 ./modules/mavlink/message_definitions/v1.0/ardupilotmega.xml -o ./generated/mavlink --wire-protocol=2.0

mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j4

