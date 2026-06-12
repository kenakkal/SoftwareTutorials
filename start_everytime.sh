#!/bin/bash
#setting up the environment variables 
source /cvmfs/sw.hsf.org/key4hep/setup.sh
#cd SoftwareTutorials
k4_local_repo
cd build
make 
cd ../DD4hepTutorials/
echo "your present working directory is $(pwd)" 