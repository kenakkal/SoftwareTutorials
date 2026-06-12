#!/bin/bash
#setting up the environment variables 
#source /cvmfs/sw.hsf.org/key4hep/setup.sh
#cd SoftwareTutorials
#k4_local_repo
echo "current working directory is $(pwd)"
cd ../build
cmake .. -DCMAKE_INSTALL_PREFIX=../install
make install -j6
cd ../DD4hepTutorials/
echo "your present working directory is $(pwd)" 